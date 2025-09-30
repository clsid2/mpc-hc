#pragma once
#include <afxcmn.h>

class CMPCThemePlayerListCtrl;

class CMPCThemeHeaderCtrl :
    public CHeaderCtrl
{
    DECLARE_DYNAMIC(CMPCThemeHeaderCtrl)
protected:
    int hotItem;
    bool colDrag = false;
    CMPCThemePlayerListCtrl* parent;
    void checkHot(CPoint point, bool invalidate);
    void drawSortArrow(CDC* dc, COLORREF arrowClr, CRect arrowRect, bool ascending);
    void drawItem(int nItem, CRect rText, CDC* pDC);
public:
    CMPCThemeHeaderCtrl();
    virtual ~CMPCThemeHeaderCtrl();
    void DrawAllItems(CDC* pDC, CPoint offset, const CRect& clipRect);
    void SetParent(CMPCThemePlayerListCtrl* parent) { this->parent = parent; }

    DECLARE_MESSAGE_MAP()
    afx_msg void OnNMCustomdraw(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnHdnTrack(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnMouseLeave();
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg BOOL OnHdnBegintrack(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg BOOL OnHdnEndtrack(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnWindowPosChanging(WINDOWPOS* lpwndpos);
};



