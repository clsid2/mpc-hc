#pragma once
#include "DpiAwareResizableDialog.h"
#include "CMPCThemeButton.h"
#include "CMPCThemeGroupBox.h"
#include "CMPCThemeLinkCtrl.h"
#include "CMPCThemeUtil.h"

class CMPCThemeResizableDialog : public CDpiAwareResizableDialog, public CMPCThemeUtil
{
public:
    CMPCThemeResizableDialog();
    CMPCThemeResizableDialog(UINT nIDTemplate, CWnd* pParent = nullptr);
    CMPCThemeResizableDialog(LPCTSTR lpszTemplateName, CWnd* pParent = nullptr);
    virtual ~CMPCThemeResizableDialog();
    BOOL OnInitDialog();
    void fulfillThemeReqs();
protected:
    DECLARE_MESSAGE_MAP()
public:
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    afx_msg void OnSize(UINT nType, int cx, int cy);
};

