/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2014 see Authors.txt
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

#include <atlbase.h>
#include <atlcoll.h>

namespace MatroskaWriter
{
    class CID
    {
    protected:
        DWORD m_id;
        QWORD HeaderSize(QWORD len);
        HRESULT HeaderWrite(IStream* pStream);

    public:
        CID(DWORD id);
        DWORD GetID() const {
            return m_id;
        }
        virtual QWORD Size(bool fWithHeader = true);
        virtual HRESULT Write(IStream* pStream);
    };

    class CLength : public CID
    {
        UINT64 m_len;
    public:
        CLength(UINT64 len = 0) : CID(0), m_len(len) {}
        operator UINT64() {
            return m_len;
        }
        QWORD Size(bool fWithHeader = false);
        HRESULT Write(IStream* pStream);
    };

    class CBinary : public CAtlArray<BYTE>, public CID
    {
    public:
        CBinary(DWORD id) : CID(id) {}
        CBinary& operator = (const CBinary& b) {
            Copy(b);
            return *this;
        }
        operator BYTE* () {
            return (BYTE*)GetData();
        }
        CBinary& Set(CStringA str) {
            SetCount(str.GetLength() + 1);
            strcpy_s((char*)GetData(), str.GetLength() + 1, str);
            return *this;
        }
        //CBinary& Set(CStringA str) {SetCount(str.GetLength()); memcpy((char*)GetData(), str, str.GetLength()); return *this;}
        QWORD Size(bool fWithHeader = true);
        HRESULT Write(IStream* pStream);
    };

    class CANSI : public CStringA, public CID
    {
    public:
        CANSI(DWORD id) : CID(id) {}
        CANSI& Set(CStringA str) {
            CStringA::operator = (str);
            return *this;
        }
        QWORD Size(bool fWithHeader = true);
        HRESULT Write(IStream* pStream);
    };

    class CUTF8 : public CStringW, public CID
    {
    public:
        CUTF8(DWORD id) : CID(id) {}
        CUTF8& Set(CStringW str) {
            CStringW::operator = (str);
            return *this;
        }
        QWORD Size(bool fWithHeader = true);
        HRESULT Write(IStream* pStream);
    };

    template<class T, class BASE>
    class CSimpleVar : public CID
    {
    protected:
        T m_val;
        bool m_fSet;
    public:
        explicit CSimpleVar(DWORD id, T val = 0) : CID(id), m_val(val) {
            m_fSet = !!val;
        }
        operator T() {
            return m_val;
        }
        BASE& Set(T val) {
            m_val = val;
            m_fSet = true;
            return (*(BASE*)this);
        }
        void UnSet() {
            m_fSet = false;
        }
        QWORD Size(bool fWithHeader = true);
        HRESULT Write(IStream* pStream);
    };

    class CUInt : public CSimpleVar<UINT64, CUInt>
    {
    public:
        explicit CUInt(DWORD id, UINT64 val = 0) : CSimpleVar<UINT64, CUInt>(id, val) {}
        QWORD Size(bool fWithHeader = true);
        HRESULT Write(IStream* pStream);
    };

    class CInt : public CSimpleVar<INT64, CInt>
    {
    public:
        explicit CInt(DWORD id, INT64 val = 0) : CSimpleVar<INT64, CInt>(id, val) {}
        QWORD Size(bool fWithHeader = true);
        HRESULT Write(IStream* pStream);
    };

    class CByte : public CSimpleVar<BYTE, CByte>
    {
    public:
        explicit CByte(DWORD id, BYTE val = 0) : CSimpleVar<BYTE, CByte>(id, val) {}
    };

    class CShort : public CSimpleVar<short, CShort>
    {
    public:
        explicit CShort(DWORD id, short val = 0) : CSimpleVar<short, CShort>(id, val) {}
    };

    class CFloat : public CSimpleVar<float, CFloat>
    {
    public:
        explicit CFloat(DWORD id, float val = 0) : CSimpleVar<float, CFloat>(id, val) {}
    };

    template<class T>
    class CNode : public CAutoPtrList<T>
    {
    public:
        QWORD Size(bool fWithHeader = true) {
            QWORD len = 0;
            POSITION pos = CAutoPtrList<T>::GetHeadPosition();
            while (pos) {
                len += CAutoPtrList<T>::GetNext(pos)->Size(fWithHeader);
            }
            return len;
        }
        HRESULT Write(IStream* pStream) {
            POSITION pos = CAutoPtrList<T>::GetHeadPosition();
            while (pos) {
                HRESULT hr;
                if (FAILED(hr = CAutoPtrList<T>::GetNext(pos)->Write(pStream))) {
                    return hr;
                }
            }
            return S_OK;
        }
    };

    class EBML : public CID
    {
    public:
        CUInt EBMLVersion, EBMLReadVersion;
        CUInt EBMLMaxIDLength, EBMLMaxSizeLength;
        CANSI DocType;
        CUInt DocTypeVersion, DocTypeReadVersion;

        EBML(DWORD id = 0x1A45DFA3);
        QWORD Size(bool fWithHeader = true);
        HRESULT Write(IStream* pStream);
    };

    class Info : public CID
    {
    public:
        CBinary SegmentUID, PrevUID, NextUID;
        CUTF8 SegmentFilename, PrevFilename, NextFilename;
        CUInt TimeCodeScale; // [ns], default: 1.000.000
        CFloat Duration;
        CInt DateUTC;
        CUTF8 Title, MuxingApp, WritingApp;

        Info(DWORD id = 0x1549A966);
        QWORD Size(bool fWithHeader = true);
        HRESULT Write(IStream* pStream);
    };

    class Video : public CID
    {
    public:
        CUInt FlagInterlaced, StereoMode;
        CUInt PixelWidth, PixelHeight, DisplayWidth, DisplayHeight, DisplayUnit;
        CUInt AspectRatioType;
        CUInt ColourSpace;
        CFloat GammaValue;
        CFloat FramePerSec;

        Video(DWORD id = 0xE0);
        QWORD Size(bool fWithHeader = true);
        HRESULT Write(IStream* pStream);
    };

    class Audio : public CID
    {
    public:
        CFloat SamplingFrequency;
        CFloat OutputSamplingFrequency;
        CUInt Channels;
        CBinary ChannelPositions;
        CUInt BitDepth;

        Audio(DWORD id = 0xE1);
        QWORD Size(bool fWithHeader = true);
        HRESULT Write(IStream* pStream);
    };

    class TrackEntry : public CID
    {
    public:
        enum {
            TypeVideo = 1,
            TypeAudio = 2,
            TypeComplex = 3,
            TypeLogo = 0x10,
            TypeSubtitle = 0x11,
            TypeControl = 0x20
        };
        CUInt TrackNumber, TrackUID, TrackType;
        CUInt FlagEnabled, FlagDefault, FlagLacing;
        CUInt MinCache, MaxCache;
        CUTF8 Name;
        CANSI Language;
        CBinary CodecID;
        CBinary CodecPrivate;
        CUTF8 CodecName;
        CUTF8 CodecSettings;
        CANSI CodecInfoURL;
        CANSI CodecDownloadURL;
        CUInt CodecDecodeAll;
        CUInt TrackOverlay;
        CUInt DefaultDuration;
        enum { NoDesc = 0, DescVideo = 1, DescAudio = 2 };
        int DescType;
        Video v;
        Audio a;

        TrackEntry(DWORD id = 0xAE);
        QWORD Size(bool fWithHeader = true);
        HRESULT Write(IStream* pStream);
    };

    class Track : public CID
    {
    public:
        CNode<TrackEntry> TrackEntries;

        Track(DWORD id = 0x1654AE6B);
        QWORD Size(bool fWithHeader = true);
        HRESULT Write(IStream* pStream);
    };

    class CBlock : public CID
    {
    public:
        CLength TrackNumber;
        REFERENCE_TIME TimeCode, TimeCodeStop;
        CNode<CBinary> BlockData;

        CBlock(DWORD id = 0xA1);
        QWORD Size(bool fWithHeader = true);
        HRESULT Write(IStream* pStream);
    };

    class BlockGroup : public CID
    {
    public:
        CUInt BlockDuration;
        CUInt ReferencePriority;
        CInt ReferenceBlock;
        CInt ReferenceVirtual;
        CBinary CodecState;
        CBlock Block;
        //CNode<TimeSlice> TimeSlices;

        BlockGroup(DWORD id = 0xA0);
        QWORD Size(bool fWithHeader = true);
        HRESULT Write(IStream* pStream);
    };

    class Cluster : public CID
    {
    public:
        CUInt TimeCode, Position, PrevSize;
        CNode<BlockGroup> BlockGroups;

        Cluster(DWORD id = 0x1F43B675);
        QWORD Size(bool fWithHeader = true);
        HRESULT Write(IStream* pStream);
    };

    /*class CueReference : public CID
    {
    public:
        CUInt CueRefTime, CueRefCluster, CueRefNumber, CueRefCodecState;

        CueReference(DWORD id = 0xDB);
        QWORD Size(bool fWithHeader = true);
        HRESULT Write(IStream* pStream);
    };*/

    class CueTrackPosition : public CID
    {
    public:
        CUInt CueTrack, CueClusterPosition, CueBlockNumber, CueCodecState;
        //                  CNode<CueReference> CueReferences;

        CueTrackPosition(DWORD id = 0xB7);
        QWORD Size(bool fWithHeader = true);
        HRESULT Write(IStream* pStream);
    };

    class CuePoint : public CID
    {
    public:
        CUInt CueTime;
        CNode<CueTrackPosition> CueTrackPositions;

        CuePoint(DWORD id = 0xBB);
        QWORD Size(bool fWithHeader = true);
        HRESULT Write(IStream* pStream);
    };

    class Cue : public CID
    {
    public:
        CNode<CuePoint> CuePoints;

        Cue(DWORD id = 0x1C53BB6B);
        QWORD Size(bool fWithHeader = true);
        HRESULT Write(IStream* pStream);
    };

    class SeekID : public CID
    {
        CID m_cid;
    public:
        SeekID(DWORD id = 0x53AB);
        void Set(DWORD id) {
            m_cid = id;
        }
        DWORD GetSeekID() const {
            return m_cid.GetID();
        }
        QWORD Size(bool fWithHeader = true);
        HRESULT Write(IStream* pStream);
    };

    class SeekHead : public CID
    {
    public:
        SeekID ID;
        CUInt Position;

        SeekHead(DWORD id = 0x4DBB);
        QWORD Size(bool fWithHeader = true);
        HRESULT Write(IStream* pStream);
    };

    class Seek : public CID
    {
    public:
        CNode<SeekHead> SeekHeads;

        Seek(DWORD id = 0x114D9B74);
        INT64 FitInto(QWORD space);
        QWORD Size(bool fWithHeader = true);
        HRESULT Write(IStream* pStream);
    };

    class Segment : public CID
    {
    public:
        Segment(DWORD id = 0x18538067);
        QWORD Size(bool fWithHeader = true);
        HRESULT Write(IStream* pStream);
    };

    class Tags : public CID
    {
    public:
        // TODO

        Tags(DWORD id = 0x1254C367);
        QWORD Size(bool fWithHeader = true);
        HRESULT Write(IStream* pStream);
    };

    class Void : public CID
    {
        QWORD m_len;
    public:
        Void(QWORD len, DWORD id = 0xEC);
        static HRESULT Fill(IStream* pStream, QWORD len);
        QWORD Size(bool fWithHeader = true);
        HRESULT Write(IStream* pStream);
    };
}
