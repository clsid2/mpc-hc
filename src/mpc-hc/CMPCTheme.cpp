#include "stdafx.h"
#include "CMPCTheme.h"
#include "mplayerc.h"

#define RGBGS(x)          ((COLORREF)(((BYTE)(x)|((WORD)((BYTE)(x))<<8))|(((DWORD)(BYTE)(x))<<16)))

const int CMPCTheme::GroupBoxTextIndent = 8;
bool CMPCTheme::drawThemedControls = false;
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

void CMPCTheme::InitializeColors() {
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
