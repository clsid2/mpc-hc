/*
 * (C) 2012-2014, 2017 see Authors.txt
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
#include "mpc-hc_config.h"
#include "VersionInfo.h"
#include "UpdateChecker.h"
#include "UpdateCheckerDlg.h"
#include "SettingsDefines.h"
#include "mplayerc.h"
#include "AppSettings.h"

#include <afxinet.h>

const Version UpdateChecker::MPC_HC_VERSION = {
    VersionInfo::GetMajorNumber(),
    VersionInfo::GetMinorNumber(),
    VersionInfo::GetPatchNumber(),
    VersionInfo::GetRevisionNumber()
};
const LPCTSTR UpdateChecker::MPC_HC_UPDATE_URL = UPDATE_URL;

bool UpdateChecker::bIsCheckingForUpdate = false;
CCritSec UpdateChecker::csIsCheckingForUpdate;

UpdateChecker::UpdateChecker(CString versionFileURL)
    : versionFileURL(versionFileURL)
    , latestVersion()
{
}

UpdateChecker::~UpdateChecker()
{
}

Update_Status UpdateChecker::IsUpdateAvailable(const Version& currentVersion)
{
    Update_Status update_status = IsUpdateAvailable(currentVersion, false);
    if (update_status == UPDATER_ERROR) update_status = IsUpdateAvailable(currentVersion, true);
    return update_status;
}

Update_Status UpdateChecker::IsUpdateAvailable(const Version& currentVersion, bool useBackupURL)
{
    Update_Status updateAvailable = UPDATER_LATEST_STABLE;

    try {
        CInternetSession internet;
        internet.SetOption(INTERNET_OPTION_CONNECT_TIMEOUT, 5000);
        internet.SetOption(INTERNET_OPTION_RECEIVE_TIMEOUT, 5000);
        internet.SetOption(INTERNET_OPTION_SEND_TIMEOUT, 5000);

#pragma warning(push)
#pragma warning(disable: 4996)
        OSVERSIONINFOEX osVersion = { sizeof(OSVERSIONINFOEX) };
        GetVersionEx(reinterpret_cast<LPOSVERSIONINFO>(&osVersion));
#pragma warning(pop)
        CString osVersionStr;
        osVersionStr.Format(_T("Windows %1u.%1u"), osVersion.dwMajorVersion, osVersion.dwMinorVersion);

#if !defined(_WIN64)
        // 32-bit programs run on both 32-bit and 64-bit Windows
        // so must sniff
        BOOL f64 = FALSE;
        if (IsWow64Process(GetCurrentProcess(), &f64) && f64)
#endif
        {
            osVersionStr += _T(" x64");
        }

        CString headersFmt = _T("User-Agent: MPC-HC");
        if (VersionInfo::Is64Bit()) {
            headersFmt += _T(" (64-bit)");
        }
#ifdef MPCHC_LITE
        headersFmt += _T(" Lite");
#endif
        headersFmt += _T(" (%s)/");
        headersFmt += VersionInfo::GetFullVersionString();
        headersFmt += _T("\r\n");

        CString headers;
        headers.Format(headersFmt, osVersionStr.GetString());

        CString fileURL;
        if (useBackupURL) fileURL = BACKUP_UPDATE_URL;
        else fileURL = versionFileURL;

        CHttpFile* versionFile = (CHttpFile*) internet.OpenURL(fileURL,
                                                               1,
                                                               INTERNET_FLAG_TRANSFER_ASCII | INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_RELOAD,
                                                               headers,
                                                               DWORD(-1));

        if (versionFile) {
            CString latestVersionStr;
            char buffer[101];
            UINT br = 0;

            while ((br = versionFile->Read(buffer, 50)) > 0) {
                buffer[br] = '\0';
                latestVersionStr += buffer;
            }

            if (!ParseVersion(latestVersionStr, latestVersion)) {
                updateAvailable = UPDATER_ERROR;
            } else {
                time_t lastCheck = time(nullptr);
                AfxGetApp()->WriteProfileBinary(IDS_R_SETTINGS, IDS_RS_UPDATER_LAST_CHECK, (LPBYTE)&lastCheck, sizeof(time_t));

                int comp = CompareVersion(currentVersion, latestVersion);

                if (comp < 0) {
                    CString ignoredVersionStr = AfxGetApp()->GetProfileString(IDS_R_SETTINGS, IDS_RS_UPDATER_IGNORE_VERSION, _T("0.0.0.0"));
                    Version ignoredVersion;
                    bool ignored = false;

                    if (ParseVersion(ignoredVersionStr, ignoredVersion)) {
                        ignored = (CompareVersion(ignoredVersion, latestVersion) >= 0);
                    }

                    updateAvailable = ignored ? UPDATER_UPDATE_AVAILABLE_IGNORED : UPDATER_UPDATE_AVAILABLE;
                } else if (comp > 0) {
                    updateAvailable = UPDATER_NEWER_VERSION;
                }
            }

            versionFile->Close(); // Close() isn't called by the destructor
            delete versionFile;
        } else {
            updateAvailable = UPDATER_ERROR;
        }
    } catch (CInternetException* pEx) {
        updateAvailable = UPDATER_ERROR;
        pEx->Delete();
    }

    return updateAvailable;
}

Update_Status UpdateChecker::IsUpdateAvailable()
{
    return IsUpdateAvailable(MPC_HC_VERSION);
}

void UpdateChecker::IgnoreLatestVersion()
{
    CString ignoredVersionStr;
    ignoredVersionStr.Format(_T("%u.%u.%u.%u"), latestVersion.major, latestVersion.minor, latestVersion.patch, latestVersion.revision);

    AfxGetApp()->WriteProfileString(IDS_R_SETTINGS, IDS_RS_UPDATER_IGNORE_VERSION, ignoredVersionStr);
}

bool UpdateChecker::ParseVersion(const CString& versionStr, Version& version)
{
    bool success = false;

    if (!versionStr.IsEmpty()) {
        UINT v[4];
        int curPos = 0;
        UINT i = 0;
        CString resToken = versionStr.Tokenize(_T("."), curPos);

        success = !resToken.IsEmpty();

        while (!resToken.IsEmpty() && i < _countof(v) && success) {
            if (1 != _stscanf_s(resToken, _T("%u"), v + i)) {
                success = false;
            }

            resToken = versionStr.Tokenize(_T("."), curPos);
            i++;
        }

        success = success && (i == _countof(v));

        if (success) {
            version.major = v[0];
            version.minor = v[1];
            version.patch = v[2];
            version.revision = v[3];
        }
    }

    return success;
}

int UpdateChecker::CompareVersion(const Version& v1, const Version& v2)
{
    if (v1.major > v2.major) {
        return 1;
    } else if (v1.major < v2.major) {
        return -1;
    } else if (v1.minor > v2.minor) {
        return 1;
    } else if (v1.minor < v2.minor) {
        return -1;
    } else if (v1.patch > v2.patch) {
        return 1;
    } else if (v1.patch < v2.patch) {
        return -1;
    } else if (v1.revision > v2.revision) {
        return 1;
    } else if (v1.revision < v2.revision) {
        return -1;
    } else {
        return 0;
    }
}

bool UpdateChecker::IsAutoUpdateEnabled()
{
    int& status = AfxGetAppSettings().nUpdaterAutoCheck;

    if (status == AUTOUPDATE_UNKNOWN) { // First run
        status = (AfxMessageBox(IDS_UPDATE_CONFIG_AUTO_CHECK, MB_ICONQUESTION | MB_YESNO, 0) == IDYES) ? AUTOUPDATE_ENABLE : AUTOUPDATE_DISABLE;
    }

    return (status == AUTOUPDATE_ENABLE);
}

bool UpdateChecker::IsTimeToAutoUpdate()
{
    time_t* lastCheck = nullptr;
    UINT nRead;

    if (!AfxGetApp()->GetProfileBinary(IDS_R_SETTINGS, IDS_RS_UPDATER_LAST_CHECK, (LPBYTE*)&lastCheck, &nRead) || nRead != sizeof(time_t)) {
        if (lastCheck) {
            delete [] lastCheck;
        }

        return true;
    }

    bool isTimeToAutoUpdate = (time(nullptr) >= *lastCheck + AfxGetAppSettings().nUpdaterDelay * 24 * 3600);

    delete [] lastCheck;

    return isTimeToAutoUpdate;
}

static UINT RunCheckForUpdateThread(LPVOID pParam)
{
    bool autoCheck = !!pParam;

    if (!autoCheck || UpdateChecker::IsTimeToAutoUpdate()) {
        UpdateChecker updateChecker(UpdateChecker::MPC_HC_UPDATE_URL);

        Update_Status status = updateChecker.IsUpdateAvailable();

        if (!autoCheck || status == UPDATER_UPDATE_AVAILABLE) {
            UpdateCheckerDlg dlg(status, updateChecker.GetLatestVersion());

            try {
                if (dlg.DoModal() == IDC_UPDATE_IGNORE_BUTTON) {
                    updateChecker.IgnoreLatestVersion();
                }
            } catch (...) {
                AfxGetAppSettings().nUpdaterAutoCheck = AUTOUPDATE_DISABLE;
            }
        }
    }

    UpdateChecker::bIsCheckingForUpdate = false;

    return 0;
}

void UpdateChecker::CheckForUpdate(bool autoCheck /*= false*/)
{
    CAutoLock lock(&csIsCheckingForUpdate);

    if (!bIsCheckingForUpdate) {
        bIsCheckingForUpdate = true;
        AfxBeginThread(RunCheckForUpdateThread, (LPVOID)autoCheck);
    }
}
