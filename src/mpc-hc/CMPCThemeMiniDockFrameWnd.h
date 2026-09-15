#pragma once
#include <afxpriv.h>

class CMPCThemeMiniDockFrameWnd:
    CMiniDockFrameWnd
{
    DECLARE_DYNCREATE(CMPCThemeMiniDockFrameWnd)
public:
    DECLARE_MESSAGE_MAP()
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg LRESULT OnDpiChanged(WPARAM wParam, LPARAM lParam);
    virtual void CalcWindowRect(LPRECT lpClientRect, UINT nAdjustType = adjustBorder) override;
};

