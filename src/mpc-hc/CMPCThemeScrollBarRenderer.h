#pragma once
#include "XeScrollBar/XeScrollBarBase.h"

class CMPCThemeScrollBarRenderer
{
public:
    CMPCThemeScrollBarRenderer();
    virtual ~CMPCThemeScrollBarRenderer();

    struct ClipRegionState {
        HRGN hOldClipRgn;
        bool bModifiedDC;
        bool bFullyClipped;
        
        ClipRegionState() : hOldClipRgn(NULL), bModifiedDC(false), bFullyClipped(false) {}
        
        bool IsFullyClipped() const { return bFullyClipped; }
    };

    void ProcessMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    void DrawThemedScrollBars(CDC* pDC, HWND hWnd, const CRect& vScrollRect, const CRect& hScrollRect, bool bHasVScroll, bool bHasHScroll, bool bDrawCorner);
    void HandleNcPaint(HWND hWnd);
    ClipRegionState ApplyScrollbarClipping(HDC hdc, HWND hWnd, const CRect& drawRect, bool bDrawThemedScrollbars = false);
    static void RestoreClipping(HDC hdc, ClipRegionState& clipState);
    bool GetScrollBarRects(HWND hWnd, CRect& vScrollRect, CRect& hScrollRect, bool& bHasVScroll, bool& bHasHScroll);
    bool GetScrollBarCornerRect(HWND hWnd, CRect& cornerRect);
    void DrawScrollBarCorner(CDC* pDC, HWND hWnd, const CRect& cornerRect);
    inline bool GetClippedScrollBarRects(HWND hWnd, HDC hdc, const CRect& drawRect, CRect& vScrollRect, CRect& hScrollRect, bool& bNeedsVScroll, bool& bNeedsHScroll, bool& bNeedsCorner);

protected:
    enum arrowOrientation {
        arrowLeft,
        arrowRight,
        arrowTop,
        arrowBottom
    };

    struct ScrollBarState {
        stXSB_AREA eMouseOverArea;
        stXSB_AREA eMouseDownArea;
        bool bDragging;
        bool bIgnoreNextLeave;

        ScrollBarState() {
            eMouseOverArea.eArea = eNone;
            eMouseDownArea.eArea = eNone;
            bDragging = false;
            bIgnoreNextLeave = false;
        }
    };

    ScrollBarState m_vScrollState;
    ScrollBarState m_hScrollState;
    bool m_bDrawingScrollbar;

    HHOOK m_hMouseHook;
    HWND m_hWndTracking;
    DWORD m_dwThreadId;

    static LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam);
    static CMPCThemeScrollBarRenderer* s_pActiveRenderer;

    void InstallMouseHook(HWND hWnd);
    void UninstallMouseHook();
    void HandleMouseEvent(WPARAM wParam, const POINT& pt);

    bool GetScrollBarState(HWND hWnd, int nBar, SCROLLBARINFO& sbi);

    void OnNcMouseMove(HWND hWnd, WPARAM wParam, LPARAM lParam);
    void OnNcLButtonDown(HWND hWnd, WPARAM wParam, LPARAM lParam);
    void OnNcLButtonUp(HWND hWnd, WPARAM wParam, LPARAM lParam);
    void OnNcMouseLeave(HWND hWnd);

    eXSB_AREA GetScrollBarArea(HWND hWnd, CPoint clientPoint, bool bVertical, const CRect& scrollRect);
    void CalculateScrollBarRects(HWND hWnd, int nBar, const CRect& scrollRect, CRect& rectTLArrow, CRect& rectBRArrow, CRect& rectThumb, CRect& rectTLChannel, CRect& rectBRChannel, bool* pbEnabled = nullptr);
    BOOL GetScrollBarRect(HWND hWnd, BOOL bVertical, CRect& rect);
    static void drawSBArrow(CDC& dc, COLORREF arrowClr, CRect arrowRect, arrowOrientation orientation, int dpi);
    void DrawScrollBar(CDC* pDC, HWND hWnd, int nBar, const CRect& targetRect);
};
