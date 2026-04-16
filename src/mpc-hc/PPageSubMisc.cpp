/*
 * (C) 2006-2017 see Authors.txt
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
#include <WinAPIUtils.h>
#include "mplayerc.h"
#include "MainFrm.h"
#include "AuthDlg.h"
#include "PPageSubMisc.h"
#include "SubtitlesProviders.h"

// CPPageSubMisc dialog

IMPLEMENT_DYNAMIC(CPPageSubMisc, CMPCThemePPageBase)

CPPageSubMisc::CPPageSubMisc()
    : CMPCThemePPageBase(CPPageSubMisc::IDD, CPPageSubMisc::IDD)
    , m_pSubtitlesProviders(nullptr)
    , m_fPreferDefaultForcedSubtitles(TRUE)
    , m_fPrioritizeExternalSubtitles(TRUE)
    , m_fDisableInternalSubtitles(FALSE)
    , m_bAutoDownloadSubtitles(FALSE)
    , m_strAutoDownloadSubtitlesExclude()
    , m_bPreferHearingImpairedSubtitles(FALSE)
    , m_strSubtitlesProviders()
    , m_strSubtitlesLanguageOrder()
    , m_strAutoloadPaths()
    , m_bAutoSaveDownloadedSubtitles(FALSE)
{
}

CPPageSubMisc::~CPPageSubMisc()
{
}

void CPPageSubMisc::DoDataExchange(CDataExchange* pDX)
{
    CMPCThemePPageBase::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_CHECK1, m_fPreferDefaultForcedSubtitles);
    DDX_Check(pDX, IDC_CHECK2, m_fPrioritizeExternalSubtitles);
    DDX_Check(pDX, IDC_CHECK3, m_fDisableInternalSubtitles);
    DDX_Check(pDX, IDC_CHECK4, m_bAutoDownloadSubtitles);
    DDX_Check(pDX, IDC_CHECK5, m_bPreferHearingImpairedSubtitles);
    DDX_Text(pDX, IDC_EDIT1, m_strAutoloadPaths);
    DDX_Text(pDX, IDC_EDIT2, m_strAutoDownloadSubtitlesExclude);
    DDX_Text(pDX, IDC_EDIT3, m_strSubtitlesLanguageOrder);
    DDX_Control(pDX, IDC_LIST1, m_list);
    DDX_Check(pDX, IDC_CHECK_AUTOSAVE_ONLINE_SUBTITLE, m_bAutoSaveDownloadedSubtitles);
}

BOOL CPPageSubMisc::OnInitDialog()
{
    __super::OnInitDialog();

    const CAppSettings& s = AfxGetAppSettings();

    m_fPreferDefaultForcedSubtitles = s.bPreferDefaultForcedSubtitles;
    m_fPrioritizeExternalSubtitles = s.fPrioritizeExternalSubtitles;
    m_fDisableInternalSubtitles = s.fDisableInternalSubtitles;
    m_strAutoloadPaths = s.strSubtitlePaths;
    m_bAutoDownloadSubtitles = s.bAutoDownloadSubtitles;
    m_bAutoSaveDownloadedSubtitles = s.bAutoSaveDownloadedSubtitles;
    m_strAutoDownloadSubtitlesExclude = s.strAutoDownloadSubtitlesExclude;
    m_bPreferHearingImpairedSubtitles = s.bPreferHearingImpairedSubtitles;
    m_strSubtitlesLanguageOrder = s.strSubtitlesLanguageOrder;
    m_strSubtitlesProviders = s.strSubtitlesProviders;

    GetDlgItem(IDC_CHECK5)->EnableWindow(m_bAutoDownloadSubtitles);
    GetDlgItem(IDC_STATIC1)->EnableWindow(m_bAutoDownloadSubtitles);
    GetDlgItem(IDC_EDIT2)->EnableWindow(m_bAutoDownloadSubtitles);

    m_list.SetExtendedStyle(m_list.GetExtendedStyle()
                            /*| LVS_EX_DOUBLEBUFFER*/ | LVS_EX_FULLROWSELECT
                            | LVS_EX_CHECKBOXES | LVS_EX_LABELTIP);
    m_list.setAdditionalStyles(LVS_EX_DOUBLEBUFFER);

    // Do not check dynamic_cast, because if it fails we cannot recover from the error anyway.
    const CMainFrame* pMainFrame = AfxGetMainFrame();
    ASSERT(pMainFrame);
    m_pSubtitlesProviders = pMainFrame->m_pSubtitlesProviders.get();
    ASSERT(m_pSubtitlesProviders);

    m_list.SetImageList(&m_pSubtitlesProviders->GetImageList(), LVSIL_SMALL);

    CArray<int> columnWidth;
    if (columnWidth.GetCount() != COL_TOTAL_COLUMNS) {
        // default sizes
        columnWidth.RemoveAll();
        columnWidth.Add(130);
        columnWidth.Add(75);
        columnWidth.Add(300);
    }

    m_list.InsertColumn(COL_PROVIDER, ResStr(IDS_SUBDL_DLG_PROVIDER_COL), LVCFMT_LEFT, columnWidth[COL_PROVIDER]);
    m_list.InsertColumn(COL_USERNAME, ResStr(IDS_SUBUL_DLG_USERNAME_COL), LVCFMT_LEFT, columnWidth[COL_USERNAME]);
    m_list.InsertColumn(COL_LANGUAGES, ResStr(IDS_SUBPP_DLG_LANGUAGES_COL), LVCFMT_LEFT, columnWidth[COL_LANGUAGES]);

    m_list.SetRedraw(FALSE);
    m_list.DeleteAllItems();

    int i = 0;
    for (const auto& iter : m_pSubtitlesProviders->Providers()) {
        int iItem = m_list.InsertItem(i++, CString(iter->DisplayName().c_str()), iter->GetIconIndex());
        m_list.SetItemText(iItem, COL_USERNAME, UTF8To16(iter->UserName().c_str()));
        m_list.SetItemText(iItem, COL_LANGUAGES, ResStr(IDS_SUBPP_DLG_FETCHING_LANGUAGES));
        m_list.SetCheck(iItem, iter->Enabled(SPF_SEARCH));
        m_list.SetItemData(iItem, (DWORD_PTR)(iter.get()));
    }

    m_list.SetRedraw(TRUE);
    m_list.Invalidate();
    m_list.UpdateWindow();

    m_threadFetchSupportedLanguages = std::thread([this]() {
        for (const auto& iter : m_pSubtitlesProviders->Providers()) {
            iter->Languages();
        }
        PostMessage(WM_SUPPORTED_LANGUAGES_READY); // Notify the window that languages have been fetched
    });

    AdjustDynamicWidgets();
    //    EnableToolTips(TRUE);
    CreateToolTip();
    m_wndToolTip.AddTool(GetDlgItem(IDC_EDIT2), ResStr(IDS_SUB_AUTODL_IGNORE_TOOLTIP));
    m_wndToolTip.AddTool(GetDlgItem(IDC_EDIT3), ResStr(IDS_LANG_PREF_EXAMPLE));

    UpdateData(FALSE);

    return TRUE;
}

BOOL CPPageSubMisc::OnApply()
{
    UpdateData();

    CAppSettings& s = AfxGetAppSettings();

    s.bPreferDefaultForcedSubtitles = !!m_fPreferDefaultForcedSubtitles;
    s.fPrioritizeExternalSubtitles = !!m_fPrioritizeExternalSubtitles;
    s.fDisableInternalSubtitles = !!m_fDisableInternalSubtitles;
    s.strSubtitlePaths = m_strAutoloadPaths;
    s.bAutoDownloadSubtitles = !!m_bAutoDownloadSubtitles;
    s.bAutoSaveDownloadedSubtitles = !!m_bAutoSaveDownloadedSubtitles;
    s.strAutoDownloadSubtitlesExclude = m_strAutoDownloadSubtitlesExclude;
    s.bPreferHearingImpairedSubtitles = !!m_bPreferHearingImpairedSubtitles;
    s.strSubtitlesLanguageOrder = m_strSubtitlesLanguageOrder;

    for (int i = 0; i < m_list.GetItemCount(); ++i) {
        SubtitlesProvider* provider = reinterpret_cast<SubtitlesProvider*>(m_list.GetItemData(i));
        provider->Enabled(SPF_SEARCH, m_list.GetCheck(i));
    }

    s.strSubtitlesProviders = m_pSubtitlesProviders->WriteSettings().c_str();

    return __super::OnApply();
}


BEGIN_MESSAGE_MAP(CPPageSubMisc, CMPCThemePPageBase)
    ON_MESSAGE_VOID(WM_SUPPORTED_LANGUAGES_READY, OnSupportedLanguagesReady)
    ON_WM_DESTROY()
    ON_BN_CLICKED(IDC_BUTTON1, OnBnClickedResetSubsPath)
    ON_BN_CLICKED(IDC_CHECK4, OnAutoDownloadSubtitlesClicked)
    ON_NOTIFY(NM_RCLICK, IDC_LIST1, OnRightClick)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST1, OnItemChanged)
END_MESSAGE_MAP()

void CPPageSubMisc::OnSupportedLanguagesReady()
{
    int i = 0;
    for (const auto& iter : m_pSubtitlesProviders->Providers()) {
        CString languages(SubtitlesProvidersUtils::JoinContainer(iter->Languages(), ",").c_str());
        m_list.SetItemText(i++, COL_LANGUAGES, languages.IsEmpty() ? ResStr(IDS_SUBPP_DLG_LANGUAGES_ERROR) : languages);
    }
}

void CPPageSubMisc::OnDestroy()
{
    if (m_threadFetchSupportedLanguages.joinable()) {
        m_threadFetchSupportedLanguages.join();
    }
}

void CPPageSubMisc::OnRightClick(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)pNMHDR;

    if (lpnmlv->iItem >= 0 && lpnmlv->iSubItem >= 0) {
        SubtitlesProvider& provider = *(SubtitlesProvider*)(m_list.GetItemData(lpnmlv->iItem));

        enum {
            SET_CREDENTIALS = 0x1000,
            RESET_CREDENTIALS,
            MOVE_UP,
            MOVE_DOWN,
            OPEN_URL,
            COPY_URL
        };

        CMPCThemeMenu m;
        m.CreatePopupMenu();
        m.AppendMenu(MF_STRING | (provider.Flags(SPF_LOGIN) ? MF_ENABLED : MF_DISABLED), SET_CREDENTIALS, ResStr(IDS_SUBMENU_SETUP));
        m.AppendMenu(MF_STRING | (provider.Flags(SPF_LOGIN) && provider.UserName().length() ? MF_ENABLED : MF_DISABLED), RESET_CREDENTIALS, ResStr(IDS_SUBMENU_RESET));
        m.AppendMenu(MF_SEPARATOR);
        m.AppendMenu(MF_STRING | (lpnmlv->iItem > 0 ? MF_ENABLED : MF_DISABLED), MOVE_UP, ResStr(IDS_SUBMENU_MOVEUP));
        m.AppendMenu(MF_STRING | (lpnmlv->iItem < m_list.GetItemCount() - 1  ? MF_ENABLED : MF_DISABLED), MOVE_DOWN, ResStr(IDS_SUBMENU_MOVEDOWN));
        m.AppendMenu(MF_SEPARATOR);
        m.AppendMenu(MF_STRING | MF_ENABLED, OPEN_URL, ResStr(IDS_SUBMENU_OPENURL));
        m.AppendMenu(MF_STRING | MF_ENABLED, COPY_URL, ResStr(IDS_SUBMENU_COPYURL));

        CPoint pt = lpnmlv->ptAction;
        ::MapWindowPoints(lpnmlv->hdr.hwndFrom, HWND_DESKTOP, &pt, 1);

        if (AppNeedsThemedControls()) {
            m.fulfillThemeReqs();
        }
        switch (m.TrackPopupMenu(TPM_LEFTBUTTON | TPM_RETURNCMD, pt.x, pt.y, this)) {
            case OPEN_URL:
                provider.OpenUrl();
                break;
            case COPY_URL: {
                if (!provider.Url().empty()) {
                    CClipboard clipboard(this);
                    VERIFY(clipboard.SetText(provider.Url().c_str()));
                }
                break;
            }
            case SET_CREDENTIALS: {
                CString szUser(UTF8To16(provider.UserName().c_str()));
                CString szPass(UTF8To16(provider.Password().c_str()));
                CString szDomain(provider.Name().c_str());
                if (ERROR_SUCCESS == PromptForCredentials(GetSafeHwnd(),
                                                          ResStr(IDS_SUB_CREDENTIALS_TITLE), ResStr(IDS_SUB_CREDENTIALS_MSG) + CString(provider.Url().c_str()),
                                                          szDomain, szUser, szPass, /*&bSave*/nullptr)) {
                    provider.LogOut();
                    provider.UserName(static_cast<const char*>(UTF16To8(szUser)));
                    provider.Password(static_cast<const char*>(UTF16To8(szPass)));
                    if (provider.LoginInternal()) {
                        m_list.SetItemText(lpnmlv->iItem, 1, szUser);
                    } else {
                        // login failed
                        provider.UserName("");
                        provider.Password("");
                        m_list.SetItemText(lpnmlv->iItem, 1, _T(""));
                    }
                    SetModified();
                }
                break;
            }
            case RESET_CREDENTIALS:
                provider.LogOut();
                provider.UserName("");
                provider.Password("");
                m_list.SetItemText(lpnmlv->iItem, 1, _T(""));
                SetModified();
                break;
            case MOVE_UP:
                m_pSubtitlesProviders->MoveUp(lpnmlv->iItem);
                ListView_SortItemsEx(m_list.GetSafeHwnd(), SortCompare, m_list.GetSafeHwnd());
                SetModified();
                break;
            case MOVE_DOWN:
                m_pSubtitlesProviders->MoveDown(lpnmlv->iItem);
                ListView_SortItemsEx(m_list.GetSafeHwnd(), SortCompare, m_list.GetSafeHwnd());
                SetModified();
                break;
            default:
                break;
        }
    }
}

void CPPageSubMisc::OnAutoDownloadSubtitlesClicked()
{
    m_bAutoDownloadSubtitles = IsDlgButtonChecked(IDC_CHECK4);
    GetDlgItem(IDC_CHECK5)->EnableWindow(m_bAutoDownloadSubtitles);
    GetDlgItem(IDC_STATIC1)->EnableWindow(m_bAutoDownloadSubtitles);
    GetDlgItem(IDC_EDIT2)->EnableWindow(m_bAutoDownloadSubtitles);
    UpdateWindow();

    SetModified();
}

void CPPageSubMisc::OnBnClickedResetSubsPath()
{
    m_strAutoloadPaths = DEFAULT_SUBTITLE_PATHS;

    UpdateData(FALSE);
    SetModified();
}

void CPPageSubMisc::OnItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);

    int provider_count = (int) m_pSubtitlesProviders->Providers().size();
    for (int i = 0; i < provider_count; i++) {
        if (pNMLV->iItem == i && pNMLV->uNewState == 8192) {
            auto& subprovider = *m_pSubtitlesProviders->Providers()[i].get();
            std::string provname = subprovider.Name();
            if (provname.compare("OpenSubtitles") == 0 || provname.compare("OpenSubtitles2") == 0) {
                if (subprovider.Enabled(SPF_SEARCH) == 0 && subprovider.UserName().size() == 0) {
                    bool allow_anon = false;
                    CString msg;
                    if (provname.compare("OpenSubtitles") == 0) {
                        msg = L"You must enter your OpenSubtitles login information to continue.\r\n\r\n" \
                            "If you do not yet have an OpenSubtitles account, you can create a free account on http://www.opensubtitles.org\r\n\r\n" \
                            "Click OK if you have an account and want to fill in your login details. Click CANCEL to disable this subtitle search provider.";
                    } else {
                        msg = L"You should enter your OpenSubtitles login information to continue.\r\n\r\n" \
                            "If you do not yet have an OpenSubtitles account, you can create a free account on http://www.opensubtitles.com\r\n\r\n" \
                            "Click OK if you have an account and want to fill in your login details.\r\n" \
                            "Click CANCEL to use this subtitle search provider without login.\r\n\r\n" \
                            "Important warning:\r\n" \
                            "There is a maximum amount of daily downloads for all global anonymous users combined.\r\n" \
                            "This quota means that downloads are very likely to fail if you do not use a login.\r\n" \
                            "A login is also required if you wish to use this provider for automatic downloads.";
                        allow_anon = true;
                    }
                    if (AfxMessageBox(msg, MB_OKCANCEL | MB_ICONINFORMATION) == IDCANCEL) {
                        if (!allow_anon) {
                            ListView_SetCheckState(pNMHDR->hwndFrom, i, FALSE);
                            return;
                        }
                        break;
                    }

                    CString szUser(UTF8To16(subprovider.UserName().c_str()));
                    CString szPass(UTF8To16(subprovider.Password().c_str()));
                    CString szDomain(UTF8To16(provname.c_str()));
                    if (ERROR_SUCCESS == PromptForCredentials(GetSafeHwnd(),
                        ResStr(IDS_SUB_CREDENTIALS_TITLE), ResStr(IDS_SUB_CREDENTIALS_MSG) +
                        CString(subprovider.Url().c_str()), szDomain, szUser, szPass, nullptr)) {
                        subprovider.LogOut();
                        subprovider.UserName(static_cast<const char*>(UTF16To8(szUser)));
                        subprovider.Password(static_cast<const char*>(UTF16To8(szPass)));
                        if (subprovider.LoginInternal()) {
                            m_list.SetItemText(pNMLV->iItem, 1, szUser);
                        } else {
                            // login failed
                            subprovider.UserName("");
                            subprovider.Password("");
                            m_list.SetItemText(pNMLV->iItem, 1, _T(""));
                            ListView_SetCheckState(pNMHDR->hwndFrom, i, FALSE);
                        }
                    } else if (!allow_anon) {
                        ListView_SetCheckState(pNMHDR->hwndFrom, i, FALSE);
                        return;
                    }
                }
                break;
            }
        }
    }

    if (pNMLV->uOldState + pNMLV->uNewState == 0x3000) {
        SetModified();
    }
}

int CALLBACK CPPageSubMisc::SortCompare(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
    CListCtrl& list = *(CListCtrl*)CListCtrl::FromHandle((HWND)lParamSort);
    size_t left = ((SubtitlesProvider*)list.GetItemData((int)lParam1))->Index();
    size_t right = ((SubtitlesProvider*)list.GetItemData((int)lParam2))->Index();
    return int(left - right);
}

void CPPageSubMisc::AdjustDynamicWidgets() {
    AdjustDynamicWidgetPair(this, IDC_STATIC1, IDC_EDIT2);
    AdjustDynamicWidgetPair(this, IDC_STATIC2, IDC_EDIT3);
}
