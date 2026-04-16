#pragma once
#include "DpiAwareResizableDialog.h"
class CModelessResizableDialog : public CDpiAwareResizableDialog {
public:
    CModelessResizableDialog(UINT nIDTemplate, CWnd* pParent);
    virtual BOOL Create(UINT nIDTemplate, CWnd* pParentWnd);
    void HideDialog(INT_PTR ret);
    DECLARE_MESSAGE_MAP()
protected:
    virtual void OnOK();
    virtual void OnCancel();
};

