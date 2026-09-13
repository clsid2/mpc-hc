/*
 * (C) 2006-2017 see Authors.txt
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
#include "PGSSub.h"
#include "../DSUtil/GolombBuffer.h"
#include <vector>
#include <algorithm>
#include "SubtitleHelpers.h"

#if (0) // Set to 1 to activate PGS subtitles traces
#define TRACE_PGSSUB TRACE
#else
#define TRACE_PGSSUB __noop
#endif


CPGSSub::CPGSSub(CCritSec* pLock, const CString& name, LCID lcid)
    : CRLECodedSubtitle(pLock, name, lcid)
    , m_nCurSegment(NO_SEGMENT)
    , m_pSegBuffer(nullptr)
    , m_nTotalSegBuffer(0)
    , m_nSegBufferPos(0)
    , m_nSegSize(0)
{
    if (m_name.IsEmpty() || m_name == _T("Unknown")) {
        m_name = _T("PGS Embedded Subtitle");
    }
}

CPGSSub::~CPGSSub()
{
    Reset();

    delete [] m_pSegBuffer;
}

// ISubPicProvider

STDMETHODIMP_(POSITION) CPGSSub::GetStartPosition(REFERENCE_TIME rt, double fps)
{
    CAutoLock cAutoLock(&m_csCritSec);

    POSITION pos = m_pPresentationSegments.GetHeadPosition();
    while (pos) {
        const auto& pPresentationSegment = m_pPresentationSegments.GetAt(pos);
        if (pPresentationSegment->rtStop <= rt) {
            m_pPresentationSegments.GetNext(pos);
        } else {
            break;
        }
    }

    return pos;
}

STDMETHODIMP_(POSITION) CPGSSub::GetNext(POSITION pos)
{
    CAutoLock cAutoLock(&m_csCritSec);
    m_pPresentationSegments.GetNext(pos);
    return pos;
}

STDMETHODIMP_(REFERENCE_TIME) CPGSSub::GetStart(POSITION pos, double fps)
{
    const auto& pPresentationSegment = m_pPresentationSegments.GetAt(pos);
    return pPresentationSegment ? pPresentationSegment->rtStart : INVALID_TIME;
}

STDMETHODIMP_(REFERENCE_TIME) CPGSSub::GetStop(POSITION pos, double fps)
{
    const auto& pPresentationSegment = m_pPresentationSegments.GetAt(pos);
    return pPresentationSegment ? pPresentationSegment->rtStop : INVALID_TIME;
}

STDMETHODIMP_(bool) CPGSSub::IsAnimated(POSITION pos)
{
    const auto& pPresentationSegment = m_pPresentationSegments.GetAt(pos);
    // If the end time isn't known yet, we consider the subtitle as animated to be sure it will properly updated
    return (pPresentationSegment && pPresentationSegment->rtStop == UNKNOWN_TIME);
}

STDMETHODIMP CPGSSub::Render(SubPicDesc& spd, REFERENCE_TIME rt, double fps, RECT& bbox)
{
    return Render(spd, rt, bbox, true);
}

STDMETHODIMP CPGSSub::GetTextureSize(POSITION pos, SIZE& MaxTextureSize, SIZE& VideoSize, POINT& VideoTopLeft)
{
    const auto& pPresentationSegment = m_pPresentationSegments.GetAt(pos);
    if (pPresentationSegment) {
        MaxTextureSize.cx = VideoSize.cx = pPresentationSegment->video_descriptor.nVideoWidth;
        MaxTextureSize.cy = VideoSize.cy = pPresentationSegment->video_descriptor.nVideoHeight;

        // The subs will be directly rendered into the proper position!
        VideoTopLeft.x = 0; //pObject->m_horizontal_position;
        VideoTopLeft.y = 0; //pObject->m_vertical_position;

        return S_OK;
    }

    ASSERT(FALSE);
    return E_INVALIDARG;
}

HRESULT CPGSSub::ParseSample(REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, BYTE* pData, size_t nLen)
{
    CheckPointer(pData, E_POINTER);

    CAutoLock cAutoLock(&m_csCritSec);

    CGolombBuffer sampleBuffer(pData, nLen);

    while (!sampleBuffer.IsEOF()) {
        if (m_nCurSegment == NO_SEGMENT) {
            HDMV_SEGMENT_TYPE nSegType = (HDMV_SEGMENT_TYPE)sampleBuffer.ReadByte();
            unsigned short nUnitSize = sampleBuffer.ReadShort();
            nLen -= 3;

            switch (nSegType) {
                case PALETTE:
                case OBJECT:
                case PRESENTATION_SEG:
                case END_OF_DISPLAY:
                    m_nCurSegment = nSegType;
                    AllocSegment(nUnitSize);
                    break;

                case WINDOW_DEF:
                case INTERACTIVE_SEG:
                case HDMV_SUB1:
                case HDMV_SUB2:
                    // Ignored stuff...
                    sampleBuffer.SkipBytes(nUnitSize);
                    break;
                default:
                    return VFW_E_SAMPLE_REJECTED;
            }
        }

        if (m_nCurSegment != NO_SEGMENT) {
            if (m_nSegBufferPos < m_nSegSize) {
                size_t nSize = std::min(m_nSegSize - m_nSegBufferPos, nLen);
                sampleBuffer.ReadBuffer(m_pSegBuffer + m_nSegBufferPos, nSize);
                m_nSegBufferPos += nSize;
            }

            if (m_nSegBufferPos >= m_nSegSize) {
                CGolombBuffer SegmentBuffer(m_pSegBuffer, m_nSegSize);

                switch (m_nCurSegment) {
                    case PALETTE:
                        TRACE_PGSSUB(_T("CPGSSub:PALETTE            %s\n"), ReftimeToString(rtStart));
                        ParsePalette(&SegmentBuffer, m_nSegSize);
                        break;
                    case OBJECT:
                        TRACE_PGSSUB(_T("CPGSSub:OBJECT             %s\n"), ReftimeToString(rtStart));
                        ParseObject(&SegmentBuffer, m_nSegSize);
                        break;
                    case PRESENTATION_SEG:
                        TRACE_PGSSUB(_T("CPGSSub:PRESENTATION_SEG   %s (size=%d)\n"), ReftimeToString(rtStart), m_nSegSize);

                        if (rtStart == INVALID_TIME) {
                            break;
                        }

                        // Update the timestamp for the previous segment
                        UpdateTimeStamp(rtStart);

                        // Parse the new presentation segment
                        ParsePresentationSegment(rtStart, &SegmentBuffer);

                        break;
                    case WINDOW_DEF:
                        //TRACE_PGSSUB(_T("CPGSSub:WINDOW_DEF         %s\n"), ReftimeToString(rtStart));
                        break;
                    case END_OF_DISPLAY:
                        TRACE_PGSSUB(_T("CPGSSub:END_OF_DISPLAY     %s\n"), ReftimeToString(rtStart));
                        // Enqueue the current presentation segment if any
                        EnqueuePresentationSegment();
                        break;
                    default:
                        TRACE_PGSSUB(_T("CPGSSub:UNKNOWN Seg %d     %s\n"), m_nCurSegment, ReftimeToString(rtStart));
                }

                m_nCurSegment = NO_SEGMENT;
            }
        }
    }

    return S_OK;
}

void CPGSSub::Reset()
{
    CAutoLock cAutoLock(&m_csCritSec);

    m_nSegBufferPos = m_nSegSize = 0;
    m_nCurSegment = NO_SEGMENT;
    m_pCurrentPresentationSegment.Free();
    m_pPresentationSegments.RemoveAll();
    for (auto& compositionObject : m_compositionObjects) {
        compositionObject.Reset();
    }
}

void CPGSSub::AllocSegment(size_t nSize)
{
    if (nSize > m_nTotalSegBuffer) {
        delete[] m_pSegBuffer;
        m_pSegBuffer = DEBUG_NEW BYTE[nSize];
        m_nTotalSegBuffer = nSize;
    }
    m_nSegBufferPos = 0;
    m_nSegSize = nSize;
}

HRESULT CPGSSub::GetPresentationSegmentTextureSize(REFERENCE_TIME rt, CSize& size) {
    POSITION posPresentationSegment = FindPresentationSegment(rt);

    if (posPresentationSegment) {
        const auto& pPresentationSegment = m_pPresentationSegments.GetAt(posPresentationSegment);
        if (pPresentationSegment->video_descriptor.nVideoWidth > 0) {
            size.cx = pPresentationSegment->video_descriptor.nVideoWidth;
            size.cy = pPresentationSegment->video_descriptor.nVideoHeight;
            return S_OK;
        }
    }
    return E_FAIL;
}

HRESULT CPGSSub::Render(SubPicDesc& spd, REFERENCE_TIME rt, RECT& bbox, bool bRemoveOldSegments)
{
    CAutoLock cAutoLock(&m_csCritSec);

    bool bRendered = false;

    if (bRemoveOldSegments) {
        RemoveOldSegments(rt);
    }

    POSITION posPresentationSegment = FindPresentationSegment(rt);

    if (posPresentationSegment) {
        const auto& pPresentationSegment = m_pPresentationSegments.GetAt(posPresentationSegment);

        if (m_eSourceMatrix == ColorConvTable::AUTO) {
            m_eSourceMatrix = (pPresentationSegment->video_descriptor.nVideoWidth > 720) ? ColorConvTable::BT709 : ColorConvTable::BT601;
        }

        TRACE_PGSSUB(_T("CPGSSub:Render Presentation segment %d --> %s - %s\n"), pPresentationSegment->composition_descriptor.nNumber,
                     ReftimeToString(pPresentationSegment->rtStart),
                     pPresentationSegment->rtStop == UNKNOWN_TIME ? _T("?") : ReftimeToString(pPresentationSegment->rtStop).GetString());

        bbox.left = bbox.top = LONG_MAX;
        bbox.right = bbox.bottom = 0;

        for (const auto& pObject : pPresentationSegment->objects) {
            if (pObject->GetRLEDataSize() && pObject->m_width > 0 && pObject->m_height > 0
                    && spd.w >= (pObject->m_horizontal_position + pObject->m_width) && spd.h >= (pObject->m_vertical_position + pObject->m_height)) {
                pObject->SetPalette(pPresentationSegment->CLUT.size, pPresentationSegment->CLUT.palette.data(), m_eSourceMatrix);
                bbox.left = std::min(pObject->m_horizontal_position, bbox.left);
                bbox.top = std::min(pObject->m_vertical_position, bbox.top);
                bbox.right = std::max(pObject->m_horizontal_position + pObject->m_width, bbox.right);
                bbox.bottom = std::max(pObject->m_vertical_position + pObject->m_height, bbox.bottom);

                TRACE_PGSSUB(_T(" --> Object %d (Pos=%dx%d, Res=%dx%d, SPDRes=%dx%d)\n"),
                             pObject->m_object_id_ref, pObject->m_horizontal_position, pObject->m_vertical_position, pObject->m_width, pObject->m_height, spd.w, spd.h);
                pObject->RenderHdmv(spd);

                bRendered = true;
            } else {
                TRACE_PGSSUB(_T(" --> Invalid object %d\n"), pObject->m_object_id_ref);
            }
        }
    }

    if (!bRendered) {
        bbox = { 0, 0, 0, 0 };
    }

    return S_OK;
}

int CPGSSub::ParsePresentationSegment(REFERENCE_TIME rt, CGolombBuffer* pGBuffer)
{
    if (pGBuffer->RemainingSize() < 11) {
        ASSERT(FALSE);
        return 0;
    }

    m_pCurrentPresentationSegment.Free();
    m_pCurrentPresentationSegment.Attach(DEBUG_NEW HDMV_PRESENTATION_SEGMENT());

    m_pCurrentPresentationSegment->rtStart = rt;
    m_pCurrentPresentationSegment->rtStop = UNKNOWN_TIME; // Unknown for now

    ParseVideoDescriptor(pGBuffer, &m_pCurrentPresentationSegment->video_descriptor);
    ParseCompositionDescriptor(pGBuffer, &m_pCurrentPresentationSegment->composition_descriptor);
    m_pCurrentPresentationSegment->palette_update_flag = !!(pGBuffer->ReadByte() & 0x80);
    m_pCurrentPresentationSegment->CLUT.id = pGBuffer->ReadByte();
    m_pCurrentPresentationSegment->objectCount = pGBuffer->ReadByte();

    TRACE_PGSSUB(_T("CPGSSub::ParsePresentationSegment Size = %d, state = %#x, nObjectNumber = %d\n"), pGBuffer->GetSize(),
                 m_pCurrentPresentationSegment->composition_descriptor.bState, m_pCurrentPresentationSegment->objectCount);

    if (pGBuffer->RemainingSize() < (m_pCurrentPresentationSegment->objectCount * 8)) {
        ASSERT(FALSE);
        return 0;
    }

    for (int i = 0; i < m_pCurrentPresentationSegment->objectCount; i++) {
        std::unique_ptr<CompositionObject> pCompositionObject(DEBUG_NEW CompositionObject());
        if (ParseCompositionObject(pGBuffer, pCompositionObject)) {
            m_pCurrentPresentationSegment->objects.emplace_back(std::move(pCompositionObject));
        }
    }

    return m_pCurrentPresentationSegment->objectCount;
}

void CPGSSub::EnqueuePresentationSegment()
{
    if (m_pCurrentPresentationSegment) {
        if (m_pCurrentPresentationSegment->objectCount > 0) {
            m_pCurrentPresentationSegment->CLUT = m_CLUTs[m_pCurrentPresentationSegment->CLUT.id];

            // Get the objects' data
            for (auto& pObject : m_pCurrentPresentationSegment->objects) {
                const CompositionObject& pObjectData = m_compositionObjects[pObject->m_object_id_ref];

                pObject->m_width = pObjectData.m_width;
                pObject->m_height = pObjectData.m_height;

                if (pObjectData.GetRLEData()) {
                    pObject->SetRLEData(pObjectData.GetRLEData(), pObjectData.GetRLEPos(), pObjectData.GetRLEDataSize());
                }
            }

            TRACE_PGSSUB(_T("CPGSSub: Enqueue Presentation Segment %d - %s => ?\n"), m_pCurrentPresentationSegment->composition_descriptor.nNumber,
                         ReftimeToString(m_pCurrentPresentationSegment->rtStart));
            m_pPresentationSegments.AddTail(m_pCurrentPresentationSegment);
        } else {
            TRACE_PGSSUB(_T("CPGSSub: Delete empty Presentation Segment %d\n"), m_pCurrentPresentationSegment->composition_descriptor.nNumber);
            m_pCurrentPresentationSegment.Free();
        }
    }
}

void CPGSSub::UpdateTimeStamp(REFERENCE_TIME rtStop)
{
    if (!m_pPresentationSegments.IsEmpty()) {
        const auto& pPresentationSegment = m_pPresentationSegments.GetTail();

        // Since we drop empty segments we might be trying to update a segment that isn't
        // in the queue so we update the timestamp only if it was previously unknown.
        if (pPresentationSegment->rtStop == UNKNOWN_TIME) {
            pPresentationSegment->rtStop = rtStop;

            TRACE_PGSSUB(_T("CPGSSub: Update Presentation Segment TimeStamp %d - %s => %s\n"), pPresentationSegment->composition_descriptor.nNumber,
                         ReftimeToString(pPresentationSegment->rtStart),
                         ReftimeToString(pPresentationSegment->rtStop));
        }
    }
}

void CPGSSub::ParsePalette(CGolombBuffer* pGBuffer, size_t nSize)  // #497
{
    if (nSize < 2 || (nSize - 2) % sizeof(HDMV_PALETTE) != 0) {
        ASSERT(FALSE);
        return;
    }

    BYTE palette_id = pGBuffer->ReadByte();
    HDMV_CLUT& CLUT = m_CLUTs[palette_id];

    CLUT.id = palette_id;
    CLUT.version_number = pGBuffer->ReadByte();

    // the entry count comes from the segment length, the palette holds 256 entries at most
    size_t nEntries = (nSize - 2) / sizeof(HDMV_PALETTE);
    if (nEntries > CLUT.palette.size()) {
        ASSERT(FALSE);
        nEntries = CLUT.palette.size();
    }
    CLUT.size = WORD(nEntries);

    for (WORD i = 0; i < CLUT.size; i++) {
        CLUT.palette[i].entry_id = pGBuffer->ReadByte();

        CLUT.palette[i].Y  = pGBuffer->ReadByte();
        CLUT.palette[i].Cr = pGBuffer->ReadByte();
        CLUT.palette[i].Cb = pGBuffer->ReadByte();
        CLUT.palette[i].T  = pGBuffer->ReadByte();
    }
}

void CPGSSub::ParseObject(CGolombBuffer* pGBuffer, size_t nUnitSize)   // #498
{
    if (nUnitSize <= 4) {
        return;
    }
    short object_id = pGBuffer->ReadShort();
    if (object_id < 0 || size_t(object_id) >= m_compositionObjects.size()) {
        ASSERT(FALSE); // This is not supposed to happen
        return;
    }

    CompositionObject& pObject = m_compositionObjects[object_id];

    pObject.m_version_number = pGBuffer->ReadByte();
    BYTE m_sequence_desc = pGBuffer->ReadByte();

    if (m_sequence_desc & 0x80) {
        if (nUnitSize <= 8) {
            return;
        }

        int object_data_length = (int)pGBuffer->BitRead(24);

        pObject.m_width = pGBuffer->ReadShort();
        pObject.m_height = pGBuffer->ReadShort();

        pObject.SetRLEData(pGBuffer->GetBufferPos(), nUnitSize - 11, object_data_length - 4);

        TRACE_PGSSUB(_T("CPGSSub:ParseObject %d (size=%ld, %dx%d)\n"), object_id, object_data_length, pObject.m_width, pObject.m_height);
    } else {
        pObject.AppendRLEData(pGBuffer->GetBufferPos(), nUnitSize - 4);
    }
}

bool CPGSSub::ParseCompositionObject(CGolombBuffer* pGBuffer, const std::unique_ptr<CompositionObject>& pCompositionObject)
{
    short object_id_ref = pGBuffer->ReadShort();
    if (object_id_ref < 0 || size_t(object_id_ref) >= m_compositionObjects.size()) {
        ASSERT(FALSE); // This is not supposed to happen
        return false;
    }

    pCompositionObject->m_object_id_ref = object_id_ref;
    pCompositionObject->m_window_id_ref = pGBuffer->ReadByte();
    BYTE bTemp = pGBuffer->ReadByte();
    pCompositionObject->m_object_cropped_flag = !!(bTemp & 0x80);
    pCompositionObject->m_forced_on_flag = !!(bTemp & 0x40);
    pCompositionObject->m_horizontal_position = pGBuffer->ReadShort();
    pCompositionObject->m_vertical_position = pGBuffer->ReadShort();

    if (pCompositionObject->m_horizontal_position < 0) {
        TRACE(_T("PGS - negative horizontal position.\n"));
        pCompositionObject->m_horizontal_position = 0;
    }
    if (pCompositionObject->m_vertical_position < 0) {
        TRACE(_T("PGS - negative vertical position.\n"));
        pCompositionObject->m_vertical_position = 0;
    }

    if (pCompositionObject->m_object_cropped_flag) {
        if (pGBuffer->RemainingSize() < 8) {
            ASSERT(FALSE);
            return false;
        }
        pCompositionObject->m_cropping_horizontal_position = pGBuffer->ReadShort();
        pCompositionObject->m_cropping_vertical_position = pGBuffer->ReadShort();
        pCompositionObject->m_cropping_width = pGBuffer->ReadShort();
        pCompositionObject->m_cropping_height = pGBuffer->ReadShort();
    }

    return true;
}

void CPGSSub::ParseVideoDescriptor(CGolombBuffer* pGBuffer, VIDEO_DESCRIPTOR* pVideoDescriptor)
{
    pVideoDescriptor->nVideoWidth = pGBuffer->ReadShort();
    pVideoDescriptor->nVideoHeight = pGBuffer->ReadShort();
    pVideoDescriptor->bFrameRate = pGBuffer->ReadByte();
}

void CPGSSub::ParseCompositionDescriptor(CGolombBuffer* pGBuffer, COMPOSITION_DESCRIPTOR* pCompositionDescriptor)
{
    pCompositionDescriptor->nNumber = pGBuffer->ReadShort();
    pCompositionDescriptor->bState  = pGBuffer->ReadByte() >> 6;
}

POSITION CPGSSub::FindPresentationSegment(REFERENCE_TIME rt) const
{
    POSITION pos = m_pPresentationSegments.GetHeadPosition();

    while (pos) {
        POSITION currentPos = pos;
        const auto& pPresentationSegment = m_pPresentationSegments.GetNext(pos);

        if (pPresentationSegment->rtStart <= rt && pPresentationSegment->rtStop > rt) {
            return currentPos;
        }
    }

    return nullptr;
}

void CPGSSub::RemoveOldSegments(REFERENCE_TIME rt)
{
    // Cleanup the old presentation segments. We keep a 2 min buffer to play nice with the queue.
    while (!m_pPresentationSegments.IsEmpty()
            && m_pPresentationSegments.GetHead()->rtStop != UNKNOWN_TIME
            && m_pPresentationSegments.GetHead()->rtStop + 120 * 10000000i64 < rt) {
        TRACE_PGSSUB(_T("CPGSSub::RemoveOldSegments Remove presentation segment %d %s => %s (rt=%s)\n"),
                     m_pPresentationSegments.GetHead()->composition_descriptor.nNumber,
                     ReftimeToString(m_pPresentationSegments.GetHead()->rtStart),
                     ReftimeToString(m_pPresentationSegments.GetHead()->rtStop),
                     ReftimeToString(rt));
        m_pPresentationSegments.RemoveHeadNoReturn();
    }
}

STDMETHODIMP CPGSSub::GetRelativeTo(POSITION pos, RelativeTo& relativeTo)
{
    relativeTo = RelativeTo::BEST_FIT;
    return S_OK;
}

CPGSSubFile::CPGSSubFile(CCritSec* pLock)
    : CPGSSub(pLock, _T("PGS External Subtitle"), 0)
    , m_bStopParsing(false)
{
}

CPGSSubFile::~CPGSSubFile()
{
    m_bStopParsing = true;
    if (m_parsingThread.joinable()) {
        m_parsingThread.join();
    }
}

STDMETHODIMP CPGSSubFile::Render(SubPicDesc& spd, REFERENCE_TIME rt, double fps, RECT& bbox)
{
    return __super::Render(spd, rt, bbox, false);
}

bool CPGSSubFile::Open(CString fn, CString name /*= _T("")*/, CString videoName /*= _T("")*/)
{
    bool bOpened = false;

    CString tmp;
    CString guessed = Subtitle::GuessSubtitleName(fn, videoName, m_lcid, tmp, m_eHearingImpaired);
    if (name.IsEmpty()) {
        m_name = guessed;
    } else {
        m_name = name;
    }

    m_path = fn;

    try {
        CFile f;
        if (f.Open(fn, CFile::modeRead | CFile::shareDenyWrite)) {
            WORD wSyncCode = 0;
            f.Read(&wSyncCode, sizeof(wSyncCode));
            wSyncCode = _byteswap_ushort(wSyncCode);
            if (wSyncCode == PGS_SYNC_CODE) {
                m_parsingThread = std::thread([this, fn] { ParseFile(fn); });
                bOpened = true;
            }
        }
    } catch (CFileException*) {
    }

    return bOpened;
}

CString CPGSSubFile::GetPath() {
    return m_path;
}

void CPGSSubFile::ParseFile(CString fn)
{
    CFile f;
    if (!f.Open(fn, CFile::modeRead | CFile::shareDenyWrite)) {
        return;
    }

    // Header: Sync code | start time | stop time | segment type | segment size
    std::array < BYTE, 2 + 2 * 4 + 1 + 2 > header;
    const int nExtraSize = 1 + 2; // segment type + segment size
    std::vector<BYTE> segBuff;

    while (!m_bStopParsing && f.Read(header.data(), (UINT)header.size()) == header.size()) {
        // Parse the header
        CGolombBuffer headerBuffer(header.data(), (int)header.size());

        if (WORD(headerBuffer.ReadShort()) != PGS_SYNC_CODE) {
            break;
        }

        REFERENCE_TIME rtStart = REFERENCE_TIME(headerBuffer.ReadDword()) * 1000 / 9;
        REFERENCE_TIME rtStop  = REFERENCE_TIME(headerBuffer.ReadDword()) * 1000 / 9;
        headerBuffer.ReadByte(); // segment type
        WORD wLenSegment = (WORD)headerBuffer.ReadShort();

        // Leave some room to add the segment type and size
        int nLenData = nExtraSize + wLenSegment;
        segBuff.resize(nLenData);
        memcpy(segBuff.data(), &header[header.size() - nExtraSize], nExtraSize);

        // Read the segment
        if (wLenSegment && f.Read(&segBuff[nExtraSize], wLenSegment) != wLenSegment) {
            break;
        }

        // Parse the data (even if the segment size is 0 because the header itself is important)
        TRACE_PGSSUB(_T("--------- CPGSSubFile::ParseFile rtStart=%s, rtStop=%s, len=%d ---------\n"),
                     ReftimeToString(rtStart), ReftimeToString(rtStop), nLenData);
        ParseSample(rtStart, rtStop, segBuff.data(), nLenData);
    }
}
