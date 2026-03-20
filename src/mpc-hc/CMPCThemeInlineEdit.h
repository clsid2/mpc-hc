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
    afx_msg void OnNcPaint();
    afx_msg HBRUSH CtlColor(CDC* /*pDC*/, UINT /*nCtlColor*/);
    afx_msg void OnWindowPosChanged(WINDOWPOS* lpwndpos);
private:
    int overrideX, overrideMaxWidth;
    bool offsetEnabled;
public:
    afx_msg void OnPaint();
};

