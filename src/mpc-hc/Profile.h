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

// Settings store engine for MPC-HC, adapted from MPC-BE's CProfile
// (src/DSUtil/Profile.{h,cpp}). This is the NEW versioned settings format;
// the pre-existing inline profile code in CMPlayerCApp remains the LEGACY
// format and is imported once (see the legacy importer, added separately).
//
// New-format store locations (see Profile.cpp for the name constants):
//   - Program-dir INI  : a NEW .ini file next to the executable (portable).
//   - Registry         : a NEW key SIBLING to the legacy one, under the
//                        shared HKCU\Software\MPC-HC parent.
// The legacy store (old .ini / old registry key) is left untouched so older
// MPC-HC builds keep working (downgrade-safe).

#pragma once

#include <mutex>
#include <map>
#include <vector>
#include "DSUtil.h" // CStringUtils::IgnoreCaseLess

enum SettingsLocation {
    SETS_REGISTRY,
    SETS_PROGRAMDIR
};

// Sections and keys are matched case-insensitively; sections are kept ordered
// (std::map) because EnumSectionNames() relies on the ordering to range-walk
// subsections by "<section>\" prefix.
using ProfileSection = std::map<CStringW, CStringW, CStringUtils::IgnoreCaseLess>;
using ProfileMap     = std::map<CStringW, ProfileSection, CStringUtils::IgnoreCaseLess>;

class CProfile
{
private:
    std::recursive_mutex m_Mutex;

    // registry
    HKEY m_hAppRegKey = nullptr;

    // INI file
    CStringW m_IniPath;

    ProfileMap m_ProfileMap;
    bool      m_bIniFirstInit = false;
    bool      m_bIniNeedFlush = false;
    ULONGLONG m_IniLastAccessTick = 0;

public:
    CProfile();
    ~CProfile();

private:
    LONG OpenRegistryKey();
    // read all the fields from the ini file
    void InitIni();

public:
    bool StoreSettingsTo(const SettingsLocation newLocation);

    bool ReadBool  (const wchar_t* section, const wchar_t* entry, bool&     value);
    bool ReadInt   (const wchar_t* section, const wchar_t* entry, int&      value);
    bool ReadInt   (const wchar_t* section, const wchar_t* entry, int&      value, const int lo, const int hi);
    bool ReadUInt  (const wchar_t* section, const wchar_t* entry, unsigned& value);
    bool ReadUInt  (const wchar_t* section, const wchar_t* entry, unsigned& value, const unsigned lo, const unsigned hi);
    bool ReadInt64 (const wchar_t* section, const wchar_t* entry, __int64&  value);
    bool ReadInt64 (const wchar_t* section, const wchar_t* entry, __int64&  value, const __int64 lo, const __int64 hi);
    bool ReadDouble(const wchar_t* section, const wchar_t* entry, double&   value);
    bool ReadDouble(const wchar_t* section, const wchar_t* entry, double&   value, const double lo, const double hi);
    bool ReadHex32 (const wchar_t* section, const wchar_t* entry, unsigned& value);
    bool ReadString(const wchar_t* section, const wchar_t* entry, CStringW& value);
    bool ReadBinary(const wchar_t* section, const wchar_t* entry, BYTE** ppdata, unsigned& nbytes);

    bool WriteBool  (const wchar_t* section, const wchar_t* entry, const bool      value);
    bool WriteInt   (const wchar_t* section, const wchar_t* entry, const int       value);
    bool WriteUInt  (const wchar_t* section, const wchar_t* entry, const unsigned  value);
    bool WriteInt64 (const wchar_t* section, const wchar_t* entry, const __int64   value);
    bool WriteDouble(const wchar_t* section, const wchar_t* entry, const double    value);
    bool WriteHex32 (const wchar_t* section, const wchar_t* entry, const unsigned  value);
    bool WriteString(const wchar_t* section, const wchar_t* entry, const CStringW& value);
    bool WriteBinary(const wchar_t* section, const wchar_t* entry, const BYTE* pdata, const unsigned nbytes);

    void EnumValueNames(const wchar_t* section, std::vector<CStringW>& valuenames);
    void EnumSectionNames(const wchar_t* section, std::vector<CStringW>& sectionnames);

    bool DeleteValue(const wchar_t* section, const wchar_t* entry);
    bool DeleteSection(const wchar_t* section);

    void Flush(bool bForce);
    void Clear();

    // One-time legacy import: copy the OLD MPC-HC settings (legacy registry key
    // or legacy <exe>.ini) into this (new) store, in the matching storage mode.
    // Registry is copied with native value types; INI is copied verbatim (old
    // A-P binary values are read back via the un-prefixed path in ReadBinary).
    // Returns true if a legacy store was found and imported. Deletable at the
    // legacy-format sunset. Caller guards against re-running.
    bool MigrateFromLegacy();

    // Downgrade protection: when an older build opens a store last written by a
    // newer build, switch this instance to a private INI next to the executable
    // (loaded if it already exists, else started empty) and DETACH from the
    // shared store WITHOUT deleting it, so the newer build's settings survive.
    // Registry mode only needs this; a portable store is already local.
    bool ForkToLocalIni();

    SettingsLocation GetSettingsLocation() const;

    CStringW GetIniPath() const {
        return m_IniPath;
    }
};

// App base class that routes MFC's profile calls to CProfile, so existing
// call sites (theApp.GetProfileInt(...) etc.) keep working unchanged.
// CMPlayerCApp will derive from this instead of CWinAppEx (wired separately).
class CModApp : public CWinAppEx
{
public:
    CProfile m_Profile;

    // Must override GetProfile/WriteProfile, because MFC and other MFC
    // dependent libraries use them.
    UINT GetProfileInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, int nDefault) override {
        int value;
        if (m_Profile.ReadInt(lpszSection, lpszEntry, value)) {
            return value;
        }
        return nDefault;
    }
    BOOL WriteProfileInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, int nValue) override {
        return m_Profile.WriteInt(lpszSection, lpszEntry, nValue);
    }
    CString GetProfileString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpszDefault = nullptr) override {
        CStringW value;
        if (!m_Profile.ReadString(lpszSection, lpszEntry, value) && lpszDefault) {
            value = lpszDefault;
        }
        return value;
    }
    BOOL WriteProfileString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpszValue) override {
        if (!lpszValue) {
            if (!lpszEntry) {
                return m_Profile.DeleteSection(lpszSection);
            }
            return m_Profile.DeleteValue(lpszSection, lpszEntry);
        }
        return m_Profile.WriteString(lpszSection, lpszEntry, lpszValue);
    }
    BOOL GetProfileBinary(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPBYTE* ppData, UINT* pBytes) override {
        return m_Profile.ReadBinary(lpszSection, lpszEntry, ppData, *pBytes) ? TRUE : FALSE;
    }
    BOOL WriteProfileBinary(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPBYTE pData, UINT nBytes) override {
        return m_Profile.WriteBinary(lpszSection, lpszEntry, pData, nBytes);
    }
};

#define AfxGetProfile() static_cast<CModApp*>(AfxGetApp())->m_Profile
