#pragma once
#include <afxext.h>
class CMPCThemeStatusBar :
    public CStatusBar
{
public:
    CMPCThemeStatusBar();
    virtual ~CMPCThemeStatusBar();
    void PreSubclassWindow();
    void SetText(LPCTSTR lpszText, int nPane, int nType);
    BOOL SetParts(int nParts, int* pWidths);
    int GetParts(int nParts, int* pParts);
    BOOL GetRect(int nPane, LPRECT lpRect);
    void SetProgressBar(CWnd* pProgress, int nPane);
    DECLARE_MESSAGE_MAP()
protected:
    std::map<int, CString> texts;
    int numParts;
    CWnd* m_pProgressBar;
    int m_nProgressPane;
    void UpdateProgressBarLayout();
public:
    afx_msg void OnNcPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnPaint();
    afx_msg void OnSize(UINT nType, int cx, int cy);
};

