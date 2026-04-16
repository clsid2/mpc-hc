#pragma once
#include <afxwin.h>
class CMPCThemeGroupBox :  public CButton
{
    DECLARE_DYNAMIC(CMPCThemeGroupBox)
public:
    CMPCThemeGroupBox();
    virtual ~CMPCThemeGroupBox();
    DECLARE_MESSAGE_MAP()
    afx_msg void OnPaint();
    afx_msg void OnEnable(BOOL bEnable);
};

