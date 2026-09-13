/*
 * (C) 2009-2016 see Authors.txt
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
#include "DVBSub.h"
#include "../DSUtil/GolombBuffer.h"
#include <algorithm>

#if (0) // Set to 1 to activate DVB subtitles traces
#define TRACE_DVB TRACE
#else
#define TRACE_DVB __noop
#endif

#define BUFFER_CHUNK_GROW 0x1000

CDVBSub::CDVBSub(CCritSec* pLock, const CString& name, LCID lcid)
    : CRLECodedSubtitle(pLock, name, lcid)
    , m_nBufferSize(0)
    , m_nBufferReadPos(0)
    , m_nBufferWritePos(0)
    , m_pBuffer(nullptr)
{
    if (m_name.IsEmpty() || m_name == _T("Unknown")) {
        m_name = _T("DVB Embedded Subtitle");
    }
}

CDVBSub::~CDVBSub()
{
    Reset();
    SAFE_DELETE(m_pBuffer);
}

// ISubPicProvider

STDMETHODIMP_(POSITION) CDVBSub::GetStartPosition(REFERENCE_TIME rt, double fps)
{
    CAutoLock cAutoLock(&m_csCritSec);

    POSITION pos = m_pages.GetHeadPosition();
    while (pos) {
        const auto& pPage = m_pages.GetAt(pos);
        if (pPage->rtStop <= rt) {
            m_pages.GetNext(pos);
        } else {
            break;
        }
    }

    return pos;
}

STDMETHODIMP_(POSITION) CDVBSub::GetNext(POSITION pos)
{
    CAutoLock cAutoLock(&m_csCritSec);
    m_pages.GetNext(pos);
    return pos;
}

STDMETHODIMP_(REFERENCE_TIME) CDVBSub::GetStart(POSITION pos, double fps)
{
    const auto& pPage = m_pages.GetAt(pos);
    return pPage ? pPage->rtStart : INVALID_TIME;
}

STDMETHODIMP_(REFERENCE_TIME) CDVBSub::GetStop(POSITION pos, double fps)
{
    const auto& pPage = m_pages.GetAt(pos);
    return pPage ? pPage->rtStop : INVALID_TIME;
}

STDMETHODIMP_(bool) CDVBSub::IsAnimated(POSITION pos)
{
    return false;
}

STDMETHODIMP CDVBSub::Render(SubPicDesc& spd, REFERENCE_TIME rt, double fps, RECT& bbox)
{
    CAutoLock cAutoLock(&m_csCritSec);

    RemoveOldPages(rt);

    if (POSITION posPage = FindPage(rt)) {
        const auto& pPage = m_pages.GetAt(posPage);
        if (m_eSourceMatrix == ColorConvTable::AUTO) {
            m_eSourceMatrix = (m_displayInfo.width > 720) ? ColorConvTable::BT709 : ColorConvTable::BT601;
        }

        pPage->rendered = true;
        TRACE_DVB(_T("DVB - Renderer - %s - %s\n"), ReftimeToString(pPage->rtStart), ReftimeToString(pPage->rtStop));

        size_t nRegion = 1;
        for (const auto& regionPos : pPage->regionsPos) {
            auto itRegion = FindRegion(pPage, regionPos.id);
            if (itRegion != pPage->regions.cend()) {
                const auto& pRegion = *itRegion;
                auto itCLUT = FindClut(pPage, pRegion->CLUT_id);

                if (itCLUT != pPage->CLUTs.cend()) {
                    const auto& pCLUT = *itCLUT;

                    size_t nObject = 1;
                    for (const auto& objectPos : pRegion->objects) {
                        auto itObject = FindObject(pPage, objectPos.object_id);

                        if (itObject != pPage->objects.cend()) {
                            const auto& pObject = *itObject;

                            short nX = regionPos.horizAddr + objectPos.object_horizontal_position;
                            short nY = regionPos.vertAddr + objectPos.object_vertical_position;
                            pObject->m_width = pRegion->width;
                            pObject->m_height = pRegion->height;
                            pObject->SetPalette(pCLUT->size, pCLUT->palette.data(), m_eSourceMatrix);
                            pObject->RenderDvb(spd, nX, nY);

                            TRACE_DVB(_T(" --> %Iu/%Iu - %Iu/%Iu\n"), nRegion, pPage->regionsPos.size(), nObject, pRegion->objects.size());
                        }

                        nObject++;
                    }
                }
            }

            nRegion++;
        }

        bbox.left = 0;
        bbox.top = 0;
        bbox.right = m_displayInfo.width;
        bbox.bottom = m_displayInfo.height;
    }

    return S_OK;
}

HRESULT CDVBSub::GetTextureSize(POSITION pos, SIZE& MaxTextureSize, SIZE& VideoSize, POINT& VideoTopLeft)
{
    MaxTextureSize.cx = VideoSize.cx = m_displayInfo.width;
    MaxTextureSize.cy = VideoSize.cy = m_displayInfo.height;

    VideoTopLeft.x = 0;
    VideoTopLeft.y = 0;

    return S_OK;
}

#define MARKER                \
    if (gb.BitRead(1) != 1) { \
        ASSERT(FALSE);        \
        return E_FAIL;        \
    }

HRESULT CDVBSub::ParseSample(REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, BYTE* pData, size_t nLen)
{
    CheckPointer(pData, E_POINTER);

    CAutoLock cAutoLock(&m_csCritSec);

    HRESULT hr;
    DVB_SEGMENT_TYPE nCurSegment;


    if (*((LONG*)pData) == 0xBD010000) {
        CGolombBuffer gb(pData, nLen);

        size_t headerSize = 9;

        gb.SkipBytes(4);
        WORD wLength = (WORD)gb.BitRead(16);
        UNREFERENCED_PARAMETER(wLength);

        if (gb.BitRead(2) != 2) {
            return E_FAIL;  // type
        }

        gb.BitRead(2);      // scrambling
        gb.BitRead(1);      // priority
        gb.BitRead(1);      // alignment
        gb.BitRead(1);      // copyright
        gb.BitRead(1);      // original
        BYTE fpts = (BYTE)gb.BitRead(1);    // fpts
        BYTE fdts = (BYTE)gb.BitRead(1);    // fdts
        gb.BitRead(1);      // escr
        gb.BitRead(1);      // esrate
        gb.BitRead(1);      // dsmtrickmode
        gb.BitRead(1);      // morecopyright
        gb.BitRead(1);      // crc
        gb.BitRead(1);      // extension
        gb.BitRead(8);      // hdrlen

        if (fpts) {
            BYTE b = (BYTE)gb.BitRead(4);
            if (!(fdts && b == 3 || !fdts && b == 2)) {
                ASSERT(FALSE);
                return E_FAIL;
            }

            REFERENCE_TIME pts = 0;
            pts |= gb.BitRead(3) << 30;
            MARKER; // 32..30
            pts |= gb.BitRead(15) << 15;
            MARKER; // 29..15
            pts |= gb.BitRead(15);
            MARKER; // 14..0
            pts = 10000 * pts / 90;

            TRACE_DVB(_T("DVB - ParseSample: Received a packet with a presentation timestamp PTS=%s\n"), ReftimeToString(pts));
            if (pts != rtStart) {
                TRACE_DVB(_T("DVB - ParseSample: WARNING: The parsed PTS doesn't match the sample start time (%s)\n"), ReftimeToString(rtStart));
                ASSERT(FALSE);
                rtStart = pts;
            }

            headerSize += 5;
        }

        nLen  -= headerSize;
        pData += headerSize;
    }

    hr = AddToBuffer(pData, nLen);
    if (hr == S_OK) {
        CGolombBuffer gb(m_pBuffer + m_nBufferReadPos, m_nBufferWritePos - m_nBufferReadPos);
        size_t nLastPos = 0;

        while (gb.RemainingSize() >= 6) { // Ensure there is enough data to parse the entire segment header
            if (gb.ReadByte() == 0x0F) {
                TRACE_DVB(_T("DVB - ParseSample\n"));

                WORD wPageId;
                WORD wSegLength;

                nCurSegment = (DVB_SEGMENT_TYPE)gb.ReadByte();
                wPageId = gb.ReadShort();
                wSegLength = gb.ReadShort();

                if (gb.RemainingSize() < wSegLength) {
                    TRACE_DVB(_T("DVB - Full segment isn't availabled yet, delaying parsing (%Iu/%hu)\n"), gb.RemainingSize(), wSegLength);
                    hr = S_FALSE;
                    break;
                }

                hr = S_OK;
                switch (nCurSegment) {
                    case PAGE: {
                        if (rtStart == INVALID_TIME) {
                            TRACE_DVB(_T("DVB - Page update ignored, %s\n"), ReftimeToString(rtStart));
                            break;
                        }

                        if (m_pCurrentPage) {
                            TRACE_DVB(_T("DVB - Force End display\n"));
                            EnqueuePage(rtStart);
                        }
                        UpdateTimeStamp(rtStart);

                        CAutoPtr<DVB_PAGE> pPage;
                        hr = ParsePage(gb, wSegLength, pPage);
                        pPage->rtStart = rtStart;
                        pPage->rtStop = pPage->rtStart + pPage->pageTimeOut * 10000000i64;

                        if (FAILED(hr)) {
                            pPage.Free();
                        } else if (pPage->pageState == DPS_ACQUISITION || pPage->pageState == DPS_MODE_CHANGE) {
                            m_pCurrentPage = pPage;

                            TRACE_DVB(_T("DVB - Page started [pageState = %d] %s, TimeOut = %ds\n"), m_pCurrentPage->pageState,
                                      ReftimeToString(m_pCurrentPage->rtStart), m_pCurrentPage->pageTimeOut);
                        } else if (pPage->pageState == DPS_NORMAL && !m_pages.IsEmpty()) {
                            m_pCurrentPage = pPage;

                            // Copy data from the previous page
                            const auto& pPrevPage = m_pages.GetTail();

                            for (const auto& region : pPrevPage->regions) {
                                m_pCurrentPage->regions.emplace_back(DEBUG_NEW DVB_REGION(*region));
                            }

                            for (const auto& object : pPrevPage->objects) {
                                m_pCurrentPage->objects.emplace_back(DEBUG_NEW CompositionObject(*object));
                            }

                            for (const auto& CLUT : pPrevPage->CLUTs) {
                                m_pCurrentPage->CLUTs.emplace_back(DEBUG_NEW DVB_CLUT(*CLUT));
                            }

                            TRACE_DVB(_T("DVB - Page started [update] %s, TimeOut = %ds\n"),
                                      ReftimeToString(m_pCurrentPage->rtStart), m_pCurrentPage->pageTimeOut);
                        } else {
                            TRACE_DVB(_T("DVB - Page update ignored %s\n"), ReftimeToString(rtStart));
                        }
                    }
                    break;
                    case REGION:
                        ParseRegion(gb, wSegLength);
                        TRACE_DVB(_T("DVB - Region\n"));
                        break;
                    case CLUT:
                        ParseClut(gb, wSegLength);
                        TRACE_DVB(_T("DVB - Clut\n"));
                        break;
                    case OBJECT:
                        ParseObject(gb, wSegLength);
                        TRACE_DVB(_T("DVB - Object\n"));
                        break;
                    case DISPLAY:
                        ParseDisplay(gb, wSegLength);
                        TRACE_DVB(_T("DVB - Display\n"));
                        break;
                    case END_OF_DISPLAY:
                        if (m_pCurrentPage == nullptr) {
                            TRACE_DVB(_T("DVB - Ignored End display %s: no current page\n"), ReftimeToString(rtStart));
                        } else if (m_pCurrentPage->rtStart < rtStart) {
                            TRACE_DVB(_T("DVB - End display\n"));
                            EnqueuePage(rtStart);
                        } else {
                            TRACE_DVB(_T("DVB - Ignored End display %s: no information on page duration\n"), ReftimeToString(rtStart));
                        }
                        break;
                    default:
                        TRACE_DVB(_T("DVB - Ignored segment with unknown type %d\n"), nCurSegment);
                        break;
                }
                if (FAILED(hr)) {
                    gb.SkipBytes(6 + wSegLength - (gb.GetPos() - nLastPos));
                    TRACE_DVB(_T("Parsing failed with code %x, skipping to the end of the segment\n"), hr);
                }
            }
            nLastPos = gb.GetPos();
        }
        m_nBufferReadPos += nLastPos;
    }

    return hr;
}

void CDVBSub::EndOfStream()
{
    CAutoLock cAutoLock(&m_csCritSec);

    // Enqueue the last page if necessary.
    if (m_pCurrentPage) {
        TRACE_DVB(_T("DVB - EndOfStream: Enqueue last page\n"));
        EnqueuePage(INVALID_TIME);
    } else {
        TRACE_DVB(_T("DVB - EndOfStream ignored: no page to enqueue\n"));
    }
}

void CDVBSub::Reset()
{
    CAutoLock cAutoLock(&m_csCritSec);

    m_nBufferReadPos = 0;
    m_nBufferWritePos = 0;
    m_pCurrentPage.Free();
    m_pages.RemoveAll();
}

HRESULT CDVBSub::AddToBuffer(BYTE* pData, size_t nSize)
{
    bool bFirstChunk = (*((LONG*)pData) & 0x00FFFFFF) == 0x000f0020; // DVB sub start with 0x20 0x00 0x0F ...

    if (m_nBufferWritePos > 0 || bFirstChunk) {
        if (bFirstChunk) {
            m_nBufferWritePos = 0;
            m_nBufferReadPos  = 0;
        }

        if (m_nBufferWritePos + nSize > m_nBufferSize) {
            if (m_nBufferWritePos + nSize > 20 * BUFFER_CHUNK_GROW) {
                // Too big to be a DVB sub !
                TRACE_DVB(_T("DVB - Too much data received...\n"));
                ASSERT(FALSE);

                Reset();
                return E_INVALIDARG;
            }

            BYTE* pPrev = m_pBuffer;
            m_nBufferSize = std::max(m_nBufferWritePos + nSize, m_nBufferSize + BUFFER_CHUNK_GROW);
            m_pBuffer = DEBUG_NEW BYTE[m_nBufferSize];
            if (pPrev != nullptr) {
                memcpy_s(m_pBuffer, m_nBufferSize, pPrev, m_nBufferWritePos);
                SAFE_DELETE(pPrev);
            }
        }
        memcpy_s(m_pBuffer + m_nBufferWritePos, m_nBufferSize, pData, nSize);
        m_nBufferWritePos += nSize;
        return S_OK;
    }
    return S_FALSE;
}

POSITION CDVBSub::FindPage(REFERENCE_TIME rt) const
{
    POSITION pos = m_pages.GetHeadPosition();

    while (pos) {
        POSITION currentPos = pos;
        const auto& pPage = m_pages.GetNext(pos);

        if (rt >= pPage->rtStart && rt < pPage->rtStop) {
            return currentPos;
        }
    }

    return nullptr;
}

CDVBSub::RegionList::const_iterator CDVBSub::FindRegion(const CAutoPtr<DVB_PAGE>& pPage, BYTE bRegionId) const
{
    ENSURE(pPage);

    return std::find_if(pPage->regions.cbegin(), pPage->regions.cend(),
    [bRegionId](const std::unique_ptr<DVB_REGION>& pRegion) {
        return pRegion->id == bRegionId;
    });
}

CDVBSub::ClutList::const_iterator CDVBSub::FindClut(const CAutoPtr<DVB_PAGE>& pPage, BYTE bClutId) const
{
    ENSURE(pPage);

    return std::find_if(pPage->CLUTs.cbegin(), pPage->CLUTs.cend(),
    [bClutId](const std::unique_ptr<DVB_CLUT>& pCLUT) {
        return pCLUT->id == bClutId;
    });
}

CDVBSub::CompositionObjectList::const_iterator CDVBSub::FindObject(const CAutoPtr<DVB_PAGE>& pPage, short sObjectId) const
{
    ENSURE(pPage);

    return std::find_if(pPage->objects.cbegin(), pPage->objects.cend(),
    [sObjectId](const std::unique_ptr<CompositionObject>& pObject) {
        return pObject->m_object_id_ref == sObjectId;
    });
}

HRESULT CDVBSub::ParsePage(CGolombBuffer& gb, WORD wSegLength, CAutoPtr<DVB_PAGE>& pPage)
{
    size_t nExpectedSize = 2;
    size_t nEnd = gb.GetPos() + wSegLength;

    pPage.Attach(DEBUG_NEW DVB_PAGE());

    pPage->pageTimeOut = gb.ReadByte();
    pPage->pageVersionNumber = (BYTE)gb.BitRead(4);
    pPage->pageState = (BYTE)gb.BitRead(2);
    gb.BitRead(2);  // Reserved
    while (gb.GetPos() < nEnd) {
        nExpectedSize += 6;
        DVB_REGION_POS regionPos;
        regionPos.id = gb.ReadByte();
        gb.ReadByte();  // Reserved
        regionPos.horizAddr = gb.ReadShort();
        regionPos.vertAddr = gb.ReadShort();
        pPage->regionsPos.emplace_back(std::move(regionPos));
    }

    return (wSegLength == nExpectedSize) ? S_OK : E_UNEXPECTED;
}

HRESULT CDVBSub::ParseDisplay(CGolombBuffer& gb, WORD wSegLength)
{
    int nExpectedSize = 5;

    m_displayInfo.version_number = (BYTE)gb.BitRead(4);
    m_displayInfo.display_window_flag = (BYTE)gb.BitRead(1);
    gb.BitRead(3);  // reserved
    m_displayInfo.width = gb.ReadShort() + 1;
    m_displayInfo.height = gb.ReadShort() + 1;
    if (m_displayInfo.display_window_flag) {
        nExpectedSize += 8;
        m_displayInfo.horizontal_position_minimun = gb.ReadShort();
        m_displayInfo.horizontal_position_maximum = gb.ReadShort();
        m_displayInfo.vertical_position_minimun = gb.ReadShort();
        m_displayInfo.vertical_position_maximum = gb.ReadShort();
    }

    return (wSegLength == nExpectedSize) ? S_OK : E_UNEXPECTED;
}

HRESULT CDVBSub::ParseRegion(CGolombBuffer& gb, WORD wSegLength)
{
    HRESULT hr = E_POINTER;

    if (m_pCurrentPage) {
        size_t nExpectedSize = 10;
        size_t nEnd = gb.GetPos() + wSegLength;

        BYTE id = gb.ReadByte();
        auto itRegion = FindRegion(m_pCurrentPage, id);
        if (itRegion == m_pCurrentPage->regions.cend()) {
            m_pCurrentPage->regions.emplace_back(DEBUG_NEW DVB_REGION());
            itRegion = std::prev(m_pCurrentPage->regions.cend());
        }
        const auto& pRegion = *itRegion;

        pRegion->id = id;
        pRegion->version_number = (BYTE)gb.BitRead(4);
        pRegion->fill_flag = (BYTE)gb.BitRead(1);
        gb.BitRead(3);  // Reserved
        pRegion->width = gb.ReadShort();
        pRegion->height = gb.ReadShort();
        pRegion->level_of_compatibility = (BYTE)gb.BitRead(3);
        pRegion->depth = (BYTE)gb.BitRead(3);
        gb.BitRead(2);  // Reserved
        pRegion->CLUT_id = gb.ReadByte();
        pRegion->_8_bit_pixel_code = gb.ReadByte();
        pRegion->_4_bit_pixel_code = (BYTE)gb.BitRead(4);
        pRegion->_2_bit_pixel_code = (BYTE)gb.BitRead(2);
        gb.BitRead(2);  // Reserved

        while (gb.GetPos() < nEnd) {
            nExpectedSize += 6;
            DVB_OBJECT object;
            object.object_id = gb.ReadShort();
            object.object_type = (BYTE)gb.BitRead(2);
            object.object_provider_flag = (BYTE)gb.BitRead(2);
            object.object_horizontal_position = (short)gb.BitRead(12);
            gb.BitRead(4);  // Reserved
            object.object_vertical_position = (short)gb.BitRead(12);
            if (object.object_type == 0x01 || object.object_type == 0x02) {
                nExpectedSize += 2;
                object.foreground_pixel_code = gb.ReadByte();
                object.background_pixel_code = gb.ReadByte();
            }
            pRegion->objects.emplace_back(std::move(object));
        }

        hr = (wSegLength == nExpectedSize) ? S_OK : E_UNEXPECTED;
    }

    return hr;
}

HRESULT CDVBSub::ParseClut(CGolombBuffer& gb, WORD wSegLength)
{
    HRESULT hr = E_POINTER;

    if (m_pCurrentPage) {
        size_t nExpectedSize = 2;
        size_t nEnd = gb.GetPos() + wSegLength;

        BYTE id = gb.ReadByte();
        auto itClut = FindClut(m_pCurrentPage, id);
        if (itClut == m_pCurrentPage->CLUTs.cend()) {
            m_pCurrentPage->CLUTs.emplace_back(DEBUG_NEW DVB_CLUT());
            itClut = std::prev(m_pCurrentPage->CLUTs.cend());
        }
        const auto& pClut = *itClut;

        pClut->id = id;
        pClut->version_number = (BYTE)gb.BitRead(4);
        gb.BitRead(4);  // Reserved

        pClut->size = 0;
        // the segment length bounds the loop, the palette holds 256 entries at most
        while (gb.GetPos() < nEnd && pClut->size < pClut->palette.size()) {
            nExpectedSize += 2;
            pClut->palette[pClut->size].entry_id = gb.ReadByte();

            BYTE _2_bit   = (BYTE)gb.BitRead(1);
            BYTE _4_bit   = (BYTE)gb.BitRead(1);
            BYTE _8_bit   = (BYTE)gb.BitRead(1);
            UNREFERENCED_PARAMETER(_2_bit);
            UNREFERENCED_PARAMETER(_4_bit);
            UNREFERENCED_PARAMETER(_8_bit);
            gb.BitRead(4);  // Reserved

            if (gb.BitRead(1)) {
                nExpectedSize += 4;
                pClut->palette[pClut->size].Y  = gb.ReadByte();
                pClut->palette[pClut->size].Cr = gb.ReadByte();
                pClut->palette[pClut->size].Cb = gb.ReadByte();
                pClut->palette[pClut->size].T  = 0xff - gb.ReadByte();
            } else {
                nExpectedSize += 2;
                pClut->palette[pClut->size].Y  = (BYTE)gb.BitRead(6) << 2;
                pClut->palette[pClut->size].Cr = (BYTE)gb.BitRead(4) << 4;
                pClut->palette[pClut->size].Cb = (BYTE)gb.BitRead(4) << 4;
                pClut->palette[pClut->size].T  = 0xff - ((BYTE)gb.BitRead(2) << 6);
            }
            if (!pClut->palette[pClut->size].Y) {
                pClut->palette[pClut->size].Cr = 0;
                pClut->palette[pClut->size].Cb = 0;
                pClut->palette[pClut->size].T  = 0;
            }

            pClut->size++;
        }

        hr = (wSegLength == nExpectedSize) ? S_OK : E_UNEXPECTED;
    }

    return hr;
}

HRESULT CDVBSub::ParseObject(CGolombBuffer& gb, WORD wSegLength)
{
    HRESULT hr = E_POINTER;

    if (m_pCurrentPage) {
        size_t nExpectedSize = 3;
        // size_t nEnd = gb.GetPos() + wSegLength;

        short id = gb.ReadShort();
        auto itObject = FindObject(m_pCurrentPage, id);
        if (itObject == m_pCurrentPage->objects.cend()) {
            m_pCurrentPage->objects.emplace_back(DEBUG_NEW CompositionObject());
            itObject = std::prev(m_pCurrentPage->objects.cend());
        }
        const auto& pObject = *itObject;

        pObject->m_object_id_ref  = id;
        pObject->m_version_number = (BYTE)gb.BitRead(4);

        BYTE object_coding_method = (BYTE)gb.BitRead(2); // object_coding_method
        gb.BitRead(1);  // non_modifying_colour_flag
        gb.BitRead(1);  // reserved

        if (object_coding_method == 0x00) {
            // check before subtracting, a short segment would otherwise wrap the size,
            // and an object needs at least one byte of RLE data after the header
            if (wSegLength > nExpectedSize) {
                pObject->SetRLEData(gb.GetBufferPos(), wSegLength - nExpectedSize, wSegLength - nExpectedSize);
                gb.SkipBytes(wSegLength - nExpectedSize);
                hr = S_OK;
            } else {
                hr = E_UNEXPECTED;
            }
        } else {
            TRACE_DVB(_T("DVB - Text subtitles are currently not supported\n"));
            m_pCurrentPage->objects.pop_back();
            hr = E_NOTIMPL;
        }
    }

    return hr;
}

HRESULT CDVBSub::EnqueuePage(REFERENCE_TIME rtStop)
{
    ASSERT(m_pCurrentPage != nullptr);
    if (m_pCurrentPage->rtStart < rtStop && m_pCurrentPage->rtStop > rtStop) {
        m_pCurrentPage->rtStop = rtStop;
    }
    TRACE_DVB(_T("DVB - Enqueue page %s (%s - %s)\n"), ReftimeToString(rtStop), ReftimeToString(m_pCurrentPage->rtStart), ReftimeToString(m_pCurrentPage->rtStop));
    m_pages.AddTail(m_pCurrentPage);

    return S_OK;
}

HRESULT CDVBSub::UpdateTimeStamp(REFERENCE_TIME rtStop)
{
    HRESULT hr = S_FALSE;
    POSITION pos = m_pages.GetTailPosition();
    while (pos) {
        const auto& pPage = m_pages.GetPrev(pos);
        if (pPage->rtStop > rtStop) {
            TRACE_DVB(_T("DVB - Updated end of display %s - %s --> %s - %s\n"),
                      ReftimeToString(pPage->rtStart),
                      ReftimeToString(pPage->rtStop),
                      ReftimeToString(pPage->rtStart),
                      ReftimeToString(rtStop));
            pPage->rtStop = rtStop;
            hr = S_OK;
        } else {
            break;
        }
    }

    return hr;
}

void CDVBSub::RemoveOldPages(REFERENCE_TIME rt)
{
    // Cleanup the old pages. We keep a 2 min buffer to play nice with the queue.
    while (!m_pages.IsEmpty() && m_pages.GetHead()->rtStop + 120 * 10000000i64 < rt) {
        const auto& pPage = m_pages.GetHead();
        if (!pPage->rendered) {
            TRACE_DVB(_T("DVB - remove unrendered object, %s - %s\n"),
                      ReftimeToString(pPage->rtStart), ReftimeToString(pPage->rtStop));
        }
        m_pages.RemoveHeadNoReturn();
    }
}

STDMETHODIMP CDVBSub::GetRelativeTo(POSITION pos, RelativeTo& relativeTo)
{
    relativeTo = RelativeTo::BEST_FIT;
    return S_OK;
}
