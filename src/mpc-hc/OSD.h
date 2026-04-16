/*
 * (C) 2006-2023 see Authors.txt
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

#include <d3d9.h>
#include <evr9.h>
#include <mvrInterfaces.h>
#include "DpiHelper.h"
//#include <HighDPI.h>
//#include "DSUtil/DSMPropertyBag.h"

enum OSD_COLORS {
    OSD_TRANSPARENT,
    OSD_BACKGROUND,
    OSD_BORDER,
    OSD_TEXT,
    OSD_BAR,
    OSD_CURSOR,
    OSD_DEBUGCLR,
    OSD_LAST
};

enum OSD_MESSAGEPOS {
    OSD_NOMESSAGE,
    OSD_TOPLEFT,
    OSD_TOPRIGHT,
    OSD_DEBUG,
};

enum OSD_TYPE {
    OSD_TYPE_NONE,
    OSD_TYPE_BITMAP,
    OSD_TYPE_MADVR,
    OSD_TYPE_GDI
};

class CMainFrame;

class COSD : public CWnd
{
    enum :int {
        IMG_EXIT    = 0,
        IMG_EXIT_A  = 1,
        IMG_CLOSE   = 23,
        IMG_CLOSE_A = 24,
    };

    CComPtr<IVMRMixerBitmap9>    m_pVMB;
    CComPtr<IMFVideoMixerBitmap> m_pMFVMB;
    CComPtr<IMadVRTextOsd>       m_pMVTO;

    CMainFrame*         m_pMainFrame;
    CWnd*               m_pWnd = nullptr;
    DpiHelper           m_DPIHelper;

    CCritSec            m_Lock;
    CDC                 m_MemDC;
    VMR9AlphaBitmap     m_VMR9AlphaBitmap = {};
    MFVideoAlphaBitmap  m_MFVAlphaBitmap = {};
    BITMAP              m_BitmapInfo;

    COLORREF m_colors[OSD_LAST];

    CString    m_OSD_Font;
    int        m_FontSize = 0;

    CFont     m_MainFont;
    CPen      m_penBorder;
    CBrush    m_brushCursor;
    CBrush    m_brushBack;
    CBrush    m_brushBar;
    CBrush    m_brushBar2;
    CBrush    m_brushChapter;
    CPen      m_debugPenBorder;
    CBrush    m_debugBrushBack;

    CRect    m_rectWnd;

    CRect    m_rectFlyBar;
    CRect    m_rectCloseButton;
    CRect    m_rectExitButton;

    CImageList *m_pButtonImages = nullptr;
    int        m_nButtonHeight = 0;
    int        m_externalFlyBarHeight = 0;

    bool    m_bCursorMoving   = false;
    bool    m_bShowSeekBar = false;
    bool    m_bSeekBarVisible = false;
    bool    m_bFlyBarVisible  = false;
    bool    m_bMouseOverCloseButton = false;
    bool    m_bMouseOverExitButton  = false;
    int     m_lastMovePosX = -1;

    bool    m_bShowMessage = true;

    CRect   m_MainWndRect;

    const CWnd*     m_pWndInsertAfter;

    OSD_TYPE        m_OSDType = OSD_TYPE_NONE;

    CString         m_strMessage;
    OSD_MESSAGEPOS     m_nMessagePos = OSD_NOMESSAGE;
    std::list<CString> m_debugMessages;

    CCritSec        m_CBLock;

    UINT m_nDEFFLAGS;

    CRect          m_MainWndRectCached;
    CString        m_strMessageCached;
    OSD_MESSAGEPOS m_nMessagePosCached = OSD_NOMESSAGE;
    CString        m_OSD_FontCached;
    int            m_FontSizeCached = 0;
    bool           m_bFontAACached = false;

    // Seekbar
    __int64 m_llSeekStop = 0;
    __int64 m_llSeekPos  = 0;
    CRect   m_rectSeekBar;
    CRect   m_rectSlider;
    CRect   m_rectPosText;
    CRect   m_rectCursor;
    int     m_SeekbarTextHeight = 0;
    CFont   m_SeekbarFont;
    CComPtr<IDSMChapterBag> m_pChapterBag;

    int SliderCursorHeight = 0;
    int SliderCursorWidth  = 0;
    int SliderChapHeight   = 0;
    int SliderChapWidth    = 0;

public:
    COSD(CMainFrame* pMainFrame);
    ~COSD();

    HRESULT Create(CWnd* pWnd);

    void Start(CWnd* pWnd, CComPtr<IVMRMixerBitmap9> pVMB, CComPtr<IMFVideoMixerBitmap> pMFVMB, bool bShowSeekBar);
    void Start(CWnd* pWnd, IMadVRTextOsd* pMVTO);
    void Start(CWnd* pWnd);
    void Stop();

    void DisplayMessage(
        OSD_MESSAGEPOS nPos,
        LPCWSTR strMsg,
        int nDuration = 5000,
        const int FontSize = 0,
        LPCWSTR OSD_Font = nullptr,
        const bool bPeriodicallyDisplayed = false);
    void DebugMessage(LPCWSTR format, ...);
    void ClearMessage(bool hide = false, bool repaint = true);

    bool CanShowMessage();

    void DisplayTime(CStringW strTime);

    void ClearTime();

    void HideMessage(bool hide);
    void EnableShowMessage(bool bValue = true) { m_bShowMessage = bValue; };

    __int64 GetPos() const;
    __int64 GetRange() const;
    void SetPos(__int64 pos);
    void SetRange(__int64 stop);
    void SetPosAndRange(__int64 pos, __int64 stop);

    void OnSize(UINT nType, int cx, int cy);
    bool OnMouseMove(UINT nFlags, CPoint point);
    void OnMouseLeave();
    bool OnLButtonDown(UINT nFlags, CPoint point);
    bool OnLButtonUp(UINT nFlags, CPoint point);

    void SetWndRect(const CRect& rc) { m_MainWndRect = rc; };

    OSD_TYPE GetOSDType() { return m_OSDType; };

    void SetChapterBag(CComPtr<IDSMChapterBag>& pCB);
    void RemoveChapters();

    void OverrideDPI(int dpix, int dpiy);
    bool UpdateButtonImages();
    void EnableShowSeekBar(bool enabled);

    void SetCursorArrow();
    void SetCursorHand();
    void SetCursorName(LPCWSTR lpCursorName);

    DECLARE_DYNAMIC(COSD)

private:
    void UpdateBitmap();
    void CalcSeekbar();
    void CalcFlybar();
    void UpdateSeekBarPos(CPoint point);
    void DrawSeekbar();
    void DrawFlybar();
    void DrawRect(CRect& rect, CBrush* pBrush = nullptr, CPen* pPen = nullptr);
    void InvalidateBitmapOSD();
    void DrawMessage();
    void DrawDebug();

    void Reset();

    void DrawWnd();

    void GradientFill(CDC* pDc, CRect* rc);
    void SimpleFill(CDC* pDc, CRect* rc);

    void CreateFontInternal();

    bool m_bPeriodicallyDisplayed = false;
    std::atomic_bool m_bTimerStarted = false;
    void StartTimer(const UINT nTimerDurarion);
    void EndTimer();

protected:
    BOOL PreCreateWindow(CREATESTRUCT& cs);
    BOOL PreTranslateMessage(MSG* pMsg);
    void OnPaint();
    bool NeedsHiding();
    BOOL OnEraseBkgnd(CDC* pDC);

    DECLARE_MESSAGE_MAP()

public:
    afx_msg void OnHide();
    afx_msg void OnDrawWnd();
};
