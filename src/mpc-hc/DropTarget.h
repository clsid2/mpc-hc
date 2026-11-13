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

#include <afxole.h>

#define DROPEFFECT_APPEND 16

struct CDropClient {
    virtual void OnDropFiles(std::list<CStringW>& slFiles, DROPEFFECT dropEffect) PURE;
    virtual DROPEFFECT OnDropAccept(COleDataObject* pDataObject, DWORD dwKeyState, CPoint point) PURE;
};

class CDropTarget : public COleDropTarget
{
    const CLIPFORMAT CF_URLA = static_cast<CLIPFORMAT>(RegisterClipboardFormat(_T("UniformResourceLocator")));
    const CLIPFORMAT CF_URLW = static_cast<CLIPFORMAT>(RegisterClipboardFormat(_T("UniformResourceLocatorW")));
    CComPtr<IDropTargetHelper> m_pDropHelper;

public:
    CDropTarget();
    virtual ~CDropTarget() = default;

protected:
    DROPEFFECT OnDragEnter(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point) override;
    DROPEFFECT OnDragOver(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point) override;
    BOOL OnDrop(CWnd* pWnd, COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point) override;
    DROPEFFECT OnDropEx(CWnd* pWnd, COleDataObject* pDataObject, DROPEFFECT dropDefault, DROPEFFECT dropList, CPoint point) override;
    void OnDragLeave(CWnd* pWnd) override;
    DROPEFFECT OnDragScroll(CWnd* pWnd, DWORD dwKeyState, CPoint point) override;
};
