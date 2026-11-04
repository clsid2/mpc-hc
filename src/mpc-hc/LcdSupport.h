/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2012, 2016 see Authors.txt
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

#include <atomic>
#include "lglcd/lglcd.h"
#include "LCDUI/LCDUI.h"

class CLCDMyProgressBar : public CLCDProgressBar
{
public:
    enum eMY_PROGRESS_STYLE {
        STYLE_FILLED_H,
        STYLE_CURSOR,
        STYLE_DASHED_CURSOR,
        STYLE_FILLED_V
    };

    virtual void OnDraw(CLCDGfxBase& rGfx);
    virtual void SetProgressStyle(ePROGRESS_STYLE eStyle);
    virtual void SetProgressStyle(eMY_PROGRESS_STYLE eMyStyle);

protected:
    eMY_PROGRESS_STYLE m_eMyStyle;
};

class CLCDMyMonoPage : public CLCDPage
{
public:
    enum PlayState {
        PS_PLAY   = 0,
        PS_PAUSE  = 1,
        PS_STOP   = 2,
        PS_UNUSED = 3
    };

    virtual HRESULT Initialize();
    virtual void OnLCDButtonUp(int nButton);
    void SetPlayState(PlayState ps);

    CLCDMyMonoPage();
    ~CLCDMyMonoPage();

    CLCDScrollingText  m_Text1;
    CLCDText           m_Text[2];
    CLCDMyProgressBar  m_ProgBar[2];
    CLCDAnimatedBitmap m_PlayState;
private:
    HBITMAP            hBmp[PS_UNUSED];
};

class CLCDMyColorPage : public CLCDPage
{
public:
    enum PlayState {
        PS_PLAY   = 0,
        PS_PAUSE  = 1,
        PS_STOP   = 2,
        PS_UNUSED = 3
    };

    virtual HRESULT Initialize();
    virtual void OnLCDButtonUp(int nButton);
    void SetPlayState(PlayState ps);

    CLCDMyColorPage();
    ~CLCDMyColorPage();

    CLCDScrollingText  m_Text1;
    CLCDText           m_Text[2];
    CLCDMyProgressBar  m_ProgBar[2];
    CLCDAnimatedBitmap m_PlayState;
private:
    HBITMAP            hBmp[PS_UNUSED];
};

class CMPC_Lcd
{
public:
    enum PlayState {
        PS_PLAY   = 0,
        PS_PAUSE  = 1,
        PS_STOP   = 2,
        PS_UNUSED = 3
    };

private:
    lgLcdConnectContextEx m_ConnCtx;
    HANDLE                hLCD_UpdateThread;

    __int64               m_nMediaStart;
    __int64               m_nMediaStop;
    __int64               m_nVolumeStart;
    __int64               m_nVolumeStop;

    HRESULT SetAsForeground(BOOL setAsForeground);

public:
    CLCDConnection        m_Connection;
    CLCDOutput*           m_MonoOutput;
    CLCDOutput*           m_ColorOutput;
    CLCDMyMonoPage        m_MonoPage;
    CLCDMyColorPage       m_ColorPage;
    std::atomic<bool>     Thread_Loop;
    __time64_t            nThread_tTimeout;
    CRITICAL_SECTION      cs;

    CMPC_Lcd();
    ~CMPC_Lcd();

    void SetMediaTitle(const TCHAR* text);
    void SetMediaRange(__int64 nStart, __int64 nStop);
    void SetMediaPos(__int64 nPos);
    void SetVolumeRange(__int64 nStart, __int64 nStop);
    void SetVolume(__int64 nVol);
    void SetStatusMessage(const TCHAR* text, int nTimeOut);
    void SetPlayState(PlayState ps);
};
