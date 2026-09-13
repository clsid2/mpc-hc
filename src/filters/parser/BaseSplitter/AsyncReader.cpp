/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2013 see Authors.txt
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
#include "AsyncReader.h"
#include <afxsock.h>
#include <afxinet.h>
#include "../../../DSUtil/DSUtil.h"

//
// CAsyncFileReader
//

CAsyncFileReader::CAsyncFileReader(CString fn, HRESULT& hr)
    : CUnknown(NAME("CAsyncFileReader"), nullptr, &hr)
    , m_len(ULONGLONG_MAX)
    , m_hBreakEvent(nullptr)
    , m_lOsError(0)
{
    hr = Open(fn, modeRead | shareDenyNone | typeBinary | osSequentialScan) ? S_OK : E_FAIL;
    if (SUCCEEDED(hr)) {
        m_len = GetLength();
    }
}

CAsyncFileReader::CAsyncFileReader(CAtlList<CHdmvClipInfo::PlaylistItem>& Items, HRESULT& hr)
    : CUnknown(NAME("CAsyncFileReader"), nullptr, &hr)
    , m_len(ULONGLONG_MAX)
    , m_hBreakEvent(nullptr)
    , m_lOsError(0)
{
    hr = OpenFiles(Items, modeRead | shareDenyNone | typeBinary | osSequentialScan) ? S_OK : E_FAIL;
    if (SUCCEEDED(hr)) {
        m_len = GetLength();
    }
}

STDMETHODIMP CAsyncFileReader::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    CheckPointer(ppv, E_POINTER);

    return
        QI(IAsyncReader)
        QI(ISyncReader)
        QI(IFileHandle)
        __super::NonDelegatingQueryInterface(riid, ppv);
}

// IAsyncReader

STDMETHODIMP CAsyncFileReader::SyncRead(LONGLONG llPosition, LONG lLength, BYTE* pBuffer)
{
    do {
        try {
            if ((ULONGLONG)llPosition + lLength > GetLength()) {
                return E_FAIL;    // strange, but the Seek below can return llPosition even if the file is not that big (?)
            }
            if ((ULONGLONG)llPosition != Seek(llPosition, begin)) {
                return E_FAIL;
            }
            // a short read (bad sector, unplugged drive, dropped share) must not
            // hand the splitter a partly filled buffer as success
            if (Read(pBuffer, lLength) < (UINT)lLength) {
                return E_FAIL;
            }

#if 0 // def DEBUG
            static __int64 s_total = 0, s_laststoppos = 0;
            s_total += lLength;
            if (s_laststoppos > llPosition) {
                TRACE(_T("[%I64d - %I64d] %d (%I64d)\n"), llPosition, llPosition + lLength, lLength, s_total);
            }
            s_laststoppos = llPosition + lLength;
#endif

            return S_OK;
        } catch (CFileException* e) {
            m_lOsError = e->m_lOsError;
            e->Delete();
            Sleep(1);
            CString fn = m_strFileName;
            try {
                Close();
            } catch (CFileException* fe) {
                fe->Delete();
            }
            try {
                Open(fn, modeRead | shareDenyNone | typeBinary | osSequentialScan);
            } catch (CFileException* fe) {
                fe->Delete();
            }
            m_strFileName = fn;
        }
    } while (m_hBreakEvent && WaitForSingleObject(m_hBreakEvent, 0) == WAIT_TIMEOUT);

    return E_FAIL;
}

STDMETHODIMP CAsyncFileReader::Length(LONGLONG* pTotal, LONGLONG* pAvailable)
{
    LONGLONG len = (m_len != ULONGLONG_MAX) ? m_len : GetLength();
    if (pTotal) {
        *pTotal = len;
    }
    if (pAvailable) {
        *pAvailable = len;
    }
    return S_OK;
}

// IFileHandle

STDMETHODIMP_(HANDLE) CAsyncFileReader::GetFileHandle()
{
    return m_hFile;
}

STDMETHODIMP_(LPCTSTR) CAsyncFileReader::GetFileName()
{
    return m_nCurPart != -1 ? m_strFiles[m_nCurPart] : m_strFiles[0];
}

//
// CAsyncUrlReader
//

CAsyncUrlReader::CAsyncUrlReader(CString url, HRESULT& hr)
    : CAsyncFileReader(url, hr)
{
    if (SUCCEEDED(hr)) {
        return;
    }

    m_url = url;

    if (CAMThread::Create()) {
        CallWorker(CMD_INIT);
    }

    hr = Open(m_fn, modeRead | shareDenyRead | typeBinary | osSequentialScan) ? S_OK : E_FAIL;
    m_len = ULONGLONG_MAX; // force GetLength() return actual length always
}

CAsyncUrlReader::~CAsyncUrlReader()
{
    if (ThreadExists()) {
        CallWorker(CMD_EXIT);
    }

    if (!m_fn.IsEmpty()) {
        CMultiFiles::Close();
        DeleteFile(m_fn);
    }
}

// IAsyncReader

STDMETHODIMP CAsyncUrlReader::Length(LONGLONG* pTotal, LONGLONG* pAvailable)
{
    if (pTotal) {
        *pTotal = 0;
    }
    return __super::Length(nullptr, pAvailable);
}

// CAMThread

DWORD CAsyncUrlReader::ThreadProc()
{
    AfxSocketInit(nullptr);

    DWORD cmd = GetRequest();
    if (cmd != CMD_INIT) {
        Reply((DWORD)E_FAIL);
        return DWORD_ERROR;
    }

    try {
        CInternetSession is;
        CAutoPtr<CStdioFile> fin(is.OpenURL(m_url, 1, INTERNET_FLAG_TRANSFER_BINARY | INTERNET_FLAG_EXISTING_CONNECT | INTERNET_FLAG_NO_CACHE_WRITE));

        TCHAR path[MAX_PATH], fn[MAX_PATH];
        CFile fout;
        if (GetTempPath(MAX_PATH, path) && GetTempFileName(path, _T("mpc_http"), 0, fn)
                && fout.Open(fn, modeCreate | modeWrite | shareDenyWrite | typeBinary)) {
            m_fn = fn;

            char buff[1024];
            int len = fin->Read(buff, sizeof(buff));
            if (len > 0) {
                fout.Write(buff, len);
            }

            Reply(S_OK);

            while (!CheckRequest(&cmd)) {
                int len2 = fin->Read(buff, sizeof(buff));
                if (len2 > 0) {
                    fout.Write(buff, len2);
                }
            }
        } else {
            Reply((DWORD)E_FAIL);
        }

        fin->Close(); // must close it because the destructor doesn't seem to do it and we will get an exception when "is" is destroying
    } catch (CInternetException* ie) {
        ie->Delete();
        Reply((DWORD)E_FAIL);
    }

    //

    cmd = GetRequest();
    ASSERT(cmd == CMD_EXIT);
    Reply(S_OK);

    //

    m_hThread = nullptr;

    return S_OK;
}
