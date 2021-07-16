/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2014, 2016 see Authors.txt
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
#include <atlbase.h>
#include <afxinet.h>
#include <algorithm>
#include "TextFile.h"
#include "Utf8.h"
#include "libiconv/libiconv-for-Windows/include/iconv.h"

#define TEXTFILE_BUFFER_SIZE (64 * 1024)

CTextFile::CTextFile(enc e, bool use_chardetlib)
    : m_encoding(e)
    , m_defaultencoding(e)
    , m_fallbackencoding(DEFAULT_ENCODING)
    , m_offset(0)
    , m_posInFile(0)
    , m_posInBuffer(0)
    , m_nInBuffer(0)
    , bUseChardetlib(use_chardetlib)
{
    m_buffer.Allocate(TEXTFILE_BUFFER_SIZE);
    m_wbuffer.Allocate(TEXTFILE_BUFFER_SIZE);
}

CTextFile::~CTextFile() {
    if (m_detectResult) free(m_detectResult);
}

bool CTextFile::Open(LPCTSTR lpszFileName)
{
    if (!__super::Open(lpszFileName, modeRead | typeBinary | shareDenyNone)) {
        return false;
    }

    m_encoding = m_defaultencoding;
    m_offset = 0;
    m_nInBuffer = m_posInBuffer = 0;
    if (m_detectResult) free(m_detectResult);
    m_detectResult = NULL;

    try {
        if (bUseChardetlib) {
            char buffer[4097];
            UINT le = Read(buffer, 4096);
            if (le <= 0) return Close(), false;
            buffer[le] = '\0';
            m_detectResult = charDet::charDetect(buffer, le);
        }
        if (__super::GetLength() >= 2 && m_detectResult == NULL) {
            if (bUseChardetlib) Seek(0, begin);
            WORD w;
            if (sizeof(w) != Read(&w, sizeof(w))) {
                return Close(), false;
            }

            if (w == 0xfeff) {
                m_encoding = LE16;
                m_offset = 2;
            } else if (w == 0xfffe) {
                m_encoding = BE16;
                m_offset = 2;
            } else if (w == 0xbbef && __super::GetLength() >= 3) {
                BYTE b;
                if (sizeof(b) != Read(&b, sizeof(b))) {
                    return Close(), false;
                }

                if (b == 0xbf) {
                    m_encoding = UTF8;
                    m_offset = 3;
                }
            }
        }
        if (m_detectResult) {
            if (m_detectResult->bom) {
                if (!strcmp(m_detectResult->encoding, "UTF-16LE") || !strcmp(m_detectResult->encoding, "UTF-16BE")) m_offset = 2;
                if (!strcmp(m_detectResult->encoding, "UTF-8") || !strcmp(m_detectResult->encoding, "UTF-7") || !strcmp(m_detectResult->encoding, "UTF-1")) m_offset = 3;
                if (!strcmp(m_detectResult->encoding, "UTF-32LE") || !strcmp(m_detectResult->encoding, "UTF-32BE")) m_offset = 4;
            }
            Seek(0, begin);
            // No need use iconv to convert.
            if (!strcmp(m_detectResult->encoding, "ASCII")) {
                free(m_detectResult);
                m_detectResult = NULL;
                m_encoding = ANSI;
            } else if (!strcmp(m_detectResult->encoding, "UTF-8")) {
                free(m_detectResult);
                m_detectResult = NULL;
                m_encoding = UTF8;
            } else if (!strcmp(m_detectResult->encoding, "UTF-16LE")) {
                free(m_detectResult);
                m_detectResult = NULL;
                m_encoding = LE16;
            } else if (!strcmp(m_detectResult->encoding, "UTF-16BE")) {
                free(m_detectResult);
                m_detectResult = NULL;
                m_encoding = BE16;
            }
        } else if (m_encoding == DEFAULT_ENCODING) {
            if (!ReopenAsText()) {
                return false;
            }
        } else if (m_offset == 0) { // No BOM detected, ensure the file is read from the beginning
            Seek(0, begin);
        } else {
            m_posInFile = __super::GetPosition();
        }
    } catch (CFileException*) {
        return false;
    }

    return true;
}

bool CTextFile::ReopenAsText()
{
    CString strFileName = m_strFileName;

    __super::Close(); // CWebTextFile::Close() would delete the temp file if we called it...

    return !!__super::Open(strFileName, modeRead | typeText | shareDenyNone);
}

bool CTextFile::Save(LPCTSTR lpszFileName, enc e)
{
    if (!__super::Open(lpszFileName, modeCreate | modeWrite | shareDenyWrite | (e == DEFAULT_ENCODING ? typeText : typeBinary))) {
        return false;
    }

    if (e == UTF8) {
        BYTE b[3] = {0xef, 0xbb, 0xbf};
        Write(b, sizeof(b));
    } else if (e == LE16) {
        BYTE b[2] = {0xff, 0xfe};
        Write(b, sizeof(b));
    } else if (e == BE16) {
        BYTE b[2] = {0xfe, 0xff};
        Write(b, sizeof(b));
    }

    m_encoding = e;

    return true;
}

void CTextFile::SetEncoding(enc e)
{
    m_encoding = e;
}

void CTextFile::SetFallbackEncoding(enc e)
{
    m_fallbackencoding = e;
}

CTextFile::enc CTextFile::GetEncoding()
{
    return m_encoding;
}

bool CTextFile::IsUnicode()
{
    return m_encoding == UTF8 || m_encoding == LE16 || m_encoding == BE16 || (m_detectResult && m_detectResult->encoding && (!strcmp(m_detectResult->encoding, "UTF-7") || !strcmp(m_detectResult->encoding, "UTF-1") || !strcmp(m_detectResult->encoding, "UTF-32LE") || !strcmp(m_detectResult->encoding, "UTF-32BE")));
}

// CFile

CString CTextFile::GetFilePath() const
{
    // to avoid a CException coming from CTime
    return m_strFileName; // __super::GetFilePath();
}

// CStdioFile

ULONGLONG CTextFile::GetPosition() const
{
    return (__super::GetPosition() - m_offset - (m_nInBuffer - m_posInBuffer));
}

ULONGLONG CTextFile::GetLength() const
{
    return (__super::GetLength() - m_offset);
}

size_t CTextFile::GetLengthInOriginEncoding(CStringW in) {
    if (!m_detectResult || !m_detectResult->encoding) return -1;
    wchar_t* p = const_cast<wchar_t*> (in.GetString());
    char* p2 = (char*)p;
    size_t inlen = in.GetLength() * sizeof(wchar_t);
    iconv_t cd;
    cd = iconv_open(m_detectResult->encoding, "WCHAR_T");
    if (cd == (iconv_t)-1) return -1;
    char buf[TEXTFILE_BUFFER_SIZE];
    char* buf2 = NULL;
    size_t avail, re(0);
    do {
        avail = TEXTFILE_BUFFER_SIZE;
        buf2 = buf;
        iconv(cd, &p2, &inlen, &buf2, &avail);
        re += (TEXTFILE_BUFFER_SIZE - avail);
    } while (!avail);
    iconv_close(cd);
    return re;
}

CStringW CTextFile::GetBufferContent() {
    if (!m_detectResult || !m_detectResult->encoding) return L"";
    FillBuffer();
    if (!m_nInBuffer) return L"";
    iconv_t cd;
    cd = iconv_open("WCHAR_T", m_detectResult->encoding);
    if (cd == (iconv_t)-1) return L"";
    CStringW re;
    wchar_t buffer[TEXTFILE_BUFFER_SIZE];
    char* in = m_buffer.m_p;
    char* out = (char*)buffer;
    size_t inlen(m_nInBuffer), outlen;
    do {
        outlen = (TEXTFILE_BUFFER_SIZE - 1) * sizeof(wchar_t);
        out = (char*)buffer;
        iconv(cd, &in, &inlen, &out, &outlen);
        *((wchar_t *)out) = L'\0';
        re.Append(CStringW(buffer));
    } while (!outlen);
    iconv_close(cd);
    return re;
}

ULONGLONG CTextFile::Seek(LONGLONG lOff, UINT nFrom)
{
    ULONGLONG newPos;

    // Try to reuse the buffer if any
    if (m_nInBuffer > 0) {
        ULONGLONG pos = GetPosition();
        ULONGLONG len = GetLength();

        switch (nFrom) {
            default:
            case begin:
                break;
            case current:
                lOff = pos + lOff;
                break;
            case end:
                lOff = len - lOff;
                break;
        }

        lOff = std::max((LONGLONG)std::min((ULONGLONG)lOff, len), 0ll);

        m_posInBuffer += LONGLONG(ULONGLONG(lOff) - pos);
        if (m_posInBuffer < 0 || m_posInBuffer >= m_nInBuffer) {
            // If we would have to end up out of the buffer, we just reset it and seek normally
            m_nInBuffer = m_posInBuffer = 0;
            newPos = __super::Seek(lOff + m_offset, begin) - m_offset;
        } else { // If we can reuse the buffer, we have nothing special to do
            newPos = ULONGLONG(lOff);
        }
    } else { // No buffer, we can use the base implementation
        if (nFrom == begin) {
            lOff += m_offset;
        }
        newPos = __super::Seek(lOff, nFrom) - m_offset;
    }

    m_posInFile = newPos + m_offset + (m_nInBuffer - m_posInBuffer);

    return newPos;
}

void CTextFile::WriteString(LPCSTR lpsz/*CStringA str*/)
{
    CStringA str(lpsz);

    if (m_encoding == DEFAULT_ENCODING) {
        __super::WriteString(AToT(str));
    } else if (m_encoding == ANSI) {
        str.Replace("\n", "\r\n");
        Write((LPCSTR)str, str.GetLength());
    } else if (m_encoding == UTF8) {
        WriteString(AToW(str));
    } else if (m_encoding == LE16) {
        WriteString(AToW(str));
    } else if (m_encoding == BE16) {
        WriteString(AToW(str));
    }
}

void CTextFile::WriteString(LPCWSTR lpsz/*CStringW str*/)
{
    CStringW str(lpsz);

    if (m_encoding == DEFAULT_ENCODING) {
        __super::WriteString(WToT(str));
    } else if (m_encoding == ANSI) {
        str.Replace(L"\n", L"\r\n");
        CStringA stra(str); // TODO: codepage
        Write((LPCSTR)stra, stra.GetLength());
    } else if (m_encoding == UTF8) {
        str.Replace(L"\n", L"\r\n");
        for (unsigned int i = 0, l = str.GetLength(); i < l; i++) {
            DWORD c = (WORD)str[i];

            if (c < 0x80) { // 0xxxxxxx
                Write(&c, 1);
            } else if (0x80 <= c && c < 0x800) { // 110xxxxx 10xxxxxx
                c = 0xc080 | ((c << 2) & 0x1f00) | (c & 0x003f);
                Write((BYTE*)&c + 1, 1);
                Write(&c, 1);
            } else if (0x800 <= c && c < 0xFFFF) { // 1110xxxx 10xxxxxx 10xxxxxx
                c = 0xe08080 | ((c << 4) & 0x0f0000) | ((c << 2) & 0x3f00) | (c & 0x003f);
                Write((BYTE*)&c + 2, 1);
                Write((BYTE*)&c + 1, 1);
                Write(&c, 1);
            } else {
                c = '?';
                Write(&c, 1);
            }
        }
    } else if (m_encoding == LE16) {
        str.Replace(L"\n", L"\r\n");
        Write((LPCWSTR)str, str.GetLength() * 2);
    } else if (m_encoding == BE16) {
        str.Replace(L"\n", L"\r\n");
        for (unsigned int i = 0, l = str.GetLength(); i < l; i++) {
            str.SetAt(i, ((str[i] >> 8) & 0x00ff) | ((str[i] << 8) & 0xff00));
        }
        Write((LPCWSTR)str, str.GetLength() * 2);
    }
}

bool CTextFile::FillBuffer()
{
    if (m_posInBuffer < m_nInBuffer) {
        m_nInBuffer -= m_posInBuffer;
        memcpy(m_buffer, &m_buffer[m_posInBuffer], (size_t)m_nInBuffer * sizeof(char));
    } else {
        m_nInBuffer = 0;
    }
    m_posInBuffer = 0;

    UINT nBytesRead = Read(&m_buffer[m_nInBuffer], UINT(TEXTFILE_BUFFER_SIZE - m_nInBuffer) * sizeof(char));
    if (nBytesRead) {
        m_nInBuffer += nBytesRead;
    }
    m_posInFile = __super::GetPosition();

    return !nBytesRead;
}

ULONGLONG CTextFile::GetPositionFastBuffered() const
{
    return (m_posInFile - m_offset - (m_nInBuffer - m_posInBuffer));
}

BOOL CTextFile::ReadString(CStringA& str)
{
    bool fEOF = true;

    str.Truncate(0);
    if (m_detectResult) {
        CStringW temp;
        BOOL re = ReadString(temp);
        str.Append(CW2A(temp));
        return re;
    }
    else if (m_encoding == DEFAULT_ENCODING) {
        CString s;
        fEOF = !__super::ReadString(s);
        str = TToA(s);
        // For consistency with other encodings, we continue reading
        // the file even when a NUL char is encountered.
        char c;
        while (fEOF && (Read(&c, sizeof(c)) == sizeof(c))) {
            str += c;
            fEOF = !__super::ReadString(s);
            str += TToA(s);
        }
    } else if (m_encoding == ANSI) {
        bool bLineEndFound = false;
        fEOF = false;

        do {
            int nCharsRead;

            for (nCharsRead = 0; m_posInBuffer + nCharsRead < m_nInBuffer; nCharsRead++) {
                if (m_buffer[m_posInBuffer + nCharsRead] == '\n') {
                    break;
                } else if (m_buffer[m_posInBuffer + nCharsRead] == '\r') {
                    break;
                }
            }

            str.Append(&m_buffer[m_posInBuffer], nCharsRead);

            m_posInBuffer += nCharsRead;
            while (m_posInBuffer < m_nInBuffer && m_buffer[m_posInBuffer] == '\r') {
                m_posInBuffer++;
            }
            if (m_posInBuffer < m_nInBuffer && m_buffer[m_posInBuffer] == '\n') {
                bLineEndFound = true; // Stop at end of line
                m_posInBuffer++;
            }

            if (!bLineEndFound) {
                bLineEndFound = FillBuffer();
                if (!nCharsRead) {
                    fEOF = bLineEndFound;
                }
            }
        } while (!bLineEndFound);
    } else if (m_encoding == UTF8) {
        ULONGLONG lineStartPos = GetPositionFastBuffered();
        bool bValid = true;
        bool bLineEndFound = false;
        fEOF = false;

        do {
            int nCharsRead;
            char* abuffer = (char*)(WCHAR*)m_wbuffer;

            for (nCharsRead = 0; m_posInBuffer < m_nInBuffer; m_posInBuffer++, nCharsRead++) {
                if (Utf8::isSingleByte(m_buffer[m_posInBuffer])) { // 0xxxxxxx
                    abuffer[nCharsRead] = m_buffer[m_posInBuffer] & 0x7f;
                } else if (Utf8::isFirstOfMultibyte(m_buffer[m_posInBuffer])) {
                    int nContinuationBytes = Utf8::continuationBytes(m_buffer[m_posInBuffer]);
                    bValid = (nContinuationBytes <= 2);

                    // We don't support characters wider than 16 bits
                    if (bValid) {
                        if (m_posInBuffer + nContinuationBytes >= m_nInBuffer) {
                            // If we are at the end of the file, the buffer won't be full
                            // and we won't be able to read any more continuation bytes.
                            bValid = (m_nInBuffer == TEXTFILE_BUFFER_SIZE);
                            break;
                        } else {
                            for (int j = 1; j <= nContinuationBytes; j++) {
                                if (!Utf8::isContinuation(m_buffer[m_posInBuffer + j])) {
                                    bValid = false;
                                }
                            }

                            switch (nContinuationBytes) {
                                case 0: // 0xxxxxxx
                                    abuffer[nCharsRead] = m_buffer[m_posInBuffer] & 0x7f;
                                    break;
                                case 1: // 110xxxxx 10xxxxxx
                                case 2: // 1110xxxx 10xxxxxx 10xxxxxx
                                    // Unsupported for non unicode strings
                                    abuffer[nCharsRead] = '?';
                                    break;
                            }
                            m_posInBuffer += nContinuationBytes;
                        }
                    }
                } else {
                    bValid = false;
                    TRACE(_T("Invalid UTF8 character found\n"));
                }

                if (!bValid) {
                    abuffer[nCharsRead] = '?';
                    m_posInBuffer++;
                    nCharsRead++;
                    break;
                } else if (abuffer[nCharsRead] == '\n') {
                    bLineEndFound = true; // Stop at end of line
                    m_posInBuffer++;
                    break;
                } else if (abuffer[nCharsRead] == '\r') {
                    nCharsRead--; // Skip \r
                }
            }

            if (bValid || m_offset) {
                str.Append(abuffer, nCharsRead);

                if (!bLineEndFound) {
                    bLineEndFound = FillBuffer();
                    if (!nCharsRead) {
                        fEOF = bLineEndFound;
                    }
                }
            } else {
                // Switch to text and read again
                m_encoding = m_fallbackencoding;
                // Stop using the buffer
                m_posInBuffer = m_nInBuffer = 0;

                fEOF = !ReopenAsText();

                if (!fEOF) {
                    // Seek back to the beginning of the line where we stopped
                    Seek(lineStartPos, begin);

                    fEOF = !ReadString(str);
                }
            }
        } while (bValid && !bLineEndFound);
    } else if (m_encoding == LE16) {
        bool bLineEndFound = false;
        fEOF = false;

        do {
            int nCharsRead;
            WCHAR* wbuffer = (WCHAR*)&m_buffer[m_posInBuffer];
            char* abuffer = (char*)(WCHAR*)m_wbuffer;

            for (nCharsRead = 0; m_posInBuffer + 1 < m_nInBuffer; nCharsRead++, m_posInBuffer += sizeof(WCHAR)) {
                if (wbuffer[nCharsRead] == L'\n') {
                    break; // Stop at end of line
                } else if (wbuffer[nCharsRead] == L'\r') {
                    break; // Skip \r
                } else if (!(wbuffer[nCharsRead] & 0xff00)) {
                    abuffer[nCharsRead] = char(wbuffer[nCharsRead] & 0xff);
                } else {
                    abuffer[nCharsRead] = '?';
                }
            }

            str.Append(abuffer, nCharsRead);

            while (m_posInBuffer + 1 < m_nInBuffer && wbuffer[nCharsRead] == L'\r') {
                nCharsRead++;
                m_posInBuffer += sizeof(WCHAR);
            }
            if (m_posInBuffer + 1 < m_nInBuffer && wbuffer[nCharsRead] == L'\n') {
                bLineEndFound = true; // Stop at end of line
                nCharsRead++;
                m_posInBuffer += sizeof(WCHAR);
            }

            if (!bLineEndFound) {
                bLineEndFound = FillBuffer();
                if (!nCharsRead) {
                    fEOF = bLineEndFound;
                }
            }
        } while (!bLineEndFound);
    } else if (m_encoding == BE16) {
        bool bLineEndFound = false;
        fEOF = false;

        do {
            int nCharsRead;
            char* abuffer = (char*)(WCHAR*)m_wbuffer;

            for (nCharsRead = 0; m_posInBuffer + 1 < m_nInBuffer; nCharsRead++, m_posInBuffer += sizeof(WCHAR)) {
                if (!m_buffer[m_posInBuffer]) {
                    abuffer[nCharsRead] = m_buffer[m_posInBuffer + 1];
                } else {
                    abuffer[nCharsRead] = '?';
                }

                if (abuffer[nCharsRead] == '\n') {
                    bLineEndFound = true; // Stop at end of line
                    m_posInBuffer += sizeof(WCHAR);
                    break;
                } else if (abuffer[nCharsRead] == L'\r') {
                    nCharsRead--; // Skip \r
                }
            }

            str.Append(abuffer, nCharsRead);

            if (!bLineEndFound) {
                bLineEndFound = FillBuffer();
                if (!nCharsRead) {
                    fEOF = bLineEndFound;
                }
            }
        } while (!bLineEndFound);
    }

    return !fEOF;
}

BOOL CTextFile::ReadString(CStringW& str)
{
    bool fEOF = true;

    str.Truncate(0);

    if (m_detectResult) {
        bool bLineEndFound = false;
        fEOF = false;

        do {
            CStringW buf = GetBufferContent();
            if (!buf.GetLength()) return FALSE;
            CStringW temp;
            int pos = buf.Find(L"\n");
            if (pos > -1) {
                temp = buf.Left(pos + 1);
                bLineEndFound = true;
            } else temp = buf;
            m_posInBuffer += GetLengthInOriginEncoding(temp);
            temp.Trim(L"\n");
            temp.Trim(L"\r");
            str.Append(temp);
            if (!str.GetLength() && bLineEndFound) bLineEndFound = false;  // Ignore empty line
        } while (!bLineEndFound);
    } else if (m_encoding == DEFAULT_ENCODING) {
        CString s;
        fEOF = !__super::ReadString(s);
        str = TToW(s);
        // For consistency with other encodings, we continue reading
        // the file even when a NUL char is encountered.
        char c;
        while (fEOF && (Read(&c, sizeof(c)) == sizeof(c))) {
            str += c;
            fEOF = !__super::ReadString(s);
            str += TToW(s);
        }
    } else if (m_encoding == ANSI) {
        bool bLineEndFound = false;
        fEOF = false;

        do {
            int nCharsRead;

            for (nCharsRead = 0; m_posInBuffer + nCharsRead < m_nInBuffer; nCharsRead++) {
                if (m_buffer[m_posInBuffer + nCharsRead] == '\n') {
                    break;
                } else if (m_buffer[m_posInBuffer + nCharsRead] == '\r') {
                    break;
                }
            }

            // TODO: codepage
            str.Append(CStringW(&m_buffer[m_posInBuffer], nCharsRead));

            m_posInBuffer += nCharsRead;
            while (m_posInBuffer < m_nInBuffer && m_buffer[m_posInBuffer] == '\r') {
                m_posInBuffer++;
            }
            if (m_posInBuffer < m_nInBuffer && m_buffer[m_posInBuffer] == '\n') {
                bLineEndFound = true; // Stop at end of line
                m_posInBuffer++;
            }

            if (!bLineEndFound) {
                bLineEndFound = FillBuffer();
                if (!nCharsRead) {
                    fEOF = bLineEndFound;
                }
            }
        } while (!bLineEndFound);
    } else if (m_encoding == UTF8) {
        ULONGLONG lineStartPos = GetPositionFastBuffered();
        bool bValid = true;
        bool bLineEndFound = false;
        fEOF = false;

        do {
            int nCharsRead;

            for (nCharsRead = 0; m_posInBuffer < m_nInBuffer; m_posInBuffer++, nCharsRead++) {
                if (Utf8::isSingleByte(m_buffer[m_posInBuffer])) { // 0xxxxxxx
                    m_wbuffer[nCharsRead] = m_buffer[m_posInBuffer] & 0x7f;
                } else if (Utf8::isFirstOfMultibyte(m_buffer[m_posInBuffer])) {
                    int nContinuationBytes = Utf8::continuationBytes(m_buffer[m_posInBuffer]);
                    bValid = (nContinuationBytes <= 2);

                    // We don't support characters wider than 16 bits
                    if (bValid) {
                        if (m_posInBuffer + nContinuationBytes >= m_nInBuffer) {
                            // If we are at the end of the file, the buffer won't be full
                            // and we won't be able to read any more continuation bytes.
                            bValid = (m_nInBuffer == TEXTFILE_BUFFER_SIZE);
                            break;
                        } else {
                            for (int j = 1; j <= nContinuationBytes; j++) {
                                if (!Utf8::isContinuation(m_buffer[m_posInBuffer + j])) {
                                    bValid = false;
                                }
                            }

                            switch (nContinuationBytes) {
                                case 0: // 0xxxxxxx
                                    m_wbuffer[nCharsRead] = m_buffer[m_posInBuffer] & 0x7f;
                                    break;
                                case 1: // 110xxxxx 10xxxxxx
                                    m_wbuffer[nCharsRead] = (m_buffer[m_posInBuffer] & 0x1f) << 6 | (m_buffer[m_posInBuffer + 1] & 0x3f);
                                    break;
                                case 2: // 1110xxxx 10xxxxxx 10xxxxxx
                                    m_wbuffer[nCharsRead] = (m_buffer[m_posInBuffer] & 0x0f) << 12 | (m_buffer[m_posInBuffer + 1] & 0x3f) << 6 | (m_buffer[m_posInBuffer + 2] & 0x3f);
                                    break;
                            }
                            m_posInBuffer += nContinuationBytes;
                        }
                    }
                } else {
                    bValid = false;
                    TRACE(_T("Invalid UTF8 character found\n"));
                }

                if (!bValid) {
                    m_wbuffer[nCharsRead] = L'?';
                    m_posInBuffer++;
                    nCharsRead++;
                    break;
                } else if (m_wbuffer[nCharsRead] == L'\n') {
                    bLineEndFound = true; // Stop at end of line
                    m_posInBuffer++;
                    break;
                } else if (m_wbuffer[nCharsRead] == L'\r') {
                    nCharsRead--; // Skip \r
                }
            }

            if (bValid || m_offset) {
                str.Append(m_wbuffer, nCharsRead);

                if (!bLineEndFound) {
                    bLineEndFound = FillBuffer();
                    if (!nCharsRead) {
                        fEOF = bLineEndFound;
                    }
                }
            } else {
                // Switch to text and read again
                m_encoding = m_fallbackencoding;
                // Stop using the buffer
                m_posInBuffer = m_nInBuffer = 0;

                fEOF = !ReopenAsText();

                if (!fEOF) {
                    // Seek back to the beginning of the line where we stopped
                    Seek(lineStartPos, begin);

                    fEOF = !ReadString(str);
                }
            }
        } while (bValid && !bLineEndFound);
    } else if (m_encoding == LE16) {
        bool bLineEndFound = false;
        fEOF = false;

        do {
            int nCharsRead;
            WCHAR* wbuffer = (WCHAR*)&m_buffer[m_posInBuffer];

            for (nCharsRead = 0; m_posInBuffer + 1 < m_nInBuffer; nCharsRead++, m_posInBuffer += sizeof(WCHAR)) {
                if (wbuffer[nCharsRead] == L'\n') {
                    break; // Stop at end of line
                } else if (wbuffer[nCharsRead] == L'\r') {
                    break; // Skip \r
                }
            }

            str.Append(wbuffer, nCharsRead);

            while (m_posInBuffer + 1 < m_nInBuffer && wbuffer[nCharsRead] == L'\r') {
                nCharsRead++;
                m_posInBuffer += sizeof(WCHAR);
            }
            if (m_posInBuffer + 1 < m_nInBuffer && wbuffer[nCharsRead] == L'\n') {
                bLineEndFound = true; // Stop at end of line
                nCharsRead++;
                m_posInBuffer += sizeof(WCHAR);
            }

            if (!bLineEndFound) {
                bLineEndFound = FillBuffer();
                if (!nCharsRead) {
                    fEOF = bLineEndFound;
                }
            }
        } while (!bLineEndFound);
    } else if (m_encoding == BE16) {
        bool bLineEndFound = false;
        fEOF = false;

        do {
            int nCharsRead;

            for (nCharsRead = 0; m_posInBuffer + 1 < m_nInBuffer; nCharsRead++, m_posInBuffer += sizeof(WCHAR)) {
                m_wbuffer[nCharsRead] = ((WCHAR(m_buffer[m_posInBuffer]) << 8) & 0xff00) | (WCHAR(m_buffer[m_posInBuffer + 1]) & 0x00ff);
                if (m_wbuffer[nCharsRead] == L'\n') {
                    bLineEndFound = true; // Stop at end of line
                    m_posInBuffer += sizeof(WCHAR);
                    break;
                } else if (m_wbuffer[nCharsRead] == L'\r') {
                    nCharsRead--; // Skip \r
                }
            }

            str.Append(m_wbuffer, nCharsRead);

            if (!bLineEndFound) {
                bLineEndFound = FillBuffer();
                if (!nCharsRead) {
                    fEOF = bLineEndFound;
                }
            }
        } while (!bLineEndFound);
    }

    return !fEOF;
}

//
// CWebTextFile
//

CWebTextFile::CWebTextFile(CTextFile::enc e, LONGLONG llMaxSize)
    : CTextFile(e)
    , m_llMaxSize(llMaxSize)
{
}

bool CWebTextFile::Open(LPCTSTR lpszFileName)
{
    CString fn(lpszFileName);

    if (fn.Find(_T("://")) == -1) {
        return __super::Open(lpszFileName);
    }

    try {
        CInternetSession is;

        CAutoPtr<CStdioFile> f(is.OpenURL(fn, 1, INTERNET_FLAG_TRANSFER_BINARY | INTERNET_FLAG_EXISTING_CONNECT));
        if (!f) {
            return false;
        }

        TCHAR path[MAX_PATH];
        GetTempPath(MAX_PATH, path);

        fn = path + fn.Mid(fn.ReverseFind('/') + 1);
        int i = fn.Find(_T("?"));
        if (i > 0) {
            fn = fn.Left(i);
        }
        CFile temp;
        if (!temp.Open(fn, modeCreate | modeWrite | typeBinary | shareDenyWrite)) {
            f->Close();
            return false;
        }

        BYTE buff[1024];
        int len, total = 0;
        while ((len = f->Read(buff, 1024)) == 1024 && (m_llMaxSize < 0 || (total += 1024) < m_llMaxSize)) {
            temp.Write(buff, len);
        }
        if (len > 0) {
            temp.Write(buff, len);
        }

        m_tempfn = fn;

        f->Close(); // must close it because the desctructor doesn't seem to do it and we will get an exception when "is" is destroying
    } catch (CInternetException* ie) {
        ie->Delete();
        return false;
    }

    return __super::Open(m_tempfn);
}

bool CWebTextFile::Save(LPCTSTR lpszFileName, enc e)
{
    // CWebTextFile is read-only...
    ASSERT(0);
    return false;
}

void CWebTextFile::Close()
{
    __super::Close();

    if (!m_tempfn.IsEmpty()) {
        _tremove(m_tempfn);
        m_tempfn.Empty();
    }
}

///////////////////////////////////////////////////////////////

CStringW AToW(CStringA str)
{
    CStringW ret;
    for (int i = 0, j = str.GetLength(); i < j; i++) {
        ret += (WCHAR)(BYTE)str[i];
    }
    return ret;
}

CStringA WToA(CStringW str)
{
    CStringA ret;
    for (int i = 0, j = str.GetLength(); i < j; i++) {
        ret += (CHAR)(WORD)str[i];
    }
    return ret;
}

CString AToT(CStringA str)
{
#ifdef UNICODE
    CString ret;
    for (int i = 0, j = str.GetLength(); i < j; i++) {
        ret += (TCHAR)(BYTE)str[i];
    }
    return ret;
#else
    return str;
#endif
}

CString WToT(CStringW str)
{
#ifdef UNICODE
    return str;
#else
    CString ret;
    for (int i = 0, j = str.GetLength(); i < j; i++) {
        ret += (TCHAR)(WORD)str[i];
    }
    return ret;
#endif
}

CStringA TToA(CString str)
{
#ifdef UNICODE
    CStringA ret;
    for (int i = 0, j = str.GetLength(); i < j; i++) {
        ret += (CHAR)(BYTE)str[i];
    }
    return ret;
#else
    return str;
#endif
}

CStringW TToW(CString str)
{
#ifdef UNICODE
    return str;
#else
    CStringW ret;
    for (size_t i = 0, j = str.GetLength(); i < j; i++) {
        ret += (WCHAR)(BYTE)str[i];
    }
    return ret;
#endif
}
