/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2012, 2015 see Authors.txt
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
#include "CMPCThemeToolTipCtrl.h"

// CVolumeCtrl

class CVolumeCtrl : public CSliderCtrl
{
    DECLARE_DYNAMIC(CVolumeCtrl)

private:
    bool m_fSelfDrawn;

public:
    CVolumeCtrl(bool fSelfDrawn = true);
    virtual ~CVolumeCtrl();

    bool Create(CWnd* pParentWnd);
    void IncreaseVolume(), DecreaseVolume();
    void SetPosInternal(int pos);

protected:
    afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
    void getCustomChannelRect(LPRECT rc);
    void drawPercentage(CDC& dc, const CStringW& str, const CRect& rc);
    void updateModernVolCtrl(CPoint point);
    bool m_bDrag, m_bHover;
    bool modernStyle;
    bool showPercentage;
    CMPCThemeToolTipCtrl themedToolTip;

    DECLARE_MESSAGE_MAP()

public:
    afx_msg BOOL OnToolTipNotify(UINT id, NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnNMCustomdraw(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void HScroll(UINT nSBCode, UINT nPos);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint point);
    void invalidateThumb();
    void checkHover(CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnMouseLeave();
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
};
