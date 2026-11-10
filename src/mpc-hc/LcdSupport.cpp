/*
 * (C) 2006-2013, 2015 see Authors.txt
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
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <process.h>    // _beginthreadex
#include <time.h>
#include <locale.h>

#include "LcdSupport.h"
#include "LCDUI/LCDUI.h"

#include "mplayerc.h"

#define LCD_APP_NAME  "MPC-HC"
#define LCD_UPD_TIMER 40


static unsigned __stdcall LCD_UpdateThread(void* Control)
{
    CMPC_Lcd* ctrl = static_cast<CMPC_Lcd*>(Control);
    wchar_t str[40];
    __time64_t ltime;
    __time64_t otime = 0;
    struct tm thetime;
    _locale_t locale = _create_locale(LC_TIME, "");

    while (ctrl->Thread_Loop) {
        EnterCriticalSection(&ctrl->cs);
        if (_time64(&ltime) != otime) { // Retrieve the time
            otime = ltime;
            _localtime64_s(&thetime, &ltime);

            // Format the current time structure into a string
            // using %#x is the long date representation,
            // appropriate to the current locale
            if (_wcsftime_l(str, _countof(str), _T("%F"), (const struct tm*)&thetime, locale) &&
                    (ltime > ctrl->nThread_tTimeout || ltime < otime)) {    // message displayed, no update until timeout
                ctrl->m_MonoPage.m_Text[0].SetText(str);
                ctrl->m_ColorPage.m_Text[0].SetText(str);
            }

            if (_wcsftime_l(str, _countof(str), _T("%X"), (const struct tm*)&thetime, locale)) {
                ctrl->m_MonoPage.m_Text[1].SetText(str);
                ctrl->m_ColorPage.m_Text[1].SetText(str);
            }
        }
        ctrl->m_Connection.Update();
        LeaveCriticalSection(&ctrl->cs);
        Sleep(LCD_UPD_TIMER);
    }

    _free_locale(locale);
    return 0;
}

/******************************************************************************************************
 ****************************************** CLCDMyProgressBar ****************************************/

void CLCDMyProgressBar::OnDraw(CLCDGfxBase& rGfx)
{
    // draw the border
    RECT r = { 0, 0, GetWidth(), GetHeight() };

    FrameRect(rGfx.GetHDC(), &r, m_hBrush);

    // draw the progress
    switch (m_eMyStyle) {
        case STYLE_CURSOR: {
            int nCursorPos = (int)Scalef((float)m_Range.nMin, (float)m_Range.nMax,
                                         (float)1, (float)(GetWidth() - m_nCursorWidth - 1),
                                         m_fPos);
            r.left = nCursorPos;
            r.right = r.left + m_nCursorWidth;
            FillRect(rGfx.GetHDC(), &r, m_hBrush);
        }
        break;
        case STYLE_FILLED_H:
        case STYLE_FILLED_V: {
            int nBar = (int)Scalef((float)m_Range.nMin, (float)m_Range.nMax,
                                   0.0f, (m_eMyStyle == STYLE_FILLED_H ? (float)GetWidth() : (float)GetHeight()) - 4,
                                   m_fPos);
            r.left   = r.left + 2;
            r.bottom = r.bottom - 2;
            if (m_eMyStyle == STYLE_FILLED_H) {
                r.right = nBar + 2;
                r.top   = r.top + 2;
            } else {
                r.right = r.right - 2;
                r.top   = r.bottom - nBar;
            }

            FillRect(rGfx.GetHDC(), &r, m_hBrush);
        }
        break;
        case STYLE_DASHED_CURSOR: {
            int nCursorPos = (int)Scalef((float)m_Range.nMin, (float)m_Range.nMax,
                                         (float)1, (float)(GetWidth() - m_nCursorWidth - 1),
                                         m_fPos);
            r.left = nCursorPos;
            r.right = r.left + m_nCursorWidth;
            FillRect(rGfx.GetHDC(), &r, m_hBrush);
            HPEN hOldPen = (HPEN)::SelectObject(rGfx.GetHDC(), m_hPen);

            ::MoveToEx(rGfx.GetHDC(), 0, (r.bottom - r.top) / 2, nullptr);
            ::LineTo(rGfx.GetHDC(), nCursorPos, (r.bottom - r.top) / 2);
            ::SelectObject(rGfx.GetHDC(), hOldPen);
        }
        break;
        default:
            break;
    }
}

void CLCDMyProgressBar::SetProgressStyle(ePROGRESS_STYLE eStyle)
{
    m_eStyle = eStyle;

    //Convert and update the new Style type
    switch (eStyle) {
        case CLCDProgressBar::STYLE_CURSOR:
            m_eMyStyle = STYLE_CURSOR;
            break;
        case CLCDProgressBar::STYLE_FILLED:
            m_eMyStyle = STYLE_FILLED_H;
            break;
        case CLCDProgressBar::STYLE_DASHED_CURSOR:
            m_eMyStyle = STYLE_DASHED_CURSOR;
            break;
        default:
            break;
    }
}

void CLCDMyProgressBar::SetProgressStyle(eMY_PROGRESS_STYLE eMyStyle)
{
    m_eMyStyle = eMyStyle;

    //Convert and update the old Style type
    switch (eMyStyle) {
        case STYLE_CURSOR:
            m_eStyle = CLCDProgressBar::STYLE_CURSOR;
            break;
        case STYLE_FILLED_V:
        case STYLE_FILLED_H:
            m_eStyle = CLCDProgressBar::STYLE_FILLED;
            break;
        case STYLE_DASHED_CURSOR:
            m_eStyle = CLCDProgressBar::STYLE_DASHED_CURSOR;
            break;
        default:
            break;
    }
}

/******************************************************************************************************
 ****************************************** CLCDMyMonoPage ****************************************/
CLCDMyMonoPage::CLCDMyMonoPage()
{
    Initialize();
}

CLCDMyMonoPage::~CLCDMyMonoPage()
{
    DeleteObject(hBmp[PS_PLAY]);
    DeleteObject(hBmp[PS_PAUSE]);
    DeleteObject(hBmp[PS_STOP]);
}

HRESULT CLCDMyMonoPage::Initialize()
{
    LOGFONT lf;
    HFONT hFont;
    unsigned int x, y;

    // max dimensions: 160 x 43
    x = 10;
    y = 0;

    BYTE bPause[] = {0x93, 0xFF,
                     0x93, 0xFF,
                     0x93, 0xFF,
                     0x93, 0xFF,
                     0x93, 0xFF,
                     0x93, 0xFF,
                     0x93, 0xFF
                    };

    BYTE bStop[]  = {0x00, 0x00,
                     0x00, 0x00,
                     0x00, 0x00,
                     0x00, 0x00,
                     0x00, 0x00,
                     0x00, 0x00,
                     0x00, 0x00
                    };

    BYTE bPlay[]  = {0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
                     0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
                     0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
                     0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
                     0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
                     0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
                     0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F
                    };

    hBmp[PS_PLAY]  = CreateBitmap(56, 7, 1, 1, bPlay);
    hBmp[PS_PAUSE] = CreateBitmap(14, 7, 1, 1, bPause);
    hBmp[PS_STOP]  = CreateBitmap(8, 7, 1, 1, bStop);

    // Initialize the text control (media)
    m_Text1.Initialize();
    m_Text1.SetOrigin(x, y);
    m_Text1.SetSize(160 - x, 13);
    m_Text1.SetAlignment(DT_CENTER);
    m_Text1.SetWordWrap(false);
    m_Text1.SetText(_T(""));
    m_Text1.SetStartDelay(5000);
    m_Text1.SetEndDelay(2000);
    m_Text1.EnableRepeat(true);
    m_Text1.SetScrollDirection(SCROLL_HORZ);
    m_Text1.SetSpeed(24);

    // Initialize the progressbar control (media progress)
    y += 15;
    m_ProgBar[1].Initialize();
    m_ProgBar[1].SetOrigin(x + 10, y);
    m_ProgBar[1].SetSize(160 - x - 10, 7);
    m_ProgBar[1].SetPos(0);
    m_ProgBar[1].SetRange(0, 100);
    m_ProgBar[1].SetProgressStyle(CLCDMyProgressBar::STYLE_FILLED_H);

    // gfx
    m_PlayState.Initialize();
    m_PlayState.SetOrigin(x, y);
    m_PlayState.SetSize(7, 7);
    m_PlayState.SetBitmap(hBmp[PS_PLAY]);
    m_PlayState.ResetUpdate();
    m_PlayState.SetSubpicWidth(7);
    m_PlayState.SetAnimationRate(300);
    m_PlayState.SetAlpha(false);
    m_PlayState.SetZoomLevel(1);

    // Initialize the text control (time / mpc messages)
    y += 6;
    m_Text[0].Initialize();
    m_Text[0].SetOrigin(x, y);
    m_Text[0].SetSize(160 - x, /*13*/25);
    m_Text[0].SetAlignment(DT_CENTER);
    m_Text[0].SetWordWrap(false);
    m_Text[0].SetText(_T(""));
    m_Text[0].SetFontPointSize(7);
    hFont = m_Text[0].GetFont();
    GetObject(hFont, sizeof(LOGFONT), &lf);
    wcscpy_s(lf.lfFaceName, LF_FACESIZE, _T("Microsoft Sans Serif"));
    m_Text[0].SetFont(lf);

    y += 10;
    m_Text[1].Initialize();
    m_Text[1].SetOrigin(x, y);
    m_Text[1].SetSize(160 - x, /*13*/25);
    m_Text[1].SetAlignment(DT_CENTER);
    m_Text[1].SetWordWrap(false);
    m_Text[1].SetText(_T(""));
    m_Text[1].SetFontPointSize(7);
    hFont = m_Text[1].GetFont();
    GetObject(hFont, sizeof(LOGFONT), &lf);
    wcscpy_s(lf.lfFaceName, LF_FACESIZE, _T("Microsoft Sans Serif"));
    m_Text[1].SetFont(lf);

    // Initialize the progressbar control (volume)
    m_ProgBar[0].Initialize();
    m_ProgBar[0].SetOrigin(0, 0);
    m_ProgBar[0].SetSize(7, 43);
    m_ProgBar[0].SetPos(0);
    m_ProgBar[0].SetRange(0, 100);
    m_ProgBar[0].SetProgressStyle(CLCDMyProgressBar::STYLE_FILLED_V);

    AddObject(&m_Text1);
    AddObject(&m_Text[0]);
    AddObject(&m_Text[1]);
    AddObject(&m_ProgBar[0]);
    AddObject(&m_ProgBar[1]);
    AddObject(&m_PlayState);

    return CLCDPage::Initialize();
}

void CLCDMyMonoPage::OnLCDButtonUp(int nButton)
{
    switch (nButton) {
        case LGLCDBUTTON_BUTTON0: {
            /*LOGFONT lf;
            HFONT hFont = m_Text1.GetFont();

            GetObject(hFont, sizeof(LOGFONT), &lf);

            CFontDialog cfd(&lf);
            if (cfd.DoModal() == IDOK)
            {
                cfd.GetCurrentFont(&lf);
                m_Text1.SetFont(lf);
            }*/
            break;
        }
        case LGLCDBUTTON_BUTTON1:
            break;
        case LGLCDBUTTON_BUTTON2:
            break;
        case LGLCDBUTTON_BUTTON3:
            break;
        default:
            break;
    }
}

/* update play state bitmap */
void CLCDMyMonoPage::SetPlayState(PlayState ps)
{
    switch (ps) {
        case PS_PLAY:
            m_PlayState.SetBitmap(hBmp[PS_PLAY]);
            m_PlayState.ResetUpdate();
            m_PlayState.SetSubpicWidth(7);
            m_PlayState.SetAnimationRate(300);
            break;

        case PS_PAUSE:
            m_PlayState.SetBitmap(hBmp[PS_PAUSE]);
            m_PlayState.ResetUpdate();
            m_PlayState.SetSubpicWidth(7);
            m_PlayState.SetAnimationRate(800);
            break;

        case PS_STOP:
            m_ProgBar[1].SetPos(0);
            m_PlayState.SetBitmap(hBmp[PS_STOP]);
            m_PlayState.ResetUpdate();
            m_PlayState.SetSubpicWidth(7);
            m_PlayState.SetAnimationRate(5000); // dummy, only one picture
            break;

        default:
            break;
    }
}

/******************************************************************************************************
 ****************************************** CLCDMyColorPage ****************************************/
CLCDMyColorPage::CLCDMyColorPage()
{
    Initialize();
}

CLCDMyColorPage::~CLCDMyColorPage()
{
    DeleteObject(hBmp[PS_PLAY]);
    DeleteObject(hBmp[PS_PAUSE]);
    DeleteObject(hBmp[PS_STOP]);
}

HRESULT CLCDMyColorPage::Initialize()
{
    LOGFONT lf;
    HFONT hFont;
    unsigned int x, y;

    // max dimensions: 320 x 240
    x = 20;
    y = 0;

    BYTE bPause[] = {0x93, 0xFF,
                     0x93, 0xFF,
                     0x93, 0xFF,
                     0x93, 0xFF,
                     0x93, 0xFF,
                     0x93, 0xFF,
                     0x93, 0xFF
                    };

    BYTE bStop[]  = {0x00, 0x00,
                     0x00, 0x00,
                     0x00, 0x00,
                     0x00, 0x00,
                     0x00, 0x00,
                     0x00, 0x00,
                     0x00, 0x00
                    };

    BYTE bPlay[]  = {0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
                     0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
                     0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
                     0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
                     0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
                     0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
                     0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F
                    };

    hBmp[PS_PLAY]  = CreateBitmap(56, 7, 1, 1, bPlay);
    hBmp[PS_PAUSE] = CreateBitmap(14, 7, 1, 1, bPause);
    hBmp[PS_STOP]  = CreateBitmap(8, 7, 1, 1, bStop);


    // Initialize the text control (media)
    m_Text1.Initialize();
    m_Text1.SetOrigin(x, y);
    m_Text1.SetSize(320 - x, 26);
    m_Text1.SetAlignment(DT_CENTER | DT_VCENTER);
    m_Text1.SetWordWrap(false);
    m_Text1.SetText(_T(""));
    m_Text1.SetFontPointSize(16);
    m_Text1.SetStartDelay(5000);
    m_Text1.SetEndDelay(2000);
    m_Text1.EnableRepeat(true);
    m_Text1.SetScrollDirection(SCROLL_HORZ);
    m_Text1.SetSpeed(24);

    // Initialize the progressbar control (media progress)
    y += 30;
    m_ProgBar[1].Initialize();
    m_ProgBar[1].SetOrigin(x + 20, y);
    m_ProgBar[1].SetSize(320 - x - 20, 14);
    m_ProgBar[1].SetPos(0);
    m_ProgBar[1].SetRange(0, 100);
    m_ProgBar[1].SetProgressStyle(CLCDMyProgressBar::STYLE_FILLED_H);

    // gfx
    m_PlayState.Initialize();
    m_PlayState.SetOrigin(x, y);
    m_PlayState.SetSize(14, 14);
    m_PlayState.SetBitmap(hBmp[PS_PLAY]);
    m_PlayState.ResetUpdate();
    m_PlayState.SetZoomLevel(2);
    m_PlayState.SetSubpicWidth(7);
    m_PlayState.SetAnimationRate(300);
    m_PlayState.SetAlpha(false);
    m_PlayState.GetSize();

    // Initialize the text control (time / mpc messages)
    y += 14;
    m_Text[0].Initialize();
    m_Text[0].SetOrigin(x, y);
    m_Text[0].SetSize(320 - x, /*13*/50);
    m_Text[0].SetAlignment(DT_CENTER);
    m_Text[0].SetWordWrap(false);
    m_Text[0].SetText(_T(""));
    m_Text[0].SetFontPointSize(14);
    hFont = m_Text[0].GetFont();
    GetObject(hFont, sizeof(LOGFONT), &lf);
    wcscpy_s(lf.lfFaceName, LF_FACESIZE, _T("Microsoft Sans Serif"));
    m_Text[0].SetFont(lf);

    y += 22;
    m_Text[1].Initialize();
    m_Text[1].SetOrigin(x, y);
    m_Text[1].SetSize(320 - x, /*13*/50);
    m_Text[1].SetAlignment(DT_CENTER);
    m_Text[1].SetWordWrap(false);
    m_Text[1].SetText(_T(""));
    m_Text[1].SetFontPointSize(14);
    hFont = m_Text[1].GetFont();
    GetObject(hFont, sizeof(LOGFONT), &lf);
    wcscpy_s(lf.lfFaceName, LF_FACESIZE, _T("Microsoft Sans Serif"));
    m_Text[1].SetFont(lf);

    // Initialize the progressbar control (volume)
    m_ProgBar[0].Initialize();
    m_ProgBar[0].SetOrigin(0, 0);
    m_ProgBar[0].SetSize(14, 86);
    m_ProgBar[0].SetPos(0);
    m_ProgBar[0].SetRange(0, 100);
    m_ProgBar[0].SetProgressStyle(CLCDMyProgressBar::STYLE_FILLED_V);

    AddObject(&m_Text1);
    AddObject(&m_Text[0]);
    AddObject(&m_Text[1]);
    AddObject(&m_ProgBar[0]);
    AddObject(&m_ProgBar[1]);
    AddObject(&m_PlayState);

    return CLCDPage::Initialize();
}

void CLCDMyColorPage::OnLCDButtonUp(int nButton)
{
    switch (nButton) {
        case LGLCDBUTTON_BUTTON0: {
            /*LOGFONT lf;
            HFONT hFont = m_Text1.GetFont();

            GetObject(hFont, sizeof(LOGFONT), &lf);

            CFontDialog cfd(&lf);
            if (cfd.DoModal() == IDOK)
            {
                cfd.GetCurrentFont(&lf);
                m_Text1.SetFont(lf);
            }*/
            break;
        }
        case LGLCDBUTTON_BUTTON1:
            break;
        case LGLCDBUTTON_BUTTON2:
            break;
        case LGLCDBUTTON_BUTTON3:
            break;
        default:
            break;
    }
}

/* update play state bitmap */
void CLCDMyColorPage::SetPlayState(PlayState ps)
{
    switch (ps) {
        case PS_PLAY:
            m_PlayState.SetBitmap(hBmp[PS_PLAY]);
            m_PlayState.ResetUpdate();
            m_PlayState.SetSubpicWidth(7);
            m_PlayState.SetAnimationRate(300);
            break;

        case PS_PAUSE:
            m_PlayState.SetBitmap(hBmp[PS_PAUSE]);
            m_PlayState.ResetUpdate();
            m_PlayState.SetSubpicWidth(7);
            m_PlayState.SetAnimationRate(800);
            break;

        case PS_STOP:
            m_ProgBar[1].SetPos(0);
            m_PlayState.SetBitmap(hBmp[PS_STOP]);
            m_PlayState.ResetUpdate();
            m_PlayState.SetSubpicWidth(7);
            m_PlayState.SetAnimationRate(5000); // dummy, only one picture
            break;

        default:
            break;
    }
}

/******************************************************************************************************
 ********************************************** CMPC_Lcd *********************************************/

/* attach to an available lcd */
CMPC_Lcd::CMPC_Lcd()
    : hLCD_UpdateThread(nullptr)
    , m_nMediaStart(0)
    , m_nMediaStop(0)
    , m_nVolumeStart(0)
    , m_nVolumeStop(100)
    , m_MonoOutput(nullptr)
    , m_ColorOutput(nullptr)
    , Thread_Loop(false)
    , nThread_tTimeout(0)
{
    InitializeCriticalSection(&cs);

    // lcd init
    ZeroMemory(&m_ConnCtx, sizeof(m_ConnCtx));

    m_ConnCtx.appFriendlyName = _T(LCD_APP_NAME);
    m_ConnCtx.dwAppletCapabilitiesSupported = LGLCD_APPLET_CAP_BW | LGLCD_APPLET_CAP_QVGA;
    m_ConnCtx.isAutostartable = FALSE;
    m_ConnCtx.isPersistent = FALSE;
    m_ConnCtx.onConfigure.configCallback = nullptr;     // we don't have a configuration screen
    m_ConnCtx.onConfigure.configContext = nullptr;
    m_ConnCtx.onNotify.notificationCallback = nullptr;
    m_ConnCtx.onNotify.notifyContext = nullptr;
    m_ConnCtx.connection = LGLCD_INVALID_CONNECTION; // the "connection" member will be returned upon return

    const CAppSettings& s = AfxGetAppSettings();
    if (!s.fLCDSupport) {
        return;
    }

    if (FALSE == m_Connection.Initialize(m_ConnCtx)) {
        //_tperror(_T("Initialize"));
        return;
    }

    m_MonoOutput = m_Connection.MonoOutput();
    m_MonoOutput->ShowPage(&m_MonoPage);

    m_ColorOutput = m_Connection.ColorOutput();
    m_ColorOutput->ShowPage(&m_ColorPage);

    SetAsForeground(TRUE);

    m_Connection.Update();

    if (m_Connection.IsConnected()) {
        Thread_Loop = true;
        SetPlayState(PS_STOP);
        hLCD_UpdateThread = (HANDLE) _beginthreadex(nullptr, 512 /* stack */, LCD_UpdateThread, (void*) this /* arg */, 0, nullptr);
    }
}

/* detach from lcd */
CMPC_Lcd::~CMPC_Lcd()
{
    if (hLCD_UpdateThread) {
        Thread_Loop = false;
        WaitForSingleObject(hLCD_UpdateThread, INFINITE);
        CloseHandle(hLCD_UpdateThread);
        hLCD_UpdateThread = nullptr;
    }

    DeleteCriticalSection(&cs);

    m_Connection.Shutdown();
}

/* update title name */
void CMPC_Lcd::SetMediaTitle(const TCHAR* text)
{
    EnterCriticalSection(&cs);
    m_MonoPage.m_Text1.SetText(text);
    m_MonoPage.m_ProgBar[1].SetPos(0);
    m_ColorPage.m_Text1.SetText(text);
    m_ColorPage.m_ProgBar[1].SetPos(0);
    LeaveCriticalSection(&cs);
}

/* set volume min/max */
void CMPC_Lcd::SetVolumeRange(__int64 nStart, __int64 nStop)
{
    //handle 64bit integers LCDUI only supports int for Range
    //Since it also supports floats for position,
    //This will always work in percentage
    m_nVolumeStart = nStart;
    m_nVolumeStop = nStop;
}

/* update volume */
void CMPC_Lcd::SetVolume(__int64 nVol)
{
    //handle 64bit integers LCDUI only supports int for Range
    //Since it also supports floats for position,
    //This will always work in percentage
    float fVol;
    if (m_nVolumeStart != m_nVolumeStop) {
        fVol = float(nVol - m_nVolumeStart) * 100.0f / float(m_nVolumeStop - m_nVolumeStart);
    } else {
        fVol = 0.0f;
        ASSERT(FALSE); // This isn't supposed to happen
    }

    EnterCriticalSection(&cs);
    m_MonoPage.m_ProgBar[0].SetPos(fVol);
    m_ColorPage.m_ProgBar[0].SetPos(fVol);
    LeaveCriticalSection(&cs);
}

/* set media min/max */
void CMPC_Lcd::SetMediaRange(__int64 nStart, __int64 nStop)
{
    //handle 64bit integers LCDUI only supports int for Range
    //Since it also supports floats for position,
    //This will always work in percentage
    m_nMediaStart = nStart;
    m_nMediaStop = nStop;
}

/* update media position */
void CMPC_Lcd::SetMediaPos(__int64 nPos)
{
    //handle 64bit integers LCDUI only supports int for Range
    //Since it also supports floats for position,
    //This will always work in percentage
    float fPos;
    if (m_nMediaStart != m_nMediaStop) {
        fPos = float(nPos - m_nMediaStart) * 100.0f / float(m_nMediaStop - m_nMediaStart);
    } else { // The duration might be unknown
        fPos = 0.0f;
    }

    EnterCriticalSection(&cs);
    m_MonoPage.m_ProgBar[1].SetPos(fPos);
    m_ColorPage.m_ProgBar[1].SetPos(fPos);
    LeaveCriticalSection(&cs);
}

/* update status message (displayed for nTimeOut milliseconds) */
void CMPC_Lcd::SetStatusMessage(const TCHAR* text, int nTimeOut)
{
    if (!m_Connection.IsConnected()) {
        return;
    }

    __time64_t ltime;
    _time64(&ltime);
    if ((nTimeOut /= 1000) < 1) {
        nTimeOut = 1;
    }

    EnterCriticalSection(&cs);
    nThread_tTimeout = ltime + nTimeOut;
    m_MonoPage.m_Text[0].SetText(text);
    m_ColorPage.m_Text[0].SetText(text);
    LeaveCriticalSection(&cs);
}

/* update play state bitmap */
void CMPC_Lcd::SetPlayState(CMPC_Lcd::PlayState ps)
{
    if (!m_Connection.IsConnected()) {
        return;
    }

    EnterCriticalSection(&cs);
    switch (ps) {
        case PS_PLAY:
            SetAsForeground(true);
            break;
        case PS_PAUSE:
            break;
        case PS_STOP:
            SetAsForeground(false);
            break;
        default:
            break;
    }

    m_MonoPage.SetPlayState((CLCDMyMonoPage::PlayState)ps);
    m_ColorPage.SetPlayState((CLCDMyColorPage::PlayState)ps);

    LeaveCriticalSection(&cs);
}

HRESULT CMPC_Lcd::SetAsForeground(BOOL setAsForeground)
{
    if (nullptr != m_Connection.MonoOutput()) {
        m_Connection.MonoOutput()->SetAsForeground(setAsForeground);
    }

    if (nullptr != m_Connection.ColorOutput()) {
        m_Connection.ColorOutput()->SetAsForeground(setAsForeground);
    }

    return S_OK;
}
