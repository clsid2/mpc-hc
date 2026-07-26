#include "stdafx.h"
#include "CMPCThemeMsgBox.h"
#include "CMPCTheme.h"
#include "mplayerc.h"


CMPCThemeMsgBox::CMPCThemeMsgBox(CWnd* pParent, CString strMessage, CString strTitle, UINT nStyle, UINT nHelp)
    : CMessageBoxDialog(pParent, strMessage, strTitle, nStyle, nHelp)
{
}

CMPCThemeMsgBox::CMPCThemeMsgBox(CWnd* pParent, UINT nMessageID, UINT nTitleID, UINT nStyle, UINT nHelp)
    : CMessageBoxDialog(pParent, nMessageID, nTitleID, nStyle, nHelp)
{
}

IMPLEMENT_DYNAMIC(CMPCThemeMsgBox, CMessageBoxDialog)

CMPCThemeMsgBox::~CMPCThemeMsgBox()
{
}

BOOL CMPCThemeMsgBox::OnInitDialog()
{
    BOOL ret = __super::OnInitDialog();
    fulfillThemeReqs();
    CMPCThemeUtil::enableWindows10DarkFrame(this);
    return ret;
}

BEGIN_MESSAGE_MAP(CMPCThemeMsgBox, CMessageBoxDialog)
    ON_WM_CTLCOLOR()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()


HBRUSH CMPCThemeMsgBox::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    if (AppNeedsThemedControls()) {
        return getCtlColor(pDC, pWnd, nCtlColor);
    } else {
        HBRUSH hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
        return hbr;
    }
}


BOOL CMPCThemeMsgBox::OnEraseBkgnd(CDC* pDC)
{
    if (AppNeedsThemedControls()) {
        CRect rect, messageArea, buttonArea;
        GetClientRect(&rect);
        messageArea = rect;
        buttonArea = rect;
        messageArea.bottom = buttonAreaY;
        buttonArea.top = buttonAreaY;
        pDC->FillSolidRect(messageArea, CMPCTheme::WindowBGColor);
        pDC->FillSolidRect(buttonArea, CMPCTheme::StatusBarBGColor);
        return TRUE;
    } else {
        return __super::OnEraseBkgnd(pDC);
    }
}

BOOL CMPCThemeMsgBox::MessageBoxW(CWnd* parent, LPCWSTR lpText)
{
    return CMPCThemeMsgBox::MessageBox(parent, lpText, NULL, MB_OK);
}

BOOL CMPCThemeMsgBox::MessageBoxW(CWnd* parent, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType)
{
    if (AppNeedsThemedControls()) {
        CMPCThemeMsgBox dlgMessage(parent, lpText, lpCaption, uType, NULL);
        return (BOOL)dlgMessage.DoModal();
    } else {
        if (parent) {
            return parent->MessageBoxW(lpText, lpCaption, uType);
        } else {
            return ::MessageBoxW(NULL, lpText, lpCaption, uType);
        }
    }
}
