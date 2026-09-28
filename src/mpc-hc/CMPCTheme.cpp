#include "stdafx.h"
#include "CMPCTheme.h"
#include "mplayerc.h"
#include <VersionHelpersInternal.h>

//used for ReadAccentColors
#include <wrl.h>
#include <Windows.UI.ViewManagement.h>
//end ReadAccentColors

#define RGBGS(x)          ((COLORREF)(((BYTE)(x)|((WORD)((BYTE)(x))<<8))|(((DWORD)(BYTE)(x))<<16)))

//the Windows 11 palette is derived from WinUI's Fluent colour tokens, which are mostly translucent and
//meant to sit over Mica; MPC-HC has no Mica, so each one is flattened over the solid surface it is drawn on
//token: 0xAARRGGBB as written in the xaml; per channel: s + (c - s) * a / 255, rounded
static COLORREF Flatten(DWORD token, COLORREF surface) {
    const int a = (token >> 24) & 0xFF;
    auto blend = [a](int c, int s) { return (BYTE)((s * 255 + (c - s) * a + 127) / 255); };
    return RGB(blend((token >> 16) & 0xFF, GetRValue(surface)), blend((token >> 8) & 0xFF, GetGValue(surface)), blend(token & 0xFF, GetBValue(surface)));
}

static COLORREF Opaque(DWORD token) {
    return RGB((token >> 16) & 0xFF, (token >> 8) & 0xFF, token & 0xFF);
}

//WinUI 2 Common_themeresources_any.xaml, "Default" (dark) dictionary
namespace FluentDark {
    constexpr DWORD TextFillColorPrimary = 0xFFFFFFFF;
    constexpr DWORD TextFillColorSecondary = 0xC5FFFFFF;
    constexpr DWORD TextFillColorTertiary = 0x87FFFFFF;
    constexpr DWORD TextFillColorDisabled = 0x5DFFFFFF;
    constexpr DWORD ControlFillColorDefault = 0x0FFFFFFF;
    constexpr DWORD ControlFillColorSecondary = 0x15FFFFFF;
    constexpr DWORD ControlFillColorTertiary = 0x08FFFFFF;
    constexpr DWORD ControlFillColorDisabled = 0x0BFFFFFF;
    constexpr DWORD ControlStrongFillColorDefault = 0x8BFFFFFF;
    constexpr DWORD ControlStrongFillColorDisabled = 0x3FFFFFFF;
    constexpr DWORD ControlSolidFillColorDefault = 0xFF454545;
    constexpr DWORD SubtleFillColorSecondary = 0x0FFFFFFF;
    constexpr DWORD SubtleFillColorTertiary = 0x0AFFFFFF;
    constexpr DWORD ControlAltFillColorSecondary = 0x19000000;
    constexpr DWORD ControlAltFillColorTertiary = 0x0BFFFFFF;
    constexpr DWORD ControlStrokeColorDefault = 0x12FFFFFF;
    constexpr DWORD ControlStrokeColorSecondary = 0x18FFFFFF;
    constexpr DWORD ControlStrokeColorOnAccentSecondary = 0x23000000;
    constexpr DWORD CardStrokeColorDefault = 0x19000000;
    constexpr DWORD ControlStrongStrokeColorDefault = 0x8BFFFFFF;
    constexpr DWORD ControlStrongStrokeColorDisabled = 0x28FFFFFF;
    constexpr DWORD SurfaceStrokeColorDefault = 0x66757575;
    constexpr DWORD SurfaceStrokeColorFlyout = 0x33000000;
    constexpr DWORD DividerStrokeColorDefault = 0x15FFFFFF;
    constexpr DWORD FocusStrokeColorOuter = 0xFFFFFFFF;
    constexpr DWORD LayerFillColorDefault = 0x4C3A3A3A;
    constexpr DWORD SolidBackgroundFillColorBase = 0xFF202020;
    constexpr DWORD SolidBackgroundFillColorSecondary = 0xFF1C1C1C;
    constexpr DWORD SolidBackgroundFillColorQuarternary = 0xFF2C2C2C;
    constexpr DWORD SolidBackgroundFillColorBaseAlt = 0xFF0A0A0A;
    constexpr DWORD SystemFillColorCriticalBackground = 0xFF442726;
}

//WinUI 2 Common_themeresources_any.xaml, "Light" dictionary
namespace FluentLight {
    constexpr DWORD TextFillColorPrimary = 0xE4000000;
    constexpr DWORD TextFillColorSecondary = 0x9E000000;
    constexpr DWORD TextFillColorTertiary = 0x72000000;
    constexpr DWORD TextFillColorDisabled = 0x5C000000;
    constexpr DWORD TextOnAccentFillColorPrimary = 0xFFFFFFFF;
    constexpr DWORD ControlFillColorDefault = 0xB3FFFFFF;
    constexpr DWORD ControlFillColorSecondary = 0x80F9F9F9;
    constexpr DWORD ControlFillColorTertiary = 0x4DF9F9F9;
    constexpr DWORD ControlStrongFillColorDefault = 0x72000000;
    constexpr DWORD ControlStrongFillColorDisabled = 0x51000000;
    constexpr DWORD SubtleFillColorSecondary = 0x09000000;
    constexpr DWORD SubtleFillColorTertiary = 0x06000000;
    constexpr DWORD ControlAltFillColorSecondary = 0x06000000;
    constexpr DWORD ControlAltFillColorTertiary = 0x0F000000;
    constexpr DWORD ControlStrokeColorDefault = 0x0F000000;
    constexpr DWORD ControlStrokeColorSecondary = 0x29000000;
    constexpr DWORD ControlStrokeColorOnAccentSecondary = 0x66000000;
    constexpr DWORD CardStrokeColorDefault = 0x0F000000;
    constexpr DWORD ControlStrongStrokeColorDefault = 0x72000000;
    constexpr DWORD ControlStrongStrokeColorDisabled = 0x37000000;
    constexpr DWORD SurfaceStrokeColorDefault = 0x66757575;
    constexpr DWORD SurfaceStrokeColorFlyout = 0x0F000000;
    constexpr DWORD DividerStrokeColorDefault = 0x0F000000;
    constexpr DWORD FocusStrokeColorOuter = 0xE4000000;
    constexpr DWORD LayerFillColorDefault = 0x80FFFFFF;
    constexpr DWORD SolidBackgroundFillColorBase = 0xFFF3F3F3;
    constexpr DWORD SolidBackgroundFillColorSecondary = 0xFFEEEEEE;
    constexpr DWORD SolidBackgroundFillColorTertiary = 0xFFF9F9F9;
    constexpr DWORD SolidBackgroundFillColorBaseAlt = 0xFFDADADA;
    constexpr DWORD SystemFillColorCritical = 0xFFC42B1C;
}

const int CMPCTheme::GroupBoxTextIndent = 8;
bool CMPCTheme::drawThemedControls = false;
bool CMPCTheme::isWindows11Style = false;
COLORREF CMPCTheme::CloseHoverColor = RGB(232, 17, 35);
COLORREF CMPCTheme::ClosePushColor = RGB(139, 10, 20);
COLORREF CMPCTheme::DebugColorRed = RGB(255, 0, 0);
COLORREF CMPCTheme::DebugColorYellow = RGB(255, 255, 0);
COLORREF CMPCTheme::DebugColorGreen = RGB(0, 255, 0);
COLORREF CMPCTheme::W10DarkThemeFileDialogInjectedTextColor = RGB(255, 255, 255);
COLORREF CMPCTheme::W10DarkThemeFileDialogInjectedBGColor = RGB(56, 56, 56);
COLORREF CMPCTheme::W10DarkThemeFileDialogInjectedEditBorderColor = RGB(155, 155, 155);
COLORREF CMPCTheme::W10DarkThemeTitlebarBGColor = RGB(0, 0, 0);
COLORREF CMPCTheme::W10DarkThemeTitlebarInactiveBGColor = RGB(43, 43, 43);
COLORREF CMPCTheme::W10DarkThemeTitlebarFGColor = RGB(255, 255, 255);
COLORREF CMPCTheme::W10DarkThemeTitlebarInactiveFGColor = RGB(170, 170, 170);
COLORREF CMPCTheme::W10DarkThemeTitlebarIconPenColor = RGB(255, 255, 255);
COLORREF CMPCTheme::W10DarkThemeTitlebarControlHoverBGColor = RGB(43, 43, 43);
COLORREF CMPCTheme::W10DarkThemeTitlebarInactiveControlHoverBGColor = RGB(65, 65, 65);
COLORREF CMPCTheme::W10DarkThemeTitlebarControlPushedBGColor = RGB(70, 70, 70);
COLORREF CMPCTheme::W10DarkThemeWindowBorderColor = RGB(57, 57, 57);



COLORREF CMPCTheme::MenuBGColor;
COLORREF CMPCTheme::MenubarBGColor;
COLORREF CMPCTheme::WindowBGColor;
COLORREF CMPCTheme::ControlAreaBGColor;

COLORREF CMPCTheme::ContentBGColor;
COLORREF CMPCTheme::ContentSelectedColor;
COLORREF CMPCTheme::PlayerBGColor;

COLORREF CMPCTheme::HighLightColor;

COLORREF CMPCTheme::MenuSelectedColor;
COLORREF CMPCTheme::MenubarSelectedBGColor;
COLORREF CMPCTheme::MenuSeparatorColor;
COLORREF CMPCTheme::MenuItemDisabledColor;
//COLORREF CMPCTheme::MenuItemUnfocusedColor;
COLORREF CMPCTheme::MainMenuBorderColor;

COLORREF CMPCTheme::TextFGColor;
COLORREF CMPCTheme::TextFGColorFade;
COLORREF CMPCTheme::PropPageCaptionFGColor;
COLORREF CMPCTheme::ContentTextDisabledFGColorFade;
COLORREF CMPCTheme::ContentTextDisabledFGColorFade2; //even more faded, used for NA text on CListCtrl/audio switcher

COLORREF CMPCTheme::SubmenuColor;

COLORREF CMPCTheme::WindowBorderColorLight;
COLORREF CMPCTheme::WindowBorderColorDim;
COLORREF CMPCTheme::NoBorderColor;
COLORREF CMPCTheme::GripperPatternColor; //visual studio, since explorer has no grippers

COLORREF CMPCTheme::ScrollBGColor;
COLORREF CMPCTheme::ScrollProgressColor;
COLORREF CMPCTheme::ScrollThumbColor;
COLORREF CMPCTheme::ScrollThumbHoverColor;
COLORREF CMPCTheme::ScrollThumbDragColor;
COLORREF CMPCTheme::ScrollButtonArrowColor;
COLORREF CMPCTheme::ScrollButtonArrowClickColor;
COLORREF CMPCTheme::ScrollButtonHoverColor;
COLORREF CMPCTheme::ScrollButtonClickColor;

COLORREF CMPCTheme::InlineEditBorderColor;
COLORREF CMPCTheme::TooltipBorderColor;

COLORREF CMPCTheme::GroupBoxBorderColor;

COLORREF CMPCTheme::PlayerButtonHotColor;
COLORREF CMPCTheme::PlayerButtonCheckedColor;
COLORREF CMPCTheme::PlayerButtonClickedColor;
COLORREF CMPCTheme::PlayerButtonBorderColor;

COLORREF CMPCTheme::ButtonBorderOuterColor;
COLORREF CMPCTheme::ButtonBorderInnerFocusedColor;
COLORREF CMPCTheme::ButtonBorderInnerColor;
COLORREF CMPCTheme::ButtonBorderSelectedKBFocusColor;
COLORREF CMPCTheme::ButtonBorderHoverKBFocusColor;
COLORREF CMPCTheme::ButtonBorderKBFocusColor;
COLORREF CMPCTheme::ButtonFillColor;
COLORREF CMPCTheme::ButtonFillHoverColor;
COLORREF CMPCTheme::ButtonFillSelectedColor;
COLORREF CMPCTheme::ButtonDisabledFGColor;

COLORREF CMPCTheme::CheckboxBorderColor;
COLORREF CMPCTheme::CheckboxBGColor;
COLORREF CMPCTheme::CheckboxBorderHoverColor;
COLORREF CMPCTheme::CheckboxBGHoverColor;

COLORREF CMPCTheme::ImageDisabledColor;

COLORREF CMPCTheme::SliderChannelColor;

COLORREF CMPCTheme::EditBorderColor;

COLORREF CMPCTheme::TreeCtrlLineColor;
COLORREF CMPCTheme::TreeCtrlHoverColor;
COLORREF CMPCTheme::TreeCtrlFocusColor;

COLORREF CMPCTheme::CheckColor;

COLORREF CMPCTheme::ColumnHeaderHotColor;

COLORREF CMPCTheme::StaticEtchedColor;

COLORREF CMPCTheme::ListCtrlDisabledBGColor;
COLORREF CMPCTheme::ListCtrlGridColor;
COLORREF CMPCTheme::ListCtrlErrorColor;
COLORREF CMPCTheme::HeaderCtrlGridColor;
COLORREF CMPCTheme::AudioSwitcherGridColor;

COLORREF CMPCTheme::TabCtrlBorderColor;
COLORREF CMPCTheme::TabCtrlInactiveColor;


COLORREF CMPCTheme::StatusBarBGColor;
COLORREF CMPCTheme::StatusBarSeparatorColor;


COLORREF CMPCTheme::ProgressBarBGColor;
COLORREF CMPCTheme::ProgressBarColor;

COLORREF CMPCTheme::SubresyncFadeText1;
COLORREF CMPCTheme::SubresyncFadeText2;
COLORREF CMPCTheme::SubresyncActiveFadeText;
COLORREF CMPCTheme::SubresyncHLColor1;
COLORREF CMPCTheme::SubresyncHLColor2;
COLORREF CMPCTheme::SubresyncGridSepColor;

COLORREF CMPCTheme::ActivePlayListItemColor;
COLORREF CMPCTheme::ActivePlayListItemHLColor;
COLORREF CMPCTheme::StaticLinkColor;

COLORREF CMPCTheme::SeekbarCurrentPositionColor;
COLORREF CMPCTheme::SeekbarChapterColor;
COLORREF CMPCTheme::SeekbarABColor;

COLORREF CMPCTheme::AccentDark3;
COLORREF CMPCTheme::AccentDark2;
COLORREF CMPCTheme::AccentDark1;
COLORREF CMPCTheme::Accent;
COLORREF CMPCTheme::AccentLight1;
COLORREF CMPCTheme::AccentLight2;
COLORREF CMPCTheme::AccentLight3;
COLORREF CMPCTheme::PlaylistSelectedColor;
COLORREF CMPCTheme::PlaylistIndicatorColor;

wchar_t* const CMPCTheme::uiTextFont = L"Segoe UI";
wchar_t* const CMPCTheme::uiStaticTextFont = L"Segoe UI Semilight";
wchar_t* const CMPCTheme::uiSymbolFont = L"MS UI Gothic";


const int CMPCTheme::gripPatternLong = 5;
const int CMPCTheme::gripPatternShort = 4;


const BYTE CMPCTheme::GripperBitsH[10] = {
    0x80, 0x00,
    0x00, 0x00,
    0x20, 0x00,
    0x00, 0x00,
    0x80, 0x00,
};

const BYTE CMPCTheme::GripperBitsV[8] = {
    0x88, 0x00,
    0x00, 0x00,
    0x20, 0x00,
    0x00, 0x00,
};

const COLORREF CMPCTheme::ComboboxArrowColor = RGB(200, 200, 200);
const COLORREF CMPCTheme::ComboboxArrowColorDisabled = RGB(100, 100, 100);

const COLORREF CMPCTheme::HeaderCtrlSortArrowColor = RGB(200, 200, 200);


const BYTE CMPCTheme::CheckBits[14] = {
    0x02, 0x00,
    0x06, 0x00,
    0x8E, 0x00,
    0xDC, 0x00,
    0xF8, 0x00,
    0x70, 0x00,
    0x20, 0x00,
};

const int CMPCTheme::CheckWidth = 7;
const int CMPCTheme::CheckHeight = 7;


const UINT CMPCTheme::ThemeCheckBoxes[5] = {
    IDB_DT_CB_96,
    IDB_DT_CB_120,
    IDB_DT_CB_144,
    IDB_DT_CB_144,
    IDB_DT_CB_192,
};

const UINT CMPCTheme::ThemeRadios[5] = {
    IDB_DT_RADIO_96,
    IDB_DT_RADIO_120,
    IDB_DT_RADIO_144,
    IDB_DT_RADIO_144,
    IDB_DT_RADIO_192,
};

const UINT CMPCTheme::ThemeGrippers[5] = {
    IDB_GRIPPER_96,
    IDB_GRIPPER_120,
    IDB_GRIPPER_144,
    IDB_GRIPPER_168,
    IDB_GRIPPER_192,
};

const std::vector<CMPCTheme::pathPoint> CMPCTheme::minimizeIcon96({
    {2, 6, newPath},
    {11, 6, closePath},
});

const std::vector<CMPCTheme::pathPoint> CMPCTheme::minimizeIcon120({
    {3, 7, newPath},
    {14, 7, closePath},
});

const std::vector<CMPCTheme::pathPoint> CMPCTheme::minimizeIcon144({
    {4, 9, newPath},
    {18, 9, closePath},
});

//same size as 144, but centered better
const std::vector<CMPCTheme::pathPoint> CMPCTheme::minimizeIcon168({
    {2, 9, newPath},
    {16, 9, closePath},
});

const std::vector<CMPCTheme::pathPoint> CMPCTheme::minimizeIcon192({
    {5.5, 12.5, newPath},
    {23.5, 12.5, closePath},
});

const std::vector<CMPCTheme::pathPoint> CMPCTheme::restoreIcon96({
    {2, 4, newPath},
    {9, 4, linePath},
    {9, 11, linePath},
    {2, 11, linePath},
    {2, 4, linePath},
    {4, 4, newPath},
    {4, 2, linePath},
    {11, 2, linePath},
    {11, 9, linePath},
    {9, 9, linePath}
});

const std::vector<CMPCTheme::pathPoint> CMPCTheme::restoreIcon120({
    {2, 4, newPath},
    {11, 4, linePath},
    {11, 13, linePath},
    {2, 13, linePath},
    {2, 4, linePath},
    {4, 4, newPath},
    {4, 2, linePath},
    {13, 2, linePath},
    {13, 11, linePath},
    {11, 11, linePath},
});

const std::vector<CMPCTheme::pathPoint> CMPCTheme::restoreIcon144({
    {2, 5, newPath},
    {13, 5, linePath},
    {13, 16, linePath},
    {2, 16, linePath},
    {2, 5, linePath},
    {5, 5, newPath},
    {5, 2, linePath},
    {16, 2, linePath},
    {16, 13, linePath},
    {13, 13, linePath},
});

const std::vector<CMPCTheme::pathPoint> CMPCTheme::restoreIcon168 = CMPCTheme::restoreIcon144;

const std::vector<CMPCTheme::pathPoint> CMPCTheme::restoreIcon192({
    { 3.5, 7.5, newPath},
    { 17.5, 7.5, linePath },
    { 17.5, 21.5, linePath },
    { 3.5, 21.5, linePath },
    { 3.5, 7.5, linePath },
    { 7.5, 7.5, newPath },
    { 7.5, 3.5, linePath },
    { 21.5, 3.5, linePath },
    { 21.5, 17.5, linePath },
    { 17.5, 17.5, linePath },
});

const std::vector<CMPCTheme::pathPoint> CMPCTheme::maximizeIcon96({
    {1, 1, newPath},
    {1, 10, linePath},
    {10, 10, linePath},
    {10, 1, linePath},
    {1, 1, linePath}
});

const std::vector<CMPCTheme::pathPoint> CMPCTheme::maximizeIcon120({
    {2, 2, newPath},
    {2, 13, linePath},
    {13, 13, linePath},
    {13, 2, linePath},
    {2, 2, linePath},
});

const std::vector<CMPCTheme::pathPoint> CMPCTheme::maximizeIcon144({
    {2, 2, newPath},
    {2, 16, linePath},
    {16, 16, linePath},
    {16, 2, linePath},
    {2, 2, linePath},
});

const std::vector<CMPCTheme::pathPoint> CMPCTheme::maximizeIcon168 = CMPCTheme::maximizeIcon144;

const std::vector<CMPCTheme::pathPoint> CMPCTheme::maximizeIcon192({
    {3.5, 3.5, newPath},
    {3.5, 21.5, linePath},
    {21.5, 21.5, linePath},
    {21.5, 3.5, linePath},
    {3.5, 3.5, linePath},
});

const std::vector<CMPCTheme::pathPoint> CMPCTheme::closeIcon96({
    {1, 1, newPath},
    {10, 10, closePath},
    {1, 10, newPath},
    {10, 1, closePath}
});

const std::vector<CMPCTheme::pathPoint> CMPCTheme::closeIcon120({
    {2, 2, newPath},
    {13, 13, linePath},
    {2, 13, newPath},
    {13, 2, linePath},
});

const std::vector<CMPCTheme::pathPoint> CMPCTheme::closeIcon144({
    {2, 2, newPath},
    {16, 16, linePath},
    {2, 16, newPath},
    {16, 2, linePath},
});

const std::vector<CMPCTheme::pathPoint> CMPCTheme::closeIcon168 = CMPCTheme::closeIcon144;

const std::vector<CMPCTheme::pathPoint> CMPCTheme::closeIcon192({
    {3.5, 3.5, newPath},
    {21.5, 21.5, linePath},
    {3.5, 21.5, newPath},
    {21.5, 3.5, linePath},
});

//windows10 centers the icon "path" on the button, inside a frame
//sometimes this frame is centered, but at different dpis it's misaligned by 1-2 pixels
//we use the width/height of the frame to tweak the "center" position
const int CMPCTheme::W10TitlebarIconPathHeight[5] = {
    12,
    15, //should be 16, but to match windows 10
    18, //should be 19
    18, //should be 19
    26,
};

const int CMPCTheme::W10TitlebarIconPathWidth[5] = {
    12,
    17, //should be 16, but to match windows 10
    19,
    19,
    28,
};

const float CMPCTheme::W10TitlebarIconPathThickness[5] = {
    1,
    1,
    1,
    1,
    2,
};

const int CMPCTheme::W10TitlebarButtonWidth[5] = {
    45,
    58,
    69,
    80,
    91,
};

const int CMPCTheme::W10TitlebarButtonSpacing[5] = {
    1,
    1,
    2,
    1, //makes no sense, but spacing goes back to 1
    2,
};

const int CMPCTheme::ToolbarIconPathDimension[5] = {
    7,
    9,
    11,
    12,
    14,
};

const int CMPCTheme::ToolbarHideButtonDimensions[5] = {
    11,
    14,
    17,
    20,
    22,
};

const int CMPCTheme::ToolbarGripperHeight[5] = {
    5,
    6,
    8,
    9,
    10,
};

const std::vector<CMPCTheme::pathPoint> CMPCTheme::hideIcon96({
    {0, 0, newPath},
    {6, 6, linePath},
    {0, 6, newPath},
    {6, 0, linePath}
});

const std::vector<CMPCTheme::pathPoint> CMPCTheme::hideIcon120({
    {0, 0, newPath},
    {8, 8, linePath},
    {0, 8, newPath},
    {8, 0, linePath},
});

const std::vector<CMPCTheme::pathPoint> CMPCTheme::hideIcon144({
    {0, 0, newPath},
    {10, 10, linePath},
    {0, 10, newPath},
    {10, 0, linePath},
});

const std::vector<CMPCTheme::pathPoint> CMPCTheme::hideIcon168({
    {0, 0, newPath},
    {11, 11, linePath},
    {0, 11, newPath},
    {11, 0, linePath},
});

const std::vector<CMPCTheme::pathPoint> CMPCTheme::hideIcon192({
    {0, 0, newPath},
    {13, 13, linePath},
    {0, 13, newPath},
    {13, 0, linePath},
});

CMPCTheme::ModernThemeMode CMPCTheme::EffectiveThemeMode() {
    ModernThemeMode themeMode = AfxGetAppSettings().eModernThemeMode;
    if (themeMode == ModernThemeMode::WINDOWSDEFAULT) {
        if (AfxGetAppSettings().bWindows10DarkThemeActive) {
            themeMode = ModernThemeMode::DARK;
        } else {
            themeMode = ModernThemeMode::LIGHT;
        }
    }
    return themeMode;
}

CMPCTheme::ModernThemeStyle CMPCTheme::EffectiveThemeStyle() {
    ModernThemeStyle themeStyle = static_cast<ModernThemeStyle>(AfxGetAppSettings().iModernThemeStyle);
    if (themeStyle == ModernThemeStyle::WINDOWSDEFAULT) {
        if (IsWindowsVersionOrGreaterBuild(10, 0, 22000)) {
            themeStyle = ModernThemeStyle::WINDOWS11;
        } else {
            themeStyle = ModernThemeStyle::WINDOWS10;
        }
    }
    return themeStyle;
}

void CMPCTheme::InitializeColors() {
    isWindows11Style = EffectiveThemeStyle() == ModernThemeStyle::WINDOWS11;
    if (isWindows11Style) {
        InitializeWindows11Colors();
    } else {
        InitializeWindows10Colors();
    }
}

void CMPCTheme::InitializeWindows10Colors() {
    if (EffectiveThemeMode() == ModernThemeMode::DARK) {
        drawThemedControls = true;

        MenuBGColor = RGB(43, 43, 43);
        MenubarBGColor = RGB(43, 43, 43);
        WindowBGColor = RGB(25, 25, 25);
        ControlAreaBGColor = RGB(56, 56, 56);

        ContentBGColor = RGB(32, 32, 32);
        ContentSelectedColor = RGB(119, 119, 119);
        PlayerBGColor = RGB(32, 32, 32);

        HighLightColor = GetSysColor(COLOR_HIGHLIGHT);

        MenuSelectedColor = RGB(65, 65, 65);
        MenubarSelectedBGColor = RGB(65, 65, 65);
        MenuSeparatorColor = RGB(128, 128, 128);
        MenuItemDisabledColor = RGB(109, 109, 109);
        MainMenuBorderColor = RGB(32, 32, 32);

        TextFGColor = RGB(255, 255, 255);
        PropPageCaptionFGColor = RGBGS(255);
        TextFGColorFade = RGB(200, 200, 200);
        ContentTextDisabledFGColorFade = RGB(109, 109, 109);
        ContentTextDisabledFGColorFade2 = RGB(60, 60, 60); //even more faded, used for NA text on CListCtrl/audio switcher

        SubmenuColor = RGB(191, 191, 191);

        WindowBorderColorLight = RGB(99, 99, 99);
        WindowBorderColorDim = RGB(43, 43, 43);
        NoBorderColor = RGB(0, 0, 0);
        GripperPatternColor = RGB(70, 70, 74); //visual studio dark, since explorer has no grippers

        ScrollBGColor = RGB(23, 23, 23);
        ScrollProgressColor = RGB(60, 60, 60);
        ScrollThumbColor = RGB(77, 77, 77);
        ScrollThumbHoverColor = RGB(144, 144, 144);
        ScrollThumbDragColor = RGB(183, 183, 183);
        ScrollButtonArrowColor = RGB(103, 103, 103);
        ScrollButtonArrowClickColor = RGBGS(103);
        ScrollButtonHoverColor = RGB(55, 55, 55);
        ScrollButtonClickColor = RGB(166, 166, 166);

        InlineEditBorderColor = RGB(255, 255, 255);
        TooltipBorderColor = RGB(118, 118, 118);

        GroupBoxBorderColor = RGB(118, 118, 118);

        PlayerButtonHotColor = RGB(43, 43, 43);
        PlayerButtonCheckedColor = RGB(66, 66, 66);
        PlayerButtonClickedColor = RGB(55, 55, 55);
        PlayerButtonBorderColor = RGB(0, 0, 0);

        ButtonBorderOuterColor = RGB(240, 240, 240);
        ButtonBorderInnerFocusedColor = RGB(255, 255, 255);
        ButtonBorderInnerColor = RGB(155, 155, 155);
        ButtonBorderSelectedKBFocusColor = RGB(150, 150, 150);
        ButtonBorderHoverKBFocusColor = RGB(181, 181, 181);
        ButtonBorderKBFocusColor = RGB(195, 195, 195);
        ButtonFillColor = RGB(51, 51, 51);
        ButtonFillHoverColor = RGB(69, 69, 69);
        ButtonFillSelectedColor = RGB(102, 102, 102);
        ButtonDisabledFGColor = RGB(109, 109, 109);

        CheckboxBorderColor = RGB(137, 137, 137);
        CheckboxBGColor = RGB(0, 0, 0);
        CheckboxBorderHoverColor = RGB(121, 121, 121);
        CheckboxBGHoverColor = RGB(8, 8, 8);

        ImageDisabledColor = RGB(109, 109, 109);

        SliderChannelColor = RGB(109, 109, 109);

        EditBorderColor = RGB(106, 106, 106);

        TreeCtrlLineColor = RGB(106, 106, 106);
        TreeCtrlHoverColor = RGB(77, 77, 77);
        TreeCtrlFocusColor = RGB(98, 98, 98);

        CheckColor = RGB(222, 222, 222);

        ColumnHeaderHotColor = RGB(67, 67, 67);

        StaticEtchedColor = RGB(65, 65, 65);

        ListCtrlDisabledBGColor = RGB(40, 40, 40);
        ListCtrlGridColor = RGB(43, 43, 43);
        ListCtrlErrorColor = RGB(242, 13, 13);
        HeaderCtrlGridColor = RGB(99, 99, 99);
        AudioSwitcherGridColor = RGB(99, 99, 99);

        TabCtrlBorderColor = RGB(99, 99, 99);
        TabCtrlInactiveColor = RGB(40, 40, 40);


        StatusBarBGColor = RGB(51, 51, 51);
        StatusBarSeparatorColor = RGB(247, 247, 247);

        ProgressBarBGColor = RGBGS(128);
        ProgressBarColor = RGB(15, 101, 31);

        SubresyncFadeText1 = RGB(190, 190, 190);
        SubresyncFadeText2 = RGB(160, 160, 160);
        SubresyncActiveFadeText = RGB(215, 215, 215);
        SubresyncHLColor1 = RGB(100, 100, 100);
        SubresyncHLColor2 = RGB(80, 80, 80);
        SubresyncGridSepColor = RGB(220, 220, 220);

        ActivePlayListItemColor = RGB(38, 160, 218);
        ActivePlayListItemHLColor = RGB(0, 40, 110);
        StaticLinkColor = RGB(38, 160, 218);

        SeekbarCurrentPositionColor = RGB(38, 160, 218);
        SeekbarChapterColor = RGB(100, 100, 100);
        SeekbarABColor = RGB(242, 13, 13);
    } else {
        MenuBGColor = RGBGS(238);
        MenubarBGColor = RGBGS(255);
        WindowBGColor = RGBGS(255);
        ControlAreaBGColor = RGBGS(240);

        ContentBGColor = RGBGS(255);
        ContentSelectedColor = RGB(0, 120, 215);
        PlayerBGColor = RGBGS(255);

        HighLightColor = GetSysColor(COLOR_HIGHLIGHT);

        MenuSelectedColor = RGBGS(255);
        MenubarSelectedBGColor = RGBGS(238);
        MenuSeparatorColor = RGBGS(145);
        MenuItemDisabledColor = RGBGS(109);
        MainMenuBorderColor = RGBGS(255);

        TextFGColor = RGBGS(0);
        PropPageCaptionFGColor = RGBGS(245);
        TextFGColorFade = RGBGS(109);
        ContentTextDisabledFGColorFade = RGBGS(176);
        ContentTextDisabledFGColorFade2 = RGBGS(224); //even more faded, used for NA text on CListCtrl/audio switcher

        SubmenuColor = RGBGS(0);

        WindowBorderColorLight = RGB(130, 135, 144);
        WindowBorderColorDim = RGB(48, 56, 62);
        NoBorderColor = RGBGS(0);
        GripperPatternColor = RGB(153, 153, 153); //visual studio light, since explorer has no grippers

        ScrollBGColor = RGBGS(240);
        ScrollProgressColor = RGB(130, 215, 146);
        ScrollThumbColor = RGBGS(205);
        ScrollThumbHoverColor = RGBGS(166);
        ScrollThumbDragColor = RGBGS(119);
        ScrollButtonArrowColor = RGBGS(96);
        ScrollButtonArrowClickColor = RGBGS(255);
        ScrollButtonHoverColor = RGBGS(218);
        ScrollButtonClickColor = RGBGS(96);

        InlineEditBorderColor = RGBGS(0);
        TooltipBorderColor = RGBGS(118);

        GroupBoxBorderColor = RGB(130, 135, 144);

        PlayerButtonHotColor = RGB(232, 239, 247);
        PlayerButtonCheckedColor = RGB(205, 228, 252);
        PlayerButtonClickedColor = RGB(201, 224, 247);
        PlayerButtonBorderColor = RGB(98, 162, 228);


        ButtonBorderOuterColor = RGBGS(173);
        ButtonBorderInnerFocusedColor = RGB(0, 120, 215);
        ButtonBorderInnerColor = RGBGS(173);
        ButtonBorderSelectedKBFocusColor = RGB(60, 20, 7);
        ButtonBorderHoverKBFocusColor = RGB(21, 1, 11);
        ButtonBorderKBFocusColor = RGBGS(17);
        ButtonFillColor = RGBGS(225);
        ButtonFillHoverColor = RGB(229, 241, 251);
        ButtonFillSelectedColor = RGB(204, 228, 247);
        ButtonDisabledFGColor = RGBGS(204);

        CheckboxBorderColor = RGB(97, 121, 160);
        CheckboxBGColor = RGBGS(255);
        CheckboxBorderHoverColor = RGB(38, 160, 218);
        CheckboxBGHoverColor = RGBGS(255);

        ImageDisabledColor = RGBGS(128);

        SliderChannelColor = RGBGS(128);

        EditBorderColor = RGB(106, 106, 106);

        TreeCtrlLineColor = RGB(255, 0, 0); //not implemented for light theme, default windows controls used
        TreeCtrlHoverColor = RGB(255, 0, 0); //not implemented for light theme, default windows controls used
        TreeCtrlFocusColor = RGB(255, 0, 0); //not implemented for light theme, default windows controls used

        CheckColor = RGB(255, 0, 0); //not implemented for light theme, default windows controls used

        ColumnHeaderHotColor = RGB(217, 235, 239);

        StaticEtchedColor = RGB(255, 0, 0); //not implemented for light theme, default windows controls used

        ListCtrlDisabledBGColor = RGB(255, 0, 0); //not implemented for light theme, default windows controls used
        ListCtrlGridColor = RGB(255, 0, 0); //not implemented for light theme, default windows controls used
        ListCtrlErrorColor = RGB(255, 0, 0); //not implemented for light theme, default windows controls used
        HeaderCtrlGridColor = RGB(255, 0, 0); //not implemented for light theme, default windows controls used
        AudioSwitcherGridColor = RGB(255, 0, 0); //not implemented for light theme, default windows controls used

        TabCtrlBorderColor = RGBGS(227);
        TabCtrlInactiveColor = RGBGS(246);

        StatusBarBGColor = RGBGS(240);
        StatusBarSeparatorColor = RGB(255, 0, 0); //not implemented for light theme, default windows controls used

        ProgressBarBGColor = RGBGS(255);
        ProgressBarColor = RGB(130, 215, 146);

        SubresyncFadeText1 = RGB(255, 0, 0); //not implemented for light theme, default windows controls used
        SubresyncFadeText2 = RGB(255, 0, 0); //not implemented for light theme, default windows controls used
        SubresyncActiveFadeText = RGB(255, 0, 0); //not implemented for light theme, default windows controls used
        SubresyncHLColor1 = RGB(255, 0, 0); //not implemented for light theme, default windows controls used
        SubresyncHLColor2 = RGB(255, 0, 0); //not implemented for light theme, default windows controls used
        SubresyncGridSepColor = RGB(255, 0, 0); //not implemented for light theme, default windows controls used

        ActivePlayListItemColor = RGB(255, 0, 0); //not implemented for light theme, default windows controls used
        ActivePlayListItemHLColor = RGB(255, 0, 0); //not implemented for light theme, default windows controls used
        StaticLinkColor = RGB(255, 0, 0); //not implemented for light theme, default windows controls used

        SeekbarCurrentPositionColor = RGB(38, 160, 218);
        SeekbarChapterColor = RGB(100, 100, 100);
        SeekbarABColor = RGB(242, 13, 13);
    }
}

void CMPCTheme::InitializeWindows11Colors() {
    if (EffectiveThemeMode() == ModernThemeMode::DARK) {
        using namespace FluentDark;
        drawThemedControls = true;

        const COLORREF base = Opaque(SolidBackgroundFillColorBase);
        const COLORREF layer = Flatten(LayerFillColorDefault, base); //content surfaces sit one layer above the base

        MenuBGColor = Opaque(SolidBackgroundFillColorQuarternary);
        MenubarBGColor = base;
        WindowBGColor = base;
        ControlAreaBGColor = Opaque(SolidBackgroundFillColorSecondary);

        ContentBGColor = layer;
        PlaylistSelectedColor = Flatten(SubtleFillColorSecondary, ContentBGColor); //list selection is neutral, the accent goes on an indicator
        ContentSelectedColor = Opaque(ControlSolidFillColorDefault); //neutral selection as in explorer; only the light selection is the accent
        PlayerBGColor = layer; //player bars share the content layer so the seekbar stays flush with the toolbar

        MenuSelectedColor = Flatten(SubtleFillColorSecondary, MenuBGColor);
        MenubarSelectedBGColor = Flatten(SubtleFillColorSecondary, MenubarBGColor);
        MenuSeparatorColor = Flatten(DividerStrokeColorDefault, MenuBGColor);
        MenuItemDisabledColor = Flatten(TextFillColorDisabled, MenuBGColor);
        MainMenuBorderColor = Flatten(SurfaceStrokeColorFlyout, MenuBGColor);

        TextFGColor = Opaque(TextFillColorPrimary);
        PropPageCaptionFGColor = Opaque(TextFillColorPrimary);
        TextFGColorFade = Flatten(TextFillColorSecondary, base);
        ContentTextDisabledFGColorFade = Flatten(TextFillColorDisabled, ContentBGColor);
        ContentTextDisabledFGColorFade2 = Flatten(ControlStrongStrokeColorDisabled, ContentBGColor); //even more faded: no fainter text token, the disabled strong stroke is the nearest role

        SubmenuColor = Flatten(TextFillColorSecondary, MenuBGColor);

        WindowBorderColorLight = Flatten(SurfaceStrokeColorDefault, base);
        WindowBorderColorDim = Flatten(CardStrokeColorDefault, base);
        NoBorderColor = RGB(0, 0, 0); //seekbar keeps its Windows 10 colours for now; following the accent is deferred (2026-09-27)
        GripperPatternColor = Flatten(ControlStrongStrokeColorDisabled, WindowBGColor); //gripper dots: a de-emphasised strong stroke

        ScrollBGColor = RGB(23, 23, 23); //seekbar keeps its Windows 10 colours for now; following the accent is deferred (2026-09-27)
        ScrollProgressColor = RGB(60, 60, 60); //seekbar keeps its Windows 10 colours for now; following the accent is deferred (2026-09-27)
        ScrollThumbColor = Flatten(ControlStrongFillColorDisabled, ScrollBGColor); //resting thumb: the dimmer of the two strong fills
        ScrollThumbHoverColor = Flatten(ControlStrongFillColorDefault, ScrollBGColor);
        ScrollThumbDragColor = Flatten(TextFillColorSecondary, ScrollBGColor); //dragged thumb: one step brighter than the hover fill
        ScrollButtonArrowColor = Flatten(TextFillColorSecondary, ScrollBGColor);
        ScrollButtonArrowClickColor = Flatten(TextFillColorTertiary, ScrollBGColor);
        ScrollButtonHoverColor = Flatten(SubtleFillColorSecondary, ScrollBGColor);
        ScrollButtonClickColor = Flatten(SubtleFillColorTertiary, ScrollBGColor);

        InlineEditBorderColor = Opaque(FocusStrokeColorOuter);
        TooltipBorderColor = Flatten(SurfaceStrokeColorDefault, MenuBGColor); //tooltips are filled with MenuBGColor

        GroupBoxBorderColor = Flatten(DividerStrokeColorDefault, WindowBGColor);

        PlayerButtonHotColor = Flatten(SubtleFillColorSecondary, PlayerBGColor);
        PlayerButtonCheckedColor = Opaque(SolidBackgroundFillColorBaseAlt); //checked toolbar button: sunken onto the alternate base surface
        PlayerButtonClickedColor = Flatten(SubtleFillColorTertiary, PlayerBGColor);
        PlayerButtonBorderColor = Flatten(ControlStrokeColorDefault, PlayerBGColor);

        ButtonBorderOuterColor = Flatten(ControlStrokeColorSecondary, WindowBGColor);
        ButtonBorderInnerColor = Flatten(ControlStrokeColorDefault, WindowBGColor);
        ButtonBorderSelectedKBFocusColor = Opaque(FocusStrokeColorOuter);
        ButtonBorderHoverKBFocusColor = Opaque(FocusStrokeColorOuter);
        ButtonBorderKBFocusColor = Opaque(FocusStrokeColorOuter);
        ButtonFillColor = Flatten(ControlFillColorDefault, WindowBGColor);
        ButtonFillHoverColor = Flatten(ControlFillColorSecondary, WindowBGColor);
        ButtonFillSelectedColor = Flatten(ControlFillColorTertiary, WindowBGColor);
        ButtonDisabledFGColor = Flatten(TextFillColorDisabled, WindowBGColor);

        CheckboxBorderColor = Flatten(ControlStrongStrokeColorDefault, WindowBGColor);
        CheckboxBGColor = Flatten(ControlAltFillColorSecondary, WindowBGColor);
        CheckboxBGHoverColor = Flatten(ControlAltFillColorTertiary, WindowBGColor);

        ImageDisabledColor = Flatten(TextFillColorDisabled, WindowBGColor);

        SliderChannelColor = Flatten(ControlStrongFillColorDefault, WindowBGColor);

        EditBorderColor = Flatten(ControlStrokeColorSecondary, WindowBGColor); //text boxes have the stronger bottom stroke

        TreeCtrlLineColor = Flatten(ControlStrongStrokeColorDisabled, ContentBGColor); //tree connector lines: a de-emphasised strong stroke
        TreeCtrlHoverColor = Flatten(SubtleFillColorSecondary, ContentBGColor);
        TreeCtrlFocusColor = Opaque(ControlSolidFillColorDefault); //selection, as ContentSelectedColor

        CheckColor = Opaque(TextFillColorPrimary);

        ColumnHeaderHotColor = Flatten(SubtleFillColorSecondary, ContentBGColor);

        StaticEtchedColor = Flatten(DividerStrokeColorDefault, WindowBGColor);

        ListCtrlDisabledBGColor = Flatten(ControlFillColorDisabled, ContentBGColor);
        ListCtrlGridColor = Flatten(DividerStrokeColorDefault, ContentBGColor);
        ListCtrlErrorColor = Opaque(SystemFillColorCriticalBackground); //drawn behind text, so the critical background rather than the critical fill
        HeaderCtrlGridColor = Flatten(DividerStrokeColorDefault, ContentBGColor);
        AudioSwitcherGridColor = Flatten(DividerStrokeColorDefault, ContentBGColor);

        TabCtrlBorderColor = Flatten(DividerStrokeColorDefault, WindowBGColor);
        TabCtrlInactiveColor = Flatten(ControlFillColorDefault, WindowBGColor);

        StatusBarBGColor = Opaque(SolidBackgroundFillColorSecondary);
        StatusBarSeparatorColor = Flatten(DividerStrokeColorDefault, StatusBarBGColor);

        ProgressBarBGColor = Flatten(ControlStrongStrokeColorDefault, WindowBGColor); //progress track

        SubresyncFadeText1 = Flatten(TextFillColorTertiary, ContentBGColor);
        SubresyncFadeText2 = Flatten(TextFillColorDisabled, ContentBGColor);
        SubresyncActiveFadeText = Flatten(TextFillColorSecondary, ContentBGColor);
        SubresyncHLColor1 = Flatten(ControlStrongFillColorDisabled, ContentBGColor); //modified rows: a dimmed strong fill
        SubresyncHLColor2 = Flatten(ControlFillColorSecondary, ContentBGColor); //adjusted rows: a control fill
        SubresyncGridSepColor = Flatten(ControlStrongStrokeColorDefault, ContentBGColor);

        SeekbarCurrentPositionColor = RGB(38, 160, 218); //seekbar keeps its Windows 10 colours for now; following the accent is deferred (2026-09-27)
        SeekbarChapterColor = RGB(100, 100, 100); //seekbar keeps its Windows 10 colours for now; following the accent is deferred (2026-09-27)
        SeekbarABColor = RGB(242, 13, 13); //seekbar keeps its Windows 10 colours for now; following the accent is deferred (2026-09-27)

        //mode independent in the Windows 10 palette
        CloseHoverColor = Opaque(FluentLight::SystemFillColorCritical); //windows 11 uses the same close red in both modes
        ClosePushColor = Flatten(ControlStrokeColorOnAccentSecondary, CloseHoverColor); //pressed close: the on-accent stroke darkens the red
        W10DarkThemeFileDialogInjectedTextColor = Opaque(TextFillColorPrimary);
        W10DarkThemeFileDialogInjectedBGColor = ControlAreaBGColor;
        W10DarkThemeFileDialogInjectedEditBorderColor = Flatten(ControlStrokeColorSecondary, ControlAreaBGColor);
        W10DarkThemeTitlebarBGColor = base;
        W10DarkThemeTitlebarInactiveBGColor = base; //windows 11 keeps the titlebar colour and dims the text when inactive
        W10DarkThemeTitlebarFGColor = Opaque(TextFillColorPrimary);
        W10DarkThemeTitlebarInactiveFGColor = Flatten(TextFillColorDisabled, base);
        W10DarkThemeTitlebarIconPenColor = Opaque(TextFillColorPrimary);
        W10DarkThemeTitlebarControlHoverBGColor = Flatten(SubtleFillColorSecondary, base);
        W10DarkThemeTitlebarInactiveControlHoverBGColor = Flatten(SubtleFillColorSecondary, base);
        W10DarkThemeTitlebarControlPushedBGColor = Flatten(SubtleFillColorTertiary, base);
        W10DarkThemeWindowBorderColor = Flatten(SurfaceStrokeColorDefault, base);
    } else {
        using namespace FluentLight;

        const COLORREF base = Opaque(SolidBackgroundFillColorBase);
        const COLORREF layer = Flatten(LayerFillColorDefault, base); //content surfaces sit one layer above the base

        MenuBGColor = Opaque(SolidBackgroundFillColorTertiary);
        MenubarBGColor = base;
        WindowBGColor = base;
        ControlAreaBGColor = Opaque(SolidBackgroundFillColorSecondary);

        ContentBGColor = layer;
        PlaylistSelectedColor = Flatten(SubtleFillColorSecondary, ContentBGColor); //list selection is neutral, the accent goes on an indicator
        PlayerBGColor = layer; //player bars share the content layer so the seekbar stays flush with the toolbar

        MenuSelectedColor = Flatten(SubtleFillColorSecondary, MenuBGColor); //darker than the menu, unlike the Windows 10 palette
        MenubarSelectedBGColor = Flatten(SubtleFillColorSecondary, MenubarBGColor);
        MenuSeparatorColor = Flatten(DividerStrokeColorDefault, MenuBGColor);
        MenuItemDisabledColor = Flatten(TextFillColorDisabled, MenuBGColor);
        MainMenuBorderColor = Flatten(SurfaceStrokeColorFlyout, MenuBGColor);

        TextFGColor = Flatten(TextFillColorPrimary, base);
        PropPageCaptionFGColor = Opaque(TextOnAccentFillColorPrimary); //the caption gradient starts at the accent selection colour
        TextFGColorFade = Flatten(TextFillColorSecondary, base);
        ContentTextDisabledFGColorFade = Flatten(TextFillColorDisabled, ContentBGColor);
        ContentTextDisabledFGColorFade2 = Flatten(ControlStrongStrokeColorDisabled, ContentBGColor); //even more faded: no fainter text token, the disabled strong stroke is the nearest role

        SubmenuColor = Flatten(TextFillColorSecondary, MenuBGColor);

        WindowBorderColorLight = Flatten(SurfaceStrokeColorDefault, base);
        WindowBorderColorDim = Flatten(CardStrokeColorDefault, base);
        NoBorderColor = RGBGS(0); //seekbar keeps its Windows 10 colours for now; following the accent is deferred (2026-09-27)
        GripperPatternColor = Flatten(ControlStrongStrokeColorDisabled, WindowBGColor); //gripper dots: a de-emphasised strong stroke

        ScrollBGColor = RGBGS(240); //seekbar keeps its Windows 10 colours for now; following the accent is deferred (2026-09-27)
        ScrollProgressColor = RGB(130, 215, 146); //seekbar keeps its Windows 10 colours for now; following the accent is deferred (2026-09-27)
        ScrollThumbColor = Flatten(ControlStrongFillColorDisabled, ScrollBGColor); //resting thumb: the dimmer of the two strong fills
        ScrollThumbHoverColor = Flatten(ControlStrongFillColorDefault, ScrollBGColor);
        ScrollThumbDragColor = Flatten(TextFillColorSecondary, ScrollBGColor); //dragged thumb: one step darker than the hover fill
        ScrollButtonArrowColor = Flatten(TextFillColorSecondary, ScrollBGColor);
        ScrollButtonArrowClickColor = Flatten(TextFillColorTertiary, ScrollBGColor);
        ScrollButtonHoverColor = Flatten(SubtleFillColorSecondary, ScrollBGColor);
        ScrollButtonClickColor = Flatten(SubtleFillColorTertiary, ScrollBGColor);

        InlineEditBorderColor = Flatten(FocusStrokeColorOuter, ContentBGColor);
        TooltipBorderColor = Flatten(SurfaceStrokeColorDefault, MenuBGColor); //tooltips are filled with MenuBGColor

        GroupBoxBorderColor = Flatten(DividerStrokeColorDefault, WindowBGColor);

        PlayerButtonHotColor = Flatten(SubtleFillColorSecondary, PlayerBGColor);
        PlayerButtonCheckedColor = Opaque(SolidBackgroundFillColorBaseAlt); //checked toolbar button: sunken onto the alternate base surface
        PlayerButtonClickedColor = Flatten(SubtleFillColorTertiary, PlayerBGColor);
        PlayerButtonBorderColor = Flatten(ControlStrokeColorDefault, PlayerBGColor);

        ButtonBorderOuterColor = Flatten(ControlStrokeColorSecondary, WindowBGColor);
        ButtonBorderInnerColor = Flatten(ControlStrokeColorDefault, WindowBGColor);
        ButtonBorderSelectedKBFocusColor = Flatten(FocusStrokeColorOuter, WindowBGColor);
        ButtonBorderHoverKBFocusColor = Flatten(FocusStrokeColorOuter, WindowBGColor);
        ButtonBorderKBFocusColor = Flatten(FocusStrokeColorOuter, WindowBGColor);
        ButtonFillColor = Flatten(ControlFillColorDefault, WindowBGColor);
        ButtonFillHoverColor = Flatten(ControlFillColorSecondary, WindowBGColor);
        ButtonFillSelectedColor = Flatten(ControlFillColorTertiary, WindowBGColor);
        ButtonDisabledFGColor = Flatten(TextFillColorDisabled, WindowBGColor);

        CheckboxBorderColor = Flatten(ControlStrongStrokeColorDefault, WindowBGColor);
        CheckboxBGColor = Flatten(ControlAltFillColorSecondary, WindowBGColor);
        CheckboxBGHoverColor = Flatten(ControlAltFillColorTertiary, WindowBGColor);

        ImageDisabledColor = Flatten(TextFillColorDisabled, WindowBGColor);

        SliderChannelColor = Flatten(ControlStrongFillColorDefault, WindowBGColor);

        EditBorderColor = Flatten(ControlStrokeColorSecondary, WindowBGColor); //text boxes have the stronger bottom stroke

        TreeCtrlLineColor = RGB(255, 0, 0); //not implemented for light theme, default windows controls used
        TreeCtrlHoverColor = RGB(255, 0, 0); //not implemented for light theme, default windows controls used
        TreeCtrlFocusColor = RGB(255, 0, 0); //not implemented for light theme, default windows controls used

        CheckColor = RGB(255, 0, 0); //not implemented for light theme, default windows controls used

        ColumnHeaderHotColor = Flatten(SubtleFillColorSecondary, ContentBGColor);

        StaticEtchedColor = RGB(255, 0, 0); //not implemented for light theme, default windows controls used

        ListCtrlDisabledBGColor = RGB(255, 0, 0); //not implemented for light theme, default windows controls used
        ListCtrlGridColor = RGB(255, 0, 0); //not implemented for light theme, default windows controls used
        ListCtrlErrorColor = RGB(255, 0, 0); //not implemented for light theme, default windows controls used
        HeaderCtrlGridColor = RGB(255, 0, 0); //not implemented for light theme, default windows controls used
        AudioSwitcherGridColor = RGB(255, 0, 0); //not implemented for light theme, default windows controls used

        TabCtrlBorderColor = Flatten(DividerStrokeColorDefault, WindowBGColor);
        TabCtrlInactiveColor = Flatten(ControlFillColorDefault, WindowBGColor);

        StatusBarBGColor = Opaque(SolidBackgroundFillColorSecondary);
        StatusBarSeparatorColor = RGB(255, 0, 0); //not implemented for light theme, default windows controls used

        ProgressBarBGColor = Flatten(ControlStrongStrokeColorDefault, WindowBGColor); //progress track

        SubresyncFadeText1 = RGB(255, 0, 0); //not implemented for light theme, default windows controls used
        SubresyncFadeText2 = RGB(255, 0, 0); //not implemented for light theme, default windows controls used
        SubresyncActiveFadeText = RGB(255, 0, 0); //not implemented for light theme, default windows controls used
        SubresyncHLColor1 = RGB(255, 0, 0); //not implemented for light theme, default windows controls used
        SubresyncHLColor2 = RGB(255, 0, 0); //not implemented for light theme, default windows controls used
        SubresyncGridSepColor = RGB(255, 0, 0); //not implemented for light theme, default windows controls used

        ActivePlayListItemColor = RGB(255, 0, 0); //not implemented for light theme, default windows controls used
        ActivePlayListItemHLColor = RGB(255, 0, 0); //not implemented for light theme, default windows controls used
        StaticLinkColor = RGB(255, 0, 0); //not implemented for light theme, default windows controls used

        SeekbarCurrentPositionColor = RGB(38, 160, 218); //seekbar keeps its Windows 10 colours for now; following the accent is deferred (2026-09-27)
        SeekbarChapterColor = RGB(100, 100, 100); //seekbar keeps its Windows 10 colours for now; following the accent is deferred (2026-09-27)
        SeekbarABColor = RGB(242, 13, 13); //seekbar keeps its Windows 10 colours for now; following the accent is deferred (2026-09-27)

        //mode independent in the Windows 10 palette; the titlebar and file dialog slots are only drawn in dark mode
        CloseHoverColor = Opaque(SystemFillColorCritical); //windows 11 uses the same close red in both modes
        ClosePushColor = Flatten(ControlStrokeColorOnAccentSecondary, CloseHoverColor); //pressed close: the on-accent stroke darkens the red
        W10DarkThemeFileDialogInjectedTextColor = TextFGColor;
        W10DarkThemeFileDialogInjectedBGColor = ControlAreaBGColor;
        W10DarkThemeFileDialogInjectedEditBorderColor = Flatten(ControlStrokeColorSecondary, ControlAreaBGColor);
        W10DarkThemeTitlebarBGColor = base;
        W10DarkThemeTitlebarInactiveBGColor = base;
        W10DarkThemeTitlebarFGColor = TextFGColor;
        W10DarkThemeTitlebarInactiveFGColor = Flatten(TextFillColorDisabled, base);
        W10DarkThemeTitlebarIconPenColor = TextFGColor;
        W10DarkThemeTitlebarControlHoverBGColor = Flatten(SubtleFillColorSecondary, base);
        W10DarkThemeTitlebarInactiveControlHoverBGColor = Flatten(SubtleFillColorSecondary, base);
        W10DarkThemeTitlebarControlPushedBGColor = Flatten(SubtleFillColorTertiary, base);
        W10DarkThemeWindowBorderColor = Flatten(SurfaceStrokeColorDefault, base);
    }
    ApplyAccentColors();
}

//reduced interface from windows.ui.viewmanagement.h in the windows 10 sdk, as DpiHelper.cpp does for IUISettings2
MIDL_INTERFACE("03021BE4-5254-4781-8194-5168F7D06D7B")
IUISettings3: public IInspectable
{
public:
    struct Color {
        BYTE A;
        BYTE R;
        BYTE G;
        BYTE B;
    };
    enum UIColorType : int {
        Background = 0,
        Foreground = 1,
        AccentDark3 = 2,
        AccentDark2 = 3,
        AccentDark1 = 4,
        Accent = 5,
        AccentLight1 = 6,
        AccentLight2 = 7,
        AccentLight3 = 8
    };
    virtual HRESULT STDMETHODCALLTYPE GetColorValue(UIColorType desiredColor, Color* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_ColorValuesChanged(void* handler, EventRegistrationToken* cookie) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_ColorValuesChanged(EventRegistrationToken cookie) = 0;
};

//shades[0..6] = AccentDark3 .. AccentLight3
static bool DoReadAccentColors(COLORREF* shades) {
    using namespace Microsoft::WRL;
    using namespace Microsoft::WRL::Wrappers;

    ComPtr<IInspectable> instance;
    if (FAILED(RoActivateInstance(HStringReference(RuntimeClass_Windows_UI_ViewManagement_UISettings).Get(), &instance))) {
        return false;
    }
    ComPtr<IUISettings3> settings3;
    if (FAILED(instance.As(&settings3))) {
        return false;
    }
    for (int i = 0; i < 7; i++) {
        IUISettings3::Color color;
        if (FAILED(settings3->GetColorValue(static_cast<IUISettings3::UIColorType>(IUISettings3::AccentDark3 + i), &color))) {
            return false;
        }
        shades[i] = RGB(color.R, color.G, color.B);
    }
    return true;
}

//the WinRT string and activation DLLs are delay loaded, so this can raise on Windows 7
static bool ReadAccentColorsGuarded(COLORREF* shades) {
    __try {
        return DoReadAccentColors(shades);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void CMPCTheme::ReadAccentColors() {
    COLORREF shades[7];
    if (!IsWindows10OrGreater() || !ReadAccentColorsGuarded(shades)) {
        std::fill_n(shades, 7, GetSysColor(COLOR_HIGHLIGHT));
    }
    AccentDark3 = shades[0];
    AccentDark2 = shades[1];
    AccentDark1 = shades[2];
    Accent = shades[3];
    AccentLight1 = shades[4];
    AccentLight2 = shades[5];
    AccentLight3 = shades[6];
}

//WCAG relative luminance
static double RelativeLuminance(COLORREF c) {
    auto lin = [](int v) { double s = v / 255.0; return s <= 0.03928 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4); };
    return 0.2126 * lin(GetRValue(c)) + 0.7152 * lin(GetGValue(c)) + 0.0722 * lin(GetBValue(c));
}

//the accent is the user's choice and can be too dark (or too light) for the surface it is drawn on;
//blend it toward white on dark surfaces, black on light ones, until it reaches minRatio (4.5 for text, 3 for graphics)
COLORREF CMPCTheme::EnsureContrast(COLORREF fg, COLORREF bg, double minRatio) {
    const double lb = RelativeLuminance(bg);
    const int target = lb < 0.18 ? 255 : 0;
    for (int step = 0; step <= 20; step++) {
        auto blend = [step, target](int c) { return (BYTE)(c + (target - c) * step / 20); };
        COLORREF c = RGB(blend(GetRValue(fg)), blend(GetGValue(fg)), blend(GetBValue(fg)));
        const double lc = RelativeLuminance(c);
        if (((std::max)(lc, lb) + 0.05) / ((std::min)(lc, lb) + 0.05) >= minRatio) {
            return c;
        }
    }
    return RGB(target, target, target);
}

//the most vivid of the accent shades that still reads on bg. WinUI's fixed choice (AccentLight3 on dark, AccentDark2 on light)
//can be washed out or nearly black depending on the user's accent, which makes accent text hard to tell from plain text
static COLORREF VividAccent(const COLORREF* shades, int count, COLORREF bg, double minRatio) {
    auto chroma = [](COLORREF c) { return (std::max)({ GetRValue(c), GetGValue(c), GetBValue(c) }) - (std::min)({ GetRValue(c), GetGValue(c), GetBValue(c) }); };
    const double lb = RelativeLuminance(bg);
    int best = -1;
    for (int i = 0; i < count; i++) {
        const double lc = RelativeLuminance(shades[i]);
        if (((std::max)(lc, lb) + 0.05) / ((std::min)(lc, lb) + 0.05) >= minRatio && (best < 0 || chroma(shades[i]) > chroma(shades[best]))) {
            best = i;
        }
    }
    //none reads: fall back to the shade nearest the readable end and blend it until it does
    return best >= 0 ? shades[best] : CMPCTheme::EnsureContrast(shades[lb < 0.18 ? count - 1 : 0], bg, minRatio);
}

//the accent slots of the Windows 11 style; called again from CMainFrame::OnSettingChange when the accent changes
void CMPCTheme::ApplyAccentColors() {
    ReadAccentColors();
    const COLORREF shades[] = { AccentDark3, AccentDark2, AccentDark1, Accent, AccentLight1, AccentLight2, AccentLight3 };
    const int count = _countof(shades);
    if (EffectiveThemeMode() == ModernThemeMode::DARK) {
        //WinUI dark: AccentFillColorDefault = AccentLight2, AccentTextFillColorPrimary = AccentLight3
        HighLightColor = AccentLight2;
        ButtonBorderInnerFocusedColor = EnsureContrast(AccentLight2, WindowBGColor, 3.0);
        CheckboxBorderHoverColor = EnsureContrast(AccentLight2, WindowBGColor, 3.0);
        ProgressBarColor = AccentLight2;
    } else {
        //WinUI light: AccentFillColorDefault = AccentDark1, AccentTextFillColorPrimary = AccentDark2
        HighLightColor = AccentDark1;
        ContentSelectedColor = AccentDark1; //the light selection is the accent, drawn with white text
        ButtonBorderInnerFocusedColor = EnsureContrast(AccentDark1, WindowBGColor, 3.0);
        CheckboxBorderHoverColor = EnsureContrast(AccentDark1, WindowBGColor, 3.0);
        ProgressBarColor = AccentDark1;
        //StaticLinkColor is not implemented for light theme; it is set below but only read by the dark static link
    }
    //accent text and the playlist indicator use the most vivid shade that reads, rather than WinUI's fixed shade;
    //the playlist is drawn from the palette in the Windows 11 style, light included
    ActivePlayListItemColor = VividAccent(shades, count, ContentBGColor, 4.5);
    ActivePlayListItemHLColor = VividAccent(shades, count, PlaylistSelectedColor, 4.5);
    StaticLinkColor = VividAccent(shades, count, WindowBGColor, 4.5);
    PlaylistIndicatorColor = VividAccent(shades, count, PlaylistSelectedColor, 3.0);
}
