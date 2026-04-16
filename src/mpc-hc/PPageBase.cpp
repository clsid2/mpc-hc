/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2014, 2016 see Authors.txt
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
#include "MainFrm.h"
#include "mplayerc.h"
#include "PPageBase.h"
#include "SettingsDefines.h"
#include "ComPropertySheet.h"


// CPPageBase dialog

IMPLEMENT_DYNAMIC(CPPageBase, CCmdUIPropertyPage)
CPPageBase::CPPageBase(UINT nIDTemplate, UINT nIDCaption)
    : CCmdUIPropertyPage(nIDTemplate, nIDCaption)
{
}

CPPageBase::~CPPageBase()
{
}

void CPPageBase::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
}

bool CPPageBase::FillComboToolTip(CComboBox& comboBox, TOOLTIPTEXT* pTTT)
{
    bool bNeedTooltip = false;

    CDC* pDC = comboBox.GetDC();
    CFont* pFont = comboBox.GetFont();
    CFont* pOldFont = pDC->SelectObject(pFont);

    TEXTMETRIC tm;
    pDC->GetTextMetrics(&tm);

    CRect comboBoxRect;
    comboBox.GetWindowRect(comboBoxRect);
    comboBoxRect.right -= GetSystemMetrics(SM_CXVSCROLL) + 2 * GetSystemMetrics(SM_CXEDGE);

    int i = comboBox.GetCurSel();
    CString str;
    comboBox.GetLBText(i, str);
    CSize textSize;
    textSize = pDC->GetTextExtent(str);
    pDC->SelectObject(pOldFont);
    comboBox.ReleaseDC(pDC);
    textSize.cx += tm.tmAveCharWidth;

    if (textSize.cx > comboBoxRect.Width()) {
        bNeedTooltip = true;
        if (str.GetLength() > _countof(pTTT->szText) - 1) {
            str.Truncate(_countof(pTTT->szText) - 1);
        }
        _tcscpy_s(pTTT->szText, str);
        pTTT->hinst = nullptr;
    }

    return bNeedTooltip;
}

void CPPageBase::CreateToolTip()
{
    m_wndToolTip.Create(this, TTS_NOPREFIX);
    m_wndToolTip.Activate(TRUE);
    m_wndToolTip.SetMaxTipWidth(300);
    m_wndToolTip.SetDelayTime(TTDT_AUTOPOP, 10000);
    for (CWnd* pChild = GetWindow(GW_CHILD); pChild; pChild = pChild->GetWindow(GW_HWNDNEXT)) {
        CString strToolTip;
        if (strToolTip.LoadString(pChild->GetDlgCtrlID())) {
            m_wndToolTip.AddTool(pChild, strToolTip);
        }
    }
}

void CPPageBase::SetButtonIcon(UINT nIDButton, IconDef iconDef)
{
    if (!m_buttonIcons.count(iconDef)) {
        CImage img;
        if (iconDef.svgTargetWidth) {
            SVGImage::LoadIconDef(iconDef, img);
        } else {
            img.LoadFromResource(AfxGetInstanceHandle(), iconDef.nIDIcon);
        }
        CImageList& imageList = m_buttonIcons[iconDef];
        imageList.Create(img.GetWidth(), img.GetHeight(), ILC_COLOR32, 1, 0);
        imageList.Add(CBitmap::FromHandle(img), nullptr);
    }

    BUTTON_IMAGELIST buttonImageList;
    buttonImageList.himl = m_buttonIcons[iconDef];
    buttonImageList.margin = { 0, 0, 0, 0 };
    buttonImageList.uAlign = BUTTON_IMAGELIST_ALIGN_CENTER;
    static_cast<CButton*>(GetDlgItem(nIDButton))->SetImageList(&buttonImageList);
}

BOOL CPPageBase::PreTranslateMessage(MSG* pMsg)
{
    if (IsWindow(m_wndToolTip))
        if (pMsg->message >= WM_MOUSEFIRST && pMsg->message <= WM_MOUSELAST) {
            MSG msg;
            memcpy(&msg, pMsg, sizeof(MSG));
            for (HWND hWndParent = ::GetParent(msg.hwnd);
                    hWndParent && hWndParent != m_hWnd;
                    hWndParent = ::GetParent(hWndParent)) {
                msg.hwnd = hWndParent;
            }

            if (msg.hwnd) {
                m_wndToolTip.RelayEvent(&msg);
            }
        }

    return __super::PreTranslateMessage(pMsg);
}

BEGIN_MESSAGE_MAP(CPPageBase, CCmdUIPropertyPage)
    ON_WM_DESTROY()
END_MESSAGE_MAP()


// CPPageBase message handlers

BOOL CPPageBase::OnSetActive()
{
    ASSERT(IS_INTRESOURCE(m_pPSP->pszTemplate));
    AfxGetAppSettings().nLastUsedPage = (WORD)(ULONG_PTR)m_pPSP->pszTemplate;
    SetRedraw(false); //adipose: disable redraw due to CPropertyPage::OnSetActive forcing ddx, which causes "optimized" redraw of comboboxes without consulting subclass paint method
    BOOL ret = __super::OnSetActive();
    SetRedraw(true); //adipose: reenable redraw. no regressions observed by enabling after ddx
    return ret;
}

BOOL CPPageBase::OnApply()
{
    // There is no main frame when the option dialog is displayed stand-alone
    if (CMainFrame* pMainFrame = AfxGetMainFrame()) {
        pMainFrame->PostMessage(WM_SAVESETTINGS);
    }
    return __super::OnApply();
}

void CPPageBase::OnDestroy()
{
    __super::OnDestroy();
    m_wndToolTip.DestroyWindow();
}

void CPPageBase::ShowPPage(CUnknown* (WINAPI* CreateInstance)(LPUNKNOWN lpunk, HRESULT* phr)) {
    if (!CreateInstance) {
        return;
    }

    HRESULT hr;
    CUnknown* pObj = CreateInstance(nullptr, &hr);

    if (!pObj) {
        return;
    }

    CComPtr<IUnknown> pUnk = (IUnknown*)(INonDelegatingUnknown*)pObj;

    if (SUCCEEDED(hr)) {
        if (CComQIPtr<ISpecifyPropertyPages> pSPP = pUnk) {
            CComPropertySheet ps(ResStr(IDS_PROPSHEET_PROPERTIES), this);
            ps.AddPages(pSPP);
            ps.DoModal();
        }
    }
}
