#pragma once
#include <afxcmn.h>

class CMPCThemeProgressCtrl : public CProgressCtrl
{
public:
    CMPCThemeProgressCtrl();
    virtual ~CMPCThemeProgressCtrl();

protected:
    DECLARE_MESSAGE_MAP()
    afx_msg void OnPaint();
    afx_msg LRESULT OnSetPos(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnStepIt(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnDpiChanged(WPARAM wParam, LPARAM lParam);
};
