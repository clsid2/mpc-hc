#include "stdafx.h"
#include "CMPCThemeTreeCtrl.h"
#include "CMPCTheme.h"
#include "CMPCThemeUtil.h"
#include "mplayerc.h"
#undef SubclassWindow


CMPCThemeTreeCtrl::CMPCThemeTreeCtrl() : CTreeCtrl(), CMPCThemeScrollBarRenderer(),
    themedToolTipCid((UINT_PTR) - 1)
{
    if (AppNeedsThemedControls()) {
        m_brBkgnd.CreateSolidBrush(CMPCTheme::InlineEditBorderColor);
    }
}

CMPCThemeTreeCtrl::~CMPCThemeTreeCtrl()
{
    m_brBkgnd.DeleteObject();
}

BOOL CMPCThemeTreeCtrl::PreCreateWindow(CREATESTRUCT& cs)
{
    if (AppNeedsThemedControls()) {
        cs.dwExStyle |= WS_EX_CLIENTEDGE;
    }
    return __super::PreCreateWindow(cs);
}

void CMPCThemeTreeCtrl::fulfillThemeReqs()
{
    if (AppNeedsThemedControls()) {
        if (CMPCThemeUtil::canUseWin10DarkTheme()) {
            SetWindowTheme(GetSafeHwnd(), L"DarkMode_Explorer", NULL);
        } else {
            SetWindowTheme(GetSafeHwnd(), L"", NULL);
        }
        SetExtendedStyle(TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER); //necessary to prevent significant flicker

        SetLineColor(CMPCTheme::TreeCtrlLineColor);
        if (nullptr == tvsTooltip.m_hWnd) {
            CToolTipCtrl* t = GetToolTips();
            if (nullptr != t) {
                tvsTooltip.SubclassWindow(t->m_hWnd);
            }
        }
    } else {
        //adipose--enabling this cuts down on a very minor flicker in classic mode;
        //the duplicate line above is necessary due to a non-default bg.
        //treat as a separate line of code to be clear that this one is "optional" while the other is not
        SetExtendedStyle(TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER); //optional
    }
}

BEGIN_MESSAGE_MAP(CMPCThemeTreeCtrl, CTreeCtrl)
    ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, &CMPCThemeTreeCtrl::OnNMCustomdraw)
    ON_WM_ERASEBKGND()
    ON_WM_DRAWITEM()
    ON_WM_NCPAINT()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSEWHEEL()
END_MESSAGE_MAP()
IMPLEMENT_DYNAMIC(CMPCThemeTreeCtrl, CTreeCtrl)

BOOL CMPCThemeTreeCtrl::PreTranslateMessage(MSG* pMsg)
{
    if (AppNeedsThemedControls()) {
        if (!IsWindow(themedToolTip.m_hWnd)) {
            themedToolTip.Create(this, TTS_ALWAYSTIP);
        }
        if (IsWindow(themedToolTip.m_hWnd)) {
            themedToolTip.RelayEvent(pMsg);
        }
    }
    return __super::PreTranslateMessage(pMsg);
}

void CMPCThemeTreeCtrl::updateToolTip(CPoint point)
{
    if (AppNeedsThemedControls() && nullptr != themedToolTip) {
        TOOLINFO ti = { 0 };
        UINT_PTR tid = OnToolHitTest(point, &ti);
        //OnToolHitTest returns -1 on failure but doesn't update uId to match

        if (tid == -1 || themedToolTipCid != ti.uId) { //if no tooltip, or id has changed, remove old tool
            if (themedToolTip.GetToolCount() > 0) {
                themedToolTip.DelTool(this);
                themedToolTip.Activate(FALSE);
            }
            themedToolTipCid = (UINT_PTR) - 1;
        }

        if (tid != -1 && themedToolTipCid != ti.uId && 0 != ti.uId) {

            themedToolTipCid = ti.uId;

            CRect cr;
            GetClientRect(&cr); //we reset the tooltip every time we move anyway, so this rect is adequate

            themedToolTip.AddTool(this, LPSTR_TEXTCALLBACK, &cr, ti.uId);
            themedToolTip.Activate(TRUE);
        }
    }
}

void CMPCThemeTreeCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
    __super::OnMouseMove(nFlags, point);
    updateToolTip(point);
}

void CMPCThemeTreeCtrl::OnNMCustomdraw(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMCUSTOMDRAW pNMCD = reinterpret_cast<LPNMCUSTOMDRAW>(pNMHDR);
    NMTVCUSTOMDRAW* pstCD = reinterpret_cast<NMTVCUSTOMDRAW*>(pNMHDR);
    *pResult = CDRF_DODEFAULT;

    if (AppNeedsThemedControls()) {

        bool isFocus, isHot;
        switch (pNMCD->dwDrawStage) {
            case CDDS_PREPAINT: {
                *pResult = CDRF_NOTIFYITEMDRAW;
                CDC dc;
                dc.Attach(pNMCD->hdc);
                dc.FillSolidRect(&pNMCD->rc, CMPCTheme::ContentBGColor);
                //doEraseBkgnd(&dc);
                dc.Detach();
                break;
            }
            case CDDS_ITEMPREPAINT:
                isFocus = 0 != (pNMCD->uItemState & CDIS_FOCUS);
                isHot = 0 != (pNMCD->uItemState & CDIS_HOT);

                //regular theme is a bit ugly but better than Explorer theme. we clear the focus states to control the highlight ourselves
                if (!CMPCThemeUtil::canUseWin10DarkTheme()) {
                    pNMCD->uItemState &= ~(CDIS_FOCUS | CDIS_HOT | CDIS_SELECTED);
                }

                if (isFocus) {
                    pstCD->clrTextBk = CMPCTheme::TreeCtrlFocusColor;
                } else if (isHot) {
                    pstCD->clrTextBk = CMPCTheme::TreeCtrlHoverColor;
                } else {
                    pstCD->clrTextBk = CMPCTheme::ContentBGColor;
                }
                if (0 == (pNMCD->uItemState & CDIS_DISABLED) && IsWindowEnabled()) {
                    pstCD->clrText = CMPCTheme::TextFGColor;
                } else {
                    pstCD->clrText = CMPCTheme::ButtonDisabledFGColor;
                }
                *pResult = CDRF_DODEFAULT;
                break;
            default:
                pResult = CDRF_DODEFAULT;
                break;
        }
    } else {
        __super::OnPaint();
    }
}

void CMPCThemeTreeCtrl::doEraseBkgnd(CDC* pDC)
{
    CRect r;
    GetWindowRect(r);
    r.OffsetRect(-r.left, -r.top);
    pDC->FillSolidRect(r, CMPCTheme::ContentBGColor);
}

BOOL CMPCThemeTreeCtrl::OnEraseBkgnd(CDC* pDC)
{
    if (AppNeedsThemedControls()) {
        //doEraseBkgnd(pDC); //we do this in the custom draw prepaint step now, to allow double buffering to work
        return TRUE;
    } else {
        return __super::OnEraseBkgnd(pDC);
    }
}


void CMPCThemeTreeCtrl::OnNcPaint()
{
    if (AppNeedsThemedControls()) {
        HandleNcPaint(m_hWnd);
    } else {
        __super::OnNcPaint();
    }
}

BOOL CMPCThemeTreeCtrl::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    BOOL ret = __super::OnMouseWheel(nFlags, zDelta, pt);
    if (AppNeedsThemedControls()) {
        ScreenToClient(&pt);
        updateToolTip(pt);
    }
    return ret;
}

LRESULT CMPCThemeTreeCtrl::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    if (AppNeedsThemedControls()) {
        CMPCThemeScrollBarRenderer::ProcessMessage(m_hWnd, message, wParam, lParam);
    }
    return __super::WindowProc(message, wParam, lParam);
}
