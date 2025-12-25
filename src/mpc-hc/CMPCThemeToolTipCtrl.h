#pragma once
#include <afxcmn.h>

class CMPCThemeToolTipCtrl;


class CMPCThemeToolTipCtrl : public CToolTipCtrl
{
    DECLARE_DYNAMIC(CMPCThemeToolTipCtrl)
private:
    CRect lastDrawRect;
public:
    CMPCThemeToolTipCtrl();
    virtual ~CMPCThemeToolTipCtrl();
    static void drawText(CDC& dc, CMPCThemeToolTipCtrl* tt, CRect& rect, bool calcRect = false);
    static void paintTT(CDC& dc, CMPCThemeToolTipCtrl* tt);
    void SetHoverPosition(CWnd* parent);
    void RedrawIfVisible();
    DECLARE_MESSAGE_MAP()
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnWindowPosChanging(WINDOWPOS* lpwndpos);
};

