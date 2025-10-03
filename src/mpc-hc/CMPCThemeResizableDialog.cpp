#include "stdafx.h"
#include "CMPCThemeResizableDialog.h"
#include "CMPCTheme.h"
#include "mplayerc.h"

CMPCThemeResizableDialog::CMPCThemeResizableDialog()
{
}

CMPCThemeResizableDialog::CMPCThemeResizableDialog(UINT nIDTemplate, CWnd* pParent): CResizableDialog(nIDTemplate, pParent)
{
}

CMPCThemeResizableDialog::CMPCThemeResizableDialog(LPCTSTR lpszTemplateName, CWnd* pParent): CResizableDialog(lpszTemplateName, pParent)
{
}


CMPCThemeResizableDialog::~CMPCThemeResizableDialog()
{
}

BOOL CMPCThemeResizableDialog::OnInitDialog() {
    BOOL ret = CResizableDialog::OnInitDialog();
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

void CMPCThemeResizableDialog::EnableSaveRestoreKey(LPCTSTR pszKey, BOOL bRectOnly) {
    SetStateStore(pszKey);
    EnableSaveRestore(L"", bRectOnly);
}

BEGIN_MESSAGE_MAP(CMPCThemeResizableDialog, CResizableDialog)
    ON_WM_CTLCOLOR()
    ON_MESSAGE_VOID(WM_KICKIDLE, OnKickIdle) //code ported from CmdUI.h to support official CResizableDialog
    ON_WM_INITMENUPOPUP()                    //code ported from CmdUI.h to support official CResizableDialog
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

//code ported from CmdUI.h to support official CResizableDialog
LRESULT CMPCThemeResizableDialog::DefWindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    LRESULT ret = __super::DefWindowProc(message, wParam, lParam);

    if (message == WM_INITDIALOG) {
        SendMessage(WM_KICKIDLE);
    }

    return ret;
}

// CCmdUIDialog message handlers

void CMPCThemeResizableDialog::OnKickIdle() {
    UpdateDialogControls(this, false);

    // TODO: maybe we should send this call to modeless child cdialogs too
}

// Q242577

void CMPCThemeResizableDialog::OnInitMenuPopup(CMenu* pPopupMenu, UINT /*nIndex*/, BOOL /*bSysMenu*/) {
    ASSERT(pPopupMenu != nullptr);
    // Check the enabled state of various menu items.

    CCmdUI state;
    state.m_pMenu = pPopupMenu;
    ASSERT(state.m_pOther == nullptr);
    ASSERT(state.m_pParentMenu == nullptr);

    // Determine if menu is popup in top-level menu and set m_pOther to
    // it if so (m_pParentMenu == nullptr) indicates that it is secondary popup.
    if (AfxGetThreadState()->m_hTrackingMenu == pPopupMenu->m_hMenu) {
        state.m_pParentMenu = pPopupMenu;    // Parent == child for tracking popup.
    } else if (::GetMenu(m_hWnd) != nullptr) {
        HMENU hParentMenu;
        CWnd* pParent = this;
        // Child windows don't have menus--need to go to the top!
        if (pParent != nullptr &&
            (hParentMenu = ::GetMenu(pParent->m_hWnd)) != nullptr) {
            int nIndexMax = ::GetMenuItemCount(hParentMenu);
            for (int nIndex = 0; nIndex < nIndexMax; nIndex++) {
                if (::GetSubMenu(hParentMenu, nIndex) == pPopupMenu->m_hMenu) {
                    // When popup is found, m_pParentMenu is containing menu.
                    state.m_pParentMenu = CMenu::FromHandle(hParentMenu);
                    break;
                }
            }
        }
    }

    state.m_nIndexMax = pPopupMenu->GetMenuItemCount();
    for (state.m_nIndex = 0; state.m_nIndex < state.m_nIndexMax;
        state.m_nIndex++) {
        state.m_nID = pPopupMenu->GetMenuItemID(state.m_nIndex);
        if (state.m_nID == 0) {
            continue;    // Menu separator or invalid cmd - ignore it.
        }

        ASSERT(state.m_pOther == nullptr);
        ASSERT(state.m_pMenu != nullptr);
        if (state.m_nID == UINT(-1)) {
            // Possibly a popup menu, route to first item of that popup.
            state.m_pSubMenu = pPopupMenu->GetSubMenu(state.m_nIndex);
            if (state.m_pSubMenu == nullptr ||
                (state.m_nID = state.m_pSubMenu->GetMenuItemID(0)) == 0 ||
                state.m_nID == UINT(-1)) {
                continue;       // First item of popup can't be routed to.
            }
            state.DoUpdate(this, TRUE);     // Popups are never auto disabled.
        } else {
            // Normal menu item.
            // Auto enable/disable if frame window has m_bAutoMenuEnable
            // set and command is _not_ a system command.
            state.m_pSubMenu = nullptr;
            state.DoUpdate(this, FALSE);
        }

        // Adjust for menu deletions and additions.
        UINT nCount = pPopupMenu->GetMenuItemCount();
        if (nCount < state.m_nIndexMax) {
            state.m_nIndex -= (state.m_nIndexMax - nCount);
            while (state.m_nIndex < nCount &&
                pPopupMenu->GetMenuItemID(state.m_nIndex) == state.m_nID) {
                state.m_nIndex++;
            }
        }
        state.m_nIndexMax = nCount;
    }
}


void CMPCThemeResizableDialog::OnSize(UINT nType, int cx, int cy) {
    __super::OnSize(nType, cx, cy);
    Invalidate(); //adipose: investigate need for this
}
