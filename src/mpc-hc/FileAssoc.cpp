/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2018 see Authors.txt
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
#include <VersionHelpersInternal.h>
#include "mplayerc.h"
#include "FileAssoc.h"
#include "resource.h"
#include "PathUtils.h"


// TODO: change this along with the root key for settings and the mutex name to
//       avoid possible risks of conflict with the old MPC (non HC version).
#ifdef _WIN64
#define PROGID _T("mplayerc64")
#else
#define PROGID _T("mplayerc")
#endif // _WIN64

CFileAssoc::IconLib::IconLib(GetIconIndexFunc fnGetIconIndex, GetIconLibVersionFunc fnGetIconLibVersion, HMODULE hLib)
    : m_fnGetIconIndex(fnGetIconIndex)
    , m_fnGetIconLibVersion(fnGetIconLibVersion)
    , m_hLib(hLib)
{
    ASSERT(fnGetIconIndex && fnGetIconLibVersion && hLib);
}

CFileAssoc::IconLib::~IconLib()
{
    VERIFY(FreeLibrary(m_hLib));
}

int CFileAssoc::IconLib::GetIconIndex(const CString& str) const
{
    return m_fnGetIconIndex(str);
}

UINT CFileAssoc::IconLib::GetVersion() const
{
    return m_fnGetIconLibVersion();
}

void CFileAssoc::IconLib::SaveVersion() const
{
    AfxGetApp()->WriteProfileInt(IDS_R_SETTINGS, IDS_RS_ICON_LIB_VERSION, m_fnGetIconLibVersion());
}

CFileAssoc::CFileAssoc()
    : m_iconLibPath(PathUtils::CombinePaths(PathUtils::GetProgramPath(), _T("mpciconlib.dll")))
    , m_strRegisteredAppName(_T("Media Player Classic"))
    , m_strOldAssocKey(_T("PreviousRegistration"))
    , m_strRegisteredAppKey(_T("Software\\Clients\\Media\\Media Player Classic\\Capabilities"))
    , m_strRegAppFileAssocKey(_T("Software\\Clients\\Media\\Media Player Classic\\Capabilities\\FileAssociations"))
    , m_strOpenCommand(_T("\"") + PathUtils::GetProgramPath(true) + _T("\" \"%1\""))
    , m_strEnqueueCommand(_T("\"") + PathUtils::GetProgramPath(true) + _T("\" /add \"%1\""))
    , m_bNoRecentDocs(false)
    , m_checkIconsAssocInactiveEvent(TRUE, TRUE) // initially set, manual reset
{
    m_handlers[0] = { _T("VideoFiles"), _T(" %1"), IDS_AUTOPLAY_PLAYVIDEO };
    m_handlers[1] = { _T("MusicFiles"), _T(" %1"), IDS_AUTOPLAY_PLAYMUSIC };
    m_handlers[2] = { _T("CDAudio"), _T(" %1 /cd"), IDS_AUTOPLAY_PLAYAUDIOCD };
    m_handlers[3] = { _T("DVDMovie"), _T(" %1 /dvd"), IDS_AUTOPLAY_PLAYDVDMOVIE };
}

CFileAssoc::~CFileAssoc()
{
    HANDLE hEvent = m_checkIconsAssocInactiveEvent;
    DWORD dwEvent;
    VERIFY(CoWaitForMultipleHandles(0, INFINITE, 1, &hEvent, &dwEvent) == S_OK);
}

void CFileAssoc::LoadAAR()
{
    if (!m_pAAR) {
        // Default manager (requires at least Vista)
        VERIFY(CoCreateInstance(CLSID_ApplicationAssociationRegistration, nullptr,
                            CLSCTX_INPROC, IID_PPV_ARGS(&m_pAAR)) != CO_E_NOTINITIALIZED);
    }
}

std::shared_ptr<const CFileAssoc::IconLib> CFileAssoc::GetIconLib() const
{
    std::shared_ptr<IconLib> ret;
    if (HMODULE hLib = LoadLibrary(m_iconLibPath)) {
        auto fnGetIconIndex = reinterpret_cast<IconLib::GetIconIndexFunc>(GetProcAddress(hLib, "GetIconIndex"));
        auto fnGetIconLibVersion = reinterpret_cast<IconLib::GetIconLibVersionFunc>(GetProcAddress(hLib, "GetIconLibVersion"));
        if (fnGetIconIndex && fnGetIconLibVersion) {
            ret = std::make_shared<IconLib>(fnGetIconIndex, fnGetIconLibVersion, hLib);
        } else {
            VERIFY(FreeLibrary(hLib));
        }
    }
    return ret;
}

void CFileAssoc::SetNoRecentDocs(bool bNoRecentDocs, bool bUpdateAssocs /*= false*/)
{
    if (bNoRecentDocs == m_bNoRecentDocs) {
        bUpdateAssocs = false;
    } else {
        m_bNoRecentDocs = bNoRecentDocs;
    }

    CAtlList<CString> exts;
    if (bUpdateAssocs && GetAssociatedExtensionsFromRegistry(exts)) {
        CRegKey key;
        POSITION pos = exts.GetHeadPosition();
        while (pos) {
            const CString& ext = exts.GetNext(pos);

            if (ERROR_SUCCESS == key.Open(HKEY_CLASSES_ROOT, PROGID + ext)) {
                if (m_bNoRecentDocs) {
                    key.SetStringValue(_T("NoRecentDocs"), _T(""));
                } else {
                    key.DeleteValue(_T("NoRecentDocs"));
                }
            }
        }
    }
}

bool CFileAssoc::RegisterApp()
{
    bool success = false;

    LoadAAR();
    if (m_pAAR) {
        CString appIcon = _T("\"") + PathUtils::GetProgramPath(true) + _T("\",0");

        // Register MPC-HC for the windows "Default application" manager
        CRegKey key;

        if (ERROR_SUCCESS == key.Open(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\RegisteredApplications"))) {
            key.SetStringValue(_T("Media Player Classic"), m_strRegisteredAppKey);

            if (ERROR_SUCCESS == key.Create(HKEY_LOCAL_MACHINE, m_strRegisteredAppKey)) {
                // ==>>  TODO icon !!!
                key.SetStringValue(_T("ApplicationDescription"), ResStr(IDS_APP_DESCRIPTION), REG_EXPAND_SZ);
                key.SetStringValue(_T("ApplicationIcon"), appIcon, REG_EXPAND_SZ);
                key.SetStringValue(_T("ApplicationName"), ResStr(IDR_MAINFRAME), REG_EXPAND_SZ);

                success = true;
            }
        }
    }

    return success;
}

bool CFileAssoc::Register(CString ext, CString strLabel, bool bRegister, bool bAddEnqueueContextMenu, bool bAssociatedWithIcon)
{
    CRegKey key;
    CString strProgID = PROGID + ext;

    if (!bRegister) {
        // On Windows 8, an app can't set itself as the default handler for a format
        if (!IsWindows8OrGreater() && bRegister != IsRegistered(ext)) {
            SetFileAssociation(ext, strProgID, bRegister);
        }

        key.Attach(HKEY_CLASSES_ROOT);
        key.RecurseDeleteKey(strProgID);

        if (ERROR_SUCCESS == key.Open(HKEY_LOCAL_MACHINE, m_strRegAppFileAssocKey)) {
            key.DeleteValue(ext);
        }

        return true;
    } else {
        // Create ProgID for this file type
        if (ERROR_SUCCESS != key.Create(HKEY_CLASSES_ROOT, strProgID)
                || ERROR_SUCCESS != key.SetStringValue(nullptr, strLabel)) {
            return false;
        }

        if (m_bNoRecentDocs) {
            key.SetStringValue(_T("NoRecentDocs"), _T(""));
        } else {
            key.DeleteValue(_T("NoRecentDocs"));
        }

        CString appIcon = _T("\"") + PathUtils::GetProgramPath(true) + _T("\",0");

        // Add to playlist option
        if (bAddEnqueueContextMenu) {
            if (ERROR_SUCCESS != key.Create(HKEY_CLASSES_ROOT, strProgID + _T("\\shell\\enqueue"))
                    || ERROR_SUCCESS != key.SetStringValue(nullptr, ResStr(IDS_ADD_TO_PLAYLIST))
                    || ERROR_SUCCESS != key.SetStringValue(_T("Icon"), appIcon)
                    || ERROR_SUCCESS != key.SetStringValue(_T("MultiSelectModel"), _T("Player"))
                    || ERROR_SUCCESS != key.Create(HKEY_CLASSES_ROOT, strProgID + _T("\\shell\\enqueue\\command"))
                    || ERROR_SUCCESS != key.SetStringValue(nullptr, m_strEnqueueCommand)) {
                return false;
            }
        } else {
            key.Close();
            key.Attach(HKEY_CLASSES_ROOT);
            key.RecurseDeleteKey(strProgID + _T("\\shell\\enqueue"));
        }

        // Play option
        if (ERROR_SUCCESS != key.Create(HKEY_CLASSES_ROOT, strProgID + _T("\\shell\\open"))) {
            return false;
        }
        if (ERROR_SUCCESS != key.SetStringValue(nullptr, ResStr(IDS_OPEN_WITH_MPC))
                || ERROR_SUCCESS != key.SetStringValue(_T("MultiSelectModel"), _T("Player"))
                || ERROR_SUCCESS != key.SetStringValue(_T("Icon"), appIcon)) {
            return false;
        }
        if (ERROR_SUCCESS != key.Create(HKEY_CLASSES_ROOT, strProgID + _T("\\shell\\open\\command"))
                || ERROR_SUCCESS != key.SetStringValue(nullptr, m_strOpenCommand)) {
            return false;
        }

        if (ERROR_SUCCESS != key.Create(HKEY_LOCAL_MACHINE, m_strRegAppFileAssocKey)
                || key.SetStringValue(ext, strProgID)) {
            return false;
        }

        if (bAssociatedWithIcon) {
            if (auto iconLib = GetIconLib()) {
                int iconIndex = iconLib->GetIconIndex(ext);

                /* icon_index value -1 means no icon was found in the iconlib for the file extension */
                if (iconIndex >= 0 && ExtractIcon(AfxGetApp()->m_hInstance, m_iconLibPath, iconIndex)) {
                    appIcon.Format(_T("\"%s\",%d"), m_iconLibPath.GetString(), iconIndex);
                }
            }

            if (ERROR_SUCCESS != key.Create(HKEY_CLASSES_ROOT, strProgID + _T("\\DefaultIcon"))
                    || ERROR_SUCCESS != key.SetStringValue(nullptr, appIcon)) {
                return false;
            }
        } else {
            key.Close();
            key.Attach(HKEY_CLASSES_ROOT);
            key.RecurseDeleteKey(strProgID + _T("\\DefaultIcon"));
        }

        // On Windows 8, an app can't set itself as the default handler for a format
        if (!IsWindows8OrGreater() && bRegister != IsRegistered(ext)) {
            SetFileAssociation(ext, strProgID, bRegister);
        }

        return true;
    }
}

bool CFileAssoc::SetFileAssociation(CString strExt, CString strProgID, bool bRegister)
{
    CString extOldReg;
    CRegKey key;
    HRESULT hr = S_OK;
    TCHAR   buff[MAX_PATH];
    ULONG   len = _countof(buff);
    ZeroMemory(buff, sizeof(buff));

    if (m_pAAR) {
        // The Windows 7 way
        CString strNewApp;
        if (bRegister) {
            // Create non existing file type
            if (ERROR_SUCCESS != key.Create(HKEY_CLASSES_ROOT, strExt)) {
                return false;
            }

            CComHeapPtr<WCHAR> pszCurrentAssociation;
            // Save the application currently associated
            if (SUCCEEDED(m_pAAR->QueryCurrentDefault(strExt, AT_FILEEXTENSION, AL_EFFECTIVE, &pszCurrentAssociation))) {
                if (ERROR_SUCCESS != key.Create(HKEY_CLASSES_ROOT, strProgID)) {
                    return false;
                }

                key.SetStringValue(m_strOldAssocKey, pszCurrentAssociation);
            }
            strNewApp = m_strRegisteredAppName;
        } else {
            if (ERROR_SUCCESS != key.Open(HKEY_CLASSES_ROOT, strProgID)) {
                return false;
            }

            if (ERROR_SUCCESS == key.QueryStringValue(m_strOldAssocKey, buff, &len)) {
                strNewApp = buff;
            }
        }

        hr = m_pAAR->SetAppAsDefault(strNewApp, strExt, AT_FILEEXTENSION);
    }

    return SUCCEEDED(hr);
}

bool CFileAssoc::IsRegistered(CString ext) const
{
    BOOL bIsDefault = FALSE;
    CString strProgID = PROGID + ext;

    if (IsWindows8OrGreater()) {
        bIsDefault = TRUE; // Check only if MPC-HC is registered as able to handle that format, not if it's the default.
    } else if (m_pAAR) {
        m_pAAR->QueryAppIsDefault(ext, AT_FILEEXTENSION, AL_EFFECTIVE, m_strRegisteredAppName, &bIsDefault);
    }

    // Check if association is for this instance of MPC-HC
    if (bIsDefault) {
        CRegKey key;
        TCHAR   buff[MAX_PATH];
        ULONG   len = _countof(buff);

        bIsDefault = FALSE;
        if (ERROR_SUCCESS == key.Open(HKEY_CLASSES_ROOT, strProgID + _T("\\shell\\open\\command"), KEY_READ)) {
            if (ERROR_SUCCESS == key.QueryStringValue(nullptr, buff, &len)) {
                bIsDefault = (m_strOpenCommand.CompareNoCase(CString(buff)) == 0);
            }
        }
    }

    return !!bIsDefault;
}

bool CFileAssoc::HasEnqueueContextMenuEntry(CString strExt) const
{
    CRegKey key;
    CString strProgID = PROGID + strExt;

    return (ERROR_SUCCESS == key.Open(HKEY_CLASSES_ROOT, strProgID + _T("\\shell\\enqueue"), KEY_READ));
}

bool CFileAssoc::Register(const CMediaFormatCategory& mfc, bool bRegister, bool bAddEnqueueContextMenu, bool bAssociatedWithIcon)
{
    if (!mfc.IsAssociable()) {
        ASSERT(FALSE);
        return false;
    }

    CAtlList<CString> exts;
    ExplodeMin(mfc.GetExtsWithPeriod(), exts, ' ');

    CString strLabel = mfc.GetDescription();
    bool res = true;

    POSITION pos = exts.GetHeadPosition();
    while (pos) {
        res &= Register(exts.GetNext(pos), strLabel, bRegister, bAddEnqueueContextMenu, bAssociatedWithIcon);
    }

    return res;
}

CFileAssoc::reg_state_t CFileAssoc::IsRegistered(const CMediaFormatCategory& mfc) const
{
    CAtlList<CString> exts;
    ExplodeMin(mfc.GetExtsWithPeriod(), exts, ' ');

    size_t cnt = 0;

    POSITION pos = exts.GetHeadPosition();
    while (pos) {
        if (CFileAssoc::IsRegistered(exts.GetNext(pos))) {
            cnt++;
        }
    }

    reg_state_t res;
    if (cnt == 0) {
        res = NOT_REGISTERED;
    } else if (cnt == exts.GetCount()) {
        res = ALL_REGISTERED;
    } else {
        res = SOME_REGISTERED;
    }

    return res;
}

CFileAssoc::reg_state_t CFileAssoc::HasAnyEnqueueContextMenuEntries(const CMediaFormatCategory& mfc) const
{
    CAtlList<CString> exts;
    ExplodeMin(mfc.GetExtsWithPeriod(), exts, ' ');

    size_t cnt = 0;

    POSITION pos = exts.GetHeadPosition();
    while (pos) {
        if (CFileAssoc::HasEnqueueContextMenuEntry(exts.GetNext(pos))) {
            cnt++;
        }
    }

    reg_state_t res;
    if (cnt == 0) {
        res = NOT_REGISTERED;
    } else if (cnt == exts.GetCount()) {
        res = ALL_REGISTERED;
    } else {
        res = SOME_REGISTERED;
    }

    return res;
}

bool CFileAssoc::RegisterFolderContextMenuEntries(bool bRegister)
{
    CRegKey key;
    bool success;

    if (bRegister) {
        success = false;

        CString appIcon = _T("\"") + PathUtils::GetProgramPath(true) + _T("\",0");

        if (ERROR_SUCCESS == key.Create(HKEY_CLASSES_ROOT, _T("Directory\\shell\\") PROGID _T(".enqueue"))) {
            key.SetStringValue(nullptr, ResStr(IDS_ADD_TO_PLAYLIST));
            key.SetStringValue(_T("Icon"), appIcon);

            if (ERROR_SUCCESS == key.Create(HKEY_CLASSES_ROOT, _T("Directory\\shell\\") PROGID _T(".enqueue\\command"))) {
                key.SetStringValue(nullptr, m_strEnqueueCommand);
                success = true;
            }
        }

        if (success && ERROR_SUCCESS == key.Create(HKEY_CLASSES_ROOT, _T("Directory\\shell\\") PROGID _T(".play"))) {
            success = false;

            key.SetStringValue(nullptr, ResStr(IDS_OPEN_WITH_MPC));
            key.SetStringValue(_T("Icon"), appIcon);

            if (ERROR_SUCCESS == key.Create(HKEY_CLASSES_ROOT, _T("Directory\\shell\\") PROGID _T(".play\\command"))) {
                key.SetStringValue(nullptr, m_strOpenCommand);
                success = true;
            }
        }

    } else {
        key.Attach(HKEY_CLASSES_ROOT);
        success  = (ERROR_SUCCESS == key.RecurseDeleteKey(_T("Directory\\shell\\") PROGID _T(".enqueue")));
        success &= (ERROR_SUCCESS == key.RecurseDeleteKey(_T("Directory\\shell\\") PROGID _T(".play")));
    }

    return success;
}

bool CFileAssoc::AreRegisteredFolderContextMenuEntries() const
{
    CRegKey key;
    TCHAR   buff[MAX_PATH];
    ULONG   len = _countof(buff);
    bool    registered = false;

    if (ERROR_SUCCESS == key.Open(HKEY_CLASSES_ROOT, _T("Directory\\shell\\") PROGID _T(".play\\command"), KEY_READ)) {
        if (ERROR_SUCCESS == key.QueryStringValue(nullptr, buff, &len)) {
            registered = (m_strOpenCommand.CompareNoCase(CString(buff)) == 0);
        }
    }

    return registered;
}

bool CFileAssoc::RegisterAutoPlay(autoplay_t ap, bool bRegister)
{
    CString exe = PathUtils::GetProgramPath(true);

    size_t i = (size_t)ap;
    if (i >= m_handlers.size()) {
        return false;
    }

    CRegKey key;

    if (bRegister) {
        if (ERROR_SUCCESS != key.Create(HKEY_CLASSES_ROOT, _T("MediaPlayerClassic.Autorun"))) {
            return false;
        }
        key.Close();

        if (ERROR_SUCCESS != key.Create(HKEY_CLASSES_ROOT,
                                        _T("MediaPlayerClassic.Autorun\\Shell\\Play") + m_handlers[i].verb + _T("\\Command"))) {
            return false;
        }
        key.SetStringValue(nullptr, _T("\"") + exe + _T("\"") + m_handlers[i].cmd);
        key.Close();

        if (ERROR_SUCCESS != key.Create(HKEY_LOCAL_MACHINE,
                                        _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\AutoplayHandlers\\Handlers\\MPCPlay") + m_handlers[i].verb + _T("OnArrival"))) {
            return false;
        }
        key.SetStringValue(_T("Action"), ResStr(m_handlers[i].action));
        key.SetStringValue(_T("Provider"), _T("Media Player Classic"));
        key.SetStringValue(_T("InvokeProgID"), _T("MediaPlayerClassic.Autorun"));
        key.SetStringValue(_T("InvokeVerb"), _T("Play") + m_handlers[i].verb);
        key.SetStringValue(_T("DefaultIcon"), exe + _T(",0"));
        key.Close();

        if (ERROR_SUCCESS != key.Create(HKEY_LOCAL_MACHINE,
                                        _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\AutoplayHandlers\\EventHandlers\\Play") + m_handlers[i].verb + _T("OnArrival"))) {
            return false;
        }
        key.SetStringValue(_T("MPCPlay") + m_handlers[i].verb + _T("OnArrival"), _T(""));
        key.Close();
    } else {
        if (ERROR_SUCCESS != key.Create(HKEY_LOCAL_MACHINE,
                                        _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\AutoplayHandlers\\EventHandlers\\Play") + m_handlers[i].verb + _T("OnArrival"))) {
            return false;
        }
        key.DeleteValue(_T("MPCPlay") + m_handlers[i].verb + _T("OnArrival"));
        key.Close();
    }

    return true;
}

bool CFileAssoc::IsAutoPlayRegistered(autoplay_t ap) const
{
    ULONG len;
    TCHAR buff[MAX_PATH];
    CString exe = PathUtils::GetProgramPath(true);

    size_t i = (size_t)ap;
    if (i >= m_handlers.size()) {
        return false;
    }

    CRegKey key;

    if (ERROR_SUCCESS != key.Open(HKEY_LOCAL_MACHINE,
                                  _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\AutoplayHandlers\\EventHandlers\\Play") + m_handlers[i].verb + _T("OnArrival"),
                                  KEY_READ)) {
        return false;
    }
    len = _countof(buff);
    if (ERROR_SUCCESS != key.QueryStringValue(_T("MPCPlay") + m_handlers[i].verb + _T("OnArrival"), buff, &len)) {
        return false;
    }
    key.Close();

    if (ERROR_SUCCESS != key.Open(HKEY_CLASSES_ROOT,
                                  _T("MediaPlayerClassic.Autorun\\Shell\\Play") + m_handlers[i].verb + _T("\\Command"),
                                  KEY_READ)) {
        return false;
    }
    len = _countof(buff);
    if (ERROR_SUCCESS != key.QueryStringValue(nullptr, buff, &len)) {
        return false;
    }
    if (_tcsnicmp(_T("\"") + exe, buff, exe.GetLength() + 1)) {
        return false;
    }
    key.Close();

    return true;
}

bool CFileAssoc::GetAssociatedExtensions(const CMediaFormats& mf, CAtlList<CString>& exts) const
{
    exts.RemoveAll();

    CAtlList<CString> mfcExts;
    for (size_t i = 0, cnt = mf.GetCount(); i < cnt; i++) {
        ExplodeMin(mf[i].GetExtsWithPeriod(), mfcExts, _T(' '));

        POSITION pos = mfcExts.GetHeadPosition();
        while (pos) {
            const CString ext = mfcExts.GetNext(pos);
            if (IsRegistered(ext)) {
                exts.AddTail(ext);
            }
        }
    }

    return !exts.IsEmpty();
}

bool CFileAssoc::GetAssociatedExtensionsFromRegistry(CAtlList<CString>& exts)
{
    exts.RemoveAll();

    CRegKey rkHKCR(HKEY_CLASSES_ROOT);
    LONG ret;
    DWORD i = 0;
    CString keyName, ext;
    DWORD len = MAX_PATH;

    LoadAAR();

    while ((ret = rkHKCR.EnumKey(i, keyName.GetBuffer(len), &len)) != ERROR_NO_MORE_ITEMS) {
        if (ret == ERROR_SUCCESS) {
            keyName.ReleaseBuffer(len);

            if (keyName.Find(PROGID) == 0) {
                ext = keyName.Mid(_countof(PROGID) - 1);

                if (IsRegistered(ext)) {
                    exts.AddTail(ext);
                }
            }

            i++;
            len = MAX_PATH;
        }
    }

    return !exts.IsEmpty();
}

bool CFileAssoc::ReAssocIcons(const CAtlList<CString>& exts)
{
    auto iconLib = GetIconLib();
    if (!iconLib) {
        return false;
    }
    iconLib->SaveVersion();

    const CString progPath = PathUtils::GetProgramPath(true);

    CRegKey key;

    POSITION pos = exts.GetHeadPosition();
    while (pos) {
        const CString ext = exts.GetNext(pos);
        const CString strProgID = PROGID + ext;
        CString appIcon;

        int iconIndex = iconLib->GetIconIndex(ext);

        /* icon_index value -1 means no icon was found in the iconlib for the file extension */
        if (iconIndex >= 0 && ExtractIcon(AfxGetApp()->m_hInstance, m_iconLibPath, iconIndex)) {
            appIcon.Format(_T("\"%s\",%d"), m_iconLibPath.GetString(), iconIndex);
        }

        /* no icon was found for the file extension, so use MPC-HC's icon */
        if (appIcon.IsEmpty()) {
            appIcon = _T("\"") + progPath + _T("\",0");
        }

        if (ERROR_SUCCESS != key.Create(HKEY_CLASSES_ROOT, strProgID + _T("\\DefaultIcon"))
                || ERROR_SUCCESS != key.SetStringValue(nullptr, appIcon)) {
            return false;
        }

        key.Close();
    }

    return true;
}

static HRESULT CALLBACK TaskDialogCallbackProc(HWND hwnd, UINT uNotification, WPARAM wParam, LPARAM lParam, LONG_PTR dwRefData)
{
    if (TDN_CREATED == uNotification) {
        SendMessage(hwnd, TDM_SET_BUTTON_ELEVATION_REQUIRED_STATE, IDYES, TRUE);
    }

    return S_OK;
}

void CFileAssoc::CheckIconsAssocThread()
{
    UINT nLastVersion = AfxGetApp()->GetProfileInt(IDS_R_SETTINGS, IDS_RS_ICON_LIB_VERSION, 0);

    if (auto iconLib = GetIconLib()) {
        UINT nCurrentVersion = iconLib->GetVersion();

        CAtlList<CString> registeredExts;

        if (nCurrentVersion != nLastVersion && GetAssociatedExtensionsFromRegistry(registeredExts)) {
            iconLib->SaveVersion();
            if (!IsUserAnAdmin()) {
                TASKDIALOGCONFIG config;
                ZeroMemory(&config, sizeof(TASKDIALOGCONFIG));
                config.cbSize = sizeof(config);
                config.hInstance = AfxGetInstanceHandle();
                config.hwndParent = AfxGetApp()->GetMainWnd()->GetSafeHwnd();
                config.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;
                config.pszMainIcon = TD_SHIELD_ICON;
                config.pszWindowTitle = MAKEINTRESOURCE(IDS_ICONS_REASSOC_DLG_TITLE);
                config.pszMainInstruction = MAKEINTRESOURCE(IDS_ICONS_REASSOC_DLG_INSTR);
                config.pszContent = MAKEINTRESOURCE(IDS_ICONS_REASSOC_DLG_CONTENT);
                config.pfCallback = TaskDialogCallbackProc;

                typedef HRESULT(_stdcall * pfTaskDialogIndirect)(const TASKDIALOGCONFIG*, int*, int*, BOOL*);

                HMODULE hModule = ::LoadLibrary(_T("comctl32.dll"));
                if (hModule) {
                    pfTaskDialogIndirect TaskDialogIndirect = (pfTaskDialogIndirect)(::GetProcAddress(hModule, "TaskDialogIndirect"));

                    if (TaskDialogIndirect) {
                        int nButtonPressed = 0;
                        TaskDialogIndirect(&config, &nButtonPressed, nullptr, nullptr);

                        if (IDYES == nButtonPressed) {
                            AfxGetMyApp()->RunAsAdministrator(PathUtils::GetProgramPath(true), _T("/iconsassoc"), true);
                        }
                    }

                    ::FreeLibrary(hModule);
                }
            } else {
                ReAssocIcons(registeredExts);

                SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
            }
        }
    }

    m_checkIconsAssocInactiveEvent.Set();
}

void CFileAssoc::CheckIconsAssoc()
{
    std::lock_guard<std::mutex> lock(m_checkIconsAssocMutex);
    HANDLE hEvent = m_checkIconsAssocInactiveEvent;
    DWORD dwEvent;
    VERIFY(CoWaitForMultipleHandles(0, INFINITE, 1, &hEvent, &dwEvent) == S_OK);
    m_checkIconsAssocInactiveEvent.Reset();
    try {
        std::thread([this] { CheckIconsAssocThread(); }).detach();
    } catch (...) {}
}

bool CFileAssoc::ShowWindowsAssocDialog() const
{
    IApplicationAssociationRegistrationUI* pAARUI;
    HRESULT hr = CoCreateInstance(CLSID_ApplicationAssociationRegistrationUI,
                                  nullptr,
                                  CLSCTX_INPROC,
                                  IID_PPV_ARGS(&pAARUI));

    bool success = (SUCCEEDED(hr) && pAARUI != nullptr);

    if (success) {
        pAARUI->LaunchAdvancedAssociationUI(m_strRegisteredAppName);
        pAARUI->Release();
    }

    return success;
}
