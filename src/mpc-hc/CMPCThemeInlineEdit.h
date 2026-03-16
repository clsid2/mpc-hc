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
    void suppressEndEdit() { m_bEndingEdit = true; }
    DECLARE_MESSAGE_MAP()
    afx_msg void OnNcPaint();
    afx_msg HBRUSH CtlColor(CDC* /*pDC*/, UINT /*nCtlColor*/);
    afx_msg void OnWindowPosChanged(WINDOWPOS* lpwndpos);
    afx_msg void OnKillFocus(CWnd* pNewWnd);
private:
    int overrideX, overrideMaxWidth;
    bool offsetEnabled;
    bool m_bEndingEdit;
public:
    afx_msg void OnPaint();
};

