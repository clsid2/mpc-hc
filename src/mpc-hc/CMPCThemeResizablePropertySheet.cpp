#include "stdafx.h"
#include "CMPCThemeResizablePropertySheet.h"
#include "CMPCTheme.h"
#include "mplayerc.h"
#include "DpiHelper.h"
#include "CMPCThemeResizablePropertyPage.h"

CMPCThemeResizablePropertySheet::CMPCThemeResizablePropertySheet(UINT nIDCaption, CWnd* pParentWnd, UINT iSelectPage)
    :CResizableSheet(nIDCaption, pParentWnd, iSelectPage)
    ,isModal(false)
{
}

CMPCThemeResizablePropertySheet::CMPCThemeResizablePropertySheet(LPCTSTR pszCaption, CWnd* pParentWnd, UINT iSelectPage)
    :CResizableSheet(pszCaption, pParentWnd, iSelectPage)
    ,isModal(false)
{
}

CMPCThemeResizablePropertySheet::~CMPCThemeResizablePropertySheet()
{
}

IMPLEMENT_DYNAMIC(CMPCThemeResizablePropertySheet, CResizableSheet)
BEGIN_MESSAGE_MAP(CMPCThemeResizablePropertySheet, CResizableSheet)
    ON_WM_CTLCOLOR()
    ON_WM_DESTROY()
    ON_MESSAGE(WM_DPICHANGED, OnDpiChanged)
END_MESSAGE_MAP()

BOOL CMPCThemeResizablePropertySheet::OnInitDialog()
{
    BOOL bResult = __super::OnInitDialog();
    fulfillThemeReqs();
    m_dpi = DpiHelper::GetDPIForWindow(m_hWnd);
    // PresetLayout has made the initial size the minimum. Kept as a client size: the frame does not scale with
    // the DPI (16x39 px at 96 dpi, 26x71 at 192), so twice the window would be too big
    CRect r;
    GetClientRect(r);
    m_minTrackSize = r.Size();
    m_minTrackDpi = m_dpi;
    return bResult;
}

void CMPCThemeResizablePropertySheet::OnDestroy()
{
    m_anchors.clear();
    m_dpi = 0;
    m_minTrackDpi = 0;
    __super::OnDestroy();
}

void CMPCThemeResizablePropertySheet::AddAnchor(HWND hWnd, ANCHOR anchorTopLeft, ANCHOR anchorBottomRight)
{
    CResizableSheet::AddAnchor(hWnd, anchorTopLeft, anchorBottomRight);
    m_anchors.push_back({ hWnd, anchorTopLeft, anchorBottomRight });
}

// Windows scales the fonts on a DPI change, but every control of the sheet and its pages is anchored, and the
// anchors hold pixel margins, so the next layout put each control back at its old size (labels 90x13 holding
// 200% text). The anchors are dropped, every control is scaled with the DPI, and the anchors added again.
LRESULT CMPCThemeResizablePropertySheet::OnDpiChanged(WPARAM wParam, LPARAM lParam)
{
    const UINT oldDpi = m_dpi;
    const UINT newDpi = LOWORD(wParam);
    if (!oldDpi || !newDpi || oldDpi == newDpi) {
        if (newDpi) {
            m_dpi = newDpi;
        }
        return Default();
    }
    m_dpi = newDpi;

    // Each control's rect at the old DPI in its parent's client area: the sheet's own (tab control, buttons,
    // pages) and those of every page created so far. A page created later is created at the new DPI.
    struct ChildRect {
        HWND hWnd;
        CRect rect;
    };
    std::vector<ChildRect> rects;
    auto snapshot = [&rects](HWND parent) {
        for (HWND h = ::GetWindow(parent, GW_CHILD); h; h = ::GetWindow(h, GW_HWNDNEXT)) {
            CRect r;
            ::GetWindowRect(h, r);
            ::MapWindowPoints(nullptr, parent, (LPPOINT)&r, 2);
            rects.push_back({ h, r });
        }
    };
    snapshot(m_hWnd);
    std::vector<CMPCThemeResizablePropertyPage*> pages;
    for (int i = 0; i < GetPageCount(); i++) {
        CPropertyPage* page = GetPage(i);
        if (page && ::IsWindow(page->m_hWnd)) {
            snapshot(page->m_hWnd);
            if (auto resizablePage = DYNAMIC_DOWNCAST(CMPCThemeResizablePropertyPage, page)) {
                resizablePage->RemoveAnchorsForDpiChange(newDpi);
                pages.push_back(resizablePage);
            }
        }
    }
    CRect client;
    GetClientRect(client);
    RemoveAllAnchors();

    Default(); // the fonts at the new DPI

    // The minimum first: a resize is held to the old one, so a sheet opened at 200% could not shrink at 100%
    CSize minTrack(0, 0);
    if (m_minTrackDpi) {
        CRect r(0, 0, MulDiv(m_minTrackSize.cx, newDpi, m_minTrackDpi), MulDiv(m_minTrackSize.cy, newDpi, m_minTrackDpi));
        DpiHelper::AdjustWindowRectExForDpi(&r, GetStyle(), FALSE, GetExStyle(), newDpi);
        minTrack = r.Size();
        SetMinTrackSize(minTrack);
    }

    // The client area scaled with the DPI, where Windows suggests
    const LPCRECT suggested = reinterpret_cast<LPCRECT>(lParam);
    const CSize targetClient(MulDiv(client.Width(), newDpi, oldDpi), MulDiv(client.Height(), newDpi, oldDpi));
    CRect window(CPoint(0, 0), targetClient);
    DpiHelper::AdjustWindowRectExForDpi(&window, GetStyle(), FALSE, GetExStyle(), newDpi);
    SetWindowPos(nullptr, suggested->left, suggested->top, window.Width(), window.Height(), SWP_NOZORDER | SWP_NOACTIVATE);

    auto scale = [&](int v) { return MulDiv(v, newDpi, oldDpi); };
    for (const auto& c : rects) {
        if (::IsWindow(c.hWnd)) {
            int left = scale(c.rect.left), top = scale(c.rect.top);
            int width = scale(c.rect.right) - left, height = scale(c.rect.bottom) - top;
            // an etched line keeps its thickness, as it has when created at any DPI (2 px at 200%)
            TCHAR cls[16];
            if (::GetClassName(c.hWnd, cls, _countof(cls)) && !_tcsicmp(cls, WC_STATIC)) {
                DWORD type = ::GetWindowLong(c.hWnd, GWL_STYLE) & SS_TYPEMASK;
                if (type == SS_ETCHEDHORZ) {
                    height = c.rect.Height();
                } else if (type == SS_ETCHEDVERT) {
                    width = c.rect.Width();
                }
            }
            ::SetWindowPos(c.hWnd, nullptr, left, top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }

    for (auto page : pages) {
        page->RestoreAnchorsAfterDpiChange();
    }
    PresetLayout(); // also makes the current size the minimum, so the scaled one is put back after it
    // after PresetLayout, as a derived sheet's OnInitDialog adds its own after it
    for (const auto& a : m_anchors) {
        if (::IsWindow(a.hWnd)) {
            CResizableSheet::RemoveAnchor(a.hWnd);
            CResizableSheet::AddAnchor(a.hWnd, a.topLeft, a.bottomRight);
        }
    }
    if (m_minTrackDpi) {
        SetMinTrackSize(minTrack);
    }
    UpdateSizeGrip();
    ArrangeLayout();
    RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    return 0;
}

void CMPCThemeResizablePropertySheet::fulfillThemeReqs()
{
    CMPCThemeUtil::enableWindows10DarkFrame(this);
    if (AppIsThemeLoaded()) {
        SetSizeGripBkMode(TRANSPARENT); //fix for gripper in mpc theme
    }
    CMPCThemeUtil::fulfillThemeReqs((CWnd*)this);
}

HBRUSH CMPCThemeResizablePropertySheet::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    if (AppNeedsThemedControls()) {
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

INT_PTR CMPCThemeResizablePropertySheet::DoModal() {
    isModal = true;
    PreDoModalRTL(&m_psh);
    return __super::DoModal();
}
