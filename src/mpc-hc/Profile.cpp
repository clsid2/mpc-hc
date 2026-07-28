/*
 * (C) 2024 see Authors.txt
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
#include "Profile.h"
#include "FileHandle.h" // GetModulePath
#include "Utils.h"      // StrToInt32/UInt32/Int64/Double, StrHexToUInt32
#include "text.h"       // StartsWithNoCase
#include "base64/base64.h"

// ---------------------------------------------------------------------------
// Versioned store names. These live ALONGSIDE the legacy store, which keeps its
// old location (HKCU\Software\MPC-HC\MPC-HC and <exe>.ini) untouched so older
// builds stay downgrade-safe.
// ---------------------------------------------------------------------------
// The on-disk format version is NOT in the store name; each store records it in
// a [Version] Format field. The new stores live ALONGSIDE the pre-versioning
// legacy store (HKCU\Software\MPC-HC\MPC-HC and <exe>.ini), which is untouched.
static const wchar_t* const LEGACY_REG_KEY     = L"Software\\MPC-HC\\MPC-HC";   // pre-versioning MFC key
static const wchar_t* const SETTINGS_REG_KEY   = L"Software\\MPC-HC\\Settings"; // format in [Version]Format
static const wchar_t* const OLD_INI_SUFFIX     = L".ini";                       // legacy portable ini
static const wchar_t* const NEW_INI_SUFFIX     = L".settings.ini";             // <exe-basename>.settings.ini
static const wchar_t* const HISTORY_INI_SUFFIX = L".history.ini";              // <exe-basename>.history.ini
static const wchar_t* const LOCAL_INI_SUFFIX   = L".settings.local.ini";       // downgrade fork (settings)
static const wchar_t* const HISTORY_LOCAL_SUFFIX = L".history.local.ini";      // downgrade fork (history)

// Prefix marking a NEW-format Base64 binary value in an INI. Legacy binary
// values are A-P encoded (only chars 'A'..'P'), so this lowercase-led prefix
// can never collide with a legacy value -> ReadBinary disambiguates by it.
static const wchar_t* const BINARY_B64_PREFIX = L"b64:";

static CStringW GetExeBasePath()
{
    // Full path of the executable with the extension stripped.
    CStringW path = GetModulePath(nullptr);
    int dot = path.ReverseFind(L'.');
    if (dot >= 0) {
        path = path.Left(dot);
    }
    return path;
}

static CStringW GetNewIniPath()
{
    return GetExeBasePath() + NEW_INI_SUFFIX;
}
static CStringW GetLocalIniPath()
{
    return GetExeBasePath() + LOCAL_INI_SUFFIX;
}

static CStringW GetLegacyIniPath()
{
    return GetExeBasePath() + OLD_INI_SUFFIX;
}

// Base64 helpers bridging BYTE[] <-> CStringW using the ASCII Base64 codec
// in include/base64/base64.h.
static CStringW BinaryToBase64(const BYTE* pdata, unsigned nbytes)
{
    std::string bytes(reinterpret_cast<const char*>(pdata), nbytes);
    std::string b64 = Base64::encode(bytes);
    return CStringW(CStringA(b64.c_str(), static_cast<int>(b64.size())));
}

static unsigned Base64ToBinary(const CStringW& str, BYTE** ppdata)
{
    *ppdata = nullptr;
    if (str.IsEmpty()) {
        return 0;
    }
    CStringA ascii(str); // Base64 is pure ASCII
    std::string decoded = Base64::decode(std::string(ascii.GetString(), ascii.GetLength()));
    unsigned nbytes = static_cast<unsigned>(decoded.size());
    if (nbytes == 0) {
        return 0;
    }
    *ppdata = new (std::nothrow) BYTE[nbytes];
    if (!*ppdata) {
        return 0;
    }
    memcpy(*ppdata, decoded.data(), nbytes);
    return nbytes;
}

// Legacy A-P binary decoding (each byte = 2 chars 'A'..'P', low nibble first),
// as written by the old CMPlayerCApp::WriteProfileBinary. Used only to read
// values imported verbatim from a legacy INI; deletable at the legacy sunset.
static unsigned APToBinary(const CStringW& valueStr, BYTE** ppdata)
{
    *ppdata = nullptr;
    const int length = valueStr.GetLength();
    if (length == 0 || (length % 2) != 0) {
        return 0;
    }
    for (int i = 0; i < length; i++) {
        if (valueStr[i] < L'A' || valueStr[i] > L'P') {
            return 0;
        }
    }
    unsigned nbytes = length / 2;
    *ppdata = new (std::nothrow) BYTE[nbytes];
    if (!*ppdata) {
        return 0;
    }
    for (unsigned i = 0; i < nbytes; i++) {
        (*ppdata)[i] = BYTE((valueStr[i * 2] - L'A') | ((valueStr[i * 2 + 1] - L'A') << 4));
    }
    return nbytes;
}

// Open and parse an INI file (BOM-sniffing UNICODE, falling back to ANSI) into
// a ProfileMap. Clears the map first. Returns false if the file can't be read.
static bool ReadIniFileIntoMap(const CStringW& iniPath, ProfileMap& map)
{
    map.clear();

    if (!::PathFileExistsW(iniPath)) {
        return false;
    }

    FILE* fp;
    int fpStatus;
    do { // Open the ini in UNICODE mode, retry if it is already being used by another process
        fp = _wfsopen(iniPath, L"r, ccs=UNICODE", _SH_SECURE);
        if (fp || (GetLastError() != ERROR_SHARING_VIOLATION)) {
            break;
        }
        Sleep(100);
    } while (true);
    if (!fp) {
        ASSERT(FALSE);
        return false;
    }
    if (_ftell_nolock(fp) == 0L) {
        // No BOM was consumed, assume the ini is ANSI encoded
        fpStatus = fclose(fp);
        ASSERT(fpStatus == 0);
        do { // Reopen the ini in ANSI mode, retry if it is already being used by another process
            fp = _wfsopen(iniPath, L"r", _SH_SECURE);
            if (fp || (GetLastError() != ERROR_SHARING_VIOLATION)) {
                break;
            }
            Sleep(100);
        } while (true);
        if (!fp) {
            ASSERT(FALSE);
            return false;
        }
    }

    CStdioFile file(fp);

    CStringW line, section, var, val;
    while (file.ReadString(line)) {
        // Parse the ini file, this parser:
        //  - doesn't trim whitespaces
        //  - doesn't remove quotation marks
        //  - omits keys with empty names
        //  - omits unnamed sections
        int pos = 0;
        if (line[0] == L'[') {
            pos = line.Find(L']');
            if (pos == -1) {
                continue;
            }
            section = line.Mid(1, pos - 1);
        } else if (line[0] != L';') {
            pos = line.Find(L'=');
            if (pos == -1) {
                continue;
            }
            var = line.Mid(0, pos);
            val = line.Mid(pos + 1);
            if (!section.IsEmpty() && !var.IsEmpty()) {
                map[section][var] = val;
            }
        }
    }
    fpStatus = fclose(fp);
    ASSERT(fpStatus == 0);

    return true;
}

// CProfile

CProfile::CProfile()
{
    // Portable if a settings INI (new or legacy) exists next to the executable.
    const CStringW newIni = GetNewIniPath();
    if (::PathFileExistsW(newIni) || ::PathFileExistsW(GetLegacyIniPath())) {
        m_IniPath = newIni; // write the new-format file (alongside any legacy one)
        return;
    }

    // Registry mode. Do NOT create the key here (this ctor runs at static-init
    // time for the global theApp); it is opened lazily on first access.
    m_bRegistryMode = true;
}

CProfile::CProfile(const CStringW& iniFilePath)
{
    // Explicit INI mode at a fixed path; no registry, no auto-detection.
    m_IniPath = iniFilePath;
}

CProfile::~CProfile()
{
    if (m_hAppRegKey) {
        RegCloseKey(m_hAppRegKey);
        m_hAppRegKey = nullptr;
    }
}

CStringW CProfile::HistoryIniPath()
{
    return GetExeBasePath() + HISTORY_INI_SUFFIX;
}

CStringW CProfile::HistoryLocalIniPath()
{
    return GetExeBasePath() + HISTORY_LOCAL_SUFFIX;
}

CStringW CProfile::DefaultIniPath()
{
    return GetNewIniPath();
}

CStringW CProfile::LocalIniPath()
{
    return GetLocalIniPath();
}

LONG CProfile::OpenRegistryKey()
{
    LONG lResult = ERROR_SUCCESS;

    if (!m_hAppRegKey) {
        DWORD dwDisposition = 0;
        lResult = RegCreateKeyExW(HKEY_CURRENT_USER, SETTINGS_REG_KEY, 0, nullptr, 0,
                                  KEY_READ | KEY_WRITE, nullptr, &m_hAppRegKey, &dwDisposition);
        ASSERT(lResult == ERROR_SUCCESS);
    }

    return lResult;
}

void CProfile::InitIni()
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);

    if (m_bRegistryMode) {
        return;
    }

    // Don't reread the ini if the cache needs to be flushed or it was accessed recently
    const ULONGLONG tick = GetTickCount64();
    if (m_bIniFirstInit && (m_bIniNeedFlush || tick - m_IniLastAccessTick < 100u)) {
        m_IniLastAccessTick = tick;
        return;
    }

    m_bIniFirstInit = true;
    m_IniLastAccessTick = tick;

    ASSERT(!m_bIniNeedFlush);
    ReadIniFileIntoMap(m_IniPath, m_ProfileMap);

    m_IniLastAccessTick = GetTickCount64(); // reading the file can take a long time
}

bool CProfile::StoreSettingsTo(const SettingsLocation newLocation)
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);

    if (newLocation == SETS_REGISTRY) {
        if (m_bRegistryMode) {
            return true; // already in the registry
        }

        InitIni();
        if (::PathFileExistsW(m_IniPath) && _wremove(m_IniPath) != 0) {
            return false; // ini can not be deleted; transfer canceled
        }

        m_bRegistryMode = true;
        OpenRegistryKey();
        if (m_hAppRegKey) {
            m_IniPath.Empty();
            return true;
        }
        m_bRegistryMode = false; // key creation failed; remain in INI mode
        return false;
    }

    // SETS_PROGRAMDIR
    if (!m_bRegistryMode && !m_IniPath.IsEmpty()) {
        return true; // already stored in the program folder
    }

    if (m_bRegistryMode) {
        OpenRegistryKey(); // ensure the handle so the registry store can be removed
        if (m_hAppRegKey) {
            RegDeleteTreeW(m_hAppRegKey, nullptr);
            RegCloseKey(m_hAppRegKey);
            m_hAppRegKey = nullptr;
        }
        m_bRegistryMode = false;
    }

    const CStringW newIniPath = GetNewIniPath();
    CFile file;
    if (file.Open(newIniPath, CFile::modeWrite | CFile::modeCreate | CFile::modeNoTruncate)) {
        file.Close();
        m_IniPath = newIniPath;
        Flush(true);
        return true;
    }

    return false;
}

bool CProfile::ReadBool(const wchar_t* section, const wchar_t* entry, bool& value)
{
    int v = value;
    bool ret = ReadInt(section, entry, v);
    if (ret) {
        // very strict check
        if (v == 0) {
            value = false;
        } else if (v == 1) {
            value = true;
        } else {
            ret = false;
        }
    }

    return ret;
}

bool CProfile::ReadInt(const wchar_t* section, const wchar_t* entry, int& value)
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);

    bool ret = false;

    if (m_bRegistryMode) {
        OpenRegistryKey();
        CRegKey regkey;
        if (ERROR_SUCCESS == regkey.Open(m_hAppRegKey, section, KEY_READ)) {
            if (ERROR_SUCCESS == regkey.QueryDWORDValue(entry, *(DWORD*)&value)) {
                ret = true;
            }
            regkey.Close();
        }
    } else {
        InitIni();
        auto it1 = m_ProfileMap.find(section);
        if (it1 != m_ProfileMap.end()) {
            auto it2 = it1->second.find(entry);
            if (it2 != it1->second.end()) {
                ret = StrToInt32(it2->second, value);
            }
        }
    }

    return ret;
}

bool CProfile::ReadInt(const wchar_t* section, const wchar_t* entry, int& value, const int lo, const int hi)
{
    int v = value;
    bool ret = ReadInt(section, entry, v);
    if (ret) {
        if (v >= lo && v <= hi) {
            value = v;
        } else {
            ret = false;
        }
    }

    return ret;
}

bool CProfile::ReadUInt(const wchar_t* section, const wchar_t* entry, unsigned& value)
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);

    bool ret = false;

    if (m_bRegistryMode) {
        OpenRegistryKey();
        CRegKey regkey;
        if (ERROR_SUCCESS == regkey.Open(m_hAppRegKey, section, KEY_READ)) {
            if (ERROR_SUCCESS == regkey.QueryDWORDValue(entry, *(DWORD*)&value)) {
                ret = true;
            }
            regkey.Close();
        }
    } else {
        InitIni();
        auto it1 = m_ProfileMap.find(section);
        if (it1 != m_ProfileMap.end()) {
            auto it2 = it1->second.find(entry);
            if (it2 != it1->second.end()) {
                ret = StrToUInt32(it2->second, value);
            }
        }
    }

    return ret;
}

bool CProfile::ReadUInt(const wchar_t* section, const wchar_t* entry, unsigned& value, const unsigned lo, const unsigned hi)
{
    unsigned v = value;
    bool ret = ReadUInt(section, entry, v);
    if (ret) {
        if (v >= lo && v <= hi) {
            value = v;
        } else {
            ret = false;
        }
    }

    return ret;
}

bool CProfile::ReadInt64(const wchar_t* section, const wchar_t* entry, __int64& value)
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);

    bool ret = false;

    if (m_bRegistryMode) {
        OpenRegistryKey();
        CRegKey regkey;
        if (ERROR_SUCCESS == regkey.Open(m_hAppRegKey, section, KEY_READ)) {
            if (ERROR_SUCCESS == regkey.QueryQWORDValue(entry, *(ULONGLONG*)&value)) {
                ret = true;
            }
            regkey.Close();
        }
    } else {
        InitIni();
        auto it1 = m_ProfileMap.find(section);
        if (it1 != m_ProfileMap.end()) {
            auto it2 = it1->second.find(entry);
            if (it2 != it1->second.end()) {
                ret = StrToInt64(it2->second, value);
            }
        }
    }

    return ret;
}

bool CProfile::ReadInt64(const wchar_t* section, const wchar_t* entry, __int64& value, const __int64 lo, const __int64 hi)
{
    __int64 v = value;
    bool ret = ReadInt64(section, entry, v);
    if (ret) {
        if (v >= lo && v <= hi) {
            value = v;
        } else {
            ret = false;
        }
    }

    return ret;
}

bool CProfile::ReadDouble(const wchar_t* section, const wchar_t* entry, double& value)
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);

    bool ret = false;

    if (m_bRegistryMode) {
        OpenRegistryKey();
        CRegKey regkey;
        if (ERROR_SUCCESS == regkey.Open(m_hAppRegKey, section, KEY_READ)) {
            ULONG nChars = 0;
            if (ERROR_SUCCESS == regkey.QueryStringValue(entry, nullptr, &nChars) && nChars > 0) {
                CStringW str;
                if (ERROR_SUCCESS == regkey.QueryStringValue(entry, str.GetBufferSetLength(nChars - 1), &nChars)) {
                    ret = StrToDouble(str, value);
                }
            }
            regkey.Close();
        }
    } else {
        InitIni();
        auto it1 = m_ProfileMap.find(section);
        if (it1 != m_ProfileMap.end()) {
            auto it2 = it1->second.find(entry);
            if (it2 != it1->second.end()) {
                ret = StrToDouble(it2->second, value);
            }
        }
    }

    return ret;
}

bool CProfile::ReadDouble(const wchar_t* section, const wchar_t* entry, double& value, const double lo, const double hi)
{
    double v = value;
    bool ret = ReadDouble(section, entry, v);
    if (ret) {
        if (v >= lo && v <= hi) {
            value = v;
        } else {
            ret = false;
        }
    }

    return ret;
}

bool CProfile::ReadHex32(const wchar_t* section, const wchar_t* entry, unsigned& value)
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);

    bool ret = false;

    if (m_bRegistryMode) {
        OpenRegistryKey();
        CRegKey regkey;
        if (ERROR_SUCCESS == regkey.Open(m_hAppRegKey, section, KEY_READ)) {
            if (ERROR_SUCCESS == regkey.QueryDWORDValue(entry, *(DWORD*)&value)) {
                ret = true;
            }
            regkey.Close();
        }
    } else {
        InitIni();
        auto it1 = m_ProfileMap.find(section);
        if (it1 != m_ProfileMap.end()) {
            auto it2 = it1->second.find(entry);
            if (it2 != it1->second.end()) {
                ret = StrHexToUInt32(it2->second, value);
            }
        }
    }

    return ret;
}

bool CProfile::ReadString(const wchar_t* section, const wchar_t* entry, CStringW& value)
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);

    bool ret = false;

    if (m_bRegistryMode) {
        OpenRegistryKey();
        CRegKey regkey;
        if (ERROR_SUCCESS == regkey.Open(m_hAppRegKey, section, KEY_READ)) {
            ULONG nChars = 0;
            if (ERROR_SUCCESS == regkey.QueryStringValue(entry, nullptr, &nChars) && nChars > 0) {
                if (ERROR_SUCCESS == regkey.QueryStringValue(entry, value.GetBufferSetLength(nChars - 1), &nChars)) {
                    ret = true;
                }
            }
            regkey.Close();
        }
    } else {
        InitIni();
        auto it1 = m_ProfileMap.find(section);
        if (it1 != m_ProfileMap.end()) {
            auto it2 = it1->second.find(entry);
            if (it2 != it1->second.end()) {
                value = it2->second;
                ret = true;
            }
        }
    }

    return ret;
}

bool CProfile::ReadBinary(const wchar_t* section, const wchar_t* entry, BYTE** ppdata, unsigned& nbytes)
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);

    bool ret = false;

    if (m_bRegistryMode) {
        OpenRegistryKey();
        CRegKey regkey;
        if (ERROR_SUCCESS == regkey.Open(m_hAppRegKey, section, KEY_READ)) {
            if (ERROR_SUCCESS == regkey.QueryBinaryValue(entry, nullptr, (ULONG*)&nbytes)) {
                *ppdata = new (std::nothrow) BYTE[nbytes];
                if (*ppdata && ERROR_SUCCESS == regkey.QueryBinaryValue(entry, *ppdata, (ULONG*)&nbytes)) {
                    ret = true;
                }
            }
            regkey.Close();
        }
    } else {
        CStringW valueStr;

        InitIni();
        auto it1 = m_ProfileMap.find(section);
        if (it1 != m_ProfileMap.end()) {
            auto it2 = it1->second.find(entry);
            if (it2 != it1->second.end()) {
                valueStr = it2->second;
            }
        }

        const int prefixLen = static_cast<int>(wcslen(BINARY_B64_PREFIX));
        if (wcsncmp(valueStr, BINARY_B64_PREFIX, prefixLen) == 0) {
            nbytes = Base64ToBinary(valueStr.Mid(prefixLen), ppdata);
        } else {
            // legacy value imported verbatim from an old INI (A-P encoded)
            nbytes = APToBinary(valueStr, ppdata);
        }
        ret = (nbytes != 0);
    }

    return ret;
}

bool CProfile::WriteBool(const wchar_t* section, const wchar_t* entry, const bool value)
{
    return WriteInt(section, entry, value ? 1 : 0); // strictly write "true" as 1
}

bool CProfile::WriteInt(const wchar_t* section, const wchar_t* entry, const int value)
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);

    bool ret = false;

    if (m_bRegistryMode) {
        OpenRegistryKey();
        CRegKey regkey;
        if (ERROR_SUCCESS == regkey.Create(m_hAppRegKey, section)) {
            if (ERROR_SUCCESS == regkey.SetDWORDValue(entry, (DWORD)value)) {
                ret = true;
            }
            regkey.Close();
        }
    } else {
        InitIni();
        CStringW valueStr;
        valueStr.Format(L"%d", value);
        CStringW& old = m_ProfileMap[section][entry];
        if (old != valueStr) {
            old = valueStr;
            m_bIniNeedFlush = true;
        }
        ret = true;
    }

    return ret;
}

bool CProfile::WriteUInt(const wchar_t* section, const wchar_t* entry, const unsigned value)
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);

    bool ret = false;

    if (m_bRegistryMode) {
        OpenRegistryKey();
        CRegKey regkey;
        if (ERROR_SUCCESS == regkey.Create(m_hAppRegKey, section)) {
            if (ERROR_SUCCESS == regkey.SetDWORDValue(entry, (DWORD)value)) {
                ret = true;
            }
            regkey.Close();
        }
    } else {
        InitIni();
        CStringW valueStr;
        valueStr.Format(L"%u", value);
        CStringW& old = m_ProfileMap[section][entry];
        if (old != valueStr) {
            old = valueStr;
            m_bIniNeedFlush = true;
        }
        ret = true;
    }

    return ret;
}

bool CProfile::WriteInt64(const wchar_t* section, const wchar_t* entry, const __int64 value)
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);

    bool ret = false;

    if (m_bRegistryMode) {
        OpenRegistryKey();
        CRegKey regkey;
        if (ERROR_SUCCESS == regkey.Create(m_hAppRegKey, section)) {
            if (ERROR_SUCCESS == regkey.SetQWORDValue(entry, (ULONGLONG)value)) {
                ret = true;
            }
            regkey.Close();
        }
    } else {
        CStringW valueStr;
        valueStr.Format(L"%I64d", value);

        InitIni();
        CStringW& old = m_ProfileMap[section][entry];
        if (old != valueStr) {
            old = valueStr;
            m_bIniNeedFlush = true;
        }
        ret = true;
    }

    return ret;
}

bool CProfile::WriteDouble(const wchar_t* section, const wchar_t* entry, const double value)
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);

    bool ret = false;

    CStringW valueStr;
    valueStr.Format(L"%.4f", value);

    if (m_bRegistryMode) {
        OpenRegistryKey();
        CRegKey regkey;
        if (ERROR_SUCCESS == regkey.Create(m_hAppRegKey, section)) {
            if (ERROR_SUCCESS == regkey.SetStringValue(entry, valueStr)) {
                ret = true;
            }
            regkey.Close();
        }
    } else {
        InitIni();
        CStringW& old = m_ProfileMap[section][entry];
        if (old != valueStr) {
            old = valueStr;
            m_bIniNeedFlush = true;
        }
        ret = true;
    }

    return ret;
}

bool CProfile::WriteHex32(const wchar_t* section, const wchar_t* entry, const unsigned value)
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);

    bool ret = false;

    if (m_bRegistryMode) {
        OpenRegistryKey();
        CRegKey regkey;
        if (ERROR_SUCCESS == regkey.Create(m_hAppRegKey, section)) {
            if (ERROR_SUCCESS == regkey.SetDWORDValue(entry, (DWORD)value)) {
                ret = true;
            }
            regkey.Close();
        }
    } else {
        InitIni();
        CStringW valueStr;
        valueStr.Format(L"0x%06X", value);
        CStringW& old = m_ProfileMap[section][entry];
        if (old != valueStr) {
            old = valueStr;
            m_bIniNeedFlush = true;
        }
        ret = true;
    }

    return ret;
}

bool CProfile::WriteString(const wchar_t* section, const wchar_t* entry, const CStringW& value)
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);

    bool ret = false;

    if (m_bRegistryMode) {
        OpenRegistryKey();
        CRegKey regkey;
        if (ERROR_SUCCESS == regkey.Create(m_hAppRegKey, section)) {
            if (ERROR_SUCCESS == regkey.SetStringValue(entry, value)) {
                ret = true;
            }
            regkey.Close();
        }
    } else {
        InitIni();
        CStringW& old = m_ProfileMap[section][entry];
        if (old != value) {
            old = value;
            m_bIniNeedFlush = true;
        }
        ret = true;
    }

    return ret;
}

bool CProfile::WriteBinary(const wchar_t* section, const wchar_t* entry, const BYTE* pdata, const unsigned nbytes)
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);

    bool ret = false;

    if (m_bRegistryMode) {
        OpenRegistryKey();
        CRegKey regkey;
        if (ERROR_SUCCESS == regkey.Create(m_hAppRegKey, section)) {
            if (ERROR_SUCCESS == regkey.SetBinaryValue(entry, pdata, nbytes)) {
                ret = true;
            }
            regkey.Close();
        }
    } else {
        CStringW base64 = BinaryToBase64(pdata, nbytes);
        if (!base64.IsEmpty()) {
            CStringW value = BINARY_B64_PREFIX + base64;
            InitIni();
            CStringW& old = m_ProfileMap[section][entry];
            if (old != value) {
                old = value;
                m_bIniNeedFlush = true;
            }
            ret = true;
        }
    }

    return ret;
}

void CProfile::EnumValueNames(const wchar_t* section, std::vector<CStringW>& valuenames)
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);

    valuenames.clear();

    if (m_bRegistryMode) {
        OpenRegistryKey();
        CRegKey regkey;
        if (ERROR_SUCCESS == regkey.Open(m_hAppRegKey, section, KEY_READ)) {
            DWORD cValues     = 0;
            DWORD cchMaxValue = 0;

            DWORD retCode = RegQueryInfoKeyW(regkey.m_hKey, nullptr, nullptr, nullptr, nullptr,
                                             nullptr, nullptr, &cValues, &cchMaxValue, nullptr, nullptr, nullptr);

            if (ERROR_SUCCESS == retCode && cValues && cchMaxValue) {
                std::vector<WCHAR> achValue(cchMaxValue + 1, L'\0');

                for (DWORD i = 0; i < cValues; i++) {
                    DWORD cchValue = cchMaxValue + 1;
                    achValue[0] = L'\0';
                    if (ERROR_SUCCESS == RegEnumValueW(regkey.m_hKey, i, achValue.data(), &cchValue, nullptr, nullptr, nullptr, nullptr)) {
                        valuenames.emplace_back(achValue.data(), cchValue);
                    }
                }
            }
            regkey.Close();
        }
    } else {
        InitIni();
        auto it1 = m_ProfileMap.find(section);
        if (it1 != m_ProfileMap.end()) {
            for (const auto& entry : it1->second) {
                valuenames.emplace_back(entry.first);
            }
        }
    }
}

void CProfile::EnumSectionNames(const wchar_t* section, std::vector<CStringW>& sectionnames)
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);

    sectionnames.clear();

    if (m_bRegistryMode) {
        OpenRegistryKey();
        CRegKey regkey;
        if (ERROR_SUCCESS == regkey.Open(m_hAppRegKey, section, KEY_READ)) {
            DWORD cSubKeys    = 0;
            DWORD cbMaxSubKey = 0;

            DWORD retCode = RegQueryInfoKeyW(regkey.m_hKey, nullptr, nullptr, nullptr, &cSubKeys,
                                             &cbMaxSubKey, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

            if (ERROR_SUCCESS == retCode && cSubKeys && cbMaxSubKey) {
                std::vector<WCHAR> achKey(cbMaxSubKey + 1, L'\0');

                for (DWORD i = 0; i < cSubKeys; i++) {
                    DWORD cbName = cbMaxSubKey + 1;
                    achKey[0] = L'\0';
                    if (ERROR_SUCCESS == RegEnumKeyExW(regkey.m_hKey, i, achKey.data(), &cbName, nullptr, nullptr, nullptr, nullptr)) {
                        sectionnames.emplace_back(achKey.data(), cbName);
                    }
                }
            }
            regkey.Close();
        }
    } else {
        InitIni();
        CStringW prefix(section);
        prefix += L'\\';

        // The map is ordered case-insensitively (IgnoreCaseLess), so prefix
        // matches are contiguous only when compared case-insensitively.
        auto it = m_ProfileMap.cbegin();
        while (it != m_ProfileMap.cend() && !StartsWithNoCase(it->first, prefix)) {
            ++it;
        }

        for (; it != m_ProfileMap.cend() && StartsWithNoCase(it->first, prefix); ++it) {
            CStringW child = it->first.Mid(prefix.GetLength());
            // Immediate children only, matching the registry branch and the
            // original GetSectionSubKeys semantics (skip deeper "a\b").
            if (!child.IsEmpty() && child.Find(L'\\') == -1) {
                sectionnames.emplace_back(child);
            }
        }
    }
}

bool CProfile::DeleteValue(const wchar_t* section, const wchar_t* entry)
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);

    bool ret = false;

    if (m_bRegistryMode) {
        OpenRegistryKey();
        CRegKey regkey;
        if (ERROR_SUCCESS == regkey.Open(m_hAppRegKey, section, KEY_WRITE)) {
            if (ERROR_SUCCESS == regkey.DeleteValue(entry)) {
                ret = true;
            }
            regkey.Close();
        }
    } else {
        InitIni();
        auto it = m_ProfileMap.find(section);
        if (it != m_ProfileMap.end()) {
            if (it->second.erase(entry)) {
                m_bIniNeedFlush = true;
                ret = true;
            }
        }
    }

    return ret;
}

bool CProfile::DeleteSection(const wchar_t* section)
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);

    bool ret = false;

    if (m_bRegistryMode) {
        OpenRegistryKey();
        CRegKey regkey;
        if (ERROR_SUCCESS == regkey.Open(m_hAppRegKey, L"", KEY_WRITE)) {
            if (ERROR_SUCCESS == regkey.RecurseDeleteKey(section)) {
                ret = true;
            }
            regkey.Close();
        }
    } else {
        InitIni();
        const CStringW mainsection(section);
        const CStringW prefix(mainsection + L'\\');

        // Erase the section and every subsection. Iterate-and-erase rather than a
        // contiguous range: a sibling like "Foo0" can sort between "Foo" and
        // "Foo\bar" under case-insensitive ordering, so the matches are not
        // guaranteed contiguous.
        for (auto it = m_ProfileMap.begin(); it != m_ProfileMap.end();) {
            if (it->first.CompareNoCase(mainsection) == 0 || StartsWithNoCase(it->first, prefix)) {
                it = m_ProfileMap.erase(it);
                m_bIniNeedFlush = true;
                ret = true;
            } else {
                ++it;
            }
        }
    }

    return ret;
}

void CProfile::Flush(bool bForce)
{
    if (m_bRegistryMode || (!bForce && !m_bIniNeedFlush)) {
        return;
    }

    std::lock_guard<std::recursive_mutex> lock(m_Mutex);

    m_bIniNeedFlush = false;

    ASSERT(m_bIniFirstInit);
    ASSERT(!m_IniPath.IsEmpty());

    FILE* fp;
    int fpStatus;
    do { // Open the ini, retry if it is already being used by another process
        fp = _wfsopen(m_IniPath, L"w, ccs=UTF-8", _SH_SECURE);
        if (fp || (GetLastError() != ERROR_SHARING_VIOLATION)) {
            break;
        }
        Sleep(100);
    } while (true);
    if (!fp) {
        ASSERT(FALSE);
        return;
    }
    CStdioFile file(fp);
    CStringW line;
    try {
        file.WriteString(L"; MPC-HC\n");
        for (auto it1 = m_ProfileMap.begin(); it1 != m_ProfileMap.end(); ++it1) {
            line.Format(L"[%s]\n", it1->first.GetString());
            file.WriteString(line);
            for (auto it2 = it1->second.begin(); it2 != it1->second.end(); ++it2) {
                line.Format(L"%s=%s\n", it2->first.GetString(), it2->second.GetString());
                file.WriteString(line);
            }
        }
    } catch (CFileException& e) {
        // Fail silently if disk is full
        UNREFERENCED_PARAMETER(e);
        ASSERT(FALSE);
    }

    fpStatus = fclose(fp);
    ASSERT(fpStatus == 0);
}

void CProfile::Clear()
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);

    if (m_bRegistryMode) {
        OpenRegistryKey();
        if (m_hAppRegKey) {
            RegDeleteTreeW(m_hAppRegKey, nullptr);
        }
    } else {
        ASSERT(!m_IniPath.IsEmpty());
        CFile file;
        if (file.Open(m_IniPath, CFile::modeWrite)) {
            file.SetLength(0); // clear, but do not delete
            file.Close();
        }
    }

    m_ProfileMap.clear();
    m_bIniNeedFlush = false;
}

// Recursively copy a registry key's values and subkeys from src to dst,
// preserving native value types. Unlike RegCopyTree it does NOT copy security
// descriptors, so it works with a dst handle opened only for KEY_READ|KEY_WRITE
// (RegCopyTree needs WRITE_DAC to clone the DACL and fails with ACCESS_DENIED).
// Buffer sizes (in WCHARs, incl. NUL) for enumerating a key's value and subkey
// names, from RegQueryInfoKey. Registry value names can be up to 16383 chars and
// key names up to 255, so fixed buffers would silently drop long names.
static void RegNameBufferSizes(HKEY key, DWORD& valueNameChars, DWORD& subKeyNameChars)
{
    DWORD maxValueName = 0, maxSubKeyName = 0;
    if (RegQueryInfoKeyW(key, nullptr, nullptr, nullptr, nullptr, &maxSubKeyName,
                         nullptr, nullptr, &maxValueName, nullptr, nullptr, nullptr) != ERROR_SUCCESS) {
        maxValueName = 16383;
        maxSubKeyName = 255;
    }
    valueNameChars = maxValueName + 1;
    subKeyNameChars = maxSubKeyName + 1;
}

static void CopyRegistryTree(HKEY src, HKEY dst)
{
    DWORD valueNameChars, subKeyNameChars;
    RegNameBufferSizes(src, valueNameChars, subKeyNameChars);
    std::vector<WCHAR> name(valueNameChars);
    std::vector<WCHAR> sub(subKeyNameChars);

    // Values at this level.
    for (DWORD i = 0;; i++) {
        DWORD nameLen = static_cast<DWORD>(name.size()), type = 0, dataLen = 0;
        LONG r = RegEnumValueW(src, i, name.data(), &nameLen, nullptr, &type, nullptr, &dataLen);
        if (r == ERROR_NO_MORE_ITEMS) {
            break;
        }
        if (r != ERROR_SUCCESS) {
            continue;
        }
        std::vector<BYTE> data(dataLen ? dataLen : 1);
        DWORD cb = dataLen;
        if (RegQueryValueExW(src, name.data(), nullptr, &type, data.data(), &cb) == ERROR_SUCCESS) {
            RegSetValueExW(dst, name.data(), 0, type, data.data(), cb);
        }
    }
    // Subkeys.
    for (DWORD i = 0;; i++) {
        DWORD subLen = static_cast<DWORD>(sub.size());
        LONG r = RegEnumKeyExW(src, i, sub.data(), &subLen, nullptr, nullptr, nullptr, nullptr);
        if (r == ERROR_NO_MORE_ITEMS) {
            break;
        }
        if (r != ERROR_SUCCESS) {
            continue;
        }
        HKEY hSrcSub, hDstSub;
        if (RegOpenKeyExW(src, sub.data(), 0, KEY_READ, &hSrcSub) == ERROR_SUCCESS) {
            if (RegCreateKeyExW(dst, sub.data(), 0, nullptr, 0, KEY_READ | KEY_WRITE, nullptr, &hDstSub, nullptr) == ERROR_SUCCESS) {
                CopyRegistryTree(hSrcSub, hDstSub);
                RegCloseKey(hDstSub);
            }
            RegCloseKey(hSrcSub);
        }
    }
}

// Enumerate a registry key recursively into a ProfileMap, converting each value
// to the INI textual form CProfile uses (so a registry store can seed an INI
// fork). `section` is the path built from subkeys ("" at the root).
static void EnumerateRegistryTreeToMap(HKEY hKey, const CStringW& section, ProfileMap& out)
{
    DWORD valueNameChars, subKeyNameChars;
    RegNameBufferSizes(hKey, valueNameChars, subKeyNameChars);
    std::vector<WCHAR> name(valueNameChars);
    std::vector<WCHAR> sub(subKeyNameChars);

    if (!section.IsEmpty()) {
        for (DWORD i = 0;; i++) {
            DWORD nameLen = static_cast<DWORD>(name.size()), type = 0, dataLen = 0;
            LONG r = RegEnumValueW(hKey, i, name.data(), &nameLen, nullptr, &type, nullptr, &dataLen);
            if (r == ERROR_NO_MORE_ITEMS) {
                break;
            }
            if (r != ERROR_SUCCESS || dataLen == 0) {
                continue;
            }
            std::vector<BYTE> data(dataLen);
            DWORD cb = dataLen;
            if (RegQueryValueExW(hKey, name.data(), nullptr, &type, data.data(), &cb) != ERROR_SUCCESS) {
                continue;
            }
            CStringW val;
            switch (type) {
                case REG_DWORD:
                    if (cb < sizeof(DWORD)) continue;
                    val.Format(L"%d", static_cast<int>(*reinterpret_cast<DWORD*>(data.data())));
                    break;
                case REG_QWORD:
                    if (cb < sizeof(ULONGLONG)) continue;
                    val.Format(L"%I64d", *reinterpret_cast<ULONGLONG*>(data.data()));
                    break;
                case REG_SZ:
                case REG_EXPAND_SZ:
                    val = CStringW(reinterpret_cast<LPCWSTR>(data.data()), static_cast<int>(cb / sizeof(WCHAR)));
                    { int nul = val.Find(L'\0'); if (nul >= 0) val.Truncate(nul); }
                    break;
                case REG_BINARY:
                    val = CStringW(BINARY_B64_PREFIX) + BinaryToBase64(data.data(), cb);
                    break;
                default:
                    continue;
            }
            out[section][name.data()] = val;
        }
    }

    for (DWORD i = 0;; i++) {
        DWORD subLen = static_cast<DWORD>(sub.size());
        LONG r = RegEnumKeyExW(hKey, i, sub.data(), &subLen, nullptr, nullptr, nullptr, nullptr);
        if (r == ERROR_NO_MORE_ITEMS) {
            break;
        }
        if (r != ERROR_SUCCESS) {
            continue;
        }
        HKEY hSub;
        if (RegOpenKeyExW(hKey, sub.data(), 0, KEY_READ, &hSub) == ERROR_SUCCESS) {
            CStringW child = section.IsEmpty() ? CStringW(sub.data()) : (section + L"\\" + sub.data());
            EnumerateRegistryTreeToMap(hSub, child, out);
            RegCloseKey(hSub);
        }
    }
}

bool CProfile::MigrateFromLegacy()
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);

    if (m_bRegistryMode) {
        // Registry: recursively copy the legacy MFC key into the new key,
        // preserving native value types (REG_DWORD/REG_SZ/REG_BINARY/...).
        HKEY hLegacy = nullptr;
        if (ERROR_SUCCESS != RegOpenKeyExW(HKEY_CURRENT_USER, LEGACY_REG_KEY, 0, KEY_READ, &hLegacy)) {
            return false;
        }
        OpenRegistryKey();
        CopyRegistryTree(hLegacy, m_hAppRegKey);
        RegCloseKey(hLegacy);
        return true;
    }

    // INI: parse the legacy <exe>.ini verbatim into the (new) map, then flush
    // it to the new file. Old A-P binary values are read back later via the
    // un-prefixed path in ReadBinary().
    const CStringW legacyIni = GetLegacyIniPath();
    if (legacyIni.CompareNoCase(m_IniPath) == 0) {
        return false; // defensive: never import a file onto itself
    }
    ProfileMap legacyMap;
    if (!ReadIniFileIntoMap(legacyIni, legacyMap)) {
        return false;
    }

    InitIni(); // ensure the (new, likely empty) map is loaded before merging
    for (const auto& sec : legacyMap) {
        for (const auto& kv : sec.second) {
            m_ProfileMap[sec.first][kv.first] = kv.second;
        }
    }
    m_bIniNeedFlush = true;
    Flush(true);
    return true;
}

bool CProfile::ForkToLocalIni(const CStringW& iniPath, bool seedFromCurrent)
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);

    // Capture the current store's contents first (if seeding a fresh fork).
    ProfileMap seed;
    if (seedFromCurrent) {
        if (m_bRegistryMode) {
            OpenRegistryKey();
            if (m_hAppRegKey) {
                EnumerateRegistryTreeToMap(m_hAppRegKey, CStringW(), seed);
            }
        } else {
            InitIni();
            seed = m_ProfileMap;
        }
    }

    // Detach from the shared store WITHOUT deleting it (the newer store survives).
    if (m_hAppRegKey) {
        RegCloseKey(m_hAppRegKey);
        m_hAppRegKey = nullptr;
    }
    m_bRegistryMode = false;
    m_IniPath = iniPath;
    m_ProfileMap.clear();
    m_bIniFirstInit = false; // force InitIni() to (re)read the local file if present
    m_bIniNeedFlush = false;
    InitIni();

    // Fill in from the seed without overwriting anything already in an existing
    // local fork file.
    if (seedFromCurrent && !seed.empty()) {
        for (const auto& sec : seed) {
            for (const auto& kv : sec.second) {
                auto& dst = m_ProfileMap[sec.first];
                if (dst.find(kv.first) == dst.end()) {
                    dst[kv.first] = kv.second;
                    m_bIniNeedFlush = true;
                }
            }
        }
    }

    return true;
}

void CProfile::MoveSectionTree(const wchar_t* root, CProfile& dst)
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);

    if (m_bRegistryMode) {
        return; // the MediaHistory split only applies in INI mode
    }

    InitIni();
    const CStringW rootStr(root);
    const CStringW prefix(rootStr + L"\\");
    bool moved = false;

    for (auto it = m_ProfileMap.begin(); it != m_ProfileMap.end();) {
        if (it->first.CompareNoCase(rootStr) == 0 || StartsWithNoCase(it->first, prefix)) {
            for (const auto& kv : it->second) {
                dst.WriteString(it->first, kv.first, kv.second); // raw value copy (already new-format)
            }
            it = m_ProfileMap.erase(it);
            moved = true;
        } else {
            ++it;
        }
    }

    if (moved) {
        m_bIniNeedFlush = true;
    }
}

bool CProfile::HasEntry(const wchar_t* section, const wchar_t* entry)
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);

    bool ret = false;

    if (m_bRegistryMode) {
        OpenRegistryKey();
        CRegKey regkey;
        if (ERROR_SUCCESS == regkey.Open(m_hAppRegKey, section, KEY_READ)) {
            ret = (ERROR_SUCCESS == RegQueryValueExW(regkey.m_hKey, entry, nullptr, nullptr, nullptr, nullptr));
            regkey.Close();
        }
    } else {
        InitIni();
        auto it1 = m_ProfileMap.find(section);
        if (it1 != m_ProfileMap.end()) {
            ret = (it1->second.find(entry) != it1->second.end());
        }
    }

    return ret;
}

CStringW CProfile::GetRegistryKeyPath() const
{
    return m_bRegistryMode ? CStringW(SETTINGS_REG_KEY) : CStringW();
}

SettingsLocation CProfile::GetSettingsLocation() const
{
    if (m_bRegistryMode) {
        return SETS_REGISTRY;
    }
    return SETS_PROGRAMDIR;
}
