#pragma once
#include "CMPCThemeButton.h"
#include "CMPCThemeGroupBox.h"
#include "CMPCThemeLinkCtrl.h"
#include "CMPCThemeUtil.h"
class CMPCThemeResizableDialog : public CResizableDialog, public CMPCThemeUtil
{
public:
    CMPCThemeResizableDialog();
    CMPCThemeResizableDialog(UINT nIDTemplate, CWnd* pParent = nullptr);
    CMPCThemeResizableDialog(LPCTSTR lpszTemplateName, CWnd* pParent = nullptr);
    virtual ~CMPCThemeResizableDialog();
    BOOL OnInitDialog();
    void fulfillThemeReqs();
protected:
    void EnableSaveRestoreKey(LPCTSTR pszKey, BOOL bRectOnly = FALSE);
    DECLARE_MESSAGE_MAP()
public:
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);


/* code ported from CmdUI.h to support official CResizableDialog \/  */
protected:
    virtual LRESULT DefWindowProc(UINT message, WPARAM wParam, LPARAM lParam);
public:
    afx_msg void OnKickIdle();
    afx_msg void OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu);

/* code ported from CmdUI.h to support official CResizableDialog /\ */
    afx_msg void OnSize(UINT nType, int cx, int cy);
};

