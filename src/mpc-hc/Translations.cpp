/*
* (C) 2014-2016 see Authors.txt
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
#include "Translations.h"
#include "FileVersionInfo.h"
#include "PathUtils.h"
#include "VersionInfo.h"
#include "PPageSheet.h"
#include "CMPCThemePropertySheet.h"
#include "CMPCThemeResizablePropertySheet.h"
#include "ComPropertySheet.h"

namespace
{
    static_assert(std::is_standard_layout<Translations::LanguageResource>::value, "POD type is expected.");

    constexpr Translations::LanguageResource languageResources[] = {
        { 1025,   _T("Arabic"),                   _T("Lang\\mpcresources.ar.dll")    },
        { 1067,   _T("Armenian"),                 _T("Lang\\mpcresources.hy.dll")    },
        { 1069,   _T("Basque"),                   _T("Lang\\mpcresources.eu.dll")    },
        { 1059,   _T("Belarusian"),               _T("Lang\\mpcresources.be.dll")    },
        { 1093,   _T("Bengali"),                  _T("Lang\\mpcresources.bn.dll")    },
        { 5146,   _T("Bosnian"),                  _T("Lang\\mpcresources.bs_BA.dll") },
        { 1026,   _T("Bulgarian"),                _T("Lang\\mpcresources.bg.dll")    },
        { 1027,   _T("Catalan"),                  _T("Lang\\mpcresources.ca.dll")    },
        { 2052,   _T("Chinese (Simplified)"),     _T("Lang\\mpcresources.zh_CN.dll") },
        { 1028,   _T("Chinese (Traditional)"),    _T("Lang\\mpcresources.zh_TW.dll") },
        { 1050,   _T("Croatian"),                 _T("Lang\\mpcresources.hr.dll")    },
        { 1029,   _T("Czech"),                    _T("Lang\\mpcresources.cs.dll")    },
        { 1030,   _T("Danish"),                   _T("Lang\\mpcresources.da.dll")    },
        { 1043,   _T("Dutch"),                    _T("Lang\\mpcresources.nl.dll")    },
        { 0,      _T("English"),                  nullptr                            },
        { 2057,   _T("English (British)"),        _T("Lang\\mpcresources.en_GB.dll") },
        { 1035,   _T("Finnish"),                  _T("Lang\\mpcresources.fi.dll")    },
        { 1036,   _T("French"),                   _T("Lang\\mpcresources.fr.dll")    },
        { 1110,   _T("Galician"),                 _T("Lang\\mpcresources.gl.dll")    },
        { 1031,   _T("German"),                   _T("Lang\\mpcresources.de.dll")    },
        { 1032,   _T("Greek"),                    _T("Lang\\mpcresources.el.dll")    },
        { 1037,   _T("Hebrew"),                   _T("Lang\\mpcresources.he.dll")    },
        { 1038,   _T("Hungarian"),                _T("Lang\\mpcresources.hu.dll")    },
        { 1057,   _T("Indonesian"),               _T("Lang\\mpcresources.id.dll")    },
        { 1040,   _T("Italian"),                  _T("Lang\\mpcresources.it.dll")    },
        { 1041,   _T("Japanese"),                 _T("Lang\\mpcresources.ja.dll")    },
        { 1042,   _T("Korean"),                   _T("Lang\\mpcresources.ko.dll")    },
        { 1063,   _T("Lithuanian"),               _T("Lang\\mpcresources.lt.dll")    },
        { 1086,   _T("Malay"),                    _T("Lang\\mpcresources.ms_MY.dll") },
        { 1045,   _T("Polish"),                   _T("Lang\\mpcresources.pl.dll")    },
        { 1046,   _T("Portuguese (Brazil)"),      _T("Lang\\mpcresources.pt_BR.dll") },
        { 2070,   _T("Portuguese (Portugal)"),    _T("Lang\\mpcresources.pt_PT.dll") },
        { 1094,   _T("Punjabi"),                  _T("Lang\\mpcresources.pa.dll")    },
        { 1048,   _T("Romanian"),                 _T("Lang\\mpcresources.ro.dll")    },
        { 1049,   _T("Russian"),                  _T("Lang\\mpcresources.ru.dll")    },
        { 3098,   _T("Serbian"),                  _T("Lang\\mpcresources.sr.dll")    },
        { 1051,   _T("Slovak"),                   _T("Lang\\mpcresources.sk.dll")    },
        { 1060,   _T("Slovenian"),                _T("Lang\\mpcresources.sl.dll")    },
        { 1053,   _T("Swedish"),                  _T("Lang\\mpcresources.sv.dll")    },
        { 3082,   _T("Spanish"),                  _T("Lang\\mpcresources.es.dll")    },
        { 1092,   _T("Tatar"),                    _T("Lang\\mpcresources.tt.dll")    },
        { 1054,   _T("Thai"),                     _T("Lang\\mpcresources.th_TH.dll") },
        { 1055,   _T("Turkish"),                  _T("Lang\\mpcresources.tr.dll")    },
        { 1058,   _T("Ukrainian"),                _T("Lang\\mpcresources.uk.dll")    },
        { 1066,   _T("Vietnamese"),               _T("Lang\\mpcresources.vi.dll")    },
    };

    HHOOK g_hRTLHook = nullptr;

    LRESULT CALLBACK RTLWindowsLayoutCbtFilterHook(int code, WPARAM wParam, LPARAM lParam)
    {
        if (code == HCBT_CREATEWND) {
            HWND hWnd = (HWND)wParam;
            if ((GetWindowLongPtr(hWnd, GWL_STYLE) & WS_CHILD) == 0) {
                CWnd* wnd = CWnd::FromHandle(hWnd);
                if (nullptr == DYNAMIC_DOWNCAST(CPPageSheet, wnd)
                    && nullptr == DYNAMIC_DOWNCAST(CMPCThemePropertySheet, wnd)
                    && nullptr == DYNAMIC_DOWNCAST(CMPCThemeResizablePropertySheet, wnd)
                    && nullptr == DYNAMIC_DOWNCAST(CComPropertySheet, wnd)) { //see CPPageSheet::DoModal().  in Windows 11, SetWindowLongPtr corrupts CPPageSheet dialog
                    SetWindowLongPtr(hWnd, GWL_EXSTYLE, GetWindowLongPtr(hWnd, GWL_EXSTYLE) | WS_EX_LAYOUTRTL);
                }
            }
        }
        return CallNextHookEx(g_hRTLHook, code, wParam, lParam);
    }
}

Translations::LanguageResource Translations::GetLanguageResourceByLocaleID(LANGID localeID)
{
    for (const auto& lr : languageResources) {
        if (localeID == lr.localeID) {
            return lr;
        }
    }

    return { 0, _T("English"), nullptr };
}

std::list<Translations::LanguageResource> Translations::GetAvailableLanguageResources()
{
    std::list<LanguageResource> availableResources;

    const CString appPath = PathUtils::GetProgramPath();

    for (const LanguageResource& lr : languageResources) {
        if (0 == lr.localeID || PathUtils::Exists(PathUtils::CombinePaths(appPath, lr.dllPath))) {
            availableResources.emplace_back(lr);
        }
    }

    return availableResources;
}

LANGID Translations::SetDefaultLanguage()
{
    LANGID localeID = GetUserDefaultUILanguage();
    // Try to set the language resource but don't fail if it can't be loaded
    // English will we used instead in case of error
    return SetLanguage(localeID, false) ? localeID : 0;
}

bool Translations::IsLangRTL(LANGID localeID) {
    //arabic or hebrew
    if (localeID == 1025 || localeID == 1037) {
        return true;
    }
    return false;
}

bool Translations::SetLanguage(LANGID localeID, bool showErrorMsg /*= true*/)
{
    // Note that all messages should stay in English in this method!

    LanguageResource languageResource = GetLanguageResourceByLocaleID(localeID);

    // Check if we fell back to English locale
    bool success = (languageResource.localeID == localeID);
    ASSERT(success || languageResource.localeID == 0);

    HMODULE hMod = nullptr;

    // Try to load the resource dll if any
    if (languageResource.dllPath) {
        ASSERT(success);
        hMod = LoadLibrary(languageResource.dllPath);
        if (hMod == nullptr) { // The dll failed to load for some reason
            success = false;
            if (showErrorMsg) {
                MessageBox(nullptr, _T("Error loading the chosen language.\n\nPlease reinstall MPC-HC."),
                           _T("MPC-HC"), MB_ICONWARNING | MB_OK);
            }
        } else { // Check if the version of the resource dll is correct
            CString strSatVersion = FileVersionInfo::GetFileVersionStr(languageResource.dllPath);
            CString strNeededVersion;
            strNeededVersion.Format(_T("%u.%u.%u.0"), VersionInfo::GetMajorNumber(), VersionInfo::GetMinorNumber(), VersionInfo::GetPatchNumber());

            if (strSatVersion != strNeededVersion) {
                // The version wasn't correct
                success = false;
                if (showErrorMsg) {
                    int sel = MessageBox(nullptr, _T("Your language pack will not work with this version.\n\nDo you want to visit the download page to get a full package including the translations?"),
                                         _T("MPC-HC"), MB_ICONWARNING | MB_YESNO);
                    if (sel == IDYES) {
                        ShellExecute(nullptr, _T("open"), DOWNLOAD_URL, nullptr, nullptr, SW_SHOWDEFAULT);
                    }
                }
                // Free the loaded resource dll
                FreeLibrary(hMod);
                hMod = nullptr;
            }
        }
    }

    // In case a dll was loaded, check if some special action is needed
    if (hMod != nullptr && IsLangRTL(languageResource.localeID)) {
        SetProcessDefaultLayout(LAYOUT_RTL);
        if (!g_hRTLHook) {
            g_hRTLHook = SetWindowsHookEx(WH_CBT, RTLWindowsLayoutCbtFilterHook, nullptr, GetCurrentThreadId());
        }
    } else {
        // Undo a previous RTL language, new windows are laid out left to right again
        SetProcessDefaultLayout(0);
        if (g_hRTLHook) {
            UnhookWindowsHookEx(g_hRTLHook);
            g_hRTLHook = nullptr;
        }
    }

    // In case no dll was loaded, load the English translation from the executable
    if (hMod == nullptr) {
        hMod = AfxGetApp()->m_hInstance;
    }

    // Free the old resource if it was a dll
    if (AfxGetResourceHandle() != AfxGetApp()->m_hInstance) {
        FreeLibrary(AfxGetResourceHandle());
    }
    // Set the new resource
    AfxSetResourceHandle(hMod);

    return success;
}
