#include "stdafx.h"
#include "CMPCThemeResizablePropertyPage.h"
#include "DpiHelper.h"

CMPCThemeResizablePropertyPage::CMPCThemeResizablePropertyPage(UINT nIDTemplate, UINT nIDCaption)
    : CResizablePage(nIDTemplate, nIDCaption)
{

}

CMPCThemeResizablePropertyPage::~CMPCThemeResizablePropertyPage()
{
}

BOOL CMPCThemeResizablePropertyPage::OnInitDialog()
{
    m_anchors.clear();
    __super::OnInitDialog();
    fulfillThemeReqs();
    // the sheet scales the controls on a DPI change (CMPCThemeResizablePropertySheet::OnDpiChanged); the dialog
    // manager would scale them a second time once the sheet is done
    DpiHelper::DisableDialogControlRelayout(m_hWnd);
    // CResizablePage has made the initial size the minimum
    CRect r;
    GetWindowRect(r);
    m_minTrackSize = r.Size();
    m_minTrackDpi = DpiHelper::GetDPIForWindow(m_hWnd);
    return 0;
}

void CMPCThemeResizablePropertyPage::AddAnchor(HWND hWnd, ANCHOR anchorTopLeft, ANCHOR anchorBottomRight)
{
    CResizablePage::AddAnchor(hWnd, anchorTopLeft, anchorBottomRight);
    m_anchors.push_back({ hWnd, anchorTopLeft, anchorBottomRight });
}

void CMPCThemeResizablePropertyPage::RemoveAnchorsForDpiChange(UINT dpi)
{
    if (m_minTrackDpi) {
        SetMinTrackSize(CSize(MulDiv(m_minTrackSize.cx, dpi, m_minTrackDpi), MulDiv(m_minTrackSize.cy, dpi, m_minTrackDpi)));
    }
    RemoveAllAnchors();
}

void CMPCThemeResizablePropertyPage::RestoreAnchorsAfterDpiChange()
{
    RemoveAllAnchors();
    for (const auto& a : m_anchors) {
        if (::IsWindow(a.hWnd)) {
            CResizablePage::AddAnchor(a.hWnd, a.topLeft, a.bottomRight);
        }
    }
}

IMPLEMENT_DYNCREATE(CMPCThemeResizablePropertyPage, CResizablePage)
BEGIN_MESSAGE_MAP(CMPCThemeResizablePropertyPage, CResizablePage)
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

HBRUSH CMPCThemeResizablePropertyPage::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    HBRUSH ret;
    ret = getCtlColor(pDC, pWnd, nCtlColor);
    if (nullptr != ret) {
        return ret;
    } else {
        return __super::OnCtlColor(pDC, pWnd, nCtlColor);
    }
}
