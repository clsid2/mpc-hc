/*
* (C) 2015-2017 see Authors.txt
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

#pragma once

#include "CMPCThemePPageBase.h"
#include "resource.h"
#include "CMPCThemeSpinButtonCtrl.h"
#include "CMPCThemePlayerListCtrl.h"
#include "CMPCThemeComboBox.h"

class CPPageToolBarLayout : public CMPCThemePPageBase
    , public CMPCThemeListCtrlCustomInterface
{
    DECLARE_DYNAMIC(CPPageToolBarLayout)
public:
    CPPageToolBarLayout();
    virtual ~CPPageToolBarLayout() = default;
    virtual void GetCustomTextColors(INT_PTR nItem, int iSubItem, COLORREF& clrText, COLORREF& clrTextBk, bool& overrideSelectedBG);
    virtual void DoCustomPrePaint() {};
    virtual void GetCustomGridColors(int nItem, COLORREF& horzGridColor, COLORREF& vertGridColor) {};
    virtual bool UseCustomGrid() { return false; };
    void ReloadImageLists();

private:
    enum { IDD = IDD_PPAGETOOLBARLAYOUT };
    enum {
        COL_BUTTON,
    };

    enum ButtonPosition {
        PREVIOUS_POSITION,
        NEXT_POSITION,
    };

protected:
    CMPCThemePlayerListCtrl m_list_active, m_list_inactive;
    CMPCThemeButton leftButton, rightButton, upButton, downButton;
    CImage arrow;

    virtual void DoDataExchange(CDataExchange* pDX) override;
    int DescriptiveIcon(int idCommand);
    void OnUpdateLeft(CCmdUI* pCmdUI);
    void OnUpdateRight(CCmdUI* pCmdUI);
    void OnUpdateUp(CCmdUI* pCmdUI);
    void OnUpdateDown(CCmdUI* pCmdUI);
    void LoadToolBarButtons();
    virtual BOOL OnInitDialog() override;
    virtual BOOL OnApply() override;
    bool InsertButton(int beforeID, int buttonID, int existingStyle = -1);
    bool DeleteButton(int buttonID);
    bool IsValidInsertPos(int destRow);
    bool LeftSelectedButtonLocked();
    bool MoveButton(CMPCThemePlayerListCtrl& srcList, CMPCThemePlayerListCtrl& dstList);
    bool OrderButton(ButtonPosition pos);

    DECLARE_MESSAGE_MAP()
public:
    afx_msg void OnCustomdrawList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void MoveLeft();
    afx_msg void MoveRight();
    afx_msg void MoveUp();
    afx_msg void MoveDown();
    afx_msg void DefaultButtons();
};

