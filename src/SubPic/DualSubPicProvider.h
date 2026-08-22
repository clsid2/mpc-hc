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

#include <vector>
#include "ISubPic.h"

// Composite subtitle provider that merges a primary and a secondary provider
// into a single ISubPicProvider, so that both subtitle tracks are rendered
// into the same subpic through the existing queue/presenter pipeline and
// therefore work with every video renderer that supports subtitles today.
class CDualSubPicProvider : public CUnknown, public ISubPicProvider
{
    CComPtr<ISubPicProvider> m_pPrimary;
    CComPtr<ISubPicProvider> m_pSecondary;

    struct MergedSegment {
        REFERENCE_TIME rtStart, rtStop;
        POSITION posPrimary, posSecondary; // covering segment of each provider, or null
    };
    // The merged timeline handed out to the subpic queue. Its boundaries are the
    // union of both providers' segment boundaries, so every merged segment lies
    // fully inside one segment of each provider that has content there. It is
    // rebuilt on every GetStartPosition() call, which starts each enumeration the
    // queue makes, so changes in the underlying providers (streamed-in samples,
    // edited styles) are always picked up.
    std::vector<MergedSegment> m_segments;
    CCritSec m_csSegments;

    void RebuildSegments(REFERENCE_TIME rt, double fps);

public:
    CDualSubPicProvider(ISubPicProvider* pPrimary, ISubPicProvider* pSecondary);
    virtual ~CDualSubPicProvider();

    DECLARE_IUNKNOWN;
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

    // ISubPicProvider

    STDMETHODIMP Lock();
    STDMETHODIMP Unlock();

    STDMETHODIMP_(POSITION) GetStartPosition(REFERENCE_TIME rt, double fps);
    STDMETHODIMP_(POSITION) GetNext(POSITION pos);

    STDMETHODIMP_(REFERENCE_TIME) GetStart(POSITION pos, double fps);
    STDMETHODIMP_(REFERENCE_TIME) GetStop(POSITION pos, double fps);

    STDMETHODIMP_(bool) IsAnimated(POSITION pos);

    STDMETHODIMP Render(SubPicDesc& spd, REFERENCE_TIME rt, double fps, RECT& bbox);
    STDMETHODIMP GetTextureSize(POSITION pos, SIZE& MaxTextureSize, SIZE& VirtualSize, POINT& VirtualTopLeft);
    STDMETHODIMP GetRelativeTo(POSITION pos, RelativeTo& relativeTo);
};
