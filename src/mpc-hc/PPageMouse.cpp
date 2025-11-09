/*
 * (C) 2020-2021 see Authors.txt
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
#include "MainFrm.h"
#include "../DSUtil/std_helper.h"
#include "AddCommandDlg.h"
#include "PPageMouse.h"

// CPPageMouse dialog

IMPLEMENT_DYNAMIC(CPPageMouse, CMPCThemePPageBase)
CPPageMouse::CPPageMouse()
    : CMPCThemePPageBase(CPPageMouse::IDD, CPPageMouse::IDD)
    , m_list(0)
{
    m_comands_M.Add(0);
    m_comands_M.Add(ID_PLAY_PLAYPAUSE);
    m_comands_M.Add(ID_VIEW_FULLSCREEN);
    m_comands_M.Add(ID_VIEW_PLAYLIST);
    m_comands_M.Add(ID_BOSS);
    m_comands_M.AddEllipsisEnd();

    m_comands_X.Add(0);
    m_comands_X.Add(ID_NAVIGATE_SKIPFORWARD);
    m_comands_X.Add(ID_NAVIGATE_SKIPBACK);
    m_comands_X.Add(ID_NAVIGATE_SKIPFORWARDFILE);
    m_comands_X.Add(ID_NAVIGATE_SKIPBACKFILE);
    m_comands_X.Add(ID_PLAY_SEEKBACKWARDMED);
    m_comands_X.Add(ID_PLAY_SEEKFORWARDMED);
    m_comands_X.Add(ID_PLAY_SEEKBACKWARDLARGE);
    m_comands_X.Add(ID_PLAY_SEEKFORWARDLARGE);
    m_comands_X.AddEllipsisEnd();

    m_comands_W.Add(0);
    m_comands_W.Add(ID_VOLUME_UP);
    m_comands_W.Add(ID_VOLUME_DOWN);
    m_comands_W.Add(ID_PLAY_INCRATE);
    m_comands_W.Add(ID_PLAY_DECRATE);
    m_comands_W.Add(ID_PLAY_SEEKFORWARDMED);
    m_comands_W.Add(ID_PLAY_SEEKBACKWARDMED);
    m_comands_W.Add(ID_PLAY_SEEKFORWARDLARGE);
    m_comands_W.Add(ID_PLAY_SEEKBACKWARDLARGE);
    m_comands_W.Add(ID_NAVIGATE_SKIPFORWARDFILE);
    m_comands_W.Add(ID_NAVIGATE_SKIPBACKFILE);
    m_comands_W.Add(ID_SUB_DELAY_UP);
    m_comands_W.Add(ID_SUB_DELAY_DOWN);
    m_comands_W.AddEllipsisEnd();
}

CPPageMouse::~CPPageMouse()
{
}

void CPPageMouse::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);

    DDX_Control(pDX, IDC_COMBO1, m_cmbLeftButtonClick);
    DDX_Control(pDX, IDC_COMBO2, m_cmbLeftButtonDblClick);
    DDX_Control(pDX, IDC_COMBO3, m_cmbRightButtonClick);
    DDX_Control(pDX, IDC_LIST1, m_list);
}

LPCWSTR CPPageMouse::GetCmdString(MOUSE_COMMANDS* mouse_cmds, const WORD id)
{
    const auto& ids = mouse_cmds->ids;
    const auto& str_list = mouse_cmds->str_list;

    for (size_t i = 0; i < ids.size(); i++) {
        if (id == ids[i]) {
            auto it = str_list.cbegin();
            std::advance(it, i);
            return *it;
        }
    }

    return str_list.front();
}

void CPPageMouse::SyncList()
{
    for (int j = COL_CMD; j < COL_COUNT; j++) {
        for (int i = 0; i < ROW_COUNT; i++) {
            MOUSE_COMMANDS* mouse_cmds = m_table_comands[i][j];
            WORD id = m_table_values[i][j];
            m_list.SetItemText(i, j, GetCmdString(mouse_cmds, id));
        }

        m_list.SetColumnWidth(j, LVSCW_AUTOSIZE);
    }
}

BOOL CPPageMouse::OnInitDialog()
{
    __super::OnInitDialog();

    SetCursor(m_hWnd, IDC_COMBO1, IDC_HAND);

    CAppSettings& s = AfxGetAppSettings();

    AddStringData(m_cmbLeftButtonClick, L"", 0);
    AddStringData(m_cmbLeftButtonClick, ResStr(IDS_AG_PLAYPAUSE), ID_PLAY_PLAYPAUSE);
    AddStringData(m_cmbLeftButtonClick, ResStr(IDS_AG_FULLSCREEN), ID_VIEW_FULLSCREEN);
    m_cmbLeftButtonClick.SelectByItemData(s.nMouseLeftClick);

    AddStringData(m_cmbLeftButtonDblClick, L"", 0);
    AddStringData(m_cmbLeftButtonDblClick, ResStr(IDS_AG_PLAYPAUSE), ID_PLAY_PLAYPAUSE);
    AddStringData(m_cmbLeftButtonDblClick, ResStr(IDS_AG_FULLSCREEN), ID_VIEW_FULLSCREEN);
    m_cmbLeftButtonDblClick.SelectByItemData(s.nMouseLeftDblClick);

    AddStringData(m_cmbRightButtonClick, ResStr(IDS_MPLAYERC_77), ID_MENU_PLAYER_SHORT);
    AddStringData(m_cmbRightButtonClick, ResStr(IDS_MPLAYERC_78), ID_MENU_PLAYER_LONG);
    AddStringData(m_cmbRightButtonClick, L"<...>", ID_COMBO_ADD_CMD);
    if (s.nMouseRightClick != ID_MENU_PLAYER_LONG && s.nMouseRightClick != ID_MENU_PLAYER_SHORT) {
        AddCmdToRightClick(s.nMouseRightClick, m_cmbRightButtonClick.GetCount());
    }
    m_cmbRightButtonClick.SelectByItemData(s.nMouseRightClick);

    m_table_values[ROW_BTN_M][COL_CMD]    = s.MouseMiddleClick.normal;
    m_table_values[ROW_BTN_M][COL_CTRL]   = s.MouseMiddleClick.ctrl;
    m_table_values[ROW_BTN_M][COL_SHIFT]  = s.MouseMiddleClick.shift;
    m_table_values[ROW_BTN_M][COL_RBTN]   = s.MouseMiddleClick.rbtn;
    m_table_values[ROW_BTN_X1][COL_CMD]   = s.MouseX1Click.normal;
    m_table_values[ROW_BTN_X1][COL_CTRL]  = s.MouseX1Click.ctrl;
    m_table_values[ROW_BTN_X1][COL_SHIFT] = s.MouseX1Click.shift;
    m_table_values[ROW_BTN_X1][COL_RBTN]  = s.MouseX1Click.rbtn;
    m_table_values[ROW_BTN_X2][COL_CMD]   = s.MouseX2Click.normal;
    m_table_values[ROW_BTN_X2][COL_CTRL]  = s.MouseX2Click.ctrl;
    m_table_values[ROW_BTN_X2][COL_SHIFT] = s.MouseX2Click.shift;
    m_table_values[ROW_BTN_X2][COL_RBTN]  = s.MouseX2Click.rbtn;
    m_table_values[ROW_WHL_U][COL_CMD]    = s.MouseWheelUp.normal;
    m_table_values[ROW_WHL_U][COL_CTRL]   = s.MouseWheelUp.ctrl;
    m_table_values[ROW_WHL_U][COL_SHIFT]  = s.MouseWheelUp.shift;
    m_table_values[ROW_WHL_U][COL_RBTN]   = s.MouseWheelUp.rbtn;
    m_table_values[ROW_WHL_D][COL_CMD]    = s.MouseWheelDown.normal;
    m_table_values[ROW_WHL_D][COL_CTRL]   = s.MouseWheelDown.ctrl;
    m_table_values[ROW_WHL_D][COL_SHIFT]  = s.MouseWheelDown.shift;
    m_table_values[ROW_WHL_D][COL_RBTN]   = s.MouseWheelDown.rbtn;
    m_table_values[ROW_WHL_L][COL_CMD]    = s.MouseWheelLeft.normal;
    m_table_values[ROW_WHL_L][COL_CTRL]   = s.MouseWheelLeft.ctrl;
    m_table_values[ROW_WHL_L][COL_SHIFT]  = s.MouseWheelLeft.shift;
    m_table_values[ROW_WHL_L][COL_RBTN]   = s.MouseWheelLeft.rbtn;
    m_table_values[ROW_WHL_R][COL_CMD]    = s.MouseWheelRight.normal;
    m_table_values[ROW_WHL_R][COL_CTRL]   = s.MouseWheelRight.ctrl;
    m_table_values[ROW_WHL_R][COL_SHIFT]  = s.MouseWheelRight.shift;
    m_table_values[ROW_WHL_R][COL_RBTN]   = s.MouseWheelRight.rbtn;

    for (int i = ROW_BTN_M; i < ROW_COUNT; i++) {
        for (int j = COL_CMD; j < COL_COUNT; j++) {
            WORD id = (WORD)m_table_values[i][j];
            MOUSE_COMMANDS* mc = m_table_comands[i][j];

            if (mc && !Contains(mc->ids, id)) {
                mc->Add(id);
            }
        }
    }

    //m_list.SetExtendedStyle(m_list.GetExtendedStyle() | LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    m_list.setAdditionalStyles(LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    m_list.setAdditionalStyles(WS_CLIPCHILDREN, false);


    m_list.InsertColumn(COL_ACTION, ResStr(IDS_MOUSE_ACTION));
    m_list.InsertColumn(COL_CMD,    ResStr(IDS_MOUSE_COMMAND));
    m_list.InsertColumn(COL_CTRL,   L"+ Ctrl");
    m_list.InsertColumn(COL_SHIFT,  L"+ Shift");
    m_list.InsertColumn(COL_RBTN, L"+ " + ResStr(IDS_MOUSE_RIGHT_BUTTON));

    m_list.InsertItem(ROW_BTN_M,  ResStr(IDS_MOUSE_CLICK_MIDDLE));
    m_list.InsertItem(ROW_BTN_X1, ResStr(IDS_MOUSE_CLICK_X1));
    m_list.InsertItem(ROW_BTN_X2, ResStr(IDS_MOUSE_CLICK_X2));
    m_list.InsertItem(ROW_WHL_U,  ResStr(IDS_MOUSE_WHEEL_UP));
    m_list.InsertItem(ROW_WHL_D,  ResStr(IDS_MOUSE_WHEEL_DOWN));
    m_list.InsertItem(ROW_WHL_L,  ResStr(IDS_MOUSE_WHEEL_LEFT));
    m_list.InsertItem(ROW_WHL_R,  ResStr(IDS_MOUSE_WHEEL_RIGHT));

    DpiHelper dpiWindow;
    dpiWindow.Override(GetSafeHwnd());

    int minSize = dpiWindow.ScaleX(80);
    for (int nCol = COL_ACTION; nCol < COL_COUNT; nCol++) {
        m_list.SetColumnWidth(nCol, LVSCW_AUTOSIZE_USEHEADER);
        const int headerSize = m_list.GetColumnWidth(nCol);
        m_list.SetColumnWidth(nCol, LVSCW_AUTOSIZE);

        LVCOLUMNW col;
        col.mask = LVCF_MINWIDTH;
        col.cxMin = std::max(headerSize, minSize);
        m_list.SetColumn(nCol, &col);
    }

    SyncList();

    return TRUE;
}

BOOL CPPageMouse::OnApply()
{
    UpdateData();

    CAppSettings& s = AfxGetAppSettings();

    s.nMouseLeftClick    = (UINT)GetCurItemData(m_cmbLeftButtonClick);
    s.nMouseLeftDblClick = (UINT)GetCurItemData(m_cmbLeftButtonDblClick);
    s.nMouseRightClick   = (UINT)GetCurItemData(m_cmbRightButtonClick);

    s.MouseMiddleClick.normal = m_table_values[ROW_BTN_M][COL_CMD];
    s.MouseMiddleClick.ctrl   = m_table_values[ROW_BTN_M][COL_CTRL];
    s.MouseMiddleClick.shift  = m_table_values[ROW_BTN_M][COL_SHIFT];
    s.MouseMiddleClick.rbtn   = m_table_values[ROW_BTN_M][COL_RBTN];
    s.MouseX1Click.normal     = m_table_values[ROW_BTN_X1][COL_CMD];
    s.MouseX1Click.ctrl       = m_table_values[ROW_BTN_X1][COL_CTRL];
    s.MouseX1Click.shift      = m_table_values[ROW_BTN_X1][COL_SHIFT];
    s.MouseX1Click.rbtn       = m_table_values[ROW_BTN_X1][COL_RBTN];
    s.MouseX2Click.normal     = m_table_values[ROW_BTN_X2][COL_CMD];
    s.MouseX2Click.ctrl       = m_table_values[ROW_BTN_X2][COL_CTRL];
    s.MouseX2Click.shift      = m_table_values[ROW_BTN_X2][COL_SHIFT];
    s.MouseX2Click.rbtn       = m_table_values[ROW_BTN_X2][COL_RBTN];
    s.MouseWheelUp.normal     = m_table_values[ROW_WHL_U][COL_CMD];
    s.MouseWheelUp.ctrl       = m_table_values[ROW_WHL_U][COL_CTRL];
    s.MouseWheelUp.shift      = m_table_values[ROW_WHL_U][COL_SHIFT];
    s.MouseWheelUp.rbtn       = m_table_values[ROW_WHL_U][COL_RBTN];
    s.MouseWheelDown.normal   = m_table_values[ROW_WHL_D][COL_CMD];
    s.MouseWheelDown.ctrl     = m_table_values[ROW_WHL_D][COL_CTRL];
    s.MouseWheelDown.shift    = m_table_values[ROW_WHL_D][COL_SHIFT];
    s.MouseWheelDown.rbtn     = m_table_values[ROW_WHL_D][COL_RBTN];
    s.MouseWheelLeft.normal   = m_table_values[ROW_WHL_L][COL_CMD];
    s.MouseWheelLeft.ctrl     = m_table_values[ROW_WHL_L][COL_CTRL];
    s.MouseWheelLeft.shift    = m_table_values[ROW_WHL_L][COL_SHIFT];
    s.MouseWheelLeft.rbtn     = m_table_values[ROW_WHL_L][COL_RBTN];
    s.MouseWheelRight.normal  = m_table_values[ROW_WHL_R][COL_CMD];
    s.MouseWheelRight.ctrl    = m_table_values[ROW_WHL_R][COL_CTRL];
    s.MouseWheelRight.shift   = m_table_values[ROW_WHL_R][COL_SHIFT];
    s.MouseWheelRight.rbtn    = m_table_values[ROW_WHL_R][COL_RBTN];

    return __super::OnApply();
}

BEGIN_MESSAGE_MAP(CPPageMouse, CMPCThemePPageBase)
    ON_CBN_SELCHANGE(IDC_COMBO1, OnLeftClickChange)
    ON_NOTIFY(LVN_BEGINLABELEDITW, IDC_LIST1, OnBeginlabeleditList)
    ON_NOTIFY(LVN_DOLABELEDIT, IDC_LIST1, OnDolabeleditList)
    ON_NOTIFY(LVN_ENDLABELEDITW, IDC_LIST1, OnEndlabeleditList)
    ON_BN_CLICKED(IDC_BUTTON2, OnBnClickedReset)
    ON_CBN_SELENDOK(IDC_COMBO3, OnRightClickChange)
END_MESSAGE_MAP()

// CPPageMouse message handlers

void CPPageMouse::OnLeftClickChange()
{
    SetModified();
}

void CPPageMouse::AddCmdToRightClick(WORD id, size_t idx) {
    auto& wmcmds = AfxGetAppSettings().wmcmds;
    POSITION pos = wmcmds.GetHeadPosition();
    while (pos) {
        wmcmd& wc = wmcmds.GetNext(pos);
        if (id == wc.cmd) {
            idx = m_cmbRightButtonClick.InsertString(idx - 1, ResStr(wc.dwname));
            m_cmbRightButtonClick.SetItemData(idx, id);
            break;
        }
    }
}

void CPPageMouse::OnRightClickChange() {
    int curSel = m_cmbRightButtonClick.GetCurSel();
    if (curSel != CB_ERR && m_cmbRightButtonClick.GetItemData(curSel) == ID_COMBO_ADD_CMD) {
        CAddCommandDlg dlg(this);
        if (dlg.DoModal() == IDOK) {
            const WORD id = dlg.GetSelectedCommandID();
            size_t idx;
            for (idx = 0; idx < m_cmbRightButtonClick.GetCount(); idx++) {
                if (id == m_cmbRightButtonClick.GetItemData(idx)) {
                    break;
                }
            }
            
            if (idx == m_cmbRightButtonClick.GetCount()) {
                AddCmdToRightClick(id, idx);
            }
            m_cmbRightButtonClick.SelectByItemData(id);
        } else {
            m_cmbRightButtonClick.SelectByItemData(ID_MENU_PLAYER_SHORT);
        }
        SetModified();
    }
}

void CPPageMouse::OnBeginlabeleditList(NMHDR* pNMHDR, LRESULT* pResult)
{
    NMLVDISPINFOW* pDispInfo = (NMLVDISPINFOW*)pNMHDR;
    LVITEMW* pItem = &pDispInfo->item;

    if (pItem->iItem >= 0 && pItem->iSubItem >= COL_CMD) {
        *pResult = TRUE;
    } else {
        *pResult = FALSE;
    }
}

void CPPageMouse::OnDolabeleditList(NMHDR* pNMHDR, LRESULT* pResult)
{
    NMLVDISPINFOW* pDispInfo = (NMLVDISPINFOW*)pNMHDR;
    LVITEMW* pItem = &pDispInfo->item;

    if (pItem->iItem >= 0) {
        MOUSE_COMMANDS* mouse_cmds = m_table_comands[pItem->iItem][pItem->iSubItem];

        if (mouse_cmds) {
            const auto id = m_table_values[pItem->iItem][pItem->iSubItem];
            const auto& ids = mouse_cmds->ids;
            int nSel = -1;

            for (size_t i = 0; i < ids.size(); i++) {
                if (id == ids[i]) {
                    nSel = (int)i;
                    break;
                }
            }
            CAtlList<CStringW> cmdList;
            for (auto l : mouse_cmds->str_list) {
                cmdList.AddTail(l);
            }

            m_list.ShowInPlaceComboBox(pItem->iItem, pItem->iSubItem, cmdList, nSel);

            *pResult = TRUE;

            return;
        }
    }

    *pResult = FALSE;
}

void CPPageMouse::OnEndlabeleditList(NMHDR* pNMHDR, LRESULT* pResult)
{
    NMLVDISPINFOW* pDispInfo = (NMLVDISPINFOW*)pNMHDR;
    LVITEMW* pItem = &pDispInfo->item;

    *pResult = FALSE;

    if (m_list.m_fInPlaceDirty && pItem->iItem >= 0 && pItem->iSubItem >= COL_CMD && pItem->pszText) {
        MOUSE_COMMANDS* mc = m_table_comands[pItem->iItem][pItem->iSubItem];

        if (mc) {
            size_t idx = pItem->lParam;
            LPCWSTR str = pItem->pszText;

            if (idx >= mc->ids.size()) {
                CAddCommandDlg dlg(this);
                if (dlg.DoModal() == IDOK) {
                    const WORD id = dlg.GetSelectedCommandID();

                    for (idx = 0; idx < mc->ids.size(); idx++) {
                        if (id == mc->ids[idx]) {
                            break;
                        }
                    }
                    if (idx == mc->ids.size()) {
                        mc->Add(id);
                    }
                    auto it = mc->str_list.cbegin();
                    std::advance(it, idx);
                    str = *it;
                }
            }

            if (idx < mc->ids.size()) {
                m_table_values[pItem->iItem][pItem->iSubItem] = mc->ids[idx];
                m_list.SetItemText(pItem->iItem, pItem->iSubItem, str);
                m_list.SetColumnWidth(pItem->iSubItem, LVSCW_AUTOSIZE);
                SetModified();
            }

            *pResult = TRUE;
            return;
        }
    }

    *pResult = FALSE;
}

void CPPageMouse::OnBnClickedReset()
{
    CAppSettings& s = AfxGetAppSettings();

    m_cmbLeftButtonClick.SelectByItemData(ID_PLAY_PLAYPAUSE);
    m_cmbLeftButtonDblClick.SelectByItemData(ID_VIEW_FULLSCREEN);
    m_cmbRightButtonClick.SelectByItemData(ID_MENU_PLAYER_SHORT);

    m_table_values[ROW_BTN_M][COL_CMD]    = 0;
    m_table_values[ROW_BTN_M][COL_CTRL]   = 0;
    m_table_values[ROW_BTN_M][COL_SHIFT]  = 0;
    m_table_values[ROW_BTN_M][COL_RBTN]   = 0;
    m_table_values[ROW_BTN_X1][COL_CMD]   = ID_NAVIGATE_SKIPBACK;
    m_table_values[ROW_BTN_X1][COL_CTRL]  = 0;
    m_table_values[ROW_BTN_X1][COL_SHIFT] = 0;
    m_table_values[ROW_BTN_X1][COL_RBTN]  = 0;
    m_table_values[ROW_BTN_X2][COL_CMD]   = ID_NAVIGATE_SKIPFORWARD;
    m_table_values[ROW_BTN_X2][COL_CTRL]  = 0;
    m_table_values[ROW_BTN_X2][COL_SHIFT] = 0;
    m_table_values[ROW_BTN_X2][COL_RBTN]  = 0;
    m_table_values[ROW_WHL_U][COL_CMD]    = ID_VOLUME_UP;
    m_table_values[ROW_WHL_U][COL_CTRL]   = ID_PLAY_SEEKFORWARDLARGE;
    m_table_values[ROW_WHL_U][COL_SHIFT]  = 0;
    m_table_values[ROW_WHL_U][COL_RBTN]   = ID_PLAY_SEEKFORWARDLARGE;
    m_table_values[ROW_WHL_D][COL_CMD]    = ID_VOLUME_DOWN;
    m_table_values[ROW_WHL_D][COL_CTRL]   = ID_PLAY_SEEKBACKWARDLARGE;
    m_table_values[ROW_WHL_D][COL_SHIFT]  = 0;
    m_table_values[ROW_WHL_D][COL_RBTN]   = ID_PLAY_SEEKBACKWARDLARGE;
    m_table_values[ROW_WHL_L][COL_CMD]    = 0;
    m_table_values[ROW_WHL_L][COL_CTRL]   = 0;
    m_table_values[ROW_WHL_L][COL_SHIFT]  = 0;
    m_table_values[ROW_WHL_L][COL_RBTN]   = 0;
    m_table_values[ROW_WHL_R][COL_CMD]    = 0;
    m_table_values[ROW_WHL_R][COL_CTRL]   = 0;
    m_table_values[ROW_WHL_R][COL_SHIFT]  = 0;
    m_table_values[ROW_WHL_R][COL_RBTN]   = 0;

    SyncList();

    SetModified();
}
