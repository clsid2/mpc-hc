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
// (src/DSUtil/Profile.{h,cpp}). It replaces the old inline profile code in
// CMPlayerCApp but keeps the SAME on-disk location and format, so external
// tools and older builds keep reading the settings unchanged:
//   - Registry  : HKCU\Software\MPC-HC\MPC-HC
//   - Portable  : <exe-basename>.ini next to the executable
// Binary values use the legacy A-P encoding for byte compatibility. The only
// relocation is MediaHistory, which moves to <exe-basename>.history.ini in
// portable mode (see mplayerc.cpp / SetupHistoryStore).

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

    // registry. m_bRegistryMode is the store-location flag decided (read-only) at
    // construction; m_hAppRegKey is opened/created LAZILY on first actual access
    // (OpenRegistryKey), so no registry key is materialized at static-init time -
    // only when the running app truly reads/writes settings. Registry-branch code
    // must gate on m_bRegistryMode (not the handle) and call OpenRegistryKey().
    bool m_bRegistryMode = false;
    HKEY m_hAppRegKey = nullptr;

    // INI file
    CStringW m_IniPath;

    ProfileMap m_ProfileMap;
    bool      m_bIniFirstInit = false;
    bool      m_bIniNeedFlush = false;
    ULONGLONG m_IniLastAccessTick = 0;

public:
    CProfile();
    // Force INI mode bound to a specific file (used for the separate
    // MediaHistory store), bypassing the portable/registry auto-detection.
    explicit CProfile(const CStringW& iniFilePath);
    ~CProfile();

    // Path of the separate MediaHistory INI.
    static CStringW HistoryIniPath();       // <exe-basename>.history.ini
    // Path where a portable settings INI would be created for this build. Used to
    // report a prospective target even in registry mode (e.g. for the "store to
    // ini" option's write-permission check), where GetIniPath() is empty.
    static CStringW DefaultIniPath();

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

    bool HasEntry(const wchar_t* section, const wchar_t* entry);

    bool DeleteValue(const wchar_t* section, const wchar_t* entry);
    bool DeleteSection(const wchar_t* section);

    void Flush(bool bForce);
    void Clear();

    // Move a section and all its subsections ("root" and "root\...") into
    // another profile (preserving raw values) and remove them from this one.
    // Used once to split MediaHistory out into its own store. INI mode.
    void MoveSectionTree(const wchar_t* root, CProfile& dst);

    SettingsLocation GetSettingsLocation() const;

    // Registry path ("Software\\MPC-HC\\...") of the store when in registry
    // mode, empty otherwise. Used by settings export.
    CStringW GetRegistryKeyPath() const;

    CStringW GetIniPath() const {
        return m_IniPath;
    }
};

// CMPlayerCApp owns a CProfile (m_Profile) and delegates MFC's GetProfile*/
// WriteProfile* overrides to it (see mplayerc.cpp).
#define AfxGetProfile() (AfxGetMyApp()->m_Profile)
