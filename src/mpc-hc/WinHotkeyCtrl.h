/*
 * (C) 2011-2012 see Authors.txt
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

#include "EditWithButton.h"

// CWinHotkeyCtrl

class CWinHotkeyCtrl : public CEditWithButton
{
    DECLARE_DYNAMIC(CWinHotkeyCtrl)

public:
    CWinHotkeyCtrl();
    virtual ~CWinHotkeyCtrl();

    void UpdateText();
    DWORD GetWinHotkey();
    BOOL GetWinHotkey(UINT* pvkCode, UINT* pfModifiers);
    void SetWinHotkey(DWORD dwHk);
    void SetWinHotkey(UINT vkCode, UINT fModifiers);
    virtual void DrawButton(CRect rectButton);
    void SetIsMouseModifier(bool _isMouseModifier) { isMouseModifier = _isMouseModifier; };

private:
    static HHOOK sm_hhookKb;
    static CWinHotkeyCtrl* sm_pwhcFocus;
    static DWORD sm_fModsDown;
    bool isMouseModifier;

    UINT m_vkCode, m_vkCode_def;
    DWORD m_fModSet, m_fModRel, m_fModSet_def;
    BOOL m_fIsPressed;

private:
    BOOL InstallKbHook();
    BOOL UninstallKbHook();

    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

    afx_msg LRESULT OnKey(WPARAM wParam, LPARAM lParam);

protected:
    DECLARE_MESSAGE_MAP()
public:
    afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnKillFocus(CWnd* pNewWnd);
    afx_msg void OnContextMenu(CWnd*, CPoint pt);
    afx_msg void OnDestroy();
    afx_msg LRESULT OnLeftClick(WPARAM wParam, LPARAM lParam);
protected:
    virtual void PreSubclassWindow();
};
