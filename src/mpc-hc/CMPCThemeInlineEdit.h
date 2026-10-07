#pragma once
#include <afxwin.h>
class CMPCThemeInlineEdit :
    public CEdit
{
public:
    CMPCThemeInlineEdit();
    virtual ~CMPCThemeInlineEdit();
    CBrush m_brBkgnd;
    void setOverridePos(int x, int maxWidth);
    DECLARE_MESSAGE_MAP()
    afx_msg HBRUSH CtlColor(CDC* /*pDC*/, UINT /*nCtlColor*/);
    afx_msg void OnWindowPosChanged(WINDOWPOS* lpwndpos);
    afx_msg void OnPaint();
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
private:
    int overrideX, overrideMaxWidth;
    bool offsetEnabled;
};
