#include "stdafx.h"
#include "CMPCThemeModelessResizableDialog.h"
#include "CMPCTheme.h"
#include "mplayerc.h"

CMPCThemeModelessResizableDialog::CMPCThemeModelessResizableDialog(UINT nIDTemplate, CWnd* pParent)
    : CModelessResizableDialog(nIDTemplate, pParent)
{
}

BOOL CMPCThemeModelessResizableDialog::OnInitDialog()
{
    BOOL ret = __super::OnInitDialog();
    CMPCThemeUtil::enableWindows10DarkFrame(this);
    return ret;
}

void CMPCThemeModelessResizableDialog::fulfillThemeReqs()
{
    if (AppNeedsThemedControls()) {
        CMPCThemeUtil::enableWindows10DarkFrame(this);
        SetSizeGripBkMode(TRANSPARENT);
        CMPCThemeUtil::fulfillThemeReqs((CWnd*)this);
    }
}

BEGIN_MESSAGE_MAP(CMPCThemeModelessResizableDialog, CModelessResizableDialog)
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

HBRUSH CMPCThemeModelessResizableDialog::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    if (AppIsThemeLoaded()) {
        return getCtlColor(pDC, pWnd, nCtlColor);
    } else {
        HBRUSH hbr = __super::OnCtlColor(pDC, pWnd, nCtlColor);
        return hbr;
    }
}
