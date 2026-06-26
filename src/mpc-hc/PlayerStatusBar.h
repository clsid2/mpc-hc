/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2015 see Authors.txt
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

#include "StatusLabel.h"
#include "CMPCThemeToolTipCtrl.h"
#include "CMPCThemeMenu.h"

class CMainFrame;

// CPlayerStatusBar

class CPlayerStatusBar : public CDialogBar
{
    DECLARE_DYNAMIC(CPlayerStatusBar)

private:
    CMainFrame* m_pMainFrame;

    CStatic m_type;
    CStatusLabel m_status, m_time;
    CBitmap m_bm;
    UINT m_bmid;
    CString m_typeExt;
    HICON m_hIcon;

    REFERENCE_TIME m_rtNow, m_rtDur;

    CRect m_time_rect;
    CMPCThemeMenu m_timerMenu;

    CToolTipCtrl m_tooltip;
    CMPCThemeToolTipCtrl themedToolTip;

    EventClient m_eventc;
    void EventCallback(MpcEvent ev);

    int m_initialWindowDPI = 0;

public:
    CPlayerStatusBar(CMainFrame* pMainFrame);
    virtual ~CPlayerStatusBar();

    void Relayout();

    void Clear();

    void SetStatusBitmap(UINT id);
    void SetMediaType(CString ext);
    void SetStatusMessage(CString str);
    void SetStatusTimer(CString str);
    void SetStatusTimer(REFERENCE_TIME rtNow, REFERENCE_TIME rtDur, bool fHighPrecision,
                        const GUID& timeFormat = TIME_FORMAT_MEDIA_TIME);

    REFERENCE_TIME GetTimerCurPos();
    REFERENCE_TIME GetTimerDuration();

    CString GetStatusTimer() const;
    CString GetStatusMessage() const;

    CString PreparePathStatusMessage(CPath path);

    void ShowTimer(bool fShow);

    // Builds and tracks the timer-options popup (Remaining time / High precision / Show percentage),
    // shared by the status-bar time control and the seekbar time section (#3256).
    void ShowTimerOptionsMenu(CWnd* pOwner, CPoint screenPt);

    // Overrides
    virtual BOOL Create(CWnd* pParentWnd);
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
    virtual CSize CalcFixedLayout(BOOL bStretch, BOOL bHorz) override;

    DECLARE_MESSAGE_MAP()

protected:

    void SetMediaTypeIcon();
    void ScaleFont();

    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnPaint();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    afx_msg BOOL PreTranslateMessage(MSG* pMsg);
    afx_msg void OnTimeDisplayClicked();
    afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
    afx_msg BOOL OnToolTipNotify(UINT id, NMHDR* pNMHDR, LRESULT* pResult);
};
