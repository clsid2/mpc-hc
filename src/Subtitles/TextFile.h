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

#include "StdioFile64.h"
#include "../DSUtil/chardet.h"

class CTextFile : protected CStdioFile64
{
public:
    enum enc {
        DEFAULT_ENCODING,
        UTF8,
        LE16,
        BE16,
        ANSI
    };

private:
    enc m_encoding, m_defaultencoding, m_fallbackencoding;
    int m_offset;
    ULONGLONG m_posInFile;
    CAutoVectorPtr<char> m_buffer;
    CAutoVectorPtr<WCHAR> m_wbuffer;
    LONGLONG m_posInBuffer, m_nInBuffer;
    charDet::DetectObj* m_detectResult = NULL;
    bool bUseChardetlib;

public:
    using CFile::Flush;
    using CFile::Close;
    CTextFile(enc e = DEFAULT_ENCODING, bool use_chardetlib = true);
    ~CTextFile();

    virtual bool Open(LPCTSTR lpszFileName);
    virtual bool Save(LPCTSTR lpszFileName, enc e /*= DEFAULT_ENCODING*/);

    void SetEncoding(enc e);
    void SetFallbackEncoding(enc e);
    enc GetEncoding();
    bool IsUnicode();

    // CFile

    CString GetFilePath() const;

    // CStdioFile

    ULONGLONG GetPosition() const;
    ULONGLONG GetLength() const;
    ULONGLONG Seek(LONGLONG lOff, UINT nFrom);

    void WriteString(LPCSTR lpsz/*CStringA str*/);
    void WriteString(LPCWSTR lpsz/*CStringW str*/);
    BOOL ReadString(CStringA& str);
    BOOL ReadString(CStringW& str);

protected:
    virtual bool ReopenAsText();
    bool FillBuffer();
    ULONGLONG GetPositionFastBuffered() const;
    size_t GetLengthInOriginEncoding(CStringW in);
    CStringW GetBufferContent();
};

class CWebTextFile : public CTextFile
{
    LONGLONG m_llMaxSize;
    CString m_tempfn;

public:
    CWebTextFile(CTextFile::enc e = DEFAULT_ENCODING, LONGLONG llMaxSize = 1024 * 1024);

    bool Open(LPCTSTR lpszFileName);
    bool Save(LPCTSTR lpszFileName, enc e /*= DEFAULT_ENCODING*/);
    void Close();
};

extern CStringW AToW(CStringA str);
extern CStringA WToA(CStringW str);
extern CString  AToT(CStringA str);
extern CString  WToT(CStringW str);
extern CStringA TToA(CString  str);
extern CStringW TToW(CString  str);
