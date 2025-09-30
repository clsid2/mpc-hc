#pragma once
#include <vector>
#include <map>
#include <afxcmn.h>
#include <afxdlgs.h>
#include "CMPCTheme.h"
#include "CMPCThemeToolTipCtrl.h"

#define TOOLBAR_HIDE_ICON 0xF900

class CMPCThemeTitleBarControlButton;
class CPngImage;
int CALLBACK PropSheetCallBackRTL(HWND hWnd, UINT message, LPARAM lParam);

class CMPCThemeUtil
{
public:
    enum SpecialThemeCases {
        NoSpecialCase = 0,
        ExternalPropertyPageWithDefaultButton,
        ExternalPropertyPageWithAnalogCaptureSliders,
    };

    enum WidgetPairType {
        WidgetPairAuto = 0
        , WidgetPairCheckBox
        , WidgetPairCombo
        , WidgetPairText
        , WidgetPairEdit
    };

    CMPCThemeUtil();
    virtual ~CMPCThemeUtil();

    static bool ModifyTemplates(CPropertySheet* sheet, CRuntimeClass* pageClass, DWORD id, DWORD addStyle, DWORD removeStyle = 0);


    static HBRUSH getCtlColorFileDialog(HDC hDC, UINT nCtlColor);
    static HBRUSH getCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    static bool MPCThemeEraseBkgnd(CDC* pDC, CWnd* pWnd, UINT nCtlColor);

    enum FileDialogWidgetSearch {
        RecurseSinkWidgets
        ,ThemeAllChildren
        ,ProminentControlIDWidget
    };

    HWND themableDialogHandle = nullptr;
    void enableFileDialogHook();
    void subClassFileDialogRecurse(CWnd* wnd, HWND hWnd, FileDialogWidgetSearch searchType);
    void subClassFileDialog(CWnd* wnd);
    void subClassFileDialogWidgets(HWND widget, HWND parent, wchar_t* childWindowClass);
    void redrawAllThemedWidgets();
protected:
    int dialogProminentControlStringID = 0;

    static CBrush contentBrush, windowBrush, controlAreaBrush, W10DarkThemeFileDialogInjectedBGBrush;
    static NONCLIENTMETRICS nonClientMetrics;
    std::vector<CWnd*> allocatedWindows;

    void fulfillThemeReqs(CWnd* wnd, SpecialThemeCases specialCase = SpecialThemeCases::NoSpecialCase);
    static void initHelperObjects();
    void makeThemed(CWnd* pObject, CWnd* tChild);

    //replaces tooltip from EnableTooltips()
    CMPCThemeToolTipCtrl themedDialogToolTip;
    CDialog* themedDialogToolTipParent;
    void EnableThemedDialogTooltips(CDialog* wnd);
    void PlaceThemedDialogTooltip(UINT_PTR nID);
    void RelayThemedDialogTooltip(MSG* pMsg);
    void RedrawDialogTooltipIfVisible();
    static bool metricsNeedCalculation;
public:
    static bool getFontByFace(CFont& font, CWnd *wnd, wchar_t* fontName, int size, LONG weight = FW_REGULAR);
    static bool getFixedFont(CFont& font, CDC* pDC, CWnd* wnd);
    static bool getFontByType(CFont& font, CWnd* wnd, int type, bool underline = false, bool bold = false);
    enum fontType {
        CaptionFont,
        SmallCaptionFont,
        MenuFont,
        StatusFont,
        MessageFont,
        DialogFont,
    };

    static CSize GetTextSize(CString str, HDC hDC, CFont* font);
    static CSize GetTextSize(CString str, CDC* pDC, CFont* font);
    static CSize GetTextSize(CString str, HDC hDC, CWnd* wnd, int type);
    static CSize GetTextSizeDiff(CString str, HDC hDC, CWnd* wnd, int type, CFont* curFont);


    static void GetMetrics(bool reset = false);
    static void initMemDC(CDC* pDC, CDC& dcMem, CBitmap& bmMem, CRect rect);
    static void flushMemDC(CDC* pDC, CDC& dcMem, CRect rect);
    static void DrawBufferedText(CDC* pDC, CString text, CRect rect, UINT format);
    static void Draw2BitTransparent(CDC& dc, int left, int top, int width, int height, CBitmap& bmp, COLORREF fgColor);
    static void dbg(CString text, ...);
    static float getConstantFByDPI(CWnd* window, const float* constants);
    static int getConstantByDPI(CWnd* window, const int* constants);
    static UINT getResourceByDPI(CWnd* window, CDC* pDC, const UINT* resources);
    static void MapDialogRect2(CDialog* wnd, CRect& r);
    static const std::vector<CMPCTheme::pathPoint> getIconPathByDPI(CMPCThemeTitleBarControlButton* button);
    static const std::vector<CMPCTheme::pathPoint> getIconPathByDPI(CWnd* wnd, WPARAM buttonType);
    static void drawCheckBoxInternal(UINT checkState, bool isHover, bool useSystemSize, CRect rectCheck, CDC* pDC, bool isRadio, CPngImage* image, int size);
    static void drawCheckBox(CWnd* window, UINT checkState, bool isHover, bool useSystemSize, CRect rectCheck, CDC* pDC, bool isRadio = false, UINT resourceID = 0);
    static void drawGripper(CWnd* window, CWnd* dpiRefWnd, CRect rectGripper, CDC* pDC, bool rot90);
    static void drawToolbarHideButton(CDC* pDC, CWnd* window, CRect iconRect, std::vector<CMPCTheme::pathPoint> icon, double dpiScaling, bool antiAlias, bool hover);
    static bool canUseWin10DarkTheme();
    static UINT defaultLogo();
    static HBRUSH getParentDialogBGClr(CWnd* wnd, CDC* pDC);
    static void drawParentDialogBGClr(CWnd* wnd, CDC* pDC, CRect r, bool fill = true);
    static void fulfillThemeReqs(CProgressCtrl* ctl);
    static void enableWindows10DarkFrame(CWnd* window);
    static void AdjustDynamicWidgetPair(CWnd* window, int left, int right, WidgetPairType lType = WidgetPairAuto, WidgetPairType rType = WidgetPairAuto);
    static void UpdateAnalogCaptureDeviceSlider(CScrollBar* pScrollBar);
    static bool IsWindowVisibleAndRendered(CWnd* window);

    void PreDoModalRTL(LPPROPSHEETHEADERW m_psh);

    static bool IsRTL(CWnd* window);
    static CPoint GetClientRectOffset(CWnd* window);

    enum CheckBoxStyle {
        CheckBoxRegular = 0,
        CheckBoxHover = 1,
    };

    enum RadioStyle {
        RadioRegular = 0,
        RadioRegularSet = 1,
        RadioHover = 2,
        RadioHoverSet = 3
    };
};


