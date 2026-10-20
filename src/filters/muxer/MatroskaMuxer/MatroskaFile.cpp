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

#include "stdafx.h"
#include <algorithm>
#include "MatroskaFile.h"
#include "../../../DSUtil/DSUtil.h"

using namespace MatroskaWriter;

static void bswap(BYTE* s, int len)
{
    for (BYTE* d = s + len - 1; s < d; s++, d--) {
        *s ^= *d, *d ^= *s, *s ^= *d;
    }
}

//

CID::CID(DWORD id)
    : m_id(id)
{
}

QWORD CID::Size(bool fWithHeader)
{
    return CUInt(0, m_id).Size(false);
}

HRESULT CID::Write(IStream* pStream)
{
    QWORD len = CID::Size();
    DWORD id = m_id;
    bswap((BYTE*)&id, (int)len);
    *(BYTE*)&id = ((*(BYTE*)&id) & (1 << (8 - len)) - 1) | (1 << (8 - len));
    return pStream->Write(&id, (ULONG)len, nullptr);
}

QWORD CID::HeaderSize(QWORD len)
{
    return CID::Size() + CLength(len).Size();
}

HRESULT CID::HeaderWrite(IStream* pStream)
{
    CID::Write(pStream);
    CLength(Size(false)).Write(pStream);
    return S_OK;
}

QWORD CBinary::Size(bool fWithHeader)
{
    if (IsEmpty()) {
        return 0;
    }

    QWORD len = 0;
    len += GetCount();
    if (fWithHeader) {
        len += HeaderSize(len);
    }
    return len;
}

HRESULT CBinary::Write(IStream* pStream)
{
    if (IsEmpty()) {
        return S_OK;
    }

    HeaderWrite(pStream);
    return pStream->Write(GetData(), (ULONG)GetCount(), nullptr);
}

QWORD CANSI::Size(bool fWithHeader)
{
    if (IsEmpty()) {
        return 0;
    }

    QWORD len = 0;
    len += GetLength();
    if (fWithHeader) {
        len += HeaderSize(len);
    }
    return len;
}

HRESULT CANSI::Write(IStream* pStream)
{
    if (IsEmpty()) {
        return S_OK;
    }

    HeaderWrite(pStream);
    return pStream->Write((LPCSTR) * this, GetLength(), nullptr);
}

QWORD CUTF8::Size(bool fWithHeader)
{
    if (IsEmpty()) {
        return 0;
    }

    QWORD len = 0;
    len += UTF16To8(*this).GetLength();
    if (fWithHeader) {
        len += HeaderSize(len);
    }
    return len;
}

HRESULT CUTF8::Write(IStream* pStream)
{
    if (IsEmpty()) {
        return S_OK;
    }

    HeaderWrite(pStream);
    CStringA str = UTF16To8(*this);
    return pStream->Write((BYTE*)(LPCSTR)str, str.GetLength(), nullptr);
}

template<class T, class BASE>
QWORD CSimpleVar<T, BASE>::Size(bool fWithHeader)
{
    if (!m_fSet) {
        return 0;
    }

    QWORD len = 0;
    len += sizeof(T);
    if (fWithHeader) {
        len += HeaderSize(len);
    }
    return len;
}

template<class T, class BASE>
HRESULT CSimpleVar<T, BASE>::Write(IStream* pStream)
{
    if (!m_fSet) {
        return S_OK;
    }

    HeaderWrite(pStream);
    T val = m_val;
    bswap((BYTE*)&val, sizeof(T));
    return pStream->Write(&val, sizeof(T), nullptr);
}

QWORD CUInt::Size(bool fWithHeader)
{
    if (!m_fSet) {
        return 0;
    }

    QWORD len = 0;

    if (m_val == 0) {
        len++;
    } else {
        for (int i = 8; i > 0; i--) {
            if (((0xffi64 << ((i - 1) * 8))&m_val)) {
                len += i;
                break;
            }
        }
    }
    if (fWithHeader) {
        len += HeaderSize(len);
    }
    return len;
}

HRESULT CUInt::Write(IStream* pStream)
{
    if (!m_fSet) {
        return S_OK;
    }

    CID::Write(pStream);
    CLength l(Size(false));
    l.Write(pStream);
    UINT64 val = m_val;
    bswap((BYTE*)&val, (int)l);
    return pStream->Write(&val, (ULONG)l, nullptr);
}

QWORD CInt::Size(bool fWithHeader)
{
    if (!m_fSet) {
        return 0;
    }

    QWORD len = 0;

    if (m_val == 0) {
        len++;
    } else {
        UINT64 val = m_val >= 0 ? m_val : -m_val;
        for (int i = 8; i > 0; i--) {
            if (((0xffi64 << ((i - 1) * 8))&val)) {
                len += i;
                if (m_val < 0 && !(m_val & (0x80 << (i - 1)))) {
                    len++;
                }
                break;
            }
        }
    }
    if (fWithHeader) {
        len += HeaderSize(len);
    }
    return len;
}

HRESULT CInt::Write(IStream* pStream)
{
    if (!m_fSet) {
        return S_OK;
    }

    CID::Write(pStream);
    CLength l(Size(false));
    l.Write(pStream);
    UINT64 val = m_val;
    bswap((BYTE*)&val, (int)l);
    return pStream->Write(&val, (ULONG)l, nullptr);
}

QWORD CLength::Size(bool fWithHeader)
{
    if (m_len == 0x00FFFFFFFFFFFFFFi64) {
        return 8;
    }

    QWORD len = 0;
    for (int i = 1; i <= 8; i++) {
        if (!(m_len & (~((QWORD(1) << (7 * i)) - 1))) && (m_len & ((QWORD(1) << (7 * i)) - 1)) != ((QWORD(1) << (7 * i)) - 1)) {
            len += i;
            break;
        }
    }
    return len;
}

HRESULT CLength::Write(IStream* pStream)
{
    QWORD len = Size(false);
    UINT64 val = m_len;
    bswap((BYTE*)&val, (int)len);
    *(BYTE*)&val = ((*(BYTE*)&val) & (1 << (8 - len)) - 1) | (1 << (8 - len));
    return pStream->Write(&val, (ULONG)len, nullptr);
}

//

EBML::EBML(DWORD id)
    : CID(id)
    , EBMLVersion(0x4286)
    , EBMLReadVersion(0x42F7)
    , EBMLMaxIDLength(0x42F2)
    , EBMLMaxSizeLength(0x42F3)
    , DocType(0x4282)
    , DocTypeVersion(0x4287)
    , DocTypeReadVersion(0x4285)
{
}

QWORD EBML::Size(bool fWithHeader)
{
    QWORD len = 0;
    len += EBMLVersion.Size();
    len += EBMLReadVersion.Size();
    len += EBMLMaxIDLength.Size();
    len += EBMLMaxSizeLength.Size();
    len += DocType.Size();
    len += DocTypeVersion.Size();
    len += DocTypeReadVersion.Size();
    if (fWithHeader) {
        len += HeaderSize(len);
    }
    return len;
}

HRESULT EBML::Write(IStream* pStream)
{
    HeaderWrite(pStream);
    EBMLVersion.Write(pStream);
    EBMLReadVersion.Write(pStream);
    EBMLMaxIDLength.Write(pStream);
    EBMLMaxSizeLength.Write(pStream);
    DocType.Write(pStream);
    DocTypeVersion.Write(pStream);
    DocTypeReadVersion.Write(pStream);
    return S_OK;
}

Info::Info(DWORD id)
    : CID(id)
    , SegmentUID(0x73A4)
    , SegmentFilename(0x7384)
    , PrevUID(0x3CB923)
    , PrevFilename(0x3C83AB)
    , NextUID(0x3EB923)
    , NextFilename(0x3E83BB)
    , TimeCodeScale(0x2AD7B1, 1000000ui64)
    , Duration(0x4489)
    , DateUTC(0x4461)
    , Title(0x7BA9)
    , MuxingApp(0x4D80)
    , WritingApp(0x5741)
{
}

QWORD Info::Size(bool fWithHeader)
{
    QWORD len = 0;
    len += SegmentUID.Size();
    len += PrevUID.Size();
    len += NextUID.Size();
    len += SegmentFilename.Size();
    len += PrevFilename.Size();
    len += NextFilename.Size();
    len += TimeCodeScale.Size();
    len += Duration.Size();
    len += DateUTC.Size();
    len += Title.Size();
    len += MuxingApp.Size();
    len += WritingApp.Size();
    if (fWithHeader) {
        len += HeaderSize(len);
    }
    return len;
}

HRESULT Info::Write(IStream* pStream)
{
    HeaderWrite(pStream);
    SegmentUID.Write(pStream);
    PrevUID.Write(pStream);
    NextUID.Write(pStream);
    SegmentFilename.Write(pStream);
    PrevFilename.Write(pStream);
    NextFilename.Write(pStream);
    TimeCodeScale.Write(pStream);
    Duration.Write(pStream);
    DateUTC.Write(pStream);
    Title.Write(pStream);
    MuxingApp.Write(pStream);
    WritingApp.Write(pStream);
    return S_OK;
}

Segment::Segment(DWORD id)
    : CID(id)
{
}

QWORD Segment::Size(bool fWithHeader)
{
    return 0x00FFFFFFFFFFFFFFi64;
    /*
        QWORD len = 0;
        if (fWithHeader) len += HeaderSize(len);
        return len;
    */
}

HRESULT Segment::Write(IStream* pStream)
{
    HeaderWrite(pStream);
    return S_OK;
}

Track::Track(DWORD id)
    : CID(id)
{
}

QWORD Track::Size(bool fWithHeader)
{
    QWORD len = 0;
    len += TrackEntries.Size();
    if (fWithHeader) {
        len += HeaderSize(len);
    }
    return len;
}

HRESULT Track::Write(IStream* pStream)
{
    HeaderWrite(pStream);
    TrackEntries.Write(pStream);
    return S_OK;
}

TrackEntry::TrackEntry(DWORD id)
    : CID(id)
    , TrackNumber(0xD7)
    , TrackUID(0x73C5)
    , TrackType(0x83)
    , FlagEnabled(0xB9)
    , FlagDefault(0x88)
    , FlagLacing(0x9C)
    , MinCache(0x6DE7)
    , MaxCache(0x6DF8)
    , Name(0x536E)
    , Language(0x22B59C)
    , CodecID(0x86)
    , CodecPrivate(0x63A2)
    , CodecName(0x258688)
    , CodecSettings(0x3A9697)
    , CodecInfoURL(0x3B4040)
    , CodecDownloadURL(0x26B240)
    , CodecDecodeAll(0xAA)
    , TrackOverlay(0x6FAB)
    , DefaultDuration(0x23E383)
    , v(0xE0)
    , a(0xE1)
{
    DescType = NoDesc;
}

QWORD TrackEntry::Size(bool fWithHeader)
{
    QWORD len = 0;
    len += TrackNumber.Size();
    len += TrackUID.Size();
    len += TrackType.Size();
    len += FlagEnabled.Size();
    len += FlagDefault.Size();
    len += FlagLacing.Size();
    len += MinCache.Size();
    len += MaxCache.Size();
    len += Name.Size();
    len += Language.Size();
    len += CodecID.Size();
    len += CodecPrivate.Size();
    len += CodecName.Size();
    len += CodecSettings.Size();
    len += CodecInfoURL.Size();
    len += CodecDownloadURL.Size();
    len += CodecDecodeAll.Size();
    len += TrackOverlay.Size();
    len += DefaultDuration.Size();
    if (DescType == TypeVideo) {
        len += v.Size();
    }
    if (DescType == TypeAudio) {
        len += a.Size();
    }
    if (fWithHeader) {
        len += HeaderSize(len);
    }
    return len;
}

HRESULT TrackEntry::Write(IStream* pStream)
{
    HeaderWrite(pStream);
    TrackNumber.Write(pStream);
    TrackUID.Write(pStream);
    TrackType.Write(pStream);
    FlagEnabled.Write(pStream);
    FlagDefault.Write(pStream);
    FlagLacing.Write(pStream);
    MinCache.Write(pStream);
    MaxCache.Write(pStream);
    Name.Write(pStream);
    Language.Write(pStream);
    CodecID.Write(pStream);
    CodecPrivate.Write(pStream);
    CodecName.Write(pStream);
    CodecSettings.Write(pStream);
    CodecInfoURL.Write(pStream);
    CodecDownloadURL.Write(pStream);
    CodecDecodeAll.Write(pStream);
    TrackOverlay.Write(pStream);
    DefaultDuration.Write(pStream);
    if (DescType == TypeVideo) {
        v.Write(pStream);
    }
    if (DescType == TypeAudio) {
        a.Write(pStream);
    }
    return S_OK;
}

Video::Video(DWORD id)
    : CID(id)
    , FlagInterlaced(0x9A)
    , StereoMode(0x53B8)
    , PixelWidth(0xB0)
    , PixelHeight(0xBA)
    , DisplayWidth(0x54B0)
    , DisplayHeight(0x54BA)
    , DisplayUnit(0x54B2)
    , AspectRatioType(0x54B3)
    , ColourSpace(0x2EB524)
    , GammaValue(0x2FB523)
    , FramePerSec(0x2383E3)
{
}

QWORD Video::Size(bool fWithHeader)
{
    QWORD len = 0;
    len += FlagInterlaced.Size();
    len += StereoMode.Size();
    len += PixelWidth.Size();
    len += PixelHeight.Size();
    len += DisplayWidth.Size();
    len += DisplayHeight.Size();
    len += DisplayUnit.Size();
    len += AspectRatioType.Size();
    len += ColourSpace.Size();
    len += GammaValue.Size();
    len += FramePerSec.Size();
    if (fWithHeader) {
        len += HeaderSize(len);
    }
    return len;
}

HRESULT Video::Write(IStream* pStream)
{
    HeaderWrite(pStream);
    FlagInterlaced.Write(pStream);
    StereoMode.Write(pStream);
    PixelWidth.Write(pStream);
    PixelHeight.Write(pStream);
    DisplayWidth.Write(pStream);
    DisplayHeight.Write(pStream);
    DisplayUnit.Write(pStream);
    AspectRatioType.Write(pStream);
    ColourSpace.Write(pStream);
    GammaValue.Write(pStream);
    FramePerSec.Write(pStream);
    return S_OK;
}

Audio::Audio(DWORD id)
    : CID(id)
    , SamplingFrequency(0xB5)
    , OutputSamplingFrequency(0x78B5)
    , Channels(0x9F)
    , ChannelPositions(0x7D7B)
    , BitDepth(0x6264)
{
}

QWORD Audio::Size(bool fWithHeader)
{
    QWORD len = 0;
    len += SamplingFrequency.Size();
    len += OutputSamplingFrequency.Size();
    len += Channels.Size();
    len += ChannelPositions.Size();
    len += BitDepth.Size();
    if (fWithHeader) {
        len += HeaderSize(len);
    }
    return len;
}

HRESULT Audio::Write(IStream* pStream)
{
    HeaderWrite(pStream);
    SamplingFrequency.Write(pStream);
    OutputSamplingFrequency.Write(pStream);
    Channels.Write(pStream);
    ChannelPositions.Write(pStream);
    BitDepth.Write(pStream);
    return S_OK;
}

Cluster::Cluster(DWORD id)
    : CID(id)
    , TimeCode(0xE7)
    , Position(0xA7)
    , PrevSize(0xAB)
{
}

QWORD Cluster::Size(bool fWithHeader)
{
    QWORD len = 0;
    len += TimeCode.Size();
    len += Position.Size();
    len += PrevSize.Size();
    len += BlockGroups.Size();
    if (fWithHeader) {
        len += HeaderSize(len);
    }
    return len;
}

HRESULT Cluster::Write(IStream* pStream)
{
    HeaderWrite(pStream);
    TimeCode.Write(pStream);
    Position.Write(pStream);
    PrevSize.Write(pStream);
    BlockGroups.Write(pStream);
    return S_OK;
}

BlockGroup::BlockGroup(DWORD id)
    : CID(id)
    , BlockDuration(0x9B)
    , ReferencePriority(0xFA)
    , ReferenceBlock(0xFB)
    , ReferenceVirtual(0xFD)
    , CodecState(0xA4)
{
}

QWORD BlockGroup::Size(bool fWithHeader)
{
    QWORD len = 0;
    len += BlockDuration.Size();
    len += ReferencePriority.Size();
    len += ReferenceBlock.Size();
    len += ReferenceVirtual.Size();
    len += CodecState.Size();
    len += Block.Size();
    if (fWithHeader) {
        len += HeaderSize(len);
    }
    return len;
}

HRESULT BlockGroup::Write(IStream* pStream)
{
    HeaderWrite(pStream);
    BlockDuration.Write(pStream);
    ReferencePriority.Write(pStream);
    ReferenceBlock.Write(pStream);
    ReferenceVirtual.Write(pStream);
    CodecState.Write(pStream);
    Block.Write(pStream);
    return S_OK;
}

CBlock::CBlock(DWORD id)
    : CID(id)
    , TimeCode(0)
    , TimeCodeStop(0)
{
}

QWORD CBlock::Size(bool fWithHeader)
{
    QWORD len = 0;
    len += TrackNumber.Size() + 2 + 1; // TrackNumber + TimeCode + Lacing
    if (BlockData.GetCount() > 1) {
        len += 1; // nBlockData
        POSITION pos = BlockData.GetHeadPosition();
        while (pos) {
            CBinary* b = BlockData.GetNext(pos);
            if (pos) {
                len += b->GetCount() / 255 + 1;
            }
        }
    }
    POSITION pos = BlockData.GetHeadPosition();
    while (pos) {
        CBinary* b = BlockData.GetNext(pos);
        len += b->GetCount();
    }
    if (fWithHeader) {
        len += HeaderSize(len);
    }
    return len;
}

HRESULT CBlock::Write(IStream* pStream)
{
    HeaderWrite(pStream);
    TrackNumber.Write(pStream);
    short t = (short)TimeCode;
    bswap((BYTE*)&t, 2);
    pStream->Write(&t, 2, nullptr);
    BYTE Lacing = 0;
    BYTE n = (BYTE)BlockData.GetCount();
    if (n > 1) {
        Lacing |= 2;
    }
    pStream->Write(&Lacing, 1, nullptr);
    if (n > 1) {
        pStream->Write(&n, 1, nullptr);
        POSITION pos = BlockData.GetHeadPosition();
        while (pos) {
            CBinary* b = BlockData.GetNext(pos);
            if (pos) {
                INT_PTR len = b->GetCount();
                while (len >= 0) {
                    n = (BYTE)std::min<INT_PTR>(len, 255);
                    pStream->Write(&n, 1, nullptr);
                    len -= 255;
                }
            }
        }
    }
    POSITION pos = BlockData.GetHeadPosition();
    while (pos) {
        CBinary* b = BlockData.GetNext(pos);
        pStream->Write(b->GetData(), (ULONG)b->GetCount(), nullptr);
    }
    return S_OK;
}

Cue::Cue(DWORD id)
    : CID(id)
{
}

QWORD Cue::Size(bool fWithHeader)
{
    QWORD len = 0;
    len += CuePoints.Size();
    if (fWithHeader) {
        len += HeaderSize(len);
    }
    return len;
}

HRESULT Cue::Write(IStream* pStream)
{
    HeaderWrite(pStream);
    CuePoints.Write(pStream);
    return S_OK;
}

CuePoint::CuePoint(DWORD id)
    : CID(id)
    , CueTime(0xB3)
{
}

QWORD CuePoint::Size(bool fWithHeader)
{
    QWORD len = 0;
    len += CueTime.Size();
    len += CueTrackPositions.Size();
    if (fWithHeader) {
        len += HeaderSize(len);
    }
    return len;
}

HRESULT CuePoint::Write(IStream* pStream)
{
    HeaderWrite(pStream);
    CueTime.Write(pStream);
    CueTrackPositions.Write(pStream);
    return S_OK;
}

CueTrackPosition::CueTrackPosition(DWORD id)
    : CID(id)
    , CueTrack(0xF7)
    , CueClusterPosition(0xF1)
    , CueBlockNumber(0x5387)
    , CueCodecState(0xEA)
{
}

QWORD CueTrackPosition::Size(bool fWithHeader)
{
    QWORD len = 0;
    len += CueTrack.Size();
    len += CueClusterPosition.Size();
    len += CueBlockNumber.Size();
    len += CueCodecState.Size();
    //  len += CueReferences.Size();
    if (fWithHeader) {
        len += HeaderSize(len);
    }
    return len;
}

HRESULT CueTrackPosition::Write(IStream* pStream)
{
    HeaderWrite(pStream);
    CueTrack.Write(pStream);
    CueClusterPosition.Write(pStream);
    CueBlockNumber.Write(pStream);
    CueCodecState.Write(pStream);
    //  CueReferences.Write(pStream);
    return S_OK;
}

Seek::Seek(DWORD id)
    : CID(id)
{
}

// Returns the bytes left over when written into space, or -1 if it does not fit.
// A one byte gap cannot hold a Void, so it does not fit either.
INT64 Seek::FitInto(QWORD space)
{
    INT64 len = (INT64)space - (INT64)Size();
    if (len < 0 || len == 1) {
        // drop the Cluster entries, players find clusters through the Cues
        POSITION pos = SeekHeads.GetHeadPosition();
        while (pos) {
            POSITION cur = pos;
            if (SeekHeads.GetNext(pos)->ID.GetSeekID() == 0x1F43B675) {
                SeekHeads.RemoveAt(cur);
            }
        }
        len = (INT64)space - (INT64)Size();
    }
    return (len < 0 || len == 1) ? -1 : len;
}

QWORD Seek::Size(bool fWithHeader)
{
    QWORD len = 0;
    len += SeekHeads.Size();
    if (fWithHeader) {
        len += HeaderSize(len);
    }
    return len;
}

HRESULT Seek::Write(IStream* pStream)
{
    HeaderWrite(pStream);
    SeekHeads.Write(pStream);
    return S_OK;
}

SeekID::SeekID(DWORD id)
    : CID(id)
    , m_cid(0)
{
}

QWORD SeekID::Size(bool fWithHeader)
{
    QWORD len = 0;
    len += m_cid.Size();
    if (fWithHeader) {
        len += HeaderSize(len);
    }
    return len;
}

HRESULT SeekID::Write(IStream* pStream)
{
    HeaderWrite(pStream);
    m_cid.Write(pStream);
    return S_OK;
}

SeekHead::SeekHead(DWORD id)
    : CID(id)
    , Position(0x53AC)
{
}

QWORD SeekHead::Size(bool fWithHeader)
{
    QWORD len = 0;
    len += ID.Size();
    len += Position.Size();
    if (fWithHeader) {
        len += HeaderSize(len);
    }
    return len;
}

HRESULT SeekHead::Write(IStream* pStream)
{
    HeaderWrite(pStream);
    ID.Write(pStream);
    Position.Write(pStream);
    return S_OK;
}

Tags::Tags(DWORD id)
    : CID(id)
{
}

QWORD Tags::Size(bool fWithHeader)
{
    QWORD len = 0;
    //  len += .Size();
    if (fWithHeader) {
        len += HeaderSize(len);
    }
    return len;
}

HRESULT Tags::Write(IStream* pStream)
{
    HeaderWrite(pStream);
    //  .Write(pStream);
    return S_OK;
}

Void::Void(QWORD len, DWORD id)
    : CID(id)
    , m_len(len)
{
}

// Covers len bytes with Voids. With a minimal size length one Void cannot total
// every size (129, 16386, ...), so those get a two byte Void first.
HRESULT Void::Fill(IStream* pStream, QWORD len)
{
    while (len >= 2) {
        QWORD n = 0;
        for (QWORD i = 1; i <= 8 && i + 1 <= len; i++) {
            if (Void(len - 1 - i).Size() == len) {
                n = len - 1 - i;
                break;
            }
        }
        Void v(n);
        len -= v.Size();
        HRESULT hr = v.Write(pStream);
        if (FAILED(hr)) {
            return hr;
        }
    }
    return len == 0 ? S_OK : E_INVALIDARG;
}

QWORD Void::Size(bool fWithHeader)
{
    QWORD len = 0;
    len += m_len;
    if (fWithHeader) {
        len += HeaderSize(len);
    }
    return len;
}

HRESULT Void::Write(IStream* pStream)
{
    HeaderWrite(pStream);
    BYTE buff[64];
    memset(buff, 0x80, sizeof(buff));
    QWORD len = m_len;
    for (; len >= sizeof(buff); len -= sizeof(buff)) {
        pStream->Write(buff, sizeof(buff), nullptr);
    }

    if (len > 0) {
        pStream->Write(buff, (ULONG)len, nullptr);
    }

    return S_OK;
}
