/*
 *	Extended MFC message boxes -- Version 1.2 Lite
 *	Copyright (c) 2004 Michael P. Mehl. All rights reserved.
 *
 *	The contents of this file are subject to the Mozilla Public License
 *	Version 1.1a (the "License"); you may not use this file except in
 *	compliance with the License. You may obtain a copy of the License at 
 *	http://www.mozilla.org/MPL/.
 *
 *	Software distributed under the License is distributed on an "AS IS" basis,
 *	WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 *	for the specific language governing rights and limitations under the
 *	License. 
 *
 *	The Original Code is Copyright (c) 2004 Michael P. Mehl. All rights
 *	reserved. The Initial Developer of the Original Code is Michael P. Mehl
 *	<michael.mehl@web.de>.
 *
 *	Alternatively, the contents of this file may be used under the terms of
 *	the GNU Lesser General Public License Version 2.1 (the "LGPL License"),
 *	in which case the provisions of LGPL License are applicable instead of
 *	those above. If you wish to allow use of your version of this file only
 *	under the terms of the LGPL License and not to allow others to use your
 *	version of this file under the MPL, indicate your decision by deleting
 *	the provisions above and replace them with the notice and other provisions
 *	required by the LGPL License. If you do not delete the provisions above,
 *	a recipient may use your version of this file under either the MPL or
 *	the LGPL License.
 */

/*
MPC-HC
modified from 1.2lite to include code to emulate afxmessagebox styles
modified to use Theme Font
*/

#include "stdafx.h"

#include "MessageBoxDialog.h"
#include "../mpc-hc/CMPCTheme.h"
#include "../mpc-hc/CMPCThemeUtil.h"
#include <uxtheme.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CMessageBoxDialog, CDialog)

//////////////////////////////////////////////////////////////////////////////
// Layout values (in dialog units).

#define CX_BORDER					8		// Width of the border.
#define CY_BORDER					6		// Height of the border.

#define CX_BUTTON					35		// Standard width of a button.
#define CY_BUTTON					10		// Standard height of a button.
#define CY_TEXTPADDING				14		// Space before and after message.
#define CX_BUTTON_BORDER			4		// Standard border for a button.
#define CX_WINDOW_PADDING			2		// Padding for right side of dialog

#define CY_BUTTON_BORDER			1		// Standard border for a button.
#define CX_BUTTON_SPACE				7		// Standard space for a button.

#define CX_DLGUNIT_BASE				1000	// Values used for converting
#define CY_DLGUNIT_BASE				1000	// dialog units to pixels.

//////////////////////////////////////////////////////////////////////////////
// Constructors and destructors of the class.

/**
 *	Constructor of the class.
 *
 *	This constructor is used to provide the strings directly without providing
 *	resource IDs from which these strings should be retrieved. If no title is
 *	given, the application name will be used as the title of the dialog.
 */
 CMessageBoxDialog::CMessageBoxDialog ( CWnd* pParent, CString strMessage, 
	CString strTitle, UINT nStyle, UINT nHelp ) 
{
	// Enable the active accessibility.
#if _MSC_VER >= 1300
	EnableActiveAccessibility();
#endif

	ASSERT(!strMessage.IsEmpty());

	// Save the information about the message box.
	m_pParent			= pParent;
	m_strMessage		= strMessage;
	m_strTitle			= strTitle.IsEmpty() ? CString(AfxGetAppName()) : strTitle;
	m_nStyle			= nStyle;
	m_nHelp				= nHelp;

	// Do the default initialization.
	m_uBeepType			= (UINT)-1;
	m_hIcon				= NULL;
	m_nDefaultButton	= IDC_STATIC;
	m_nEscapeButton		= IDC_STATIC;
	m_sDialogUnit		= CSize(0, 0);
	m_sIcon				= CSize(0, 0);
	m_sMessage			= CSize(0, 0);
	m_sButton			= CSize(0, 0);
}

/**
 *	Constructor of the class.
 *
 *	This constructor is used to load the strings for the title and the message
 *	text from the resources of this project. If no title is given, the
 *	application name will be used as the title of the dialog.
 */
CMessageBoxDialog::CMessageBoxDialog ( CWnd* pParent, UINT nMessageID,
	UINT nTitleID, UINT nStyle, UINT nHelp ) 
{
	// Enable the active accessibility.
#if _MSC_VER >= 1300
	EnableActiveAccessibility();
#endif

	// Check whether a title was given.
	if ( nTitleID == 0 )
	{
		// Use the application name.
		m_strTitle = AfxGetAppName();
	}
	else
	{
		// Try to load the title from the resources.
		VERIFY(m_strTitle.LoadString(nTitleID));
	}

	// Save the information about the message box.
	m_pParent			= pParent;
	VERIFY(m_strMessage.LoadString(nMessageID));
	m_nStyle			= nStyle;
	m_nHelp				= nHelp;

	// Do the default initialization.
	m_uBeepType			= (UINT)-1;
	m_hIcon				= NULL;
	m_nDefaultButton	= IDC_STATIC;
	m_nEscapeButton		= IDC_STATIC;
	m_sDialogUnit		= CSize(0, 0);
	m_sIcon				= CSize(0, 0);
	m_sMessage			= CSize(0, 0);
	m_sButton			= CSize(0, 0);
}

/**
 *	Destructor of the class.
 */
CMessageBoxDialog::~CMessageBoxDialog ( )
{
	// Check whether an icon was loaded.
	if ( m_hIcon != NULL )
	{
		// Free the icon.
		DestroyIcon(m_hIcon);

		// Reset the icon handle.
		m_hIcon = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////////
// Methods for setting and retrieving dialog options.

/**
 *	Method for setting the style of the message box.
 */
inline void CMessageBoxDialog::SetStyle ( UINT nStyle )
{
	// Set the style of the message box.
	m_nStyle = nStyle;
}

/**
 *	Method for retrieving the style of the message box.
 */
inline UINT CMessageBoxDialog::GetStyle ( )
{
	// Return the current style of the message box.
	return m_nStyle;
}

/**
 *	Method for setting the message to be displayed in the message box.
 */
void CMessageBoxDialog::SetMessage ( CString strMessage )
{
	ASSERT(!strMessage.IsEmpty());

	// Save the message text.
	m_strMessage = strMessage;
}

/**
 *	Methods for setting the message to be displayed in the message box.
 */
void CMessageBoxDialog::SetMessage ( UINT nMessageID )
{
	// Create a string for storing the message.
	CString strMessage = _T("");

	// Load the message from the resources.
	VERIFY(strMessage.LoadString(nMessageID));

	ASSERT(!strMessage.IsEmpty());

	// Save the message text.
	m_strMessage = strMessage;
}

/**
 *	Method for retrieving the message to be displayed in the message box.
 */
CString CMessageBoxDialog::GetMessage ( )
{
	// Return the message text.
	return m_strMessage;
}

/**
 *	Method for setting the title to be displayed in the message box.
 */
void CMessageBoxDialog::SetTitle ( CString strTitle )
{
	// Check whether a title was given.
	if ( strTitle.IsEmpty() )
	{
		// Use the application name as the title.
		strTitle = AfxGetAppName();
	}

	// Save the title.
	m_strTitle = strTitle;
}

/**
 *	Method for setting the title to be displayed in the message box.
 */
void CMessageBoxDialog::SetTitle ( UINT nTitleID )
{
	// Create a string for storing the title.
	CString strTitle = _T("");

	// Check whether an ID was given.
	if ( nTitleID == 0 )
	{
		// Use the application name as the title.
		strTitle = AfxGetAppName();
	}
	else
	{
		// Try to load the string from the resources.
		VERIFY(strTitle.LoadString(nTitleID));

		ASSERT(!strTitle.IsEmpty());
	}

	// Save the title.
	m_strTitle = strTitle;
}

/**
 *	Method for retrieving the title to be displayed in the message box.
 */
CString CMessageBoxDialog::GetTitle ( )
{
	// Return the title of the message box.
	return m_strTitle;
}

void CMessageBoxDialog::SetMessageBeep(UINT uType)
{
	m_uBeepType = uType;
}

/**
 *	Method for setting the icon to be displayed in the message box.
 */
void CMessageBoxDialog::SetMessageIcon ( HICON hIcon )
{
	ASSERT(hIcon != NULL);

	// Save the icon.
	m_hIcon = hIcon;
}

/**
 *	Method for setting the icon to be displayed in the message box.
 */
void CMessageBoxDialog::SetMessageIcon ( LPCTSTR lpszIconName, BOOL bStandard )
{
	// Try to load the given icon.
	if (bStandard)
	{
		if (m_uBeepType==-1)
		{
			if (lpszIconName==IDI_QUESTION)
				m_uBeepType = MB_ICONQUESTION;
			else if (lpszIconName==IDI_WARNING || lpszIconName==IDI_EXCLAMATION)
				m_uBeepType = MB_ICONWARNING;
			else if (lpszIconName==IDI_ERROR || lpszIconName==IDI_HAND)
				m_uBeepType = MB_ICONERROR;
			else if (lpszIconName==IDI_INFORMATION || lpszIconName==IDI_ASTERISK)
				m_uBeepType = MB_ICONINFORMATION;
		}
		m_hIcon = AfxGetApp()->LoadStandardIcon(lpszIconName);
	}
	else
		m_hIcon = AfxGetApp()->LoadIcon(lpszIconName);

	ASSERT(m_hIcon != NULL);
}

/**
 *	Method for retrieving the icon to be displayed in the message box.
 */
HICON CMessageBoxDialog::GetMessageIcon ( )
{
	// Return the icon for the message box.
	return m_hIcon;
}

//////////////////////////////////////////////////////////////////////////////
// Methods for handling common window functions.

/**
 *	Method for displaying the dialog.
 *
 *	If the MB_DONT_DISPLAY_AGAIN or MB_DONT_ASK_AGAIN flag is set, this
 *	method will check, whether a former result for this dialog was stored
 *	in the registry. If yes, the former result will be returned without
 *	displaying the dialog. Otherwise the message box will be displayed in
 *	the normal way.
 */
INT_PTR CMessageBoxDialog::DoModal ( )
{
	BYTE abyBuff[512];
    memset(abyBuff, 0, sizeof(abyBuff));
    DLGTEMPLATE* pdlgtmplTemplate = (DLGTEMPLATE*)abyBuff;
    pdlgtmplTemplate->x                  = 0;
    pdlgtmplTemplate->y                  = 0;
    pdlgtmplTemplate->cx              = 200;
    pdlgtmplTemplate->cy              = 200;
    pdlgtmplTemplate->style              = DS_MODALFRAME|WS_CAPTION|WS_VISIBLE|WS_POPUP|WS_SYSMENU|WS_BORDER;
    pdlgtmplTemplate->dwExtendedStyle = 0;
    pdlgtmplTemplate->cdit              = 0;

	//This is the MFC function, which does the job
	InitModalIndirect(pdlgtmplTemplate, m_pParent);
	
	return CDialog::DoModal();
}

/**
 *	Method for initializing the dialog.
 *
 *	This method is used for initializing the dialog. It will create the
 *	content of the dialog, which means it will create all controls and will
 *	size the dialog to fit it's content.
 */
BOOL CMessageBoxDialog::OnInitDialog ( )
{
	// Call the parent method.
	if ( !CDialog::OnInitDialog() )
	{
		// Return with an error.
		return FALSE;
	}

    if (!CMPCThemeUtil::getFontByType(messageFont, this, CMPCThemeUtil::MessageFont)) {
        return FALSE;
    }

	// Set the title of the dialog.
	SetWindowText(m_strTitle);

	// Set the help ID of the dialog.
	SetHelpID(m_nHelp);

	// Parse the style of the message box.
	ParseStyle();

	// Create the elements of the dialog.
	CreateIconControl();
	CreateMessageControl();
	CreateButtonControls();

	// Define the layout of the dialog.
	DefineLayout();

	// Check whether no sound should be generated.
	if ( m_uBeepType != -1 )
	{
		// Do a beep.
		MessageBeep(m_uBeepType);
	}

	// Check whether the window should be system modal.
	if ( m_nStyle & MB_SYSTEMMODAL )
	{
		// Modify the style of the window.
		ModifyStyle(0, DS_SYSMODAL);
	}

	// Check whether to bring the window to the foreground.
	if ( m_nStyle & MB_SETFOREGROUND )
	{
		// Bring the window to the foreground.
		SetForegroundWindow();
	}

	// Check whether the window should be the topmost window.
	if ( m_nStyle & MB_TOPMOST )
	{
		// Modify the style of the window.
		ModifyStyleEx(0, WS_EX_TOPMOST);
	}

	// Check whether an escape button was defined.
	if ( m_nEscapeButton == IDC_STATIC )
	{
		// Disable the close item from the system menu.
        CMenu* sysmenu = GetSystemMenu(FALSE);
        if (sysmenu)
            sysmenu->EnableMenuItem(SC_CLOSE, MF_GRAYED);
	}

	// Check whether a default button was defined.
	if ( m_nDefaultButton != IDC_STATIC )
	{
		// Set the focus to the default button.
        CWnd* item = GetDlgItem(m_nDefaultButton);
        if (item) {
            item->SetFocus();
        }

		// Set the default ID of the dialog.
		SetDefID(m_nDefaultButton);

		// Return FALSE to set the focus correctly.
		return FALSE;
	}

	// Everything seems to be done successfully.
	return TRUE;
}

/**
 *	Method for handling command messages.
 *
 *	This method will handle command messages, which are those messages, which
 *	are generated, when a user clicks a button of the dialog.
 */
BOOL CMessageBoxDialog::OnCmdMsg ( UINT nID, int nCode, void* pExtra,
	AFX_CMDHANDLERINFO* pHandlerInfo )
{
	// Check whether it's the help button.
	if ( ( nID == IDHELP ) && ( nCode == CN_COMMAND ) )
	{
		// Display the help for this message box.
		OnHelp();

		// The message has been processed successfully.
		return TRUE;
	}

	// Check whether the ID of the control element is interesting for us.
	if ( ( nID != IDC_STATIC ) && ( nCode == CN_COMMAND ) )
	{
		// End the dialog with the given ID.
		EndDialog(nID);

		// The message has been processed successfully.
		return TRUE;
	}

	// Call the parent method.
	return CDialog::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

/**
 *	Method for handling messages before dispatching them.
 *
 *	This message will handle message before they get dispatched the normal way
 *	and will therefore implement the additional behavior of this dialog.
 */
BOOL CMessageBoxDialog::PreTranslateMessage ( MSG* pMsg )
{
	// Check whether it's a key message and whether it's not a disable timeout.
	if ( pMsg->message == WM_KEYDOWN )
	{
		// Check whether it's the return key.
		if ( pMsg->wParam == VK_RETURN )
		{
			// Try to retrieve the current focus.
			CWnd* pFocusWnd = GetFocus();

			// Check whether a handle was retrieved.
			if ( pFocusWnd != NULL )
			{
				// Try to determine the ID of the element.
				int nID = pFocusWnd->GetDlgCtrlID();

				// Check whether the ID is a button.
				if ( m_aButtons.FindKey(nID)!=-1 )
				{
					EndDialog(nID);
				}
				else
				{
					// End the dialog with the default command.
					EndDialog(m_nDefaultButton);
				}

				// The message has been processed successfully.
				return TRUE;
			}
		}

		// Check whether it's the escape key.
		if ( ( pMsg->wParam == VK_ESCAPE ) || ( pMsg->wParam == VK_CANCEL ) )
		{
			// Check whether an escape button was defined.
			if ( m_nEscapeButton != IDC_STATIC )
			{
				// End the dialog with this ID.
				EndDialog(m_nEscapeButton);
			}

			// The message has been processed successfully.
			return TRUE;
		}
	}

	// Call the parent method.
	return CDialog::PreTranslateMessage(pMsg);
}

//////////////////////////////////////////////////////////////////////////////
// Other dialog handling methods.

/**
 *	Method for handling window messages.
 */
BOOL CMessageBoxDialog::OnWndMsg ( UINT message, WPARAM wParam, LPARAM lParam,
	LRESULT* pResult )
{
	// Check whether to close the dialog.
	if ( message == WM_CLOSE )
	{
		// Check whether an escape button is defined.
		if ( m_nEscapeButton != IDC_STATIC )
		{
			// End the dialog with this command.
			EndDialog(m_nEscapeButton);
		}

		// The message was handled successfully.
		return TRUE;
	}

	// Call the parent method.
	return CDialog::OnWndMsg(message, wParam, lParam, pResult);
}

//////////////////////////////////////////////////////////////////////////////
// Helper methods.

/**
 *	Method for adding a button to the list of buttons.
 *
 *	This method adds a button to the list of buttons, which will be created in
 *	the dialog, but it will not create the button control itself.
 */
void CMessageBoxDialog::AddButton ( UINT nID, UINT nTitle, BOOL bIsDefault,
	BOOL bIsEscape )
{
	CString strButtonTitle;
	VERIFY(strButtonTitle.LoadString(nTitle));

	AddButton(nID, strButtonTitle, bIsDefault, bIsEscape);
}

/**
 *	Method for adding a button to the list of buttons.
 *
 *	This method adds a button to the list of buttons, which will be created in
 *	the dialog, but it will not create the button control itself.
 */
void CMessageBoxDialog::AddButton ( UINT nID, CString strTitle, BOOL bIsDefault,
	BOOL bIsEscape )
{
	// Check if one is trying to add the same ID twice
	VERIFY( m_aButtons.FindKey(nID)==-1 );

	// Add the button to the list of buttons.
	VERIFY( m_aButtons.Add(nID, strTitle) );

	// Check whether this button is the default button.
	if ( bIsDefault )
	{
		// Save the ID of the button as the ID of the default button.
		m_nDefaultButton = nID;
	}

	// Check whether this button is the escape button.
	if ( bIsEscape )
	{
		// Save the ID of the button as the ID of the escape button.
		m_nEscapeButton = nID;
	}
}

/**
 *	Method for converting a dialog unit x value to a pixel value.
 */
int CMessageBoxDialog::XDialogUnitToPixel ( int x )
{
	// Check whether the dimension of a dialog unit has already been determined.
	if ( m_sDialogUnit.cx == 0 )
	{
		// Create a rect for mapping it to the dialog rect.
		CRect rcDialog(0, 0, CX_DLGUNIT_BASE, CY_DLGUNIT_BASE);

		// Map the rect to the dialog.

        //mpc-hc: CDialog without a template does not work well with MapDialogRect--use MapDialogRectMessageFont
        //MapDialogRect(rcDialog);
        CMPCThemeUtil::MapDialogRectMessageFont(this, rcDialog);

		// Save the rect.
        m_sDialogUnit = rcDialog.Size();
    }

	// Return the converted value.
	return ( MulDiv( x , m_sDialogUnit.cx , CX_DLGUNIT_BASE ));
}

/**
 *	Method for converting a dialog unit y value to a pixel value.
 */
int CMessageBoxDialog::YDialogUnitToPixel ( int y )
{
	// Check whether the dimension of a dialog unit has already been determined.
	if ( m_sDialogUnit.cy == 0 )
	{
		// Create a rect for mapping it to the dialog rect.
		CRect rcDialog(0, 0, CX_DLGUNIT_BASE, CY_DLGUNIT_BASE);

		// Map the rect to the dialog.
        //mpc-hc: CDialog without a template does not work well with MapDialogRect--use MapDialogRectMessageFont
        //MapDialogRect(rcDialog);
        CMPCThemeUtil::MapDialogRectMessageFont(this, rcDialog);

		// Save the rect.
		m_sDialogUnit = rcDialog.Size();
	}

	// Return the converted value.
	return ( MulDiv( y , m_sDialogUnit.cy , CY_DLGUNIT_BASE ) );
}

/**
 *	Method for parsing the given style.
 *
 *	This method will parse the given style for the message box and will create
 *	the elements of the dialog box according to it. If you want to add more
 *	user defined styles, simply modify this method.
 */
void CMessageBoxDialog::ParseStyle ( )
{
	switch ( m_nStyle & MB_TYPEMASK )
	{
		case MB_OKCANCEL:
			AddButton(IDOK, IDS_MESSAGEBOX_OK, TRUE);
			AddButton(IDCANCEL, IDS_CANCEL, FALSE, TRUE);
			break;
		case MB_ABORTRETRYIGNORE:
			AddButton(IDABORT, IDS_MESSAGEBOX_ABORT, TRUE);
			AddButton(IDRETRY, IDS_MESSAGEBOX_RETRY);
			AddButton(IDIGNORE, IDS_MESSAGEBOX_IGNORE);
			break;
		case MB_YESNOCANCEL:
			AddButton(IDYES, IDS_SUBRESYNC_YES, TRUE);
			AddButton(IDNO, IDS_SUBRESYNC_NO);
			AddButton(IDCANCEL, IDS_MESSAGEBOX_CANCEL, FALSE, TRUE);
			break;
		case MB_YESNO:
			AddButton(IDYES, IDS_SUBRESYNC_YES, TRUE);
			AddButton(IDNO, IDS_SUBRESYNC_NO);
			break;
		case MB_RETRYCANCEL:
			AddButton(IDRETRY, IDS_MESSAGEBOX_RETRY, TRUE);
			AddButton(IDCANCEL, IDS_MESSAGEBOX_CANCEL, FALSE, TRUE);
			break;
		case MB_CANCELTRYCONTINUE:
			AddButton(IDCANCEL, IDS_MESSAGEBOX_CANCEL, TRUE, TRUE);
			AddButton(IDTRYAGAIN, IDS_MESSAGEBOX_RETRY);
			AddButton(IDCONTINUE, IDS_MESSAGEBOX_CONTINUE);
			break;
		default:
		case MB_OK:
			AddButton(IDOK, IDS_MESSAGEBOX_OK, TRUE, TRUE);
			break;
	}
	// Check whether a default button was defined.
	if ( m_nStyle & MB_DEFMASK )
	{
		// Create a variable to store the index of the default button.
		int nDefaultIndex = 0;

		// Switch the default button.
		switch ( m_nStyle & MB_DEFMASK )
		{

			case MB_DEFBUTTON1:
				// Set the index of the default button.
				nDefaultIndex = 0;
				break;

			case MB_DEFBUTTON2:
				// Set the index of the default button.
				nDefaultIndex = 1;
				break;

			case MB_DEFBUTTON3:
				// Set the index of the default button.
				nDefaultIndex = 2;
				break;

			case MB_DEFBUTTON4:
				// Set the index of the default button.
				nDefaultIndex = 3;
				break;
		}

		// Check whether enough buttons are available.
		if ( m_aButtons.GetSize() >= ( nDefaultIndex + 1 ) )
		{
			// Set the new default button.
			m_nDefaultButton = m_aButtons.GetKeyAt(nDefaultIndex);
		}
	}

	// Check whether an icon was specified.
	if ( ( m_nStyle & MB_ICONMASK ) && ( m_hIcon == NULL ) )
	{
		// Switch the icon.
		switch ( m_nStyle & MB_ICONMASK )
		{
			case MB_ICONEXCLAMATION:
				// Load the icon with the exclamation mark.
				SetMessageIcon(IDI_EXCLAMATION, TRUE);
				break;

			case MB_ICONHAND:
				// Load the icon with the error symbol.
				SetMessageIcon(IDI_HAND, TRUE);
				break;

			case MB_ICONQUESTION:
				// Load the icon with the question mark.
				SetMessageIcon(IDI_QUESTION, TRUE);
				break;

			case MB_ICONASTERISK:
				// Load the icon with the information symbol.
				SetMessageIcon(IDI_ASTERISK, TRUE);
				break;
		}
	}
}

/**
 *	Method for creating the icon control.
 *
 *	This method will check whether the handle for an icon was defined and if
 *	yes it will create an control in the dialog to display that icon.
 */
void CMessageBoxDialog::CreateIconControl ( )
{
	// Check whether an icon was defined.
	if ( m_hIcon != NULL )
	{
		// Create a structure to read information about the icon.
		ICONINFO iiIconInfo;

		// Retrieve information about the icon.
		GetIconInfo(m_hIcon, &iiIconInfo);

		ASSERT(iiIconInfo.fIcon);

		// Create a handle to access the bitmap information of the icon.
		BITMAP bmIcon;

		// Retrieve the bitmap information of the icon.
		GetObject((HGDIOBJ)iiIconInfo.hbmColor, sizeof(bmIcon), &bmIcon);

		// Save the size of the icon.
		m_sIcon.cx = bmIcon.bmWidth;
		m_sIcon.cy = bmIcon.bmHeight;

		// Create a dummy rect for the icon control.
		CRect rcDummy;

		// Create the control for the icon.
		m_stcIcon.Create(NULL, WS_CHILD | WS_VISIBLE | WS_DISABLED | SS_ICON,
			rcDummy, this, (UINT)IDC_STATIC);

		// Set the icon of the control.
		m_stcIcon.SetIcon(m_hIcon);
        //m_stcIcon.UnsubclassWindow();
	}
}

/**
 *	Method for creating the text control.
 *
 *	This method create the control displaying the text of the message for the
 *	message box. It will also try to determine the size required for the
 *	message.
 */
void CMessageBoxDialog::CreateMessageControl ( )
{
	ASSERT(!m_strMessage.IsEmpty());

	// Create a DC for accessing the display driver.
	CDC dcDisplay;
	dcDisplay.CreateDC(_T("DISPLAY"), NULL, NULL, NULL);

	// Select the new font and store the old one.
    //mpc-hc use stored font for metrics
     //CFont* pOldFont = dcDisplay.SelectObject(GetFont());
    CFont* pOldFont = dcDisplay.SelectObject(&messageFont);

	// Define the maximum width of the message.
    int nMaxWidth = GetSystemMetrics(SM_CXSCREEN) - 2 * XDialogUnitToPixel(CX_BORDER);
    int minClientHeightExcludingMessage = 2 * YDialogUnitToPixel(CY_TEXTPADDING) + 2 * YDialogUnitToPixel(CY_BORDER) + YDialogUnitToPixel(CY_BUTTON) + 1;
    int yPadding = GetThemeSysSize(NULL, SM_CYSIZE) + GetThemeSysSize(NULL, SM_CXPADDEDBORDER) * 2 + minClientHeightExcludingMessage;
    int nMaxHeight = GetSystemMetrics(SM_CYSCREEN) - yPadding;

	// Check whether an icon is displayed.
	if ( m_hIcon != NULL )
	{
		// Decrease the maximum width.
		nMaxWidth -= m_sIcon.cx + XDialogUnitToPixel(CX_BORDER);
	}

	// Create a rect with the maximum width.
    //mpc-hc try to follow windows guidlines for messagebox sizes per https://devblogs.microsoft.com/oldnewthing/20110624-00/?p=10343
    int bestWidth = 99999;
    CRect rcMessage(0, 0, nMaxWidth, nMaxHeight);
    m_sMessage = rcMessage.Size();

    for (int widthType = 0; widthType < 5; widthType++) {
        int testWidth;
        switch (widthType) {
        case 0:
            testWidth = GetSystemMetrics(SM_CXSCREEN);
            break;
        case 1:
            testWidth = XDialogUnitToPixel(278);
            break;
        case 2:
            testWidth = MulDiv(GetSystemMetrics(SM_CXSCREEN), 5, 8);
            break;
        case 3:
            testWidth = MulDiv(GetSystemMetrics(SM_CXSCREEN), 3, 4);
            break;
        case 4:
        default:
            testWidth = MulDiv(GetSystemMetrics(SM_CXSCREEN), 7, 8);
            break;
        }
        rcMessage = CRect(0, 0, testWidth, nMaxHeight);
        dcDisplay.DrawText(m_strMessage, rcMessage, DT_LEFT | DT_NOPREFIX | DT_WORDBREAK | DT_CALCRECT);
        if (rcMessage.Width() <= nMaxWidth && rcMessage.Height() <= nMaxHeight) {
            if (testWidth < bestWidth) {
                m_sMessage = rcMessage.Size();
                bestWidth = testWidth;
            }
        }
    }

	// Select the old font again.
	dcDisplay.SelectObject(pOldFont);

	// Create a dummy rect for the control.
	CRect rcDummy;

	// Create a variable with the style of the control.
	DWORD dwStyle = WS_CHILD | WS_VISIBLE;

	// Check whether the text should be right aligned.
	if ( m_nStyle & MB_RIGHT )
	{
		// Align the text to the right.
		dwStyle |= SS_RIGHT;
	}
	else
	{
		// Align the text to the left.
		dwStyle |= SS_LEFT;
	}

	// Create the static control for the message.
	m_stcMessage.Create(m_strMessage,  dwStyle, rcDummy, this, 
		(UINT)IDC_STATIC);

	// Check whether the text will be read from right to left.
	if ( m_nStyle & MB_RTLREADING )
	{
		// Change the extended style of the control.
		m_stcMessage.ModifyStyleEx(0, WS_EX_RTLREADING);
	}

	// Set the font of the dialog.
	m_stcMessage.SetFont(&messageFont);
}

/**
 *	Method for creating the button controls.
 *
 *	According to the list of buttons, which should be displayed in this
 *	message box, this method will create them and add them to the dialog.
 */
void CMessageBoxDialog::CreateButtonControls ( )
{
	// Initialize the control with the size of the button.
	m_sButton = CSize(XDialogUnitToPixel(CX_BUTTON) + 1,YDialogUnitToPixel(CY_BUTTON) + 1);

	// Create a handle to access the DC of the dialog.
	CClientDC dc(this);

	// Retrieve the font for this dialog and select it.
	CFont* pWndFont = GetFont();
	CFont* pOldFont = dc.SelectObject(pWndFont);

	// Create a dummy rect.
	CRect rcDummy;

	// Run through all buttons defined in the list of the buttons.
	for ( int i = 0; i < m_aButtons.GetSize(); i++ )
	{
		// Create a string and load the title of the button.
		CString strButtonTitle = m_aButtons.GetValueAt(i);

		// Retrieve the size of the text.
		CSize sButtonText = dc.GetTextExtent(strButtonTitle);

		// Resize the button.
		m_sButton.cx = std::max(m_sButton.cx, sButtonText.cx);
		m_sButton.cy = std::max(m_sButton.cy, sButtonText.cy);

		// Create a new handle for creating a button control.
		CButton btnControl;

		// Create the button.
		btnControl.Create(strButtonTitle, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
			rcDummy, this, m_aButtons.GetKeyAt(i));

		// Set the font of the control.
		btnControl.SetFont(&messageFont);

		// Remove the subclassing again.
		btnControl.UnsubclassWindow();
	}

	// Add margins to the button size.
	m_sButton.cx += 2 * XDialogUnitToPixel(CX_BUTTON_BORDER);
	m_sButton.cy += 2 * XDialogUnitToPixel(CY_BUTTON_BORDER);

	// Select the old font again.
	dc.SelectObject(pOldFont);
}

/**
 *	Method for defining the layout of the dialog.
 *
 *	This method will define the actual layout of the dialog. This layout is
 *	based on the created controls for the dialog.
 */
void CMessageBoxDialog::DefineLayout ( )
{
	// Create a variable for storing the size of the dialog.
    CSize sClient = CSize(2 * XDialogUnitToPixel(CX_BORDER),
		2 * YDialogUnitToPixel(CY_BORDER));

	// Create a variable to store the left position for a control element.
	int nXPosition = XDialogUnitToPixel(CX_BORDER);
    long messageAreaHeight = m_sMessage.cy + 2 * YDialogUnitToPixel(CY_TEXTPADDING);
	int nYPosition = YDialogUnitToPixel(CY_TEXTPADDING);

	// Check whether an icon is defined.
	if ( m_hIcon != NULL && ::IsWindow(m_stcIcon))
	{
		// Move the icon control.
		m_stcIcon.MoveWindow(XDialogUnitToPixel(CX_BORDER), YDialogUnitToPixel(CY_BORDER), m_sIcon.cx, m_sIcon.cy);

		// Add the size of the icon to the size of the dialog.
		sClient.cx += m_sIcon.cx + XDialogUnitToPixel(CX_BORDER);
		sClient.cy += m_sIcon.cy + YDialogUnitToPixel(CY_BORDER);

		// Increase the x position for following control elements.
		nXPosition += m_sIcon.cx + XDialogUnitToPixel(CX_BORDER);
	}

	// Change the size of the dialog according to the size of the message.
	sClient.cx += m_sMessage.cx + XDialogUnitToPixel(CX_BORDER);
	sClient.cy = std::max(sClient.cy, messageAreaHeight);

	// Set the position of the message text.
	m_stcMessage.MoveWindow(nXPosition, nYPosition, m_sMessage.cx,
		m_sMessage.cy);

	// Define the new y position.
	nYPosition = messageAreaHeight;

	// Calculate the width of the buttons.
	int cxButtons =
		( m_aButtons.GetSize() - 1 ) * XDialogUnitToPixel(CX_BUTTON_SPACE) +
		m_aButtons.GetSize() * m_sButton.cx;
	int cyButtons = m_sButton.cy;

	// Add the size of the buttons to the dialog.
	sClient.cx = std::max((LONG)sClient.cx, (LONG)2 * XDialogUnitToPixel(CX_BORDER) + cxButtons);
	sClient.cy += cyButtons + 2 * YDialogUnitToPixel(CY_BORDER);

	// Calculate the start y position for the buttons.
    //mpc-hc align right instead of center
    //int nXButtonPosition = (sClient.cx - cxButtons) / 2;
    int nXButtonPosition = sClient.cx - cxButtons - XDialogUnitToPixel(CX_BORDER + CX_WINDOW_PADDING);
	int nYButtonPosition = sClient.cy - YDialogUnitToPixel(CY_BORDER) - m_sButton.cy;

    buttonAreaY = nYPosition;

	// Run through all buttons.
	for ( int i = 0; i < m_aButtons.GetSize(); i++ )
	{
		// Try to retrieve the handle to access the button.
		CWnd* pButton = GetDlgItem(m_aButtons.GetKeyAt(i));

		ASSERT(pButton);

		// Check whether the handle was retrieved successfully.
		if ( pButton != NULL )
		{
			// Move the button.
			pButton->MoveWindow(nXButtonPosition, nYButtonPosition, 
				m_sButton.cx, m_sButton.cy);

			// Set the new x position of the next button.
			nXButtonPosition += m_sButton.cx + 
				XDialogUnitToPixel(CX_BUTTON_SPACE);
		}
	}

	// Set the dimensions of the dialog.
	CRect rcClient(0, 0, sClient.cx, sClient.cy);

	// Calculate the window rect.
	CalcWindowRect(rcClient);

	// Move the window.
	MoveWindow(rcClient);

	// Center the window.
	CenterWindow();
}
