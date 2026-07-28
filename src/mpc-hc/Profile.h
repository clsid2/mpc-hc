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

// On-disk format versions. The store name encodes the version (Settings-<ver> /
// History-<ver>); bump the version when the layout changes incompatibly and add
// a migration step. Older builds only know their own version and ignore newer
// stores. See docs/settings-versioning.md.
inline const wchar_t* const SETTINGS_FORMAT_VERSION = L"v1";
inline const wchar_t* const HISTORY_FORMAT_VERSION  = L"v1";
// The pre-versioned legacy layout is identical to the first format version, so
// it seeds the migration chain as that version (not necessarily the current one).
inline const wchar_t* const LEGACY_EQUIVALENT_VERSION = L"v1";

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

    // Path of the separate MediaHistory INI (<exe-basename>.history-<ver>.ini).
    static CStringW HistoryIniPath();
    // Path of the version-independent index/manifest INI (<exe-basename>.settings.ini)
    // that records which settings/history format versions are present.
    static CStringW SettingsIndexIniPath();
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

    // One-time legacy import: copy the OLD MPC-HC settings (legacy registry key
    // or legacy <exe>.ini) into this (new) store, in the matching storage mode.
    // Registry is copied with native value types; INI is copied verbatim (old
    // A-P binary values are read back via the un-prefixed path in ReadBinary).
    // Returns true if a legacy store was found and imported. Deletable at the
    // legacy-format sunset. Caller guards against re-running.
    bool MigrateFromLegacy();

    // Seed this (empty, current-version) store with the full contents of the
    // Settings-<fromVersion> store, in this store's mode (registry or INI). Used
    // by the migration driver before applying in-place format transforms.
    // Returns true if the source store existed and was copied.
    bool SeedFromVersion(const CStringW& fromVersion);

    // Format versions for which a Settings-<ver> store currently exists, in this
    // store's mode (registry subkeys / <exe>.settings-<ver>.ini files). Used to
    // pick the best older store to seed/migrate from.
    void EnumSettingsStoreVersions(std::vector<CStringW>& versions);

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
