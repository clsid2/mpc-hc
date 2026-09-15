/*
 * (C) 2026 see Authors.txt
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

#include <afxpriv.h>
#include "DragRectWnd.h"

// CDockContext for the docking panels: same drag/resize loop as MFC,
// but the outline is a layered window instead of XOR drawing on the desktop DC.
class CPlayerBarDockContext : public CDockContext
{
public:
    explicit CPlayerBarDockContext(CControlBar* pBar);

    virtual void StartDrag(CPoint pt) override;
    virtual void StartResize(int nHitTest, CPoint pt) override;

private:
    CDragRectWnd m_dragRect;

    // the base versions are non-virtual and draw through m_pDC, so the loop is carried here
    void InitLoop();
    void CancelLoop();
    BOOL Track();
    void Move(CPoint pt);
    void Stretch(CPoint pt);
    void EndDrag();
    void EndResize();
    void OnKey(int nChar, BOOL bDown);
    void UpdateState(BOOL* pFlag, BOOL bNewValue);
    void DrawFocusRect(BOOL bRemoveRect = FALSE);
};
