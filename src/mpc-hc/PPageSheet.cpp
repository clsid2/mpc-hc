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
#include "CDarkTheme.h"
#include <prsht.h>

// CPPageSheet

IMPLEMENT_DYNAMIC(CPPageSheet, CTreePropSheet)

CPPageSheet::CPPageSheet(LPCTSTR pszCaption, IFilterGraph* pFG, CWnd* pParentWnd, UINT idPage)
    : CTreePropSheet(pszCaption, pParentWnd, 0)
    , m_bLockPage(false)
    , m_bLanguageChanged(false)
    , m_audioswitcher(pFG)
{
    EventRouter::EventSelection receives;
    receives.insert(MpcEvent::CHANGING_UI_LANGUAGE);
    GetEventd().Connect(m_eventc, receives, std::bind(&CPPageSheet::EventCallback, this, std::placeholders::_1));

    SetTreeWidth(m_dpi.ScaleX(180));
    AddPage(&m_player);
    AddPage(&m_formats);
    AddPage(&m_acceltbl);
    AddPage(&m_logo);
    AddPage(&m_webserver);
    AddPage(&m_playback);
    AddPage(&m_dvd);
    AddPage(&m_output);
    AddPage(&m_shaders);
    AddPage(&m_fullscreen);
    AddPage(&m_sync);
    AddPage(&m_tuner);
#ifndef MPCHC_LITE
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

    if (AfxGetAppSettings().bDarkThemeLoaded) {
        CDarkChildHelper::ModifyTemplates(this, RUNTIME_CLASS(CPPageShaders), IDC_LIST1, LBS_OWNERDRAWFIXED | LBS_HASSTRINGS);
        CDarkChildHelper::ModifyTemplates(this, RUNTIME_CLASS(CPPageShaders), IDC_LIST2, LBS_OWNERDRAWFIXED | LBS_HASSTRINGS);
        CDarkChildHelper::ModifyTemplates(this, RUNTIME_CLASS(CPPageShaders), IDC_LIST3, LBS_OWNERDRAWFIXED | LBS_HASSTRINGS);
/* no owner draw, override onpaint() instead
        CDarkChildHelper::ModifyTemplates(this, RUNTIME_CLASS(CPPageOutput), IDC_VIDRND_COMBO, CBS_OWNERDRAWFIXED | CBS_HASSTRINGS);
        CDarkChildHelper::ModifyTemplates(this, RUNTIME_CLASS(CPPageOutput), IDC_RMRND_COMBO, CBS_OWNERDRAWFIXED | CBS_HASSTRINGS);
        CDarkChildHelper::ModifyTemplates(this, RUNTIME_CLASS(CPPageOutput), IDC_QTRND_COMBO, CBS_OWNERDRAWFIXED | CBS_HASSTRINGS);
        CDarkChildHelper::ModifyTemplates(this, RUNTIME_CLASS(CPPageOutput), IDC_AUDRND_COMBO, CBS_OWNERDRAWFIXED | CBS_HASSTRINGS);
        CDarkChildHelper::ModifyTemplates(this, RUNTIME_CLASS(CPPageOutput), IDC_COMBO1, CBS_OWNERDRAWFIXED | CBS_HASSTRINGS);
        CDarkChildHelper::ModifyTemplates(this, RUNTIME_CLASS(CPPageOutput), IDC_D3D9DEVICE_COMBO, CBS_OWNERDRAWFIXED | CBS_HASSTRINGS);
        CDarkChildHelper::ModifyTemplates(this, RUNTIME_CLASS(CPPageOutput), IDC_DX9RESIZER_COMBO, CBS_OWNERDRAWFIXED | CBS_HASSTRINGS);
*/
        CDarkChildHelper::ModifyTemplates(this, RUNTIME_CLASS(CPPageDVD), IDC_LIST1, LBS_OWNERDRAWFIXED | LBS_HASSTRINGS);
    }

}

CPPageSheet::~CPPageSheet() {
}

void CPPageSheet::enableDarkThemeIfActive() {
    if (AfxGetAppSettings().bDarkThemeLoaded) {
        CDarkChildHelper::enableDarkThemeIfActive((CWnd*)this);
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

CDarkTreeCtrl* CPPageSheet::CreatePageTreeObject()
{
    return DEBUG_NEW CDarkTreeCtrl();
}

void CPPageSheet::SetTreeCtrlTheme(CTreeCtrl * ctrl) {
    if (AfxGetAppSettings().bDarkThemeLoaded) {
        ((CDarkTreeCtrl*)ctrl)->setDarkTheme();
    } else {
        __super::SetTreeCtrlTheme(ctrl);
    }
}

BEGIN_MESSAGE_MAP(CPPageSheet, CTreePropSheet)
    ON_WM_CONTEXTMENU()
    ON_COMMAND(ID_APPLY_NOW, OnApply)
    ON_WM_CTLCOLOR()
    ON_WM_DRAWITEM()
END_MESSAGE_MAP()


BOOL CPPageSheet::OnInitDialog()
{
    BOOL bResult = __super::OnInitDialog();

    if (CTreeCtrl* pTree = GetPageTreeControl()) {
        for (HTREEITEM node = pTree->GetRootItem(); node; node = pTree->GetNextSiblingItem(node)) {
            pTree->Expand(node, TVE_EXPAND);
        }
    }

    if (m_bLockPage) {
        GetPageTreeControl()->EnableWindow(FALSE);
    }

    enableDarkThemeIfActive();
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

TreePropSheet::CPropPageFrame* CPPageSheet::CreatePageFrame() {
    if (AfxGetAppSettings().bDarkThemeLoaded) {
        return DEBUG_NEW CDarkPropPageFrame;
    } else {
        return __super::CreatePageFrame();
    }
}


HBRUSH CPPageSheet::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) {
    if (AfxGetAppSettings().bDarkThemeLoaded) {
        LRESULT lResult;
        if (pWnd->SendChildNotifyLastMsg(&lResult)) {
            return (HBRUSH)lResult;
        }
        pDC->SetTextColor(CDarkTheme::TextFGColor);
        pDC->SetBkColor(CDarkTheme::ControlAreaBGColor);
        return darkControlAreaBrush;
    } else {
        HBRUSH hbr = __super::OnCtlColor(pDC, pWnd, nCtlColor);
        return hbr;
    }
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
