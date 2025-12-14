#include "stdafx.h"
#include "CMPCThemeResizableDialog.h"
#include "CMPCTheme.h"
#include "mplayerc.h"

CMPCThemeResizableDialog::CMPCThemeResizableDialog()
    : CDpiAwareResizableDialog()
{
}

CMPCThemeResizableDialog::CMPCThemeResizableDialog(UINT nIDTemplate, CWnd* pParent)
    : CDpiAwareResizableDialog(nIDTemplate, pParent)
{
}

CMPCThemeResizableDialog::CMPCThemeResizableDialog(LPCTSTR lpszTemplateName, CWnd* pParent)
    : CDpiAwareResizableDialog(lpszTemplateName, pParent)
{
}

CMPCThemeResizableDialog::~CMPCThemeResizableDialog()
{
}

BOOL CMPCThemeResizableDialog::OnInitDialog() {
    BOOL ret = __super::OnInitDialog();
    CMPCThemeUtil::enableWindows10DarkFrame(this);
    return ret;
}

void CMPCThemeResizableDialog::fulfillThemeReqs()
{
    if (AppNeedsThemedControls()) {
        CMPCThemeUtil::enableWindows10DarkFrame(this);
        SetSizeGripBkMode(TRANSPARENT); //fix for gripper in mpc theme
        CMPCThemeUtil::fulfillThemeReqs((CWnd*)this);
    }
}

BEGIN_MESSAGE_MAP(CMPCThemeResizableDialog, CDpiAwareResizableDialog)
    ON_WM_CTLCOLOR()
    ON_WM_SIZE()
END_MESSAGE_MAP()

HBRUSH CMPCThemeResizableDialog::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    if (AppIsThemeLoaded()) {
        return getCtlColor(pDC, pWnd, nCtlColor);
    } else {
        HBRUSH hbr = __super::OnCtlColor(pDC, pWnd, nCtlColor);
        return hbr;
    }
}

void CMPCThemeResizableDialog::OnSize(UINT nType, int cx, int cy) {
    __super::OnSize(nType, cx, cy);
    Invalidate(); //adipose: investigate need for this
}
