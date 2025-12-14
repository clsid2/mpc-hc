#pragma once
#include "ModelessResizableDialog.h"
#include "CMPCThemeUtil.h"

class CMPCThemeModelessResizableDialog : public CModelessResizableDialog, public CMPCThemeUtil {
public:
    CMPCThemeModelessResizableDialog(UINT nIDTemplate, CWnd* pParent);
    BOOL OnInitDialog();
    void fulfillThemeReqs();
    DECLARE_MESSAGE_MAP()
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
};
