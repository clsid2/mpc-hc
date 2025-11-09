/*
* (C) 2014-2017 see Authors.txt
*
* This file is part of MPC-HC.
*
* MPC-HC is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 3 of the License, or
* (at your option) any later version.
*
* MPC-HC is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*/

#include "stdafx.h"
#include "PPageToolBarLayout.h"
#include "mplayerc.h"
#include "MainFrm.h"
#include "AddCommandDlg.h"


IMPLEMENT_DYNAMIC(CPPageToolBarLayout, CMPCThemePPageBase)

CPPageToolBarLayout::CPPageToolBarLayout()
    : CMPCThemePPageBase(IDD, IDD)
{
}

void CPPageToolBarLayout::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST1, m_list_active);
    DDX_Control(pDX, IDC_LIST2, m_list_inactive);
    DDX_Control(pDX, IDC_BUTTON3, leftButton);
    DDX_Control(pDX, IDC_BUTTON4, rightButton);
    DDX_Control(pDX, IDC_BUTTON5, upButton);
    DDX_Control(pDX, IDC_BUTTON6, downButton);
}

int CPPageToolBarLayout::DescriptiveIcon(int idCommand) {
    CPlayerToolBar& tb = AfxGetMainFrame()->m_wndToolBar;
    auto supportedButtons = tb.GetSupportedSvgButtons();
    int disabledOffset = tb.GetCustomizeButtonImages()->GetImageCount() / 2;

    auto& buttonInfo = supportedButtons[idCommand];
    if (buttonInfo.positionLocked
        || idCommand == ID_BUTTON_FULLSCREEN || idCommand == ID_BUTTON_PLAYLIST) { //for these buttons, inactive state is a better representation of the icon
        return buttonInfo.svgIndex + disabledOffset;
    } else {
        return buttonInfo.svgIndex;
    }
}

void CPPageToolBarLayout::LoadToolBarButtons() {
    m_list_active.DeleteAllItems();
    m_list_inactive.DeleteAllItems();

    CPlayerToolBar& tb = AfxGetMainFrame()->m_wndToolBar;
    CToolBarCtrl& tbctrl = tb.GetToolBarCtrl();

    auto supportedButtons = tb.GetSupportedSvgButtons();
    std::set<int> idsAdded;

    for (int i = 0; i < tbctrl.GetButtonCount(); i++) {
        TBBUTTON button;
        tbctrl.GetButton(i, &button);
        if (button.fsStyle != TBBS_SEPARATOR && supportedButtons.count(button.idCommand)) {
            int index = m_list_active.InsertItem(LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM, i, tb.GetStringFromID(button.idCommand), 0, 0, DescriptiveIcon(button.idCommand), button.idCommand);
            idsAdded.insert(button.idCommand);
        }
    }

    int index = m_list_active.GetItemCount() - 1;
    if (index >= 0) {
        m_list_active.SetItemState(index, LVIS_SELECTED, LVIS_SELECTED);
        m_list_active.SetSelectionMark(index);
    }

    std::map<int, WORD> idsSortedByIndex;
    for (auto &[id, bInfo] : supportedButtons) {
        if (0 == idsAdded.count(id) && bInfo.style != TBBS_SEPARATOR) {
            idsSortedByIndex[bInfo.svgIndex] = id;
        }
    }

    int i = 0;
    for (auto &[index, id] : idsSortedByIndex) {
        auto& bInfo = supportedButtons[id];
        int index = m_list_inactive.InsertItem(LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM, i++, tb.GetStringFromID(id), 0, 0, DescriptiveIcon(id), id);
        if (index == 0) {
            m_list_inactive.SetItemState(index, LVIS_SELECTED, LVIS_SELECTED);
            m_list_inactive.SetSelectionMark(index);
        }
    }
}

void CPPageToolBarLayout::ReloadImageLists() {
    CPlayerToolBar& tb = AfxGetMainFrame()->m_wndToolBar;

    m_list_active.SetImageList(tb.GetCustomizeButtonImages().get(), LVSIL_SMALL);
    m_list_inactive.SetImageList(tb.GetCustomizeButtonImages().get(), LVSIL_SMALL);
}

void SetListCtrlFixedColumn(CListCtrl& listCtrl) {
    CRect clientRect;
    listCtrl.GetClientRect(&clientRect);

    bool hasVScroll = (listCtrl.GetStyle() & WS_VSCROLL);

    int availableWidth = clientRect.Width() - (hasVScroll ? 0 : GetSystemMetrics(SM_CXVSCROLL));

    int columnCount = listCtrl.GetHeaderCtrl()->GetItemCount();
    if (columnCount) {
        listCtrl.SetColumnWidth(0, LVSCW_AUTOSIZE_USEHEADER);
        int headerWidth = listCtrl.GetColumnWidth(0);

        listCtrl.SetColumnWidth(0, LVSCW_AUTOSIZE);
        int contentWidth = listCtrl.GetColumnWidth(0);

        int maxWidth = availableWidth;
        int finalWidth = std::min(std::max(headerWidth, contentWidth), maxWidth);

        listCtrl.SetColumnWidth(0, finalWidth);
    }

    CHeaderCtrl* pHeader = listCtrl.GetHeaderCtrl();
    if (pHeader) {
        DWORD dwStyle = pHeader->GetStyle();
        dwStyle |= HDS_NOSIZING;
        SetWindowLongPtrW(pHeader->m_hWnd, GWL_STYLE, dwStyle);
    }
}

BOOL CPPageToolBarLayout::OnInitDialog()
{
    __super::OnInitDialog();
    auto& s = AfxGetAppSettings();

    CRect brc;
    leftButton.GetWindowRect(brc);
    leftButton.SetWindowTextW(L"");
    rightButton.SetWindowTextW(L"");
    upButton.SetWindowTextW(L"");
    downButton.SetWindowTextW(L"");
    SetMPCThemeButtonIcon(IDC_BUTTON3, { IDF_SVG_ARROW, int(0.7f * brc.Width()), TRANSFORM_FLIP_HORZ });
    SetMPCThemeButtonIcon(IDC_BUTTON4, { IDF_SVG_ARROW, int(0.7f * brc.Width()) });
    SetMPCThemeButtonIcon(IDC_BUTTON5, { IDF_SVG_ARROW, int(0.7f * brc.Width()), TRANSFORM_ROT90CCW });
    SetMPCThemeButtonIcon(IDC_BUTTON6, { IDF_SVG_ARROW, int(0.7f * brc.Width()), TRANSFORM_ROT90CW });

    SetRedraw(FALSE);

    CPlayerToolBar& tb = AfxGetMainFrame()->m_wndToolBar;
    CToolBarCtrl& tbctrl = tb.GetToolBarCtrl();

    m_list_active.SetExtendedStyle(m_list_active.GetExtendedStyle() /* | LVS_EX_FULLROWSELECT */ /*| LVS_EX_DOUBLEBUFFER */ | LVS_EX_INFOTIP | LVS_ICON | LVS_SHAREIMAGELISTS);
    m_list_active.setAdditionalStyles(LVS_EX_FULLROWSELECT);
    CString col(StrRes(IDS_PPAGE_TOOLBAR_CUR_BUTTONS));
    m_list_active.InsertColumn(COL_BUTTON, col, LVCFMT_LEFT);
    m_list_active.SetColumnWidth(COL_BUTTON, LVSCW_AUTOSIZE_USEHEADER);
    m_list_active.setColorInterface(this);


    m_list_inactive.SetExtendedStyle(m_list_inactive.GetExtendedStyle() /* | LVS_EX_FULLROWSELECT */ /*| LVS_EX_DOUBLEBUFFER */ | LVS_EX_INFOTIP | LVS_ICON | LVS_SHAREIMAGELISTS);
    m_list_inactive.setAdditionalStyles(LVS_EX_FULLROWSELECT);
    CString col2(StrRes(IDS_PPAGE_TOOLBAR_AVAIL_BUTTONS));
    m_list_inactive.InsertColumn(COL_BUTTON, col2, LVCFMT_LEFT);
    m_list_inactive.SetColumnWidth(COL_BUTTON, LVSCW_AUTOSIZE_USEHEADER);

    ReloadImageLists();

    LoadToolBarButtons();

    SetListCtrlFixedColumn(m_list_active);
    SetListCtrlFixedColumn(m_list_inactive);

    SetRedraw(TRUE);

    UpdateData(FALSE);

    return TRUE;
}

BOOL CPPageToolBarLayout::OnApply()
{
    UpdateData();
    return __super::OnApply();
}

BEGIN_MESSAGE_MAP(CPPageToolBarLayout, CMPCThemePPageBase)
    ON_NOTIFY(NM_CUSTOMDRAW, IDC_LIST1, OnCustomdrawList)
    ON_BN_CLICKED(IDC_BUTTON3, &CPPageToolBarLayout::MoveLeft)
    ON_BN_CLICKED(IDC_BUTTON4, &CPPageToolBarLayout::MoveRight)
    ON_BN_CLICKED(IDC_BUTTON5, &CPPageToolBarLayout::MoveUp)
    ON_BN_CLICKED(IDC_BUTTON6, &CPPageToolBarLayout::MoveDown)
    ON_BN_CLICKED(IDC_BUTTON1, &CPPageToolBarLayout::DefaultButtons)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON3, OnUpdateLeft)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON4, OnUpdateRight)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON5, OnUpdateUp)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON6, OnUpdateDown)
END_MESSAGE_MAP()

void CPPageToolBarLayout::OnUpdateLeft(CCmdUI* pCmdUI) {
    CPlayerToolBar& tb = AfxGetMainFrame()->m_wndToolBar;
    CToolBarCtrl& tbctrl = tb.GetToolBarCtrl();
    auto supportedButtons = tb.GetSupportedSvgButtons();

    int selectedRowLeft = m_list_active.GetSelectionMark();
    int selectedRowRight = m_list_inactive.GetSelectionMark();

    if (IsValidInsertPos(selectedRowLeft) && -1 != selectedRowRight) {
        pCmdUI->Enable(true);
    } else {
        pCmdUI->Enable(false);
    }
}

bool CPPageToolBarLayout::LeftSelectedButtonLocked() {
    CPlayerToolBar& tb = AfxGetMainFrame()->m_wndToolBar;
    CToolBarCtrl& tbctrl = tb.GetToolBarCtrl();
    auto supportedButtons = tb.GetSupportedSvgButtons();

    int selectedRowLeft = m_list_active.GetSelectionMark();

    bool enable = false;
    if (-1 != selectedRowLeft) {
        int tidCommand = (int)m_list_active.GetItemData(selectedRowLeft);
        if (supportedButtons.count(tidCommand) == 0 || supportedButtons[tidCommand].positionLocked) {
            enable = false;
        } else {
            enable = true;
        }
    }
    return enable;
}

void CPPageToolBarLayout::OnUpdateRight(CCmdUI* pCmdUI) {
    pCmdUI->Enable(LeftSelectedButtonLocked());
}

void CPPageToolBarLayout::OnUpdateUp(CCmdUI* pCmdUI) {
    pCmdUI->Enable(LeftSelectedButtonLocked());
}

void CPPageToolBarLayout::OnUpdateDown(CCmdUI* pCmdUI) {
    pCmdUI->Enable(LeftSelectedButtonLocked());
}


bool CPPageToolBarLayout::InsertButton(int beforeID, int buttonID) {
    CPlayerToolBar& tb = AfxGetMainFrame()->m_wndToolBar;
    CToolBarCtrl& tbctrl = tb.GetToolBarCtrl();

    for (int i = 0; i < tbctrl.GetButtonCount(); i++) {
        TBBUTTON tButton;
        tbctrl.GetButton(i, &tButton);
        if (tButton.idCommand == beforeID) {
            if (beforeID == ID_VOLUME_MUTE) {
                i -= 1; //this is to force inserting before the hidden spacer
            }
            TBBUTTON button = tb.GetStandardButton(buttonID);
            tbctrl.InsertButton(i, &button);
            tb.ToolbarChange();
            return true;
        }
    }
    return false;
}

bool CPPageToolBarLayout::DeleteButton(int buttonID) {
    CPlayerToolBar& tb = AfxGetMainFrame()->m_wndToolBar;
    CToolBarCtrl& tbctrl = tb.GetToolBarCtrl();

    for (int i = 0; i < tbctrl.GetButtonCount(); i++) {
        TBBUTTON tButton;
        tbctrl.GetButton(i, &tButton);
        if (tButton.idCommand == buttonID) {
            tbctrl.DeleteButton(i);
            tb.ToolbarChange();
            return true;
        }
    }
    return false;
}

bool CPPageToolBarLayout::IsValidInsertPos(int destRow) {
    if (destRow == -1 || destRow >= m_list_active.GetItemCount()) {
        return false;
    }

    //avoid inserting between two locked rows, or before first locked or after last locked

    CPlayerToolBar& tb = AfxGetMainFrame()->m_wndToolBar;
    auto supportedButtons = tb.GetSupportedSvgButtons();

    int tidCommand = (int)m_list_active.GetItemData(destRow);
    if (supportedButtons.count(tidCommand) == 0) { //this should never happen
        return false;
    }
    if (supportedButtons[tidCommand].positionLocked == CPlayerToolBar::LOCK_LEFT) {
        return false;
    } else {
        int priorIdCommand = (int)m_list_active.GetItemData(destRow - 1);
        if (supportedButtons.count(priorIdCommand) == 0 || CPlayerToolBar::LOCK_RIGHT == supportedButtons[priorIdCommand].positionLocked) {
            return false;
        }
    }
    return true;
}

bool CPPageToolBarLayout::MoveButton(CMPCThemePlayerListCtrl& srcList, CMPCThemePlayerListCtrl& dstList) {
    CPlayerToolBar& tb = AfxGetMainFrame()->m_wndToolBar;
    CToolBarCtrl& tbctrl = tb.GetToolBarCtrl();
    auto supportedButtons = tb.GetSupportedSvgButtons();

    bool addingButton = (dstList == m_list_active);

    int selectedRow = srcList.GetSelectionMark(); 
    if (selectedRow == -1) {
        return false;
    }
    int buttonID = (int)srcList.GetItemData(selectedRow);
    int beforeID = -1;
    if (supportedButtons.count(buttonID) == 0 || supportedButtons[buttonID].positionLocked) {
        return false;
    }

    int destRow = dstList.GetSelectionMark();

    if (!addingButton) {
        //unused buttons are sorted by svgIndex, so insert accordingly
        int idx = supportedButtons[buttonID].svgIndex;
        for (destRow = 0; destRow < dstList.GetItemCount(); destRow++) {
            int tidCommand = (int)dstList.GetItemData(destRow);
            if (supportedButtons[tidCommand].svgIndex > idx) {
                break;
            }
        }
    } else { //active
        if (destRow == -1) {
            for (destRow = 0; destRow < dstList.GetItemCount(); destRow++) {
                if (IsValidInsertPos(destRow)) {
                    break;
                }
            }
        }
        if (!IsValidInsertPos(destRow)) {
            return false;
        }
        beforeID = (int)dstList.GetItemData(destRow);
    }

    dstList.InsertItem(LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM, destRow, tb.GetStringFromID(buttonID), 0, 0, DescriptiveIcon(buttonID), buttonID);
    srcList.DeleteItem(selectedRow);

    //we'll select the next element, which after having deleted selectedRow, will be at selectedRow
    if (selectedRow < srcList.GetItemCount()) {
        srcList.SetItemState(selectedRow, LVIS_SELECTED, LVIS_SELECTED);
        srcList.SetSelectionMark(selectedRow);
    }

    if (addingButton) {
        InsertButton(beforeID, buttonID);
    } else {
        DeleteButton(buttonID);
    }

    return true;
}

void CPPageToolBarLayout::MoveLeft() {
    MoveButton(m_list_inactive, m_list_active);
}

void CPPageToolBarLayout::MoveRight() {
    MoveButton(m_list_active, m_list_inactive);
}

bool CPPageToolBarLayout::OrderButton(ButtonPosition pos) {
    CPlayerToolBar& tb = AfxGetMainFrame()->m_wndToolBar;
    CToolBarCtrl& tbctrl = tb.GetToolBarCtrl();
    auto supportedButtons = tb.GetSupportedSvgButtons();

    int selectedRow = m_list_active.GetSelectionMark();
    if (selectedRow == -1) {
        return false;
    }
    int buttonID = (int)m_list_active.GetItemData(selectedRow);
    if (supportedButtons.count(buttonID) == 0 || supportedButtons[buttonID].positionLocked) {
        return false;
    }

    if (pos == PREVIOUS_POSITION) {
        for (int insertRow = selectedRow - 1; insertRow > -1; insertRow--) {
            if (IsValidInsertPos(insertRow)) {
                int beforeID = (int)m_list_active.GetItemData(insertRow);

                m_list_active.DeleteItem(selectedRow);
                m_list_active.InsertItem(LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM, insertRow, tb.GetStringFromID(buttonID), 0, 0, supportedButtons[buttonID].svgIndex, buttonID);

                m_list_active.SetItemState(insertRow, LVIS_SELECTED, LVIS_SELECTED);
                m_list_active.SetSelectionMark(insertRow);

                DeleteButton(buttonID);
                InsertButton(beforeID, buttonID);
                break;
            }
        }
    } else {
        for (int insertRow = selectedRow + 2; insertRow < m_list_active.GetItemCount(); insertRow++) {
            if (IsValidInsertPos(insertRow)) {
                int beforeID = (int)m_list_active.GetItemData(insertRow);

                m_list_active.InsertItem(LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM, insertRow, tb.GetStringFromID(buttonID), 0, 0, supportedButtons[buttonID].svgIndex, buttonID);

                m_list_active.SetItemState(insertRow, LVIS_SELECTED, LVIS_SELECTED);
                m_list_active.SetSelectionMark(insertRow);

                m_list_active.DeleteItem(selectedRow);

                DeleteButton(buttonID);
                InsertButton(beforeID, buttonID);
                break;
            }
        }
    }

    return true;
}

void CPPageToolBarLayout::MoveUp() {
    OrderButton(PREVIOUS_POSITION);
}


void CPPageToolBarLayout::MoveDown() {
    OrderButton(NEXT_POSITION);
}

void CPPageToolBarLayout::GetCustomTextColors(INT_PTR nItem, int iSubItem, COLORREF& clrText, COLORREF& clrTextBk, bool& overrideSelectedBG) {
    CPlayerToolBar& tb = AfxGetMainFrame()->m_wndToolBar;
    CToolBarCtrl& tbctrl = tb.GetToolBarCtrl();
    auto supportedButtons = tb.GetSupportedSvgButtons();

    int buttonID = (int)m_list_active.GetItemData(nItem);
    int beforeID = -1;
    if (supportedButtons.count(buttonID) == 0 || supportedButtons[buttonID].positionLocked) {
        overrideSelectedBG = true;
        if (AppNeedsThemedControls()) {
            clrText = CMPCTheme::ContentTextDisabledFGColorFade2;
            clrTextBk = CMPCTheme::ContentBGColor;
        } else {
            clrText = RGB(224, 224, 224);
            clrTextBk = RGB(255, 255, 255);
        }
    }
}

void CPPageToolBarLayout::OnCustomdrawList(NMHDR* pNMHDR, LRESULT* pResult) {
    //this custom draw is used only in classic mode
    *pResult = CDRF_DODEFAULT;
    if (!AppNeedsThemedControls()) {
        NMLVCUSTOMDRAW* pLVCD = reinterpret_cast<NMLVCUSTOMDRAW*>(pNMHDR);

        if (CDDS_PREPAINT == pLVCD->nmcd.dwDrawStage) {
            *pResult = CDRF_NOTIFYITEMDRAW;
        } else if (CDDS_ITEMPREPAINT == pLVCD->nmcd.dwDrawStage) {
            bool overrideSelectedBG = false;
            GetCustomTextColors(pLVCD->nmcd.dwItemSpec, pLVCD->iSubItem, pLVCD->clrText, pLVCD->clrTextBk, overrideSelectedBG);
            if (overrideSelectedBG) {
                pLVCD->nmcd.uItemState &= ~CDIS_SELECTED;
            }
        }
    }
}


void CPPageToolBarLayout::DefaultButtons() {
    CPlayerToolBar& tb = AfxGetMainFrame()->m_wndToolBar;
    tb.ToolBarReset();
    LoadToolBarButtons();
    tb.ToolbarChange();
}
