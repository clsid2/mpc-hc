/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2014, 2016-2017 see Authors.txt
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
#include "mplayerc.h"
#include "PPageSheet.h"
#include "SettingsDefines.h"
#include "CMPCTheme.h"
#include "CMPCThemeUtil.h"
#include <prsht.h>
#include "Monitors.h"

// CPPageSheet

IMPLEMENT_DYNAMIC(CPPageSheet, CTreePropSheet)

CPPageSheet::CPPageSheet(LPCTSTR pszCaption, IFilterGraph* pFG, CWnd* pParentWnd, UINT idPage)
    : CTreePropSheet(pszCaption, pParentWnd, 0)
    , m_bLockPage(false)
    , m_bLanguageChanged(false)
    , m_audioswitcher(pFG)
    , initialized(false)
    , isDummySheet(false)
    , changingDPI(false)
{
    EventRouter::EventSelection receives;
    receives.insert(MpcEvent::CHANGING_UI_LANGUAGE);
    GetEventd().Connect(m_eventc, receives, std::bind(&CPPageSheet::EventCallback, this, std::placeholders::_1));

    SetTreeWidth(216);
    AddPage(&m_player);
    AddPage(&m_toolBar);
    AddPage(&m_toolBarLayout);
    AddPage(&m_theme);
    AddPage(&m_formats);
    AddPage(&m_acceltbl);
	AddPage(&m_mouse);
    AddPage(&m_logo);
    AddPage(&m_webserver);
    AddPage(&m_playback);
    AddPage(&m_dvd);
    AddPage(&m_output);
    AddPage(&m_shaders);
    AddPage(&m_fullscreen);
    AddPage(&m_sync);
    AddPage(&m_tuner);
#if USE_LAVFILTERS
    AddPage(&m_internalfilters);
#endif
    AddPage(&m_audioswitcher);
    AddPage(&m_audiorenderer);

    AddPage(&m_externalfilters);
    AddPage(&m_subtitles);
    AddPage(&m_substyle);
    AddPage(&m_subMisc);
    AddPage(&m_tweaks);
    AddPage(&m_misc);
    AddPage(&m_advance);

    EnableStackedTabs(FALSE);

    SetTreeViewMode(TRUE, TRUE, FALSE);

    if (!idPage) {
        idPage = AfxGetAppSettings().nLastUsedPage;
    }
    if (idPage) {
        for (int i = 0; i < GetPageCount(); i++) {
            if (GetPage(i)->m_pPSP->pszTemplate == MAKEINTRESOURCE(idPage)) {
                SetActivePage(i);
                break;
            }
        }
    }

    if (AppIsThemeLoaded()) {
        CMPCThemeUtil::ModifyTemplates(this, RUNTIME_CLASS(CPPageShaders), IDC_LIST1, LBS_OWNERDRAWFIXED | LBS_HASSTRINGS);
        CMPCThemeUtil::ModifyTemplates(this, RUNTIME_CLASS(CPPageShaders), IDC_LIST2, LBS_OWNERDRAWFIXED | LBS_HASSTRINGS);
        CMPCThemeUtil::ModifyTemplates(this, RUNTIME_CLASS(CPPageShaders), IDC_LIST3, LBS_OWNERDRAWFIXED | LBS_HASSTRINGS);
        CMPCThemeUtil::ModifyTemplates(this, RUNTIME_CLASS(CPPageDVD), IDC_LIST1, LBS_OWNERDRAWFIXED | LBS_HASSTRINGS);
    }

}

//dummy constructor for calculating DPI changes.  do not use except internally!
CPPageSheet::CPPageSheet() : CTreePropSheet(_T("Dummy"), nullptr, 0)
    , m_bLockPage(false)
    , m_bLanguageChanged(false)
    , m_audioswitcher(nullptr)
    , initialized(false)
    , isDummySheet(true)
    , changingDPI(false)
{

    SetTreeWidth(216);
    AddPage(&m_dpiCalc);

    EnableStackedTabs(FALSE);
    SetTreeViewMode(TRUE, TRUE, FALSE);
}

CPPageSheet::~CPPageSheet()
{
}

void CPPageSheet::fulfillThemeReqs()
{
    if (AppIsThemeLoaded()) {
        CMPCThemeUtil::fulfillThemeReqs((CWnd*)this);
        CMPCThemeUtil::enableWindows10DarkFrame(this);
    }
}

void CPPageSheet::EventCallback(MpcEvent ev)
{
    switch (ev) {
        case MpcEvent::CHANGING_UI_LANGUAGE:
            m_bLanguageChanged = true;
            break;
        default:
            ASSERT(FALSE);
    }
}

CMPCThemeTreeCtrl* CPPageSheet::CreatePageTreeObject()
{
    return DEBUG_NEW CMPCThemeTreeCtrl();
}

void CPPageSheet::SetTreeCtrlTheme(CTreeCtrl* ctrl)
{
    if (AppIsThemeLoaded()) {
        ((CMPCThemeTreeCtrl*)ctrl)->fulfillThemeReqs();
    } else {
        __super::SetTreeCtrlTheme(ctrl);
    }
}

BEGIN_MESSAGE_MAP(CPPageSheet, CTreePropSheet)
    ON_WM_CONTEXTMENU()
    ON_COMMAND(ID_APPLY_NOW, OnApply)
    ON_WM_CTLCOLOR()
    ON_WM_DRAWITEM()
    ON_MESSAGE(WM_DPICHANGED, OnDpiChanged)
    ON_NOTIFY(TVN_SELCHANGEDA, 0x7EEE, OnPageTreeSelChanged)
    ON_NOTIFY(TVN_SELCHANGEDW, 0x7EEE, OnPageTreeSelChanged)
END_MESSAGE_MAP()


BOOL CPPageSheet::OnInitDialog()
{
    if (isDummySheet) {
        //we lie to CPropertySheet and pretend we have a modal dialog, so the buttons are included in the size
        m_bModeless = false;
    }

    BOOL bResult = __super::OnInitDialog();

    if (CTreeCtrl* pTree = GetPageTreeControl()) {
        for (HTREEITEM node = pTree->GetRootItem(); node; node = pTree->GetNextSiblingItem(node)) {
            pTree->Expand(node, TVE_EXPAND);
        }
    }

    if (m_bLockPage) {
        GetPageTreeControl()->EnableWindow(FALSE);
    }

    fulfillThemeReqs();
    initialized = true;
    return bResult;
}

void CPPageSheet::OnContextMenu(CWnd* /*pWnd*/, CPoint /*point*/)
{
    // display your own context menu handler or do nothing
}

void CPPageSheet::OnApply()
{
    // Execute the default actions first
    Default();

    // If the language was changed, we quit the dialog and inform the caller about it
    if (m_bLanguageChanged) {
        m_bLanguageChanged = false;
        EndDialog(APPLY_LANGUAGE_CHANGE);
    }
}

// TreePropSheet sets item data to (DWORD_PTR)-1 for nodes that have no page of their own
bool CPPageSheet::IsParentOnlyNode(CTreeCtrl* pTree, HTREEITEM hItem)
{
    return hItem && pTree->GetItemData(hItem) == (DWORD_PTR)-1;
}

void CPPageSheet::OnPageTreeSelChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
    // Let the base class update the caption, then override it with the first child's
    // text when a parent-only node is selected (e.g. show "General" instead of "Player").
    CTreePropSheet::OnPageTreeSelChanged(pNMHDR, pResult);

    if (m_pFrame && m_pFrame->GetShowCaption()) {
        if (CTreeCtrl* pTree = GetPageTreeControl()) {
            HTREEITEM hItem = pTree->GetSelectedItem();
            if (IsParentOnlyNode(pTree, hItem)) {
                HTREEITEM hChild = pTree->GetChildItem(hItem);
                if (hChild) {
                    m_pFrame->SetCaption(pTree->GetItemText(hChild));
                }
            }
        }
    }
}

TreePropSheet::CPropPageFrame* CPPageSheet::CreatePageFrame()
{
    if (AppIsThemeLoaded()) {
        return DEBUG_NEW CMPCThemePropPageFrame;
    } else {
        return __super::CreatePageFrame();
    }
}


HBRUSH CPPageSheet::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    if (AppIsThemeLoaded()) {
        LRESULT lResult;
        if (pWnd->SendChildNotifyLastMsg(&lResult)) {
            return (HBRUSH)lResult;
        }
        pDC->SetTextColor(CMPCTheme::TextFGColor);
        pDC->SetBkColor(CMPCTheme::ControlAreaBGColor);
        return controlAreaBrush;
    } else {
        HBRUSH hbr = __super::OnCtlColor(pDC, pWnd, nCtlColor);
        return hbr;
    }
}

AFX_STATIC_DATA int _afxPropSheetButtons[] = { IDOK, IDCANCEL, ID_APPLY_NOW, IDHELP };

bool CopyWindowPosition(CWnd* wnd, CWnd* refwindow, CWnd* refParent, bool move = true) {
    if (wnd && ::IsWindow(wnd->m_hWnd) && refwindow && ::IsWindow(refwindow->m_hWnd)) {
        CRect r;
        refwindow->GetWindowRect(&r);
        if (refParent) {
            refParent->ScreenToClient(&r);
        }
        wnd->SetWindowPos(nullptr, (move ? r.left : -1), (move ? r.top : -1), r.Width(), r.Height(), SWP_FRAMECHANGED | SWP_NOCOPYBITS | SWP_NOZORDER | SWP_NOACTIVATE | (move ? 0 : SWP_NOMOVE));
        return true;
    }
    return false;
}

LRESULT CPPageSheet::OnDpiChanged(WPARAM wParam, LPARAM lParam) {
    if (DpiHelper::CanUsePerMonitorV2() && initialized && !isDummySheet && !changingDPI) {
        //PerMonitorV2 behavior as of Win 10 1703 scales common controls and dialog layouts
        //A new window behaves correctly after some DPI tweaks were added to TreePropSheet class
        //Deficiencies:
        //1. Moved and auto-resized dialog is not the same as a dialog opened on that monitor, but a "smart" size to try to fit all elements, but doesn't fit
        //2. Fonts are not updated for default buttons (OK,Apply,Cancel)
        //3. TreePropSheet custom class logic skipped on DPI change
        //4. Hidden tab control not adjusted properly for TreePropSheet
        //Solution (1-4):
        //Create "dummy" versions of Options on target monitor and copy sizes and fonts where necessary
        //5. Image assets scaled poorly and degrade when dragging back and forth
        //Solution:
        //Set the icon and bitmaps after size to the original asset.  instead of a resized asset, the asset is dynamically chosen at the best resolution

        changingDPI = true;
        SetRedraw(false);
        WORD lastPage = AfxGetAppSettings().nLastUsedPage; //back up last used page as dummy could change it

        CPPageSheet dummy;
        dummy.Create(this, DS_MODALFRAME | DS_3DLOOK | DS_CONTEXTHELP | DS_SETFONT | WS_POPUP | WS_CAPTION, WS_EX_NOACTIVATE);
        SetActiveWindow();  //dlg create can steal focus (even with no activate), get it back right away

        //set dpi aware main window size
        CopyWindowPosition(this, &dummy, nullptr, false);

        //set dpi aware tabctrl (hidden window)
        if (CopyWindowPosition(GetTabControl(), dummy.GetTabControl(), &dummy)) {
            //the font for the tab control affects how tall the visible caption area is
            //and the offset for auto-placed dialogs, so we update it even though it's not visible
            CFont* f = dummy.GetTabControl()->GetFont();
            LOGFONT lf;
            f->GetLogFont(&lf);
            dpiTabFont.DeleteObject();
            dpiTabFont.CreateFontIndirect(&lf);
            GetTabControl()->SetFont(&dpiTabFont);
        }

        //set dpi aware tree window pos/size
        CopyWindowPosition(GetPageTreeControl(), dummy.GetPageTreeControl(), &dummy);

        //set dpi aware pos/size for all buttons
        for (int i = 0; i < _countof(_afxPropSheetButtons); i++) {
            CWnd* wnd = GetDlgItem(_afxPropSheetButtons[i]);
            CWnd* dwnd = dummy.GetDlgItem(_afxPropSheetButtons[i]);
            if (CopyWindowPosition(wnd, dwnd, &dummy)) {
                if (i == 0) { //get correct font from OK button
                    CFont* f = dwnd->GetFont();
                    LOGFONT lf;
                    f->GetLogFont(&lf);
                    dpiButtonFont.DeleteObject();
                    dpiButtonFont.CreateFontIndirect(&lf);
                }
                wnd->SetFont(&dpiButtonFont);
            }
        }

        //set dpi aware main frame pos/size
        if (m_pFrame) {
            CopyWindowPosition(m_pFrame->GetWnd(), dummy.GetTabControl(), &dummy);
            if (dummy.m_pFrame) {
                m_pFrame->SetCaptionHeight(dummy.m_pFrame->GetCaptionHeight());
            }
        }

        CWnd* pChild = GetWindow(GW_CHILD);
        while (pChild) {
            TCHAR windowClass[MAX_PATH];
            ::GetClassName(pChild->GetSafeHwnd(), windowClass, _countof(windowClass));

            if (0 == _tcsicmp(windowClass, _T("#32770"))) { //dialog class
                CWnd* dlgChild = pChild->GetWindow(GW_CHILD);
                while (dlgChild) {
                    TCHAR childClass[MAX_PATH];
                    DWORD style = dlgChild->GetStyle();
                    DWORD staticStyle = (style & SS_TYPEMASK);
                    ::GetClassName(dlgChild->GetSafeHwnd(), childClass, _countof(childClass));
                    if (0 == _tcsicmp(childClass, WC_STATIC) && SS_BITMAP == staticStyle) {
                        CStatic *sBMP = DYNAMIC_DOWNCAST(CStatic, dlgChild);
                        if (sBMP) {
                            sBMP->SetBitmap(sBMP->GetBitmap());
                        }
                    } else if (0 == _tcsicmp(childClass, WC_STATIC) && SS_ICON == staticStyle) {
                        CStatic* sBMP = DYNAMIC_DOWNCAST(CStatic, dlgChild);
                        if (sBMP) {
                            sBMP->SetIcon(sBMP->GetIcon());
                        }
                    }

                    dlgChild = dlgChild->GetNextWindow();
                }
            }
            CPPageAdvanced* ppa;
            if ((ppa = DYNAMIC_DOWNCAST(CPPageAdvanced, pChild)) != 0) {
                ppa->DoDPIChanged();
            }
            pChild = pChild->GetNextWindow();
        }


        //destroy dummy window since we used ::Create
        dummy.DestroyWindow();

        //restore last used page
        AfxGetAppSettings().nLastUsedPage = lastPage;

        //allow redraws again
        SetRedraw(true);

        //setting the active page updates the page dialog position
        SetActivePage(GetActivePage());

        RedrawWindow(nullptr, nullptr, RDW_ALLCHILDREN | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
        changingDPI = false;
    }
    return FALSE;
}

INT_PTR CPPageSheet::DoModal() {
    PreDoModalRTL(&m_psh);
    return __super::DoModal();
}


// CTreePropSheetTreeCtrl

IMPLEMENT_DYNAMIC(CTreePropSheetTreeCtrl, CTreeCtrl)
CTreePropSheetTreeCtrl::CTreePropSheetTreeCtrl()
{
}

CTreePropSheetTreeCtrl::~CTreePropSheetTreeCtrl()
{
}


BEGIN_MESSAGE_MAP(CTreePropSheetTreeCtrl, CTreeCtrl)
END_MESSAGE_MAP()

// CTreePropSheetTreeCtrl message handlers

BOOL CTreePropSheetTreeCtrl::PreCreateWindow(CREATESTRUCT& cs)
{
    cs.dwExStyle |= WS_EX_CLIENTEDGE;
    //  cs.style &= ~TVS_LINESATROOT;

    return __super::PreCreateWindow(cs);
}
