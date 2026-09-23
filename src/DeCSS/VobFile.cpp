#include "stdafx.h"
#include <io.h>
#include <algorithm>
#include "VobFile.h"
#include "CSSauth.h"
#include "CSSscramble.h"
#include "udf.h"

#include "../DSUtil/GolombBuffer.h"
#include "../DSUtil/DSUtil.h"
#include "../DSUtil/ISOLang.h"

#define AUDIO_BLOCK_SIZE    66
#define SUBTITLE_BLOCK_SIZE 194
#define IFO_HEADER_SIZE     12
#define VIDEO_TS_HEADER     "DVDVIDEO-VMG"
#define VTS_HEADER          "DVDVIDEO-VTS"

//
// CDVDSession
//

CDVDSession::CDVDSession()
    : m_hDrive(INVALID_HANDLE_VALUE)
    , m_session(DVD_END_ALL_SESSIONS)
{
    ZeroMemory(m_SessionKey, sizeof(m_SessionKey));
    ZeroMemory(m_DiscKey, sizeof(m_DiscKey));
    ZeroMemory(m_TitleKey, sizeof(m_TitleKey));
}

CDVDSession::~CDVDSession()
{
    EndSession();
}

bool CDVDSession::Open(LPCTSTR path)
{
    Close();

    CString fn = path;
    CString drive = _T("\\\\.\\") + fn.Left(fn.Find(':') + 1);

    m_hDrive = CreateFile(drive, GENERIC_READ, FILE_SHARE_READ, nullptr,
                          OPEN_EXISTING, FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN, (HANDLE)nullptr);
    if (m_hDrive == INVALID_HANDLE_VALUE) {
        return false;
    }

    return true;
}

void CDVDSession::Close()
{
    if (m_hDrive != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hDrive);
        m_hDrive = INVALID_HANDLE_VALUE;
    }
}

bool CDVDSession::BeginSession()
{
    EndSession();

    if (m_hDrive == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD BytesReturned;
    if (!DeviceIoControl(m_hDrive, IOCTL_DVD_START_SESSION, nullptr, 0, &m_session, sizeof(m_session), &BytesReturned, nullptr)) {
        m_session = DVD_END_ALL_SESSIONS;
        if (!DeviceIoControl(m_hDrive, IOCTL_DVD_END_SESSION, &m_session, sizeof(m_session), nullptr, 0, &BytesReturned, nullptr)
                || !DeviceIoControl(m_hDrive, IOCTL_DVD_START_SESSION, nullptr, 0, &m_session, sizeof(m_session), &BytesReturned, nullptr)) {
            Close();
            DWORD err = GetLastError();
            UNREFERENCED_PARAMETER(err);
            return false;
        }
    }

    return true;
}

void CDVDSession::EndSession()
{
    if (m_session != DVD_END_ALL_SESSIONS) {
        DWORD BytesReturned;
        DeviceIoControl(m_hDrive, IOCTL_DVD_END_SESSION, &m_session, sizeof(m_session), nullptr, 0, &BytesReturned, nullptr);
        m_session = DVD_END_ALL_SESSIONS;
    }
}

bool CDVDSession::Authenticate()
{
    if (m_session == DVD_END_ALL_SESSIONS) {
        return false;
    }

    BYTE Challenge[10], Key[10];

    for (BYTE i = 0; i < 10; i++) {
        Challenge[i] = i;
    }

    if (!SendKey(DvdChallengeKey, Challenge)) {
        return false;
    }

    if (!ReadKey(DvdBusKey1, Key)) {
        return false;
    }

    int varient = -1;

    for (int i = 31; i >= 0; i--) {
        BYTE KeyCheck[5];
        CSSkey1(i, Challenge, KeyCheck);
        if (!memcmp(KeyCheck, Key, 5)) {
            varient = i;
        }
    }

    if (!ReadKey(DvdChallengeKey, Challenge)) {
        return false;
    }

    CSSkey2(varient, Challenge, &Key[5]);

    if (!SendKey(DvdBusKey2, &Key[5])) {
        return false;
    }

    CSSbuskey(varient, Key, m_SessionKey);

    return true;
}

bool CDVDSession::GetDiscKey()
{
    if (m_session == DVD_END_ALL_SESSIONS) {
        return false;
    }

    BYTE DiscKeys[2048];
    if (!ReadKey(DvdDiskKey, DiscKeys)) {
        return false;
    }

    for (int i = 0; i < g_nPlayerKeys; i++) {
        for (int j = 1; j < 409; j++) {
            BYTE DiscKey[6];
            memcpy(DiscKey, &DiscKeys[j * 5], 5);
            DiscKey[5] = 0;

            CSSdisckey(DiscKey, g_PlayerKeys[i]);

            BYTE Hash[6];
            memcpy(Hash, &DiscKeys[0], 5);
            Hash[5] = 0;

            CSSdisckey(Hash, DiscKey);

            if (!memcmp(Hash, DiscKey, 6)) {
                memcpy(m_DiscKey, DiscKey, 6);
                return true;
            }
        }
    }

    return false;
}

bool CDVDSession::GetTitleKey(int lba, BYTE* pKey)
{
    if (m_session == DVD_END_ALL_SESSIONS) {
        return false;
    }

    if (!ReadKey(DvdTitleKey, pKey, lba)) {
        return false;
    }

    if (!(pKey[0] | pKey[1] | pKey[2] | pKey[3] | pKey[4])) {
        return false;
    }

    pKey[5] = 0;

    CSStitlekey(pKey, m_DiscKey);

    return true;
}

static void Reverse(BYTE* d, BYTE* s, int len)
{
    if (d == s) {
        for (s += len - 1; d < s; d++, s--) {
            *d ^= *s, *s ^= *d, *d ^= *s;
        }
    } else {
        for (int i = 0; i < len; i++) {
            d[i] = s[len - 1 - i];
        }
    }
}

bool CDVDSession::SendKey(DVD_KEY_TYPE KeyType, BYTE* pKeyData)
{
    CAutoVectorPtr<BYTE> key;
    DVD_COPY_PROTECT_KEY* pKey = nullptr;

    auto allocateKey = [&](ULONG len) {
        bool bSuccess = key.Allocate(len);
        if (bSuccess) {
            pKey = (DVD_COPY_PROTECT_KEY*)(BYTE*)key;
            pKey->KeyLength = len;
        }
        return bSuccess;
    };

    switch (KeyType) {
        case DvdChallengeKey:
            if (allocateKey(DVD_CHALLENGE_KEY_LENGTH)) {
                Reverse(pKey->KeyData, pKeyData, 10);
            }
            break;
        case DvdBusKey2:
            if (allocateKey(DVD_BUS_KEY_LENGTH)) {
                Reverse(pKey->KeyData, pKeyData, 5);
            }
            break;
        default:
            break;
    }

    if (!pKey) {
        return false;
    }

    pKey->SessionId = m_session;
    pKey->KeyType = KeyType;
    pKey->KeyFlags = 0;

    DWORD dwBytesReturned;
    return !!DeviceIoControl(m_hDrive, IOCTL_DVD_SEND_KEY, pKey, pKey->KeyLength, nullptr, 0, &dwBytesReturned, nullptr);
}

bool CDVDSession::ReadKey(DVD_KEY_TYPE KeyType, BYTE* pKeyData, int lba)
{
    CAutoVectorPtr<BYTE> key;
    DVD_COPY_PROTECT_KEY* pKey = nullptr;

    auto allocateKey = [&](ULONG len) {
        bool bSuccess = key.Allocate(len);
        if (bSuccess) {
            pKey = (DVD_COPY_PROTECT_KEY*)(BYTE*)key;
            pKey->KeyLength = len;
        }
        return bSuccess;
    };

    switch (KeyType) {
        case DvdChallengeKey:
            if (allocateKey(DVD_CHALLENGE_KEY_LENGTH)) {
                pKey->Parameters.TitleOffset.QuadPart = 0;
            }
            break;
        case DvdBusKey1:
            if (allocateKey(DVD_BUS_KEY_LENGTH)) {
                pKey->Parameters.TitleOffset.QuadPart = 0;
            }
            break;
        case DvdDiskKey:
            if (allocateKey(DVD_DISK_KEY_LENGTH)) {
                pKey->Parameters.TitleOffset.QuadPart = 0;
            }
            break;
        case DvdTitleKey:
            if (allocateKey(DVD_TITLE_KEY_LENGTH)) {
                pKey->Parameters.TitleOffset.QuadPart = 2048i64 * lba;
            }
            break;
        default:
            break;
    }

    if (!pKey) {
        return false;
    }

    pKey->SessionId = m_session;
    pKey->KeyType = KeyType;
    pKey->KeyFlags = 0;

    DWORD dwBytesReturned;
    if (!DeviceIoControl(m_hDrive, IOCTL_DVD_READ_KEY, pKey, pKey->KeyLength, pKey, pKey->KeyLength, &dwBytesReturned, nullptr)) {
        DWORD err = GetLastError();
        UNREFERENCED_PARAMETER(err);
        return false;
    }

    switch (KeyType) {
        case DvdChallengeKey:
            Reverse(pKeyData, pKey->KeyData, 10);
            break;
        case DvdBusKey1:
            Reverse(pKeyData, pKey->KeyData, 5);
            break;
        case DvdDiskKey:
            memcpy(pKeyData, pKey->KeyData, 2048);
            for (int i = 0; i < 2048 / 5; i++) {
                pKeyData[i] ^= m_SessionKey[4 - (i % 5)];
            }
            break;
        case DvdTitleKey:
            memcpy(pKeyData, pKey->KeyData, 5);
            for (int i = 0; i < 5; i++) {
                pKeyData[i] ^= m_SessionKey[4 - (i % 5)];
            }
            break;
        default:
            break;
    }

    return true;
}

//
// CLBAFile
//

CLBAFile::CLBAFile()
{
}

CLBAFile::~CLBAFile()
{
}

bool CLBAFile::IsOpen() const
{
    return (m_hFile != hFileNull);
}

bool CLBAFile::Open(LPCTSTR path)
{
    Close();

    return !!CFile::Open(path, modeRead | typeBinary | shareDenyNone | osSequentialScan);
}

void CLBAFile::Close()
{
    if (m_hFile != hFileNull) {
        CFile::Close();
    }
}

int CLBAFile::GetLengthLBA() const
{
    return (int)(CFile::GetLength() / 2048);
}

int CLBAFile::GetPositionLBA() const
{
    return (int)(CFile::GetPosition() / 2048);
}

int CLBAFile::Seek(int lba)
{
    return (int)(CFile::Seek(2048i64 * lba, CFile::begin) / 2048);
}

bool CLBAFile::Read(BYTE* buff)
{
    return CFile::Read(buff, 2048) == 2048;
}

//
// CVobFile
//

CVobFile::CVobFile()
{
    Close();
    m_iChaptersCount = -1;
}

CVobFile::~CVobFile()
{
}

bool CVobFile::IsDVD() const
{
    return m_fDVD;
}

bool CVobFile::HasDiscKey(BYTE* key) const
{
    if (key) {
        memcpy(key, m_DiscKey, 5);
    }
    return m_fHasDiscKey;
}

bool CVobFile::HasTitleKey(BYTE* key) const
{
    if (key) {
        memcpy(key, m_TitleKey, 5);
    }
    return m_fHasTitleKey;
}

DWORD CVobFile::ReadDword()
{
    return ReadByte() << 24 | ReadByte() << 16 | ReadByte() << 8 | ReadByte();
}

short CVobFile::ReadShort()
{
    return (ReadByte() << 8 | ReadByte());
}

BYTE CVobFile::ReadByte()
{
    BYTE bVal = 0;
    m_ifoFile.Read(&bVal, sizeof(bVal));
    return bVal;
}

void CVobFile::ReadBuffer(BYTE* pBuff, DWORD nLen)
{
    m_ifoFile.Read(pBuff, nLen);
}

static short GetFrames(byte val)
{
    int byte0_high = val >> 4;
    int byte0_low = val & 0x0F;
    if (byte0_high > 11) {
        return (short)(((byte0_high - 12) * 10) + byte0_low);
    }
    if ((byte0_high <= 3) || (byte0_high >= 8)) {
        return 0;
    }
    return (short)(((byte0_high - 4) * 10) + byte0_low);
}

bool CVobFile::GetTitleInfo(LPCTSTR fn, ULONG nTitleNum, ULONG& VTSN, ULONG& TTN)
{
    CFile ifoFile;
    if (!ifoFile.Open(fn, CFile::modeRead | CFile::typeBinary | CFile::shareDenyNone)) {
        return false;
    }

    if (nTitleNum < 1) {
        ifoFile.Close();
        return false;
    }

    try {
        char hdr[IFO_HEADER_SIZE + 1];
        if (ifoFile.Read(hdr, IFO_HEADER_SIZE) != IFO_HEADER_SIZE) {
            ifoFile.Close();
            return false;
        }
        hdr[IFO_HEADER_SIZE] = '\0';
        if (strcmp(hdr, VIDEO_TS_HEADER)) {
            ifoFile.Close();
            return false;
        }

        ifoFile.Seek(0xC4, CFile::begin);
        DWORD TT_SRPTPosition = 0; // Read a 32-bit unsigned big-endian integer
        if (ifoFile.Read(&TT_SRPTPosition, sizeof(TT_SRPTPosition)) != sizeof(TT_SRPTPosition)) {
            ifoFile.Close();
            return false;
        }
        TT_SRPTPosition = _byteswap_ulong(TT_SRPTPosition);
        TT_SRPTPosition *= 2048;

        // Validate the seek target (and the two bytes read from it) against the file's actual
        // length before seeking. Do the comparison in a wider type so the check itself can't be
        // fooled by wraparound; the DWORD *= 2048 above is inherent to the on-disk format's
        // 32-bit sector address and is left as-is.
        unsigned __int64 seekTarget = (unsigned __int64)TT_SRPTPosition + 8 + (unsigned __int64)(nTitleNum - 1) * 12 + 6;
        if (seekTarget + 2 > (unsigned __int64)ifoFile.GetLength()) {
            ifoFile.Close();
            return false;
        }
        ifoFile.Seek((LONGLONG)seekTarget, CFile::begin);

        BYTE tmp = 0;
        if (ifoFile.Read(&tmp, sizeof(tmp)) != sizeof(tmp)) {
            ifoFile.Close();
            return false;
        }
        VTSN = tmp;

        tmp = 0;
        if (ifoFile.Read(&tmp, sizeof(tmp)) != sizeof(tmp)) {
            ifoFile.Close();
            return false;
        }
        TTN = tmp;
    } catch (...) {
        ifoFile.Close();
        return false;
    }

    ifoFile.Close();

    return true;
}

bool CVobFile::Open(CString fn, CAtlList<CString>& vobs, ULONG nProgNum /*= 1*/, bool bAuthenticate /*= true*/)
{
    if (!m_ifoFile.Open(fn, CFile::modeRead | CFile::typeBinary | CFile::shareDenyNone)) {
        return false;
    }

    try {
        char hdr[IFO_HEADER_SIZE + 1];
        m_ifoFile.Read(hdr, IFO_HEADER_SIZE);
        hdr[IFO_HEADER_SIZE] = '\0';
        if (strcmp(hdr, VTS_HEADER)) {
            return false;
        }

        // Audio streams ...
        m_ifoFile.Seek(0x202, CFile::begin);
        BYTE buffer[SUBTITLE_BLOCK_SIZE];
        m_ifoFile.Read(buffer, AUDIO_BLOCK_SIZE);
        CGolombBuffer gb(buffer, AUDIO_BLOCK_SIZE);
        int stream_count = gb.ReadShort();
        for (int i = 0; i < std::min(stream_count, 8); i++) {
            BYTE Coding_mode = (BYTE)gb.BitRead(3);
            gb.BitRead(5);// skip
            int ToAdd = 0;
            switch (Coding_mode) {
            case 0:
                ToAdd = 0x80;
                break;
            case 4:
                ToAdd = 0xA0;
                break;
            case 6:
                ToAdd = 0x88;
                break;
            default:
                break;
            }
            gb.ReadByte();// skip
            char lang[2];
            gb.ReadBuffer((BYTE*)lang, 2);
            gb.ReadDword();// skip
            if (ToAdd) {
                m_pStream_Lang[ToAdd + i] = ISOLang::ISO6391ToLanguage(lang);
            }
        }

        // Subtitle streams ...
        m_ifoFile.Seek(0x254, CFile::begin);
        m_ifoFile.Read(buffer, SUBTITLE_BLOCK_SIZE);
        CGolombBuffer gb_s(buffer, SUBTITLE_BLOCK_SIZE);
        stream_count = gb_s.ReadShort();
        for (int i = 0; i < std::min(stream_count, 32); i++) {
            gb_s.ReadShort();
            char lang[2];
            gb_s.ReadBuffer((BYTE*)lang, 2);
            gb_s.ReadShort();
            m_pStream_Lang[0x20 + i] = ISOLang::ISO6391ToLanguage(lang);
        }

        // Chapters ...
        m_ifoFile.Seek(0xCC, CFile::begin); //Get VTS_PGCI address
        DWORD pcgITPosition = ReadDword() * 2048;
        m_ifoFile.Seek(pcgITPosition, CFile::begin);
        WORD nProgCount = (WORD)ReadShort();
        if (nProgNum > nProgCount) {
            return false;
        }
        m_ifoFile.Seek(pcgITPosition + 8 * nProgNum + 4, CFile::begin);
        DWORD chainOffset = ReadDword();
        m_ifoFile.Seek(pcgITPosition + chainOffset + 2, CFile::begin);
        BYTE programChainPrograms = ReadByte();
        m_iChaptersCount = programChainPrograms;
        m_ifoFile.Seek(pcgITPosition + chainOffset + 230, CFile::begin);
        int programMapOffset = ReadShort();
        m_ifoFile.Seek(pcgITPosition + chainOffset + 0xE8, CFile::begin);
        int cellTableOffset = ReadShort();
        REFERENCE_TIME rtDuration = 0;
        m_pChapters[0] = 0;
        for (BYTE currentProgram = 0; currentProgram < programChainPrograms; currentProgram++) {
            m_ifoFile.Seek(pcgITPosition + chainOffset + programMapOffset + currentProgram, CFile::begin);
            byte entryCell = ReadByte();
            byte exitCell = entryCell;
            if (currentProgram < (programChainPrograms - 1)) {
                m_ifoFile.Seek(pcgITPosition + chainOffset + programMapOffset + (currentProgram + 1), CFile::begin);
                exitCell = ReadByte() - 1;
            }

            REFERENCE_TIME rtTotalTime = 0;
            for (int currentCell = entryCell; currentCell <= exitCell; currentCell++) {
                int cellStart = cellTableOffset + ((currentCell - 1) * 0x18);
                m_ifoFile.Seek(pcgITPosition + chainOffset + cellStart, CFile::begin);
                BYTE bytes[4];
                ReadBuffer(bytes, 4);
                int cellType = bytes[0] >> 6;
                if (cellType == 0x00 || cellType == 0x01) {
                    m_ifoFile.Seek(pcgITPosition + chainOffset + cellStart + 4, CFile::begin);
                    ReadBuffer(bytes, 4);
                    short frames = GetFrames(bytes[3]);
                    int fpsMask = bytes[3] >> 6;
                    double fps = fpsMask == 0x01 ? 25 : fpsMask == 0x03 ? (30 / 1.001) : 0;
                    CString tmp;
                    int hours = bytes[0];
                    tmp.Format(_T("%x"), hours);
                    _stscanf_s(tmp, _T("%d"), &hours);
                    int minutes = bytes[1];
                    tmp.Format(_T("%x"), minutes);
                    _stscanf_s(tmp, _T("%d"), &minutes);
                    int seconds = bytes[2];
                    tmp.Format(_T("%x"), seconds);
                    _stscanf_s(tmp, _T("%d"), &seconds);
                    int mmseconds = 0;
                    if (fps != 0) {
                        mmseconds = (int)(1000 * frames / fps);
                    }

                    REFERENCE_TIME rtCurrentTime = 10000i64 * (((hours * 60 + minutes) * 60 + seconds) * 1000 + mmseconds);
                    rtTotalTime += rtCurrentTime;
                }
            }
            rtDuration += rtTotalTime;
            m_pChapters[currentProgram + 1] = rtDuration;
        }
    }
    catch (...) {
        m_ifoFile.Close();
        ASSERT(FALSE);
        return false;
    }

    m_ifoFile.Close();

    int offset = -1;

    vobs.RemoveAll();

    fn = fn.Left(fn.ReverseFind('.') + 1);
    fn.TrimRight(_T(".0123456789"));
    for (int i = 0; i < 100; i++) {
        CString vob;
        vob.Format(_T("%s%d.vob"), fn.GetString(), i);

        CFileStatus status;
        try {
            if (!CFile::GetStatus(vob, status)) {
                if (i == 0) {
                    continue;
                }
                else {
                    break;
                }
            }
        }
        catch (...) {
            break;
        }

        if (status.m_size & 0x7ff) {
            vobs.RemoveAll();
            break;
        }

        if (status.m_size > 0) {
            vobs.AddTail(vob);
        }

        if (i == 0) {
            offset = (int)(status.m_size / 0x800);
        }
    }

    return Open(vobs, offset, bAuthenticate);
}

bool CVobFile::Open(const CAtlList<CString>& vobs, int offset /*= -1*/, bool bAuthenticate /*= true*/)
{
    Close();

    if (vobs.IsEmpty()) {
        return false;
    }

    m_offsetAuth = offset;
    if (vobs.GetCount() == 1) {
        m_offsetAuth = -1;
    }
    m_offset = std::max(m_offsetAuth, 0);

    POSITION pos = vobs.GetHeadPosition();
    while (pos) {
        CString fn = vobs.GetNext(pos);

        WIN32_FIND_DATA fd;
        HANDLE h = FindFirstFile(fn, &fd);
        if (h == INVALID_HANDLE_VALUE) {
            m_files.RemoveAll();
            return false;
        }
        FindClose(h);

        file_t f;
        f.fn = fn;
        f.size = (int)(((__int64(fd.nFileSizeHigh) << 32) | fd.nFileSizeLow) / 2048);
        m_files.Add(f);

        m_size += f.size;
    }

    return bAuthenticate ? Authenticate() : true;
}

bool CVobFile::Authenticate()
{
    m_fDVD = m_fHasDiscKey = m_fHasTitleKey = false;

    if (!m_files.IsEmpty() && CDVDSession::Open(m_files[0].fn)) {
        for (size_t i = 0; !m_fHasTitleKey && i < m_files.GetCount(); i++) {
            if (BeginSession()) {
                m_fDVD = true;
                CDVDSession::Authenticate();
                m_fHasDiscKey = GetDiscKey();
                EndSession();
            } else {
                CString fn = m_files[0].fn;
                fn.MakeLower();

                if (fn.Find(_T(":\\video_ts")) == 1 && GetDriveType(fn.Left(3)) == DRIVE_CDROM) {
                    m_fDVD = true;
                }

                break;
            }

            if (tp_udf_file f = udf_find_file(m_hDrive, 0, CStringA(m_files[i].fn.Mid(m_files[i].fn.Find(_T(':')) + 1)))) {
                DWORD start, end;
                if (udf_get_lba(m_hDrive, f, &start, &end)) {
                    if (BeginSession()) {
                        CDVDSession::Authenticate();
                        m_fHasTitleKey = GetTitleKey(start + f->partition_lba, m_TitleKey);
                        EndSession();
                    }
                }

                udf_free(f);
            }

            BYTE key[5];
            if (HasTitleKey(key) && i == 0 && m_offsetAuth >= 0) {
                i++;

                if (BeginSession()) {
                    m_fDVD = true;
                    CDVDSession::Authenticate();
                    m_fHasDiscKey = GetDiscKey();
                    EndSession();
                } else {
                    break;
                }

                if (tp_udf_file f = udf_find_file(m_hDrive, 0, CStringA(m_files[i].fn.Mid(m_files[i].fn.Find(_T(':')) + 1)))) {
                    DWORD start, end;
                    if (udf_get_lba(m_hDrive, f, &start, &end)) {
                        if (BeginSession()) {
                            CDVDSession::Authenticate();
                            m_fHasTitleKey = GetTitleKey(start + f->partition_lba, m_TitleKey);
                            EndSession();
                        }
                    }

                    udf_free(f);
                }

                if (!m_fHasTitleKey) {
                    memcpy(m_TitleKey, key, 5);
                    m_fHasTitleKey = true;
                }
            }
        }
    }

    /*if(!m_files.IsEmpty() && !m_fDVD) {
        CString fn = m_files[0].fn;
        fn.MakeLower();

        if(fn.Find(_T(":\\video_ts")) == 1 && GetDriveType(fn.Left(3)) == DRIVE_CDROM)
        {
        m_fDVD = true;
        }
    }*/

    return true;
}

void CVobFile::Close()
{
    CDVDSession::Close();
    m_files.RemoveAll();
    m_iFile = -1;
    m_pos = m_size = m_offset = m_offsetAuth = 0;
    m_file.Close();
    m_fDVD = m_fHasDiscKey = m_fHasTitleKey = false;
}

int CVobFile::GetLength() const
{
    return (m_size - m_offset);
}

int CVobFile::GetPosition() const
{
    return (m_pos - m_offset);
}

int CVobFile::Seek(int pos)
{
    pos = std::min(std::max(pos + m_offset, m_offset), m_size - 1);

    int i = -1;
    int size = 0;

    // this suxx, but won't take long
    do {
        size += m_files[++i].size;
    } while (i < (int)m_files.GetCount() && pos >= size);

    if (i != m_iFile && i < (int)m_files.GetCount()) {
        if (!m_file.Open(m_files[i].fn)) {
            return m_pos;
        }

        m_iFile = i;
    }

    m_pos = pos;

    pos -= (size - m_files[i].size);
    try {
        m_file.Seek(pos);
    } catch (...) {
        return -1;
    }

    return GetPosition();
}

bool CVobFile::Read(BYTE* buff)
{
    if (m_pos >= m_size) {
        return false;
    }

    if (m_file.IsOpen() && m_file.GetPositionLBA() == m_file.GetLengthLBA()) {
        m_file.Close();
    }

    if (!m_file.IsOpen()) {
        if (m_iFile >= (int)m_files.GetCount() - 1) {
            return false;
        }

        if (!m_file.Open(m_files[++m_iFile].fn)) {
            m_iFile = -1;
            return false;
        }
    }

    if (!m_file.Read(buff)) {
        // dvd still locked?
        return false;
    }

    m_pos++;

    if (buff[0x14] & 0x30) {
        if (m_fHasTitleKey) {
            CSSdescramble(buff, m_TitleKey);
            buff[0x14] &= ~0x30;
        } else {
            // under win9x this is normal, but I'm not developing under win9x :P
            ASSERT(0);
        }
    }

    return true;
}

BSTR CVobFile::GetTrackName(UINT aTrackIdx) const
{
    CString trackName;
    m_pStream_Lang.Lookup(aTrackIdx, trackName);
    return trackName.AllocSysString();
}

REFERENCE_TIME CVobFile::GetChapterOffset(UINT ChapterNumber) const
{
    REFERENCE_TIME rtChapterOffset = 0;
    ASSERT(ChapterNumber < BYTE_MAX);
    m_pChapters.Lookup((BYTE)ChapterNumber, rtChapterOffset);
    return rtChapterOffset;
}
