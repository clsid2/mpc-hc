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

#include "stdafx.h"
#include <algorithm>
#include "DualSubPicProvider.h"

// The queue never prerenders more than one minute ahead of the current time, so
// this lookahead always covers a full enumeration. Segments further away are
// picked up by a later rebuild, since the queue re-enumerates on every SetTime.
static const REFERENCE_TIME LOOKAHEAD_TIME = 120 * 10000000i64;
// Safety cap on the source segments collected per provider per rebuild. Cutting
// the merged timeline short is harmless: the queue simply re-enumerates later.
static const size_t MAX_SOURCE_SEGMENTS = 1024;

CDualSubPicProvider::CDualSubPicProvider(ISubPicProvider* pPrimary, ISubPicProvider* pSecondary)
    : CUnknown(NAME("CDualSubPicProvider"), nullptr)
    , m_pPrimary(pPrimary)
    , m_pSecondary(pSecondary)
{
    ASSERT(pPrimary && pSecondary);
}

CDualSubPicProvider::~CDualSubPicProvider()
{
}

STDMETHODIMP CDualSubPicProvider::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    return
        QI(ISubPicProvider)
        __super::NonDelegatingQueryInterface(riid, ppv);
}

// ISubPicProvider

STDMETHODIMP CDualSubPicProvider::Lock()
{
    // Always lock both providers in the same order to prevent lock-order
    // inversion. In practice both providers usually share the main frame's
    // subtitle lock, which is recursive, so this simply locks it twice.
    HRESULT hr = m_pPrimary->Lock();
    if (SUCCEEDED(hr)) {
        hr = m_pSecondary->Lock();
        if (FAILED(hr)) {
            m_pPrimary->Unlock();
        }
    }
    return hr;
}

STDMETHODIMP CDualSubPicProvider::Unlock()
{
    m_pSecondary->Unlock();
    return m_pPrimary->Unlock();
}

void CDualSubPicProvider::RebuildSegments(REFERENCE_TIME rt, double fps)
{
    m_segments.clear();

    ISubPicProvider* const pProviders[] = { m_pPrimary.p, m_pSecondary.p };

    struct SourceSegment {
        REFERENCE_TIME rtStart, rtStop;
        POSITION pos;
    };
    std::vector<SourceSegment> srcSegments[2];
    std::vector<REFERENCE_TIME> boundaries;

    for (size_t i = 0; i < _countof(pProviders); i++) {
        for (POSITION pos = pProviders[i]->GetStartPosition(rt, fps); pos; pos = pProviders[i]->GetNext(pos)) {
            const REFERENCE_TIME rtStart = pProviders[i]->GetStart(pos, fps);
            const REFERENCE_TIME rtStop = pProviders[i]->GetStop(pos, fps);
            if (rtStart == rt && rtStop == rt + 1) {
                // Degenerate one-tick segment at the current time: this provider
                // (libass) has no enumerable timeline and just renders whatever is
                // visible at the requested time. The merged timeline degrades to
                // the same per-frame behavior: hand out a single segment covering
                // the current frame only, and render both providers into it.
                MergedSegment segment = { rt, rt + 1, nullptr, nullptr };
                for (size_t j = 0; j < _countof(pProviders); j++) {
                    POSITION posNow = pProviders[j]->GetStartPosition(rt, fps);
                    if (posNow && pProviders[j]->GetStart(posNow, fps) <= rt && rt < pProviders[j]->GetStop(posNow, fps)) {
                        (j == 0 ? segment.posPrimary : segment.posSecondary) = posNow;
                    }
                }
                m_segments.push_back(segment);
                return;
            }
            if (rtStart >= rtStop) {
                continue;
            }
            srcSegments[i].push_back({ rtStart, rtStop, pos });
            boundaries.push_back(rtStart);
            boundaries.push_back(rtStop);
            if (rtStart >= rt + LOOKAHEAD_TIME || srcSegments[i].size() >= MAX_SOURCE_SEGMENTS) {
                break;
            }
        }
    }

    if (boundaries.empty()) {
        return;
    }

    std::sort(boundaries.begin(), boundaries.end());
    boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());

    // A provider's own segments are ordered and non-overlapping, so the segment
    // covering a given time can be looked up with a binary search.
    auto findCovering = [](const std::vector<SourceSegment>& segments, REFERENCE_TIME t) -> POSITION {
        auto it = std::upper_bound(segments.cbegin(), segments.cend(), t,
                                   [](REFERENCE_TIME t, const SourceSegment& segment) { return t < segment.rtStart; });
        if (it != segments.cbegin()) {
            --it;
            if (t < it->rtStop) {
                return it->pos;
            }
        }
        return nullptr;
    };

    m_segments.reserve(boundaries.size() - 1);
    for (size_t i = 0; i + 1 < boundaries.size(); i++) {
        const REFERENCE_TIME rtStart = boundaries[i];
        const REFERENCE_TIME rtStop = boundaries[i + 1];
        if (rtStop <= rt) {
            // fully in the past, not interesting for the enumeration
            continue;
        }
        // The boundaries subdivide every source segment, so coverage is uniform
        // within [rtStart, rtStop) and testing the start point is enough.
        MergedSegment segment = { rtStart, rtStop, findCovering(srcSegments[0], rtStart), findCovering(srcSegments[1], rtStart) };
        if (segment.posPrimary || segment.posSecondary) {
            m_segments.push_back(segment);
        }
    }
}

STDMETHODIMP_(POSITION) CDualSubPicProvider::GetStartPosition(REFERENCE_TIME rt, double fps)
{
    CAutoLock cAutoLock(&m_csSegments);
    RebuildSegments(rt, fps);
    return m_segments.empty() ? nullptr : (POSITION)1;
}

STDMETHODIMP_(POSITION) CDualSubPicProvider::GetNext(POSITION pos)
{
    CAutoLock cAutoLock(&m_csSegments);
    const size_t i = (size_t)pos; // 1-based index into m_segments
    return (i < m_segments.size()) ? (POSITION)(i + 1) : nullptr;
}

STDMETHODIMP_(REFERENCE_TIME) CDualSubPicProvider::GetStart(POSITION pos, double fps)
{
    CAutoLock cAutoLock(&m_csSegments);
    const size_t i = (size_t)pos - 1;
    return (i < m_segments.size()) ? m_segments[i].rtStart : 0;
}

STDMETHODIMP_(REFERENCE_TIME) CDualSubPicProvider::GetStop(POSITION pos, double fps)
{
    CAutoLock cAutoLock(&m_csSegments);
    const size_t i = (size_t)pos - 1;
    return (i < m_segments.size()) ? m_segments[i].rtStop : 0;
}

STDMETHODIMP_(bool) CDualSubPicProvider::IsAnimated(POSITION pos)
{
    CAutoLock cAutoLock(&m_csSegments);
    const size_t i = (size_t)pos - 1;
    if (i >= m_segments.size()) {
        return false;
    }
    const MergedSegment& segment = m_segments[i];
    return (segment.posPrimary && m_pPrimary->IsAnimated(segment.posPrimary))
           || (segment.posSecondary && m_pSecondary->IsAnimated(segment.posSecondary));
}

STDMETHODIMP CDualSubPicProvider::Render(SubPicDesc& spd, REFERENCE_TIME rt, double fps, RECT& bbox)
{
    // Render the primary first so that the secondary is composited on top of it.
    CRect bboxPrimary(0, 0, 0, 0), bboxSecondary(0, 0, 0, 0);
    HRESULT hrPrimary = m_pPrimary->Render(spd, rt, fps, bboxPrimary);
    HRESULT hrSecondary = m_pSecondary->Render(spd, rt, fps, bboxSecondary);

    CRect r(0, 0, 0, 0);
    if (SUCCEEDED(hrPrimary)) {
        r |= bboxPrimary;
    }
    if (SUCCEEDED(hrSecondary)) {
        r |= bboxSecondary;
    }
    bbox = r;

    if (FAILED(hrPrimary)) {
        return hrPrimary;
    }
    if (FAILED(hrSecondary)) {
        return hrSecondary;
    }
    return (hrPrimary == S_OK || hrSecondary == S_OK) ? S_OK : S_FALSE;
}

STDMETHODIMP CDualSubPicProvider::GetTextureSize(POSITION pos, SIZE& MaxTextureSize, SIZE& VirtualSize, POINT& VirtualTopLeft)
{
    CAutoLock cAutoLock(&m_csSegments);
    const size_t i = (size_t)pos - 1;
    if (i >= m_segments.size()) {
        return E_NOTIMPL;
    }
    const MergedSegment& segment = m_segments[i];
    // Prefer the primary provider's texture size; the text-based secondary adapts
    // its rendering to whatever subpic size it is given.
    if (segment.posPrimary && SUCCEEDED(m_pPrimary->GetTextureSize(segment.posPrimary, MaxTextureSize, VirtualSize, VirtualTopLeft))) {
        return S_OK;
    }
    if (segment.posSecondary) {
        return m_pSecondary->GetTextureSize(segment.posSecondary, MaxTextureSize, VirtualSize, VirtualTopLeft);
    }
    return E_NOTIMPL;
}

STDMETHODIMP CDualSubPicProvider::GetRelativeTo(POSITION pos, RelativeTo& relativeTo)
{
    CAutoLock cAutoLock(&m_csSegments);
    const size_t i = (size_t)pos - 1;
    if (i < m_segments.size()) {
        const MergedSegment& segment = m_segments[i];
        if (segment.posPrimary && SUCCEEDED(m_pPrimary->GetRelativeTo(segment.posPrimary, relativeTo))) {
            return S_OK;
        }
        if (segment.posSecondary && SUCCEEDED(m_pSecondary->GetRelativeTo(segment.posSecondary, relativeTo))) {
            return S_OK;
        }
    }
    relativeTo = WINDOW;
    return S_OK;
}
