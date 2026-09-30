/*
 * (C) 2026 see Authors.txt
 *
 * This file is part of MPC-HC.
 *
 * MPC-HC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-HC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "stdafx.h"

// Reads len bytes starting at offset, tolerating short reads.
// Returns false on any error. The file handle stays owned by the caller.
static bool ReadRange(HANDLE hFile, LONGLONG offset, BYTE* buf, size_t len) {
    LARGE_INTEGER pos = {};
    pos.QuadPart = offset;

    if (!::SetFilePointerEx(hFile, pos, nullptr, FILE_BEGIN))
        return false;

    size_t done = 0;
    while (done < len) {
        DWORD n = 0;
        if (!::ReadFile(hFile, buf + done, static_cast<DWORD>(len - done), &n, nullptr) || n == 0)
            return false;
        done += n;
    }
    return true;
}

CStringW ComputeContentHash(const CString& filePath) {
    CStringW hash;

    HANDLE hFile = ::CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hFile == INVALID_HANDLE_VALUE)
        return hash;

    LARGE_INTEGER fileSize = {};
    if (!::GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart <= 0) {
        ::CloseHandle(hFile);
        return hash;
    }

    const DWORD CHUNK = 2 * 1024 * 1024; // 2 MiB
    const LONGLONG size = fileSize.QuadPart;
    const LONGLONG chunk = static_cast<LONGLONG>(CHUNK);

    // Head and tail overlap once the file is 4 MiB or smaller. Hash those whole
    // so the same bytes are never fed to the hash twice.
    const bool wholeFile = size <= 2 * chunk;
    const LONGLONG headLen = (size < chunk) ? size : chunk;
    const LONGLONG tailOff = wholeFile ? 0 : size - chunk;
    const LONGLONG tailLen = wholeFile ? 0 : chunk;

    std::vector<BYTE> head(static_cast<size_t>(headLen));
    std::vector<BYTE> tail(static_cast<size_t>(tailLen));

    const bool read = ReadRange(hFile, 0, head.data(), head.size()) && (wholeFile || ReadRange(hFile, tailOff, tail.data(), tail.size()));

    ::CloseHandle(hFile);
    if (!read)
        return hash;

    // Bind the size in, so a truncated or padded copy is never the same file.
    BYTE sizeBytes[8] = {};
    for (int i = 0; i < 8; ++i)
        sizeBytes[i] = static_cast<BYTE>((static_cast<ULONGLONG>(size) >> (i * 8)) & 0xFF);

    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    DWORD digestLen = 0, cbResult = 0;

    // Bcrypt returns NTSTATUS, where success is >= 0.
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        return hash;

    if (BCryptGetProperty(alg, BCRYPT_HASH_LENGTH, reinterpret_cast<PBYTE>(&digestLen), sizeof(digestLen), &cbResult, 0) < 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return hash;
    }

    std::vector<BYTE> digest(digestLen);
    bool ok = BCryptCreateHash(alg, &hHash, nullptr, 0, nullptr, 0, 0) >= 0;
    ok = ok && BCryptHashData(hHash, head.data(), static_cast<ULONG>(headLen), 0) >= 0;
    ok = ok && BCryptHashData(hHash, sizeBytes, sizeof(sizeBytes), 0) >= 0;
    ok = ok && (tailLen == 0 || BCryptHashData(hHash, tail.data(), static_cast<ULONG>(tailLen), 0) >= 0);
    ok = ok && BCryptFinishHash(hHash, digest.data(), digestLen, 0) >= 0;

    if (hHash)
        BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(alg, 0);

    if (!ok)
        return hash;

    // Encode the digest by hand
    static const wchar_t digits[] = L"0123456789abcdef";
    for (DWORD i = 0; i < digestLen; ++i) {
        hash += digits[digest[i] >> 4];
        hash += digits[digest[i] & 0xF];
    }

    return hash; // 64-char lowercase hex, or empty on failure
}

static CStringW SidecarPath(const CString& filePath) {
    return filePath + L".mpcresume";
}

bool LoadSidecarPosition(const CString& filePath, REFERENCE_TIME& outPos) {
    outPos = 0;

    CStdioFile f;
    if (!f.Open(SidecarPath(filePath), CFile::modeRead | CFile::shareDenyWrite | CFile::typeText))
        return false;

    CString version, hash, position, line;
    while (f.ReadString(line)) {
        line.Trim();
        const int eq = line.Find(L"=");
        if (eq <= 0)
            continue;
        const CString key = line.Left(eq).Trim();
        const CString val = line.Mid(eq + 1).Trim();
        if (key == L"version")          version = val;
        else if (key == L"hash")        hash = val;
        else if (key == L"position")    position = val;
    }
    f.Close();

    if (version != L"1" || hash.IsEmpty() || position.IsEmpty())
        return false; // unknown or corrupt format - leave the registry value alone

    // Identity check: something other than the file we saved for now sits here.
    if (hash != ComputeContentHash(filePath))
        return false;

    // Require a plain non-negative integer. At most 18 digits, so _tstoi64
    // cannot overflow and the multiplication below cannot wrap.
    if (position.IsEmpty() || position.GetLength() > 18)
        return false;

    for (int i = 0; i < position.GetLength(); ++i) {
        if (position[i] < L'0' || position[i] > L'9')
            return false;
    }

    outPos = static_cast<REFERENCE_TIME>(_tstoi64(position)) * 10000; // ms -> REFERENCE_TIME
    return true;
}

void SaveSidecarPosition(const CString& filePath, REFERENCE_TIME position, bool force) {
    static CStringW lastPath;
    static REFERENCE_TIME lastPos = 0;
    if (!force && filePath == lastPath && std::abs(lastPos - position) <= 300000000) {
        return;
    }

    const CStringW hash = ComputeContentHash(filePath);
    if (hash.IsEmpty()) {
        return; // can't hash it, so a sidecar would never validate on load
    }

    CStdioFile f;
    if (!f.Open(SidecarPath(filePath), CFile::modeCreate | CFile::modeWrite | CFile::shareExclusive | CFile::typeText)) {
        return; // read-only media folder, sharing violation
    }

    f.SetLength(0);

    CString out;
    out.Format(L"version=1\nhash=%s\nposition=%I64d\n", static_cast<LPCWSTR>(hash), static_cast<LONGLONG>(position / 10000));
    f.WriteString(out);
    f.Close();

    lastPath = filePath;
    lastPos = position;
}
