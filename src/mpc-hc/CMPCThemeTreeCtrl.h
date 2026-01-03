#pragma once
#include <afxcmn.h>
#include "CMPCThemeScrollBarRenderer.h"
#include "CMPCThemeToolTipCtrl.h"

class CMPCThemeTreeCtrl : public CTreeCtrl
    , public CMPCThemeScrollBarRenderer
{
public:
    CMPCThemeTreeCtrl();
    virtual ~CMPCThemeTreeCtrl();
    BOOL PreCreateWindow(CREATESTRUCT& cs);
    void fulfillThemeReqs();
    LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
    void updateToolTip(CPoint point);
    BOOL PreTranslateMessage(MSG* pMsg);
    DECLARE_DYNAMIC(CMPCThemeTreeCtrl)
    DECLARE_MESSAGE_MAP()
    afx_msg void OnNMCustomdraw(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnNcPaint();
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
protected:
    CBrush m_brBkgnd;
    CFont font;
    CMPCThemeToolTipCtrl themedToolTip, tvsTooltip;
    UINT_PTR themedToolTipCid;
    void doEraseBkgnd(CDC* pDC);
};

