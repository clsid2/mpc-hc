/*
 * (C) 2016-2017 see Authors.txt
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
#include "SubtitlesProvidersUtils.h"
#include "SubtitlesProviders.h"
#include "mplayerc.h"
#include "ISOLang.h"

#if !USE_STATIC_UNRAR
#include "unrar.h"
#else
#include "unrar/dll.hpp"
#endif

#include "zlib/zlib.h"
#include "zlib/zutil.h"
#include "zlib/minizip/unzip.h"

#include <afxinet.h>
#include <WinCrypt.h>
#include <sstream>
#include <cstdlib>

int SubtitlesProvidersUtils::LevenshteinDistance(std::string s, std::string t)
{
    s = CStringA(s.c_str()).MakeLower();
    t = CStringA(t.c_str()).MakeLower();
    // degenerate cases
    if (s == t) {
        return 0;
    }
    if (s.length() == size_t(0)) {
        return (int)t.length();
    }
    if (t.length() == size_t(0)) {
        return (int)s.length();
    }

    // create two work vectors of integer distances
    std::vector<int> v0(t.length() + 1);
    std::vector<int> v1(t.length() + 1);

    // initialize v0 (the previous row of distances)
    // this row is A[0][i]: edit distance for an empty s
    // the distance is just the number of characters to delete from t
    for (size_t i = 0; i < v0.size(); i++) {
        v0[i] = (int)i;
    }

    for (size_t i = 0; i < s.length(); i++) {
        // calculate v1 (current row distances) from the previous row v0

        // first element of v1 is A[i+1][0]
        //   edit distance is delete (i+1) chars from s to match empty t
        v1[0] = (int)i + 1;

        // use formula to fill in the rest of the row
        for (size_t j = 0; j < t.length(); j++) {
            bool cost = (s[i] == t[j]) ? 0 : 1;
            v1[j + 1] = std::min(std::min(v1[j] + 1, v0[j + 1] + 1), v0[j] + cost);
        }

        // copy v1 (current row) to v0 (previous row) for next iteration
        for (size_t j = 0; j < v0.size(); j++) {
            v0[j] = v1[j];
        }
    }

    return v1[t.length()];
}

std::string SubtitlesProvidersUtils::StringToHex(const std::string& data)
{
    std::ostringstream oss;
    for (auto& iter : data) {
        oss.fill('0');
        oss.width(2);
        oss << std::hex << iter;
    }
    return oss.str();
}

std::string SubtitlesProvidersUtils::StringToHex(const int& data)
{
    std::ostringstream oss;
    oss.fill('0');
    oss.width(8);
    oss << std::hex << data;
    return oss.str();
}

std::string hex_to_bytes(const std::string& hex)
{
    std::vector<unsigned char> bytes;
    bytes.reserve(hex.size() / 2);
    for (std::string::size_type i = 0, i_end = hex.size(); i < i_end; i += 2) {
        unsigned byte;
        std::istringstream hex_byte(hex.substr(i, 2));
        hex_byte >> std::hex >> byte;
        bytes.push_back(static_cast<unsigned char>(byte));
    }
    return std::string(bytes.begin(), bytes.end());
}

std::string string_bytes(const std::string& data)
{

    std::ostringstream oss;
    for (auto& iter : data) {
        oss.fill('0');
        oss.width(2);
        oss << std::hex << static_cast<const int>(iter);
    }
    return oss.str();
}

std::string SubtitlesProvidersUtils::StringToHash(const std::string& data, ALG_ID Algid)
{
    std::string result;
    HCRYPTPROV hCryptProv = NULL;
    if (CryptAcquireContext(&hCryptProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        HCRYPTPROV hHash = NULL;
        if (CryptCreateHash(hCryptProv, Algid, 0, 0, &hHash)) {
            if (CryptHashData(hHash, (const BYTE*)(data.c_str()), (DWORD)data.length(), 0)) {
                DWORD cbHashSize = 0, dwCount = sizeof(DWORD);
                if (CryptGetHashParam(hHash, HP_HASHSIZE, (BYTE*)&cbHashSize, &dwCount, 0)) {
                    std::vector<BYTE> buffer(cbHashSize);
                    if (CryptGetHashParam(hHash, HP_HASHVAL, (BYTE*)(&buffer[0]), &cbHashSize, 0)) {
                        std::ostringstream oss;
                        for (const auto& iter : buffer) {
                            oss.fill('0');
                            oss.width(2);
                            oss << std::hex << static_cast<const int>(iter);
                        }
                        result = oss.str();
                    }
                }
            }
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hCryptProv, 0);
    }
    return result;
}

std::string SubtitlesProvidersUtils::StringEncrypt(const std::string& data, const std::string& key, ALG_ID Algid)
{
    std::string result;
    HCRYPTPROV hCryptProv = NULL;
    if (CryptAcquireContext(&hCryptProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        HCRYPTHASH hHash = NULL;
        if (CryptCreateHash(hCryptProv, CALG_SHA_256, NULL, 0, &hHash)) {
            HCRYPTKEY hKey = NULL;
            if (CryptHashData(hHash, (const BYTE*)key.c_str(), (DWORD)key.length(), 0)) {
                if (CryptDeriveKey(hCryptProv, Algid, hHash, CRYPT_KEYLENGTH | CRYPT_EXPORTABLE, &hKey)) {
                    DWORD dwCount = (DWORD)data.length();
                    std::vector<BYTE> buffer(data.begin(), data.end());
                    if (CryptEncrypt(hKey, NULL, TRUE, 0, nullptr, &dwCount, NULL)) {
                        buffer.resize(dwCount);
                        dwCount = (DWORD)data.length();
                        if (CryptEncrypt(hKey, NULL, TRUE, 0, (BYTE*)(&buffer[0]), &dwCount, (DWORD)buffer.size())) {
                            result.assign((const char*)(&buffer[0]), dwCount);
                        }
                    }
                    CryptDestroyKey(hKey);
                }
                CryptDestroyHash(hHash);
            }
        }
        CryptReleaseContext(hCryptProv, 0);
    }
    return result;
}

std::string SubtitlesProvidersUtils::StringDecrypt(const std::string& data, const std::string& key, ALG_ID Algid)
{
    std::string result;
    if (!data.empty()) {
        HCRYPTPROV hCryptProv = NULL;
        if (CryptAcquireContext(&hCryptProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
            HCRYPTHASH hHash = NULL;
            if (CryptCreateHash(hCryptProv, CALG_SHA_256, NULL, 0, &hHash)) {
                HCRYPTKEY hKey = NULL;
                if (CryptHashData(hHash, (const BYTE*)key.c_str(), (DWORD)key.length(), 0)) {
                    if (CryptDeriveKey(hCryptProv, Algid, hHash, CRYPT_KEYLENGTH | CRYPT_EXPORTABLE, &hKey)) {
                        DWORD dwCount = (DWORD)data.length();
                        std::vector<BYTE> buffer(data.begin(), data.end());
                        if (CryptDecrypt(hKey, NULL, TRUE, 0, (BYTE*)(&buffer[0]), &dwCount)) {
                            result.assign((const char*)(&buffer[0]), dwCount);
                        }
                        CryptDestroyKey(hKey);
                    }
                    CryptDestroyHash(hHash);
                }
            }
            CryptReleaseContext(hCryptProv, 0);
        }
    }
    return result;
}

std::string SubtitlesProvidersUtils::StringFormat(_In_z_ _Printf_format_string_ char const* const fmt, ...)
{
    std::string str;
    va_list args;
    va_start(args, fmt);
    auto len = _vscprintf(fmt, args);
    ASSERT(len > 0);
    if (len > 0) {
        str.resize(len);
#if _HAS_CXX17
        auto data = str.data();
#else
        auto data = str._Myptr();
#endif
        VERIFY(len == vsnprintf_s(data, len + 1, len, fmt, args));
    }
    va_end(args);
    return str;
}

size_t SubtitlesProvidersUtils::stringMatch(const std::string& pattern, const std::string& text, regexResults& results)
{
    std::regex regex_pattern(pattern, RegexFlags);
    return stringMatch(regex_pattern, text, results);
}

size_t SubtitlesProvidersUtils::stringMatch(const std::regex& pattern, const std::string& text, regexResults& results)
{
    results.clear();

    std::string data(text);
    std::smatch match_pieces;
    while (std::regex_search(data, match_pieces, pattern)) {
        regexResult result;
        for (const auto& match : match_pieces) {
            if (match != *match_pieces.begin()) {
                result.push_back(match.str());
            }
        }
        results.push_back(result);
        data = match_pieces.suffix().str();
    }
    return results.size();
}

size_t SubtitlesProvidersUtils::stringMatch(const std::string& pattern, const std::string& text, regexResult& result)
{
    std::regex regex_pattern(pattern, RegexFlags);
    return stringMatch(regex_pattern, text, result);
}

size_t SubtitlesProvidersUtils::stringMatch(const std::regex& pattern, const std::string& text, regexResult& result)
{
    result.clear();

    std::smatch match_pieces;
    std::regex_search(text, match_pieces, pattern);
    for (const auto& match : match_pieces) {
        if (match != *match_pieces.begin()) {
            result.push_back(match.str());
        }
    }
    return result.size();
}

std::string SubtitlesProvidersUtils::StringGzipDeflate(const std::string& data)
{
    std::string result;

    UINT buffer_len = 32 * 1024;
    std::vector<BYTE> buffer(buffer_len);

    z_stream deflate_stream = { 0 };
    deflate_stream.next_in = (BYTE*)data.c_str();
    deflate_stream.avail_in = (UINT)data.length();

    int ret = Z_OK;
    if ((ret = deflateInit2(&deflate_stream, Z_BEST_COMPRESSION, Z_DEFLATED, DEF_WBITS + Z_ENCODING_GZIP, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY)) == Z_OK) {
        do {
            deflate_stream.next_out = &buffer[0];
            deflate_stream.avail_out = buffer_len;
            if ((ret = deflate(&deflate_stream, Z_FINISH)) >= Z_OK) {
                result.append((char*)&buffer[0], buffer_len - deflate_stream.avail_out);
            } else {
                break;
            }
        } while (deflate_stream.avail_out == 0);
        ret = deflateEnd(&deflate_stream);
    }
    return result;
}

std::string SubtitlesProvidersUtils::StringGzipCompress(const std::string& data)
{
    std::string result;

    UINT buffer_len = 32 * 1024;
    std::vector<BYTE> buffer(buffer_len);

    z_stream deflate_stream = { 0 };
    deflate_stream.next_in = (BYTE*)data.c_str();
    deflate_stream.avail_in = (UINT)data.length();

    int ret = Z_OK;
    //if ((ret = deflateInit2(&deflate_stream, Z_BEST_COMPRESSION, Z_DEFLATED, DEF_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY)) == Z_OK) {
    if ((ret = deflateInit(&deflate_stream, Z_BEST_COMPRESSION)) == Z_OK) {
        do {
            deflate_stream.next_out = &buffer[0];
            deflate_stream.avail_out = buffer_len;
            if ((ret = deflate(&deflate_stream, Z_FINISH)) >= Z_OK) {
                result.append((char*)&buffer[0], buffer_len - deflate_stream.avail_out);
            } else {
                break;
            }
        } while (deflate_stream.avail_out == 0);
        ret = deflateEnd(&deflate_stream);
    }
    return result;
}

std::string SubtitlesProvidersUtils::StringGzipInflate(const std::string& data)
{
    std::string result;

    UINT buffer_len = 32 * 1024;
    std::vector<BYTE> buffer(buffer_len);

    z_stream inflate_stream = { 0 };
    inflate_stream.next_in = (BYTE*)data.c_str();
    inflate_stream.avail_in = (UINT)data.length();

    if (inflateInit2(&inflate_stream, DEF_WBITS + Z_DECODING_ZLIB_GZIP) == Z_OK) {
        do {
            inflate_stream.next_out = &buffer[0];
            inflate_stream.avail_out = buffer_len;
            if (inflate(&inflate_stream, Z_NO_FLUSH) >= Z_OK) {
                result.append((char*)&buffer[0], buffer_len - inflate_stream.avail_out);
            } else {
                break;
            }
        } while (inflate_stream.avail_out == 0);
        inflateEnd(&inflate_stream);
    }
    return result;
}

std::string SubtitlesProvidersUtils::StringGzipUncompress(const std::string& data)
{
    std::string result;

    UINT buffer_len = 32 * 1024;
    std::vector<BYTE> buffer(buffer_len);

    z_stream inflate_stream = { 0 };
    inflate_stream.next_in = (BYTE*)data.c_str();
    inflate_stream.avail_in = (UINT)data.length();

    int ret = Z_OK;
    //if ((ret = inflateInit2(&inflate_stream, DEF_WBITS)) == Z_OK) {
    if ((ret = inflateInit(&inflate_stream)) == Z_OK) {
        do {
            inflate_stream.next_out = &buffer[0];
            inflate_stream.avail_out = buffer_len;
            if ((ret = inflate(&inflate_stream, Z_NO_FLUSH)) >= Z_OK) {
                result.append((char*)&buffer[0], buffer_len - inflate_stream.avail_out);
            } else {
                break;
            }
        } while (inflate_stream.avail_out == 0);
        ret = inflateEnd(&inflate_stream);
    }
    return result;
}

int SubtitlesProvidersUtils::FileUnzip(CStringA fn, stringMap& dataOut)
{
#define MAX_FILENAME 512
#define READ_SIZE 8192

    // Open the zip file
    unzFile zipfile = unzOpen(fn);
    if (zipfile == nullptr) {
        return -1;
    }

    // Get info about the zip file
    unz_global_info global_info;
    if (unzGetGlobalInfo(zipfile, &global_info) != UNZ_OK) {
        unzClose(zipfile);
        return -1;
    }

    // Buffer to hold data read from the zip file.
    char read_buffer[READ_SIZE];

    // Loop to extract all files
    for (uLong i = 0; i < global_info.number_entry; ++i) {
        // Get info about current file.
        unz_file_info file_info;
        char filename[MAX_FILENAME];
        if (unzGetCurrentFileInfo(zipfile, &file_info, filename, MAX_FILENAME, nullptr, 0, nullptr, 0) != UNZ_OK) {
            unzClose(zipfile);
            return -1;
        }

        if (strlen(filename) >= 4 && Subtitle::IsTextSubtitleFileName(filename)) {
            if (unzOpenCurrentFile(zipfile) != UNZ_OK) {
                unzClose(zipfile);
                return -1;
            }

            std::string data;
            data.reserve(file_info.uncompressed_size);
            int error;
            do {
                error = unzReadCurrentFile(zipfile, read_buffer, READ_SIZE);
                if (error < 0) {
                    unzCloseCurrentFile(zipfile);
                    unzClose(zipfile);
                    return -1;
                }

                // Write data to file.
                if (error > 0) {
                    data.append(read_buffer, error);
                }
            } while (error > 0);
            dataOut.emplace(filename, data);
        }

        unzCloseCurrentFile(zipfile);

        // Go the the next entry listed in the zip file.
        if (i + 1 < global_info.number_entry) {
            if (unzGoToNextFile(zipfile) != UNZ_OK) {
                unzClose(zipfile);
                return -1;
            }
        }
    }

    unzClose(zipfile);
    return 0;
}

int CALLBACK SubtitlesProvidersUtils::UnRarProc(UINT msg, LPARAM UserData, LPARAM P1, LPARAM P2)
{
    if (msg == UCM_PROCESSDATA) {
        std::string* data((std::string*)UserData);
        data->append((char*)P1, (size_t)P2);
    }
    return 1;
}

bool SubtitlesProvidersUtils::FileUnRar(CString fn, stringMap& dataOut)
{
#if !USE_STATIC_UNRAR
#ifdef _WIN64
    HMODULE h = LoadLibrary(_T("unrar64.dll"));
#else
    HMODULE h = LoadLibrary(_T("unrar.dll"));
#endif
    if (!h) {
        return false;
    }

    RAROpenArchiveEx OpenArchiveEx = (RAROpenArchiveEx)GetProcAddress(h, "RAROpenArchiveEx");
    RARCloseArchive  CloseArchive = (RARCloseArchive)GetProcAddress(h, "RARCloseArchive");
    RARReadHeaderEx  ReadHeaderEx = (RARReadHeaderEx)GetProcAddress(h, "RARReadHeaderEx");
    RARProcessFile   ProcessFile = (RARProcessFile)GetProcAddress(h, "RARProcessFile");
    RARSetCallback   SetCallback = (RARSetCallback)GetProcAddress(h, "RARSetCallback");

    if (!(OpenArchiveEx && CloseArchive && ReadHeaderEx && ProcessFile && SetCallback)) {
        FreeLibrary(h);
        return false;
    }

#else

#define OpenArchiveEx      RAROpenArchiveEx
#define CloseArchive       RARCloseArchive
#define ReadHeaderEx       RARReadHeaderEx
#define ProcessFile        RARProcessFile
#define SetCallback        RARSetCallback
#endif /* USE_STATIC_UNRAR */

    RAROpenArchiveDataEx OpenArchiveData;
    ZeroMemory(&OpenArchiveData, sizeof(OpenArchiveData));

    OpenArchiveData.ArcNameW = (LPTSTR)(LPCTSTR)fn;
    char fnA[MAX_PATH];
    size_t size;
    if (wcstombs_s(&size, fnA, fn, fn.GetLength())) {
        fnA[0] = 0;
    }
    OpenArchiveData.ArcName = fnA;
    OpenArchiveData.OpenMode = RAR_OM_EXTRACT;
    OpenArchiveData.CmtBuf = 0;
    OpenArchiveData.Callback = UnRarProc;
    std::string data;
    OpenArchiveData.UserData = (LPARAM)&data;

    HANDLE hArcData = OpenArchiveEx(&OpenArchiveData);
    if (!hArcData) {
#if !USE_STATIC_UNRAR
        FreeLibrary(h);
#endif
        return false;
    }

    RARHeaderDataEx HeaderDataEx;
    HeaderDataEx.CmtBuf = nullptr;

    while (ReadHeaderEx(hArcData, &HeaderDataEx) == 0) {
        if (wcslen(HeaderDataEx.FileNameW) >= 4 && Subtitle::IsTextSubtitleFileName(HeaderDataEx.FileNameW)) {
            data.clear();
            data.reserve(std::max(data.capacity(), (size_t)HeaderDataEx.UnpSize));
            if (ProcessFile(hArcData, RAR_TEST, nullptr, nullptr)) {
                CloseArchive(hArcData);
#if !USE_STATIC_UNRAR
                FreeLibrary(h);
#endif
                return false;
            }
            dataOut.insert(std::pair<std::string, std::string>(HeaderDataEx.FileName, data));
        } else {
            ProcessFile(hArcData, RAR_SKIP, nullptr, nullptr);
        }
    }

    CloseArchive(hArcData);
#if !USE_STATIC_UNRAR
    FreeLibrary(h);
#endif

    return true;
}

SubtitlesProvidersUtils::stringMap SubtitlesProvidersUtils::StringUncompress(const std::string& data,
        const std::string& fileName)
{
    stringMap result;
    static const char gzip[] = { 0x1F, '\x8B' };
    static const char zlib[4][2] = { { 0x78, '\xDA' }, { 0x78, '\x9C' }, { 0x78, 0x5E }, { 0x78, 0x01 } };
    static const char zip[] = { 0x50, 0x4B, 0x03, 0x04 };
    static const char rar4[] = { 0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x00 };
    static const char rar5[] = { 0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x01, 0x00 };

    if (data.compare(0, sizeof(gzip), gzip, sizeof(gzip)) == 0) {
        result.insert(std::pair<std::string, std::string>(fileName, StringGzipInflate(data)));
    } else if ((data.compare(0, sizeof(zlib[0]), zlib[0], sizeof(zlib[0])) == 0) || (data.compare(0, sizeof(zlib[1]), zlib[1], sizeof(zlib[1])) == 0) ||
               (data.compare(0, sizeof(zlib[2]), zlib[2], sizeof(zlib[2])) == 0) || (data.compare(0, sizeof(zlib[3]), zlib[3], sizeof(zlib[3])) == 0)) {
        result.insert(std::pair<std::string, std::string>(fileName, StringGzipUncompress(data)));
    } else if (data.compare(0, sizeof(zip), zip, sizeof(zip)) == 0) {
        TCHAR path[MAX_PATH], file[MAX_PATH];
        GetTempPath(MAX_PATH, path);
        GetTempFileName(path, _T("mpc"), 0, file);

        CFile f;
        if (f.Open(file, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary | CFile::shareDenyNone)) {
            f.Write((BYTE*)data.c_str(), (UINT)data.length());
            f.Close();
        }

        FileUnzip(file, result);
        DeleteFile(file);
    } else if ((data.compare(0, sizeof(rar4), rar4, sizeof(rar4)) == 0) || (data.compare(0, sizeof(rar5), rar5, sizeof(rar5)) == 0)) {
        TCHAR path[MAX_PATH], file[MAX_PATH];
        GetTempPath(MAX_PATH, path);
        GetTempFileName(path, _T("mpc"), 0, file);

        CFile f;
        if (f.Open(file, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary | CFile::shareDenyNone)) {
            f.Write((BYTE*)data.c_str(), (UINT)data.length());
            f.Close();
        }
        FileUnRar(file, result);
        DeleteFile(file);
    } else {
        result.insert(std::pair<std::string, std::string>(fileName, data));
    }
    return result;
}

std::string SubtitlesProvidersUtils::StringGenerateUniqueKey()
{
    TCHAR strFileName[MAX_PATH];
    GetModuleFileName(nullptr, strFileName, MAX_PATH);
    CPath p(strFileName);
    p.StripToRoot();

    std::vector<BYTE> buffer(16);
    GetVolumeInformation(p, nullptr, NULL, (LPDWORD)&buffer[0], nullptr, (LPDWORD)&buffer[4], nullptr, NULL);
    GetDiskFreeSpaceEx(p, nullptr, (PULARGE_INTEGER)&buffer[8], nullptr);
    return std::string((PCHAR)&buffer[0], buffer.size());
}

HRESULT SubtitlesProvidersUtils::StringDownload(const std::string& url, const stringMap& headers,
                                                std::string& data, bool bAutoRedirect, DWORD* dwStatusCode)
{
    data.clear();
    if (dwStatusCode) {
        *dwStatusCode = 0;
    }

    try {
        CInternetSession is;
        is.SetOption(INTERNET_OPTION_CONNECT_TIMEOUT, 10000); /* default=60000 */

        std::string strHeaders;
        for (const auto& iter : headers) {
            if (!iter.second.empty()) {
                strHeaders += StringFormat("%s: %s\r\n", iter.first.c_str(), iter.second.c_str());
            }
        }

        CAutoPtr<CHttpFile> pHttpFile((CHttpFile*)is.OpenURL(UTF8To16(url.c_str()),
                                                             1,
                                                             INTERNET_FLAG_TRANSFER_BINARY | INTERNET_FLAG_EXISTING_CONNECT | (bAutoRedirect ? INTERNET_FLAG_NO_AUTO_REDIRECT : NULL),
                                                             UTF8To16(strHeaders.c_str())));

        DWORD total_length = 0, length = 0, index = 0;
        while (pHttpFile->QueryInfo(HTTP_QUERY_CONTENT_LENGTH, length, &index)) {
            total_length += length;
        }

        data.reserve(std::max((size_t)total_length, (size_t)pHttpFile->GetLength()));

        std::vector<char> buff(1024);
        for (int len = 0; (len = pHttpFile->Read(&buff[0], (UINT)buff.size())) > 0; data.append(&buff[0], len));

        if (dwStatusCode) {
            pHttpFile->QueryInfoStatusCode(*dwStatusCode);
        }

        pHttpFile->Close(); // must close it because the destructor doesn't seem to do it and we will get an exception when "is" is destroying
    } catch (CInternetException* pEx) {
        HRESULT hr = HRESULT_FROM_WIN32(pEx->m_dwError);
        if (dwStatusCode) {
            *dwStatusCode = pEx->m_dwError;
        }
        TRACE("File transfer failed - 0x%lx - %s\n", hr, url.c_str());
        pEx->Delete();
        return hr;
    }
    return S_OK;
}

HRESULT SubtitlesProvidersUtils::StringUpload(const std::string& url, const stringMap& headers,
                                              const std::string& content, std::string& data,
                                              bool bAutoRedirect, DWORD* dwStatusCode)
{
    try {
        DWORD dwServiceType = NULL;
        CString strServer, strObject, strUserName, strPassword;
        INTERNET_PORT nPort = NULL;
        if (!AfxParseURLEx(UTF8To16(url.c_str()), dwServiceType, strServer, strObject, nPort, strUserName, strPassword)) {
            return E_FAIL;
        }

        CInternetSession is;
        is.SetOption(INTERNET_OPTION_CONNECT_TIMEOUT, 10000);
        CAutoPtr<CHttpConnection> pHttpConnection(is.GetHttpConnection(strServer));
        CAutoPtr<CHttpFile> pHttpFile(pHttpConnection->OpenRequest(CHttpConnection::HTTP_VERB_POST, strObject, 0, 1, 0, 0, INTERNET_FLAG_KEEP_CONNECTION | (bAutoRedirect == FALSE ? INTERNET_FLAG_NO_AUTO_REDIRECT : NULL)));

        for (const auto& iter : headers) {
            if (!iter.second.empty()) {
                pHttpFile->AddRequestHeaders(UTF8To16(StringFormat("%s: %s", iter.first.c_str(), iter.second.c_str()).c_str()), HTTP_ADDREQ_FLAG_ADD_IF_NEW);
            }
        }

        pHttpFile->SendRequestEx(DWORD(content.length()), HSR_SYNC | HSR_INITIATE);
        pHttpFile->Write(content.c_str(), (UINT)content.length());
        pHttpFile->Flush();
        pHttpFile->EndRequest(HSR_SYNC);

        DWORD total_length = 0, length = 0, index = 0;
        while (pHttpFile->QueryInfo(HTTP_QUERY_CONTENT_LENGTH, length, &index)) {
            total_length += length;
        }
        data.reserve(std::max((size_t)length, (size_t)pHttpFile->GetLength()));

        std::vector<char> buff(1024);
        for (int len = 0; (len = pHttpFile->Read(&buff[0], (UINT)buff.size())) > 0; data.append(&buff[0], len));

        if (dwStatusCode) {
            pHttpFile->QueryInfoStatusCode(*dwStatusCode);
        }

        pHttpFile->Close();
        pHttpConnection->Close();
    } catch (CInternetException* ie) {
        HRESULT hr = HRESULT_FROM_WIN32(ie->m_dwError);
        TCHAR szErr[1024];
        szErr[0] = '\0';
        if (!ie->GetErrorMessage(szErr, 1024)) {
            wcscpy_s(szErr, L"Some crazy unknown error");
        }
        TRACE("File transfer failed!! - %s", szErr);
        ie->Delete();
        return hr;
    }
    return S_OK;
}

SubtitlesProvidersUtils::stringArray SubtitlesProvidersUtils::StringTokenize(const std::string& text,
        const std::string& delimiters,
        bool blank)
{
    stringArray result;
    std::string next;
    for (const auto& iter : text) {
        if (delimiters.find(iter) != std::string::npos) {
            if (!next.empty() || blank) {
                // Add them to the result vector
                result.push_back(StringTrim(next));
                next.clear();
            }
        } else {
            next += iter;
        }
    }
    if (!next.empty()) {
        result.push_back(StringTrim(next));
    }
    return result;
}

std::string SubtitlesProvidersUtils::StringTrim(const std::string& text, const std::string& characters, int side)
{
    std::string result(text);
    if (!characters.empty()) {
        if (side <= 0)
            while (result.length() && characters.find(result.front()) != std::string::npos) {
                result.erase(result.begin());
            }

        if (side >= 0)
            while (result.length() && characters.find(result.back()) != std::string::npos) {
                result.pop_back();
            }
    }
    return result;
}

std::string SubtitlesProvidersUtils::StringReplace(const std::string& text, const std::string& find, const std::string& replace)
{
    std::string result(text);
    std::string::size_type pos = 0;
    while ((pos = result.find(find, pos)) != std::string::npos) {
        result.erase(pos, find.length());
        result.insert(pos, replace);
        pos += replace.length();
    }
    return result;
}

std::list<std::string> SubtitlesProvidersUtils::LanguagesISO6391()
{
    std::list<std::string> result;
    for (const auto& iter : StringTokenize(UTF16To8(AfxGetAppSettings().strSubtitlesLanguageOrder).GetString(), ",; ")) {
        if (iter.length() > 2) {
            std::string lang = CStringA(ISOLang::ISO6392To6391(iter.c_str())).GetString();
            if (!lang.empty()) {
                result.push_back(lang);
            }
        } else if (iter.length() == 2) {
            if (ISOLang::IsISO6391(iter.c_str())) {
                result.push_back(iter);
            }
        }
    }
    return result;
}

std::list<std::string> SubtitlesProvidersUtils::LanguagesISO6392()
{
    std::list<std::string> result;
    for (const auto& iter : StringTokenize(UTF16To8(AfxGetAppSettings().strSubtitlesLanguageOrder).GetString(), ",; ")) {
        if (iter.length() == 2) {
            std::string lang = ISOLang::ISO6391To6392(iter.c_str()).GetString();
            if (!lang.empty()) {
                result.push_back(lang);
            }
        } else if (iter.length() == 3) {
            if (ISOLang::IsISO6392(iter.c_str())) {
                result.push_back(iter);
            }
        }
        
    }
    return result;
}

UINT64 SubtitlesProvidersUtils::GenerateOSHash(SubtitlesInfo& pFileInfo)
{
    UINT64 fileHash = pFileInfo.fileSize;
    UINT64 errval = 0x123456789; // random value that should not give any search results

    UINT64* buffer = (UINT64*)std::malloc(PROBE_SIZE);
    if (!buffer) return errval;

    if (pFileInfo.pAsyncReader) {
        UINT64 position = 0;
        if (SUCCEEDED(pFileInfo.pAsyncReader->SyncRead(position, PROBE_SIZE, (BYTE*)buffer))) {
            for (int i = 0; i < PROBE_SIZE / sizeof(UINT64); ++i) {
                fileHash += buffer[i];
            }
        } else { std::free(buffer); return errval; }
        position = std::max(0ui64, pFileInfo.fileSize - PROBE_SIZE);
        if (SUCCEEDED(pFileInfo.pAsyncReader->SyncRead(position, PROBE_SIZE, (BYTE*)buffer))) {
            for (int i = 0; i < PROBE_SIZE / sizeof(UINT64); ++i) {
                fileHash += buffer[i];
            }
        } else { std::free(buffer); return errval; }
    } else {
        CFile file;
        CFileException fileException;
        if (file.Open(CString(pFileInfo.filePathW.c_str()),
                      CFile::modeRead | CFile::osSequentialScan | CFile::shareDenyNone | CFile::typeBinary, &fileException)) {
            if (file.Read(buffer, PROBE_SIZE)) {
                for (int i = 0; i < PROBE_SIZE / sizeof(UINT64); ++i) {
                    fileHash += buffer[i];
                }
            } else { std::free(buffer); return errval; }
            file.Seek(std::max(0ui64, pFileInfo.fileSize - PROBE_SIZE), CFile::begin);
            if (file.Read(buffer, PROBE_SIZE)) {
                for (int i = 0; i < PROBE_SIZE / sizeof(UINT64); ++i) {
                    fileHash += buffer[i];
                }
            } else { std::free(buffer); return errval; }
        }
    }
    std::free(buffer);
    return fileHash;
}

std::wstring SubtitlesProvidersUtils::JoinContainer(std::list<std::string> c, LPCWSTR delim) {
    std::wstring ret = L"";
    for (auto it = c.begin(); it != c.end(); it++) {
        ret += UTF8To16(it->c_str());
        if (std::next(it) != c.end()) {
            ret += delim;
        }
    }
    return ret;
};
