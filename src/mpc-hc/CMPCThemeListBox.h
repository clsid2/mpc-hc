#pragma once
#include <afxwin.h>
#include "CMPCThemeScrollBar.h"
#include "CMPCThemeToolTipCtrl.h"
#include "CMPCThemeScrollBarRenderer.h"

class CMPCThemeListBox :
    public CListBox, public CMPCThemeScrollBarRenderer
{
    DECLARE_DYNAMIC(CMPCThemeListBox)
private:
    CMPCThemeToolTipCtrl themedToolTip;
    UINT_PTR themedToolTipCid;
protected:
    virtual void PreSubclassWindow();
public:
    CMPCThemeListBox();
    virtual ~CMPCThemeListBox();
    virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
    BOOL PreTranslateMessage(MSG* pMsg);
    LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
    void updateToolTip(CPoint point);
    void setIntegralHeight();
    void EnsureVisible(int index);
    DECLARE_MESSAGE_MAP()
    afx_msg void OnNcPaint();
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnSize(UINT nType, int cx, int cy);
};

