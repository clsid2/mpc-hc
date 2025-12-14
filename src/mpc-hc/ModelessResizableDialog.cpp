#include "stdafx.h"
#include "ModelessResizableDialog.h"
#include "SysVersion.h"

BEGIN_MESSAGE_MAP(CModelessResizableDialog, CDpiAwareResizableDialog)
END_MESSAGE_MAP()

CModelessResizableDialog::CModelessResizableDialog(UINT nIDTemplate, CWnd* pParent) : CDpiAwareResizableDialog(nIDTemplate, pParent) {
}

BOOL CModelessResizableDialog::Create(UINT nIDTemplate, CWnd* pParentWnd) {
    return CDialog::Create(nIDTemplate, pParentWnd);
}

void CModelessResizableDialog::HideDialog(INT_PTR ret) {
    //EndDialog is ok because it doesn't destroy modeless dialogs, just hides them
    if (SysVersion::IsWin10orLater()) {
        //windows 11 bug with peek preview--shows hidden dialogs.  temporarily flag as tool window which is not a taskbar eligble window
        ModifyStyleEx(0, WS_EX_TOOLWINDOW);
        EndDialog(ret);
        //remove bogus style so it renders properly next time
        ModifyStyleEx(WS_EX_TOOLWINDOW, 0);
    } else {
        EndDialog(ret);
    }
}

void CModelessResizableDialog::OnOK() {
    HideDialog(IDOK);
}

void CModelessResizableDialog::OnCancel() {
    HideDialog(IDCANCEL);
}
