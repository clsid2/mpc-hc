#include "stdafx.h"
#include "CMPCThemeListBox.h"
#include "CMPCTheme.h"
#include "CMPCThemeUtil.h"
#include "mplayerc.h"

IMPLEMENT_DYNAMIC(CMPCThemeListBox, CListBox)

CMPCThemeListBox::CMPCThemeListBox()
{
    themedToolTipCid = (UINT_PTR) - 1;
    themedSBHelper = nullptr;
    if (!CMPCThemeUtil::canUseWin10DarkTheme()) {
        themedSBHelper = DEBUG_NEW CMPCThemeScrollBarHelper(this);
    }
}


CMPCThemeListBox::~CMPCThemeListBox()
{
    if (nullptr != themedSBHelper) {
        delete themedSBHelper;
    }
}

BEGIN_MESSAGE_MAP(CMPCThemeListBox, CListBox)
    ON_WM_NCPAINT()
    ON_WM_MOUSEWHEEL()
    ON_WM_TIMER()
    ON_WM_VSCROLL()
    ON_CONTROL_REFLECT_EX(LBN_SELCHANGE, &CMPCThemeListBox::OnLbnSelchange)
    ON_WM_MOUSEMOVE()
    ON_WM_SIZE()
END_MESSAGE_MAP()


void CMPCThemeListBox::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
    CDC dc;
    if (lpDrawItemStruct->itemID == -1) {
        return;
    }
    dc.Attach(lpDrawItemStruct->hDC);

    COLORREF crOldTextColor = dc.GetTextColor();
    COLORREF crOldBkColor = dc.GetBkColor();

    if ((lpDrawItemStruct->itemAction | ODA_SELECT) && (lpDrawItemStruct->itemState & ODS_SELECTED)) {
        dc.SetTextColor(CMPCTheme::TextFGColor);
        dc.SetBkColor(CMPCTheme::ContentSelectedColor);
        dc.FillSolidRect(&lpDrawItemStruct->rcItem, CMPCTheme::ContentSelectedColor);
    } else {
        dc.SetTextColor(CMPCTheme::TextFGColor);
        dc.SetBkColor(CMPCTheme::ContentBGColor);
        dc.FillSolidRect(&lpDrawItemStruct->rcItem, CMPCTheme::ContentBGColor);
    }

    lpDrawItemStruct->rcItem.left += 3;

    CString strText;
    GetText(lpDrawItemStruct->itemID, strText);

    CFont* font = GetFont();
    CFont* pOldFont = dc.SelectObject(font);
    dc.DrawTextW(strText, strText.GetLength(), &lpDrawItemStruct->rcItem, DT_VCENTER | DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

    dc.SetTextColor(crOldTextColor);
    dc.SetBkColor(crOldBkColor);
    dc.SelectObject(pOldFont);

    dc.Detach();
}


void CMPCThemeListBox::OnNcPaint()
{
    if (AppNeedsThemedControls()) {
        if (nullptr != themedSBHelper) {
            themedSBHelper->themedNcPaintWithSB();
        } else {
            CMPCThemeScrollBarHelper::themedNcPaint(this, this);
        }
    } else {
        __super::OnNcPaint();
    }
}

BOOL CMPCThemeListBox::PreTranslateMessage(MSG* pMsg)
{
    if (AppNeedsThemedControls()) {
        themedToolTip.RelayEvent(pMsg);
    }
    return CListBox::PreTranslateMessage(pMsg);
}

void CMPCThemeListBox::PreSubclassWindow()
{
    CListBox::PreSubclassWindow();
    if (AppNeedsThemedControls()) {
        if (CMPCThemeUtil::canUseWin10DarkTheme()) {
            SetWindowTheme(GetSafeHwnd(), L"DarkMode_Explorer", NULL);
        } else {
            SetWindowTheme(GetSafeHwnd(), L"", NULL);
        }
        if (nullptr == themedToolTip.m_hWnd) {
            themedToolTip.Create(this, TTS_ALWAYSTIP);
        }
    }
}


BOOL CMPCThemeListBox::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    CListBox::OnMouseWheel(nFlags, zDelta, pt);
    if (nullptr != themedSBHelper) {
        themedSBHelper->updateScrollInfo();
    }
    ScreenToClient(&pt);
    updateToolTip(pt);
    return TRUE;
}

void CMPCThemeListBox::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    CListBox::OnVScroll(nSBCode, nPos, pScrollBar);
    if (nullptr != themedSBHelper) {
        themedSBHelper->updateScrollInfo();
    }
}

BOOL CMPCThemeListBox::OnLbnSelchange()
{
    if (nullptr != themedSBHelper) {
        themedSBHelper->updateScrollInfo();
    }
    return FALSE; //allow non-reflection handling
}


void CMPCThemeListBox::updateToolTip(CPoint point)
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


void CMPCThemeListBox::OnMouseMove(UINT nFlags, CPoint point)
{
    updateToolTip(point);
}

void CMPCThemeListBox::setIntegralHeight()
{
    CWindowDC dc(this);
    CFont* font = GetFont();
    CFont* pOldFont = dc.SelectObject(font);
    CRect r(0, 0, 99, 99);
    CString test = _T("W");
    dc.DrawText(test, test.GetLength(), &r, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_CALCRECT);
    SetItemHeight(0, r.Height());

    dc.SelectObject(pOldFont);
}

void CMPCThemeListBox::OnSize(UINT nType, int cx, int cy)
{
    CListBox::OnSize(nType, cx, cy);
}

void CMPCThemeListBox::EnsureVisible(int index) {
    CRect r;
    GetClientRect(&r);
    int lbHeight = r.Height();

    int height=0;
    for (int i = index; i >= 0; i--) {
        height += GetItemHeight(i);
        if (height > lbHeight) {
            SetTopIndex(i + 1);
            return;
        }
    }
}
