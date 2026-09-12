/*
 * (C) 2008-2014 see Authors.txt
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
#include "Ifo.h"


#ifdef WORDS_BIGENDIAN
#define bswap_16(x) (x)
#define bswap_32(x) (x)
#define bswap_64(x) (x)
#else

// code from bits/byteswap.h (C) 1997, 1998 Free Software Foundation, Inc.
#define bswap_16(x) \
    ((((x) >> 8) & 0xff) | (((x) & 0xff) << 8))

// code from bits/byteswap.h (C) 1997, 1998 Free Software Foundation, Inc.
#define bswap_32(x)                                            \
    ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >>  8) | \
    (((x)  & 0x0000ff00) <<  8) | (((x) & 0x000000ff) << 24))

#define bswap_64(x)                                       \
    (__extension__                                        \
    ({ union { __extension__ unsigned long long int __ll; \
                unsigned long int __l[2]; } __w, __r;     \
        __w.__ll = (x);                                   \
        __r.__l[0] = bswap_32 (__w.__l[1]);               \
        __r.__l[1] = bswap_32 (__w.__l[0]);               \
        __r.__ll; }))
#endif

#ifdef WORDS_BIGENDIAN
#define be2me_16(x) (x)
#define be2me_32(x) (x)
#define be2me_64(x) (x)
#define le2me_16(x) bswap_16(x)
#define le2me_32(x) bswap_32(x)
#define le2me_64(x) bswap_64(x)
#else
#define be2me_16(x) bswap_16(x)
#define be2me_32(x) bswap_32(x)
#define be2me_64(x) bswap_64(x)
#define le2me_16(x) (x)
#define le2me_32(x) (x)
#define le2me_64(x) (x)
#endif

#define DVD_VIDEO_LB_LEN    2048
#define IFO_HDR_LEN            8
#define LU_SUB_LEN             8

extern HANDLE(__stdcall* Real_CreateFileW)(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);


uint32_t get4bytes(const BYTE* buf)
{
    return be2me_32(*((uint32_t*)buf));
}


// VMG files
#define OFF_VMGM_PGCI_UT(buf)   get4bytes (buf + 0xC8)

// VTS files
#define OFF_VTSM_PGCI_UT(buf)   get4bytes (buf + 0xD0)
#define OFF_VTS_PGCIT(buf)      get4bytes (buf + 0xCC)


CIfo::CIfo()
    : m_pBuffer(nullptr)
    , m_dwSize(0)
    , m_pPGCI(nullptr)
    , m_pPGCIT(nullptr)
{
}

CIfo::~CIfo()
{
    delete [] m_pBuffer;
}

// Every table offset and count below comes straight from the file, so each
// derived pointer has to be checked against the buffer before it is used.
bool CIfo::IsInBuffer(const void* p, size_t len) const
{
    const BYTE* b = static_cast<const BYTE*>(p);
    return m_pBuffer && b >= m_pBuffer && len <= m_dwSize && b <= m_pBuffer + m_dwSize - len;
}

int CIfo::GetMiscPGCI(CIfo::ifo_hdr_t* hdr, int title, uint8_t** ptr)
{
    pgci_sub_t* pgci_sub;

    *ptr = (uint8_t*) hdr;
    *ptr += IFO_HDR_LEN;
    pgci_sub = (pgci_sub_t*) *ptr + title;

    if (title < 0 || !IsInBuffer(pgci_sub, sizeof(pgci_sub_t))) {
        return -1;
    }

    *ptr = (uint8_t*) hdr + be2me_32(pgci_sub->start);

    if (!IsInBuffer(*ptr, PGC_SIZE)) {
        return -1;
    }

    return 0;
}

void CIfo::RemovePgciUOPs(uint8_t* ptr)
{
    ifo_hdr_t* hdr = (ifo_hdr_t*) ptr;
    uint16_t num;
    int i;

    if (!IsInBuffer(hdr, IFO_HDR_LEN)) {
        return;
    }

    ptr += IFO_HDR_LEN;
    num  = be2me_16(hdr->num);

    for (i = 1; i <= num; i++) {
        lu_sub_t* lu_sub = (lu_sub_t*) ptr;
        UNREFERENCED_PARAMETER(lu_sub);

        ptr += LU_SUB_LEN;
    }

    for (i = 0; i < be2me_16(hdr->num); i++) {
        uint8_t* ptr2;

        if (GetMiscPGCI(hdr, i, &ptr2) >= 0) {
            pgc_t* pgc = (pgc_t*) ptr2;
            pgc->prohibited_ops = 0;
        }
    }
}

CIfo::pgc_t* CIfo::GetFirstPGC()
{
    if (m_pBuffer) {
        return (pgc_t*)(m_pBuffer + 0x0400);
    } else {
        return nullptr;
    }
}

CIfo::pgc_t* CIfo::GetPGCI(const int title, const ifo_hdr_t* hdr)
{
    CIfo::pgci_sub_t* pgci_sub;
    uint8_t* ptr;

    ptr = (uint8_t*) hdr;
    ptr += IFO_HDR_LEN;

    pgci_sub = (pgci_sub_t*) ptr + title;

    if (title < 0 || !IsInBuffer(hdr, IFO_HDR_LEN) || !IsInBuffer(pgci_sub, sizeof(pgci_sub_t))) {
        return nullptr;
    }

    ptr = (uint8_t*) hdr + be2me_32(pgci_sub->start);

    /* jdw */
    if (ptr >= ((uint8_t*) hdr + be2me_32(hdr->len))) {
        return nullptr;
    }
    /* /jdw */

    // hdr->len is from the file as well, so check against the buffer too
    if (!IsInBuffer(ptr, PGC_SIZE)) {
        return nullptr;
    }

    return (pgc_t*) ptr;
}

bool CIfo::IsVTS()
{
    if (m_dwSize < 12 || (strncmp((char*)m_pBuffer, "DVDVIDEO-VTS", 12) != 0)) {
        return false;
    }

    return true;
}

bool CIfo::IsVMG()
{
    if (m_dwSize < 12 || (strncmp((char*)m_pBuffer, "DVDVIDEO-VMG", 12) != 0)) {
        return false;
    }

    return true;
}

bool CIfo::OpenFile(LPCTSTR strFile)
{
    bool bRet = false;

    HANDLE hFile = Real_CreateFileW(strFile, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER size;
        // min size of the ifo file comes from dvd sector size,
        // max size we allow is 8 MB (taken with reserve),
        // also the file size must a multiple of dvd sector size
        if (GetFileSizeEx(hFile, &size) && (size.QuadPart >= DVD_VIDEO_LB_LEN) &&
                (size.QuadPart <= 0x800000) && !(size.QuadPart % DVD_VIDEO_LB_LEN)) {
            ASSERT(!m_pBuffer);
            m_pBuffer = DEBUG_NEW BYTE [(size_t)size.QuadPart];
            if (ReadFile(hFile, m_pBuffer, (DWORD)size.QuadPart, &m_dwSize, nullptr)) {
                ASSERT(!m_pPGCI);
                ASSERT(!m_pPGCIT);
                uint32_t sector, sectorCount = (uint32_t)size.QuadPart / DVD_VIDEO_LB_LEN;
                if (IsVTS()) {
                    sector = OFF_VTSM_PGCI_UT(m_pBuffer);
                    if (sector && (sector < sectorCount)) {
                        m_pPGCI = (ifo_hdr_t*)(m_pBuffer + sector * DVD_VIDEO_LB_LEN);
                    } else {
                        TRACE(_T("IFO: Missing or invalid VTSM_PGCI_UT sector\n"));
                    }
                    sector = OFF_VTS_PGCIT(m_pBuffer);
                    if (sector && (sector < sectorCount)) {
                        m_pPGCIT = (ifo_hdr_t*)(m_pBuffer + sector * DVD_VIDEO_LB_LEN);
                    } else {
                        TRACE(_T("IFO: Missing or invalid VTS_PGCI sector\n"));
                    }
                } else if (IsVMG()) {
                    sector = OFF_VMGM_PGCI_UT(m_pBuffer);
                    if (sector && (sector < sectorCount)) {
                        m_pPGCI = (ifo_hdr_t*)(m_pBuffer + sector * DVD_VIDEO_LB_LEN);
                    } else {
                        TRACE(_T("IFO: Missing or invalid VTSM_PGCI_UT sector\n"));
                    }
                }
                bRet = (m_pPGCI != nullptr);
            }
        }
        CloseHandle(hFile);
    } else {
        ASSERT(FALSE);
    }

    return bRet;
}

bool CIfo::RemoveUOPs()
{
    pgc_t* pgc;

    if (m_pPGCI) {
        pgc = GetFirstPGC();
        pgc->prohibited_ops = 0;

        for (int i = 0; i < be2me_16(m_pPGCI->num); i++) {
            pgc = GetPGCI(i, m_pPGCI);
            if (pgc) {
                RemovePgciUOPs((uint8_t*)pgc);
            }
        }
    }
    if (m_pPGCIT) {
        for (int i = 0; i < be2me_16(m_pPGCIT->num); i++) {
            pgc = GetPGCI(i, m_pPGCIT);
            if (pgc) {
                pgc->prohibited_ops = 0;
            }
        }
    }
    return true;
}

bool CIfo::SaveFile(LPCTSTR strFile)
{
    bool bRet = false;

    if (m_pBuffer) {
        HANDLE hFile = Real_CreateFileW(strFile, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                        nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD written;
            if (WriteFile(hFile, m_pBuffer, m_dwSize, &written, nullptr)) {
                bRet = true;
            }
            CloseHandle(hFile);
        }
    }

    ASSERT(bRet);

    return bRet;
}
