/*
 * (C) 2012-2017 see Authors.txt
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
#include <WinAPIFunc.h>
#include <WinAPIUtils.h>
#include "AboutDlg.h"
#include "mpc-hc_config.h"
#ifndef MPCHC_LITE
#include "FGFilterLAV.h"
#endif
#include "mplayerc.h"
#include "FileVersionInfo.h"
#include "PathUtils.h"
#include "VersionInfo.h"
#include <VersionHelpers.h>
#include "Monitors.h"


/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

CAboutDlg::CAboutDlg() : CMPCThemeDialog(CAboutDlg::IDD, NULL)
{
    //{{AFX_DATA_INIT(CAboutDlg)
    //}}AFX_DATA_INIT
}

CAboutDlg::~CAboutDlg()
{
}

BOOL CAboutDlg::OnInitDialog()
{
    // Get the default text before it is overwritten by the call to __super::OnInitDialog()
#ifndef MPCHC_LITE
    GetDlgItem(IDC_LAVFILTERS_VERSION)->GetWindowText(m_LAVFiltersVersion);
#endif

    __super::OnInitDialog();

    // Because we set LR_SHARED, there is no need to explicitly destroy the icon
    m_icon.SetIcon((HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDR_MAINFRAME), IMAGE_ICON, 48, 48, LR_SHARED));

    m_appname = _T("MPC-HC");
    if (VersionInfo::Is64Bit()) {
        m_appname += _T(" (64-bit)");
    }
#ifdef MPCHC_LITE
    m_appname += _T(" (Lite)");
#endif
#ifdef _DEBUG
    m_appname += _T(" (Debug)");
#endif

    m_homepage.Format(_T("<a>%s</a>"), WEBSITE_URL);

    CString copyright;
    copyright.Format(ResStr(IDS_ABOUT_COPYRIGHT), ResStr(IDS_ABOUT_COPYRIGHT_YEAR).GetString());
    SetDlgItemText(IDC_AUTHORS_LINK, copyright);

    m_strBuildNumber = VersionInfo::GetFullVersionString();

    m_LAVFilters.Format(IDS_STRING_COLON, _T("LAV Filters"));
#ifndef MPCHC_LITE
    CString LAVFiltersVersion = CFGFilterLAV::GetVersion();
    if (!LAVFiltersVersion.IsEmpty()) {
        m_LAVFiltersVersion = LAVFiltersVersion;
    }
#endif

    m_buildDate = VersionInfo::GetBuildDateString();

#pragma warning(push)
#pragma warning(disable: 4996)
    OSVERSIONINFOEX osVersion = { sizeof(OSVERSIONINFOEX) };
    GetVersionEx(reinterpret_cast<LPOSVERSIONINFO>(&osVersion));
#pragma warning(pop)

    if (osVersion.dwMajorVersion == 10 && osVersion.dwMinorVersion == 0) {
        if (IsWindowsServer()) {
            if (osVersion.dwBuildNumber > 26100) {
                m_OSName = _T("Windows Server");
            } else if (osVersion.dwBuildNumber == 26100) {
                m_OSName = _T("Windows Server 2025");
            } else if (osVersion.dwBuildNumber >= 20348) {
                m_OSName = _T("Windows Server 2022");
            } else if (osVersion.dwBuildNumber >= 19042) {
                m_OSName = _T("Windows Server, version 20H2");
            } else if (osVersion.dwBuildNumber >= 19041) {
                m_OSName = _T("Windows Server, version 2004");
            } else if (osVersion.dwBuildNumber >= 18363) {
                m_OSName = _T("Windows Server, version 1909");
            } else if (osVersion.dwBuildNumber >= 18362) {
                m_OSName = _T("Windows Server, version 1903");
            } else if (osVersion.dwBuildNumber >= 17763) {
                m_OSName = _T("Windows Server 2019");
            } else {
                m_OSName = _T("Windows Server 2016");
            }
        } else {
            if (osVersion.dwBuildNumber > 28000) {
                m_OSName = _T("Windows 11");
            } else if (osVersion.dwBuildNumber == 28000) {
                m_OSName = _T("Windows 11 (Build 26H1)");
            } else if (osVersion.dwBuildNumber == 26200) {
                m_OSName = _T("Windows 11 (Build 25H2)");
            } else if (osVersion.dwBuildNumber == 26100) {
                m_OSName = _T("Windows 11 (Build 24H2)");
            } else if (osVersion.dwBuildNumber == 22631) {
                m_OSName = _T("Windows 11 (Build 23H2)");
            } else if (osVersion.dwBuildNumber == 22621) {
                m_OSName = _T("Windows 11 (Build 22H2)");
            } else if (osVersion.dwBuildNumber == 22000) {
                m_OSName = _T("Windows 11 (Build 21H2)");
            } else if (osVersion.dwBuildNumber >= 20000) {
                m_OSName = _T("Windows 11");
            } else if (osVersion.dwBuildNumber >= 19046) {
                m_OSName = _T("Windows 10");
            } else if (osVersion.dwBuildNumber == 19045) {
                m_OSName = _T("Windows 10 (Build 22H2)");
            } else if (osVersion.dwBuildNumber == 19044) {
                m_OSName = _T("Windows 10 (Build 21H2)");
            } else if (osVersion.dwBuildNumber == 19043) {
                m_OSName = _T("Windows 10 (Build 21H1)");
            } else if (osVersion.dwBuildNumber == 19042) {
                m_OSName = _T("Windows 10 (Build 20H2)");
            } else if (osVersion.dwBuildNumber == 19041) {
                m_OSName = _T("Windows 10 (Build 2004)");
            } else if (osVersion.dwBuildNumber >= 18363) {
                m_OSName = _T("Windows 10 (Build 1909)");
            } else if (osVersion.dwBuildNumber == 18362) {
                m_OSName = _T("Windows 10 (Build 1903)");
            } else if (osVersion.dwBuildNumber >= 17763) {
                m_OSName = _T("Windows 10 (Build 1809)");
            } else if (osVersion.dwBuildNumber >= 17134) {
                m_OSName = _T("Windows 10 (Build 1803)");
            } else if (osVersion.dwBuildNumber >= 16299) {
                m_OSName = _T("Windows 10 (Build 1709)");
            } else if (osVersion.dwBuildNumber >= 15063) {
                m_OSName = _T("Windows 10 (Build 1703)");
            } else if (osVersion.dwBuildNumber >= 14393) {
                m_OSName = _T("Windows 10 (Build 1607)");
            } else if (osVersion.dwBuildNumber >= 10586) {
                m_OSName = _T("Windows 10 (Build 1511)");
            } else if (osVersion.dwBuildNumber >= 10240) {
                m_OSName = _T("Windows 10 (Build 1507)");
            } else {
                m_OSName = _T("Windows 10");
            }
        }
    } else if (osVersion.dwMajorVersion == 6 && osVersion.dwMinorVersion == 3) {
        if (IsWindowsServer()) {
            m_OSName = _T("Windows Server 2012 R2");
        } else {
            m_OSName = _T("Windows 8.1");
        }
    } else if (osVersion.dwMajorVersion == 6 && osVersion.dwMinorVersion == 2) {
        if (IsWindowsServer()) {
            m_OSName = _T("Windows Server 2012");
        } else {
            m_OSName = _T("Windows 8");
        }
    } else if (osVersion.dwMajorVersion == 6 && osVersion.dwMinorVersion == 1) {
        if (IsWindowsServer()) {
            m_OSName = _T("Windows Server 2008 R2");
        } else {
            m_OSName = _T("Windows 7");
        }
    } else if (osVersion.dwMajorVersion == 6 && osVersion.dwMinorVersion == 0) {
        if (IsWindowsServer()) {
            m_OSName = _T("Windows Server 2008");
        } else {
            m_OSName = _T("Windows Vista");
        }
    } else {
        if (IsWindowsServer()) {
            m_OSName = _T("Windows Server");
        } else {
            m_OSName = _T("Windows NT");
        }
    }
    if (osVersion.dwMajorVersion == 6 && osVersion.dwMinorVersion < 2 && osVersion.szCSDVersion[0]) {
        m_OSName.AppendFormat(_T(" (%s)"), osVersion.szCSDVersion);
    }
    m_OSVersion.Format(_T("%1u.%1u.%u"), osVersion.dwMajorVersion, osVersion.dwMinorVersion, osVersion.dwBuildNumber);

#if !defined(_WIN64)
    // 32-bit programs run on both 32-bit and 64-bit Windows
    // so must sniff
    BOOL f64 = FALSE;
    if (IsWow64Process(GetCurrentProcess(), &f64) && f64)
#endif
    {
        m_OSVersion += _T(" (64-bit)");
    }

    UpdateData(FALSE);

    GetDlgItem(IDOK)->SetFocus();
    fulfillThemeReqs();

    return FALSE;
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CAboutDlg)
    //}}AFX_DATA_MAP
    DDX_Control(pDX, IDR_MAINFRAME, m_icon);
    DDX_Text(pDX, IDC_STATIC1, m_appname);
    DDX_Text(pDX, IDC_HOMEPAGE_LINK, m_homepage);
    DDX_Text(pDX, IDC_VERSION, m_strBuildNumber);
    DDX_Text(pDX, IDC_STATIC5, m_LAVFilters);
#ifndef MPCHC_LITE
    DDX_Text(pDX, IDC_LAVFILTERS_VERSION, m_LAVFiltersVersion);
#endif
    DDX_Text(pDX, IDC_STATIC2, m_buildDate);
    DDX_Text(pDX, IDC_STATIC3, m_OSName);
    DDX_Text(pDX, IDC_STATIC4, m_OSVersion);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CMPCThemeDialog)
    //{{AFX_MSG_MAP(CAboutDlg)
    // No message handlers
    //}}AFX_MSG_MAP
    ON_NOTIFY(NM_CLICK, IDC_HOMEPAGE_LINK, OnHomepage)
    ON_BN_CLICKED(IDC_BUTTON1, OnCopyToClipboard)
END_MESSAGE_MAP()

void CAboutDlg::OnHomepage(NMHDR* pNMHDR, LRESULT* pResult)
{
    ShellExecute(m_hWnd, _T("open"), WEBSITE_URL, nullptr, nullptr, SW_SHOWDEFAULT);
    *pResult = 0;
}

void CAboutDlg::OnCopyToClipboard()
{
    CStringW info = m_appname + _T("\r\n");
    info += CString(_T('-'), m_appname.GetLength()) + _T("\r\n\r\n");
    info += _T("Build information:\r\n");
    info += _T("    Version:            ") + m_strBuildNumber + _T("\r\n");
    info += _T("    Build date:         ") + m_buildDate + _T("\r\n\r\n");
#ifndef MPCHC_LITE
    info += _T("LAV Filters:\r\n");
    info += _T("    LAV Splitter:       ") + CFGFilterLAV::GetVersion(CFGFilterLAV::SPLITTER) + _T("\r\n");
    info += _T("    LAV Video:          ") + CFGFilterLAV::GetVersion(CFGFilterLAV::VIDEO_DECODER) + _T("\r\n");
    info += _T("    LAV Audio:          ") + CFGFilterLAV::GetVersion(CFGFilterLAV::AUDIO_DECODER) + _T("\r\n");
    info += _T("    FFmpeg compiler:    ") + VersionInfo::GetGCCVersion() + _T("\r\n\r\n");
#endif
    info += _T("Operating system:\r\n");
    info += _T("    Name:               ") + m_OSName + _T("\r\n");
    info += _T("    Version:            ") + m_OSVersion + _T("\r\n\r\n");

    info += _T("Hardware:\r\n");

    CRegKey key;
    if (key.Open(HKEY_LOCAL_MACHINE, _T("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"), KEY_READ) == ERROR_SUCCESS) {
        ULONG nChars = 0;
        if (key.QueryStringValue(_T("ProcessorNameString"), nullptr, &nChars) == ERROR_SUCCESS) {
            CString cpuName;
            if (key.QueryStringValue(_T("ProcessorNameString"), cpuName.GetBuffer(nChars), &nChars) == ERROR_SUCCESS) {
                cpuName.ReleaseBuffer(nChars);
                cpuName.Trim();
                info.AppendFormat(_T("    CPU:                %s\r\n"), cpuName.GetString());
            }
        }
    }

    IDirect3D9* pD3D9 = Direct3DCreate9(D3D_SDK_VERSION);
    if (pD3D9) {
        for (UINT adapter = 0, adapterCount = pD3D9->GetAdapterCount(); adapter < adapterCount; adapter++) {
            D3DADAPTER_IDENTIFIER9 adapterIdentifier;
            if (pD3D9->GetAdapterIdentifier(adapter, 0, &adapterIdentifier) == D3D_OK) {
                CString deviceName = adapterIdentifier.Description;
                deviceName.Trim();

                if (adapterCount > 1) {
                    info.AppendFormat(_T("    GPU%u:               %s"), adapter + 1, deviceName.GetString());
                } else {
                    info.AppendFormat(_T("    GPU:                %s"), deviceName.GetString());
                }
                if (adapterIdentifier.DriverVersion.QuadPart) {
                    info.AppendFormat(_T(" (driver version: %s)"),
                                      FileVersionInfo::FormatVersionString(adapterIdentifier.DriverVersion.LowPart, adapterIdentifier.DriverVersion.HighPart).GetString());
                }
                info += _T("\r\n");
            }
        }
        pD3D9->Release();
    }

    auto &s = AfxGetAppSettings();
    CMonitors monitors;

    CStringW currentMonitorName;
    monitors.GetNearestMonitor(AfxGetMainWnd()).GetName(currentMonitorName);

    CMonitor fullscreenMonitor = monitors.GetMonitor(s.strFullScreenMonitorID, s.strFullScreenMonitorDeviceName);

    for (int i = 0; i < monitors.GetCount(); i++) {
        CMonitor monitor = monitors.GetMonitor(i);

        if (monitor.IsMonitor()) {
            CStringW displayName, deviceName;
            monitor.GetNames(displayName, deviceName);
            CStringW str = displayName;

            if (!deviceName.IsEmpty()) {
                str.Append(L" - " + deviceName);
            }

            int bpp = monitor.GetBitsPerPixel();
            CRect mr;
            monitor.GetMonitorRect(mr);
            int dpi = DpiHelper::GetDPIForMonitor(monitor);
            CString dpis;
            if (dpi > 0) {
                dpis.Format(L" %i DPI", dpi);
            }
            str.AppendFormat(L" [%ix%i %i-bit%s]", mr.Width(), mr.Height(), bpp, dpis.GetString());
            if (displayName == currentMonitorName) {
                str.AppendFormat(L" - [%s]", ResStr(IDS_FULLSCREENMONITOR_CURRENT).GetString());
            }
            info.AppendFormat(L"    Monitor:            %s\r\n", str.GetString());
        }
    }
    info += _T("\r\nText:\r\n");
    double textScaleFactor = DpiHelper::GetTextScaleFactor();
    info.AppendFormat(L"    Scale Factor:       %f\r\n", textScaleFactor);
    int cp = GetACP();
    info.AppendFormat(L"    Ansi Codepage:      %i\r\n", cp);

    CClipboard clipboard(this);
    VERIFY(clipboard.SetText(info));
}
