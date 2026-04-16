#pragma once

class CMPCTheme
{
public:
    enum class ModernThemeMode {
        DARK,
        LIGHT,
        WINDOWSDEFAULT
    };

    static COLORREF MenuBGColor;
    static COLORREF MenubarBGColor;
    static COLORREF WindowBGColor;  //used in explorer for left nav
    static COLORREF ControlAreaBGColor;  //used in file open dialog for button / file selection bg
    static COLORREF ContentBGColor; //used in explorer for bg of file list
    static COLORREF ContentSelectedColor; //used in explorer for bg of file list
    static COLORREF PlayerBGColor;
    static COLORREF HighLightColor;

    static COLORREF MenuSelectedColor;
    static COLORREF MenubarSelectedBGColor;
    static COLORREF MenuItemDisabledColor;
    static COLORREF MainMenuBorderColor;
    static COLORREF MenuSeparatorColor;

    static COLORREF TextFGColor;
    static COLORREF TextFGColorFade;
    static COLORREF PropPageCaptionFGColor;
    static COLORREF ContentTextDisabledFGColorFade;
    static COLORREF ContentTextDisabledFGColorFade2;
    static COLORREF SubmenuColor;
    static COLORREF CloseHoverColor;
    static COLORREF ClosePushColor;
    static COLORREF CloseColor;
    static COLORREF WindowBorderColorLight;
    static COLORREF WindowBorderColorDim;
    static COLORREF NoBorderColor;
    static COLORREF GripperPatternColor;

    static COLORREF ScrollBGColor;
    static COLORREF ScrollProgressColor;
    static COLORREF ScrollThumbColor;
    static COLORREF ScrollThumbHoverColor;
    static COLORREF ScrollThumbDragColor;
    static COLORREF ScrollButtonArrowColor;
    static COLORREF ScrollButtonArrowClickColor;
    static COLORREF ScrollButtonHoverColor;
    static COLORREF ScrollButtonClickColor;

    static COLORREF InlineEditBorderColor;
    static COLORREF TooltipBorderColor;

    static COLORREF GroupBoxBorderColor;

    static COLORREF DebugColorRed;
    static COLORREF DebugColorYellow;
    static COLORREF DebugColorGreen;

    static COLORREF PlayerButtonHotColor;
    static COLORREF PlayerButtonCheckedColor;
    static COLORREF PlayerButtonClickedColor;
    static COLORREF PlayerButtonBorderColor;

    static COLORREF ButtonBorderOuterColor;
    static COLORREF ButtonBorderInnerFocusedColor;
    static COLORREF ButtonBorderInnerColor;
    static COLORREF ButtonBorderSelectedKBFocusColor;
    static COLORREF ButtonBorderHoverKBFocusColor;
    static COLORREF ButtonBorderKBFocusColor;
    static COLORREF ButtonFillColor;
    static COLORREF ButtonFillHoverColor;
    static COLORREF ButtonFillSelectedColor;
    static COLORREF ButtonDisabledFGColor;

    static COLORREF CheckboxBorderColor;
    static COLORREF CheckboxBGColor;
    static COLORREF CheckboxBorderHoverColor;
    static COLORREF CheckboxBGHoverColor;

    static COLORREF ImageDisabledColor;

    static COLORREF SliderChannelColor;

    static COLORREF EditBorderColor;

    static COLORREF TreeCtrlLineColor;
    static COLORREF TreeCtrlHoverColor;
    static COLORREF TreeCtrlFocusColor;

    static COLORREF CheckColor;

    static COLORREF ColumnHeaderHotColor;

    static COLORREF StaticEtchedColor;

    static COLORREF ListCtrlDisabledBGColor;
    static COLORREF ListCtrlGridColor;
    static COLORREF ListCtrlErrorColor;
    static COLORREF HeaderCtrlGridColor;
    static COLORREF AudioSwitcherGridColor;

    static COLORREF TabCtrlBorderColor;
    static COLORREF TabCtrlInactiveColor;

    static COLORREF StatusBarBGColor;
    static COLORREF StatusBarSeparatorColor;
    static COLORREF StatusBarEditBorderColor;

    static COLORREF W10DarkThemeFileDialogInjectedTextColor;
    static COLORREF W10DarkThemeFileDialogInjectedBGColor;
    static COLORREF W10DarkThemeFileDialogInjectedEditBorderColor;
    static COLORREF W10DarkThemeTitlebarBGColor;
    static COLORREF W10DarkThemeTitlebarInactiveBGColor;
    static COLORREF W10DarkThemeTitlebarFGColor;
    static COLORREF W10DarkThemeTitlebarInactiveFGColor;
    static COLORREF W10DarkThemeTitlebarIconPenColor;
    static COLORREF W10DarkThemeTitlebarControlHoverBGColor;
    static COLORREF W10DarkThemeTitlebarInactiveControlHoverBGColor;
    static COLORREF W10DarkThemeTitlebarControlPushedBGColor;
    static COLORREF W10DarkThemeWindowBorderColor;

    static COLORREF ProgressBarBGColor;
    static COLORREF ProgressBarColor;

    static COLORREF SubresyncFadeText1;
    static COLORREF SubresyncFadeText2;
    static COLORREF SubresyncActiveFadeText;
    static COLORREF SubresyncHLColor1;
    static COLORREF SubresyncHLColor2;
    static COLORREF SubresyncGridSepColor;

    static COLORREF ActivePlayListItemColor;
    static COLORREF ActivePlayListItemHLColor;
    static COLORREF StaticLinkColor;
    static COLORREF SeekbarCurrentPositionColor;
    static COLORREF SeekbarChapterColor;
    static COLORREF SeekbarABColor;

    static const int GroupBoxTextIndent;


    static const BYTE GripperBitsH[10];
    static const BYTE GripperBitsV[8];
    static const int gripPatternShort;
    static const int gripPatternLong;

    static wchar_t* const uiTextFont;
    static wchar_t* const uiStaticTextFont;
    static wchar_t* const uiSymbolFont;


    static const COLORREF ComboboxArrowColor;
    static const COLORREF ComboboxArrowColorDisabled;

    static const COLORREF HeaderCtrlSortArrowColor;

    static const BYTE CheckBits[14];
    static const int CheckWidth;
    static const int CheckHeight;

    const static UINT ThemeCheckBoxes[5];
    const static UINT ThemeRadios[5];
    const static UINT ThemeGrippers[5];

    enum pathState {
        linePath,
        newPath,
        closePath
    };

    struct pathPoint {
        float x;
        float y;
        pathState state;
    };
    static const std::vector<pathPoint> minimizeIcon96, minimizeIcon120, minimizeIcon144, minimizeIcon168, minimizeIcon192;
    static const std::vector<pathPoint> maximizeIcon96, maximizeIcon120, maximizeIcon144, maximizeIcon168, maximizeIcon192;
    static const std::vector<pathPoint> restoreIcon96, restoreIcon120, restoreIcon144, restoreIcon168, restoreIcon192;
    static const std::vector<pathPoint> closeIcon96, closeIcon120, closeIcon144, closeIcon168, closeIcon192;
    static const std::vector<pathPoint> hideIcon96, hideIcon120, hideIcon144, hideIcon168, hideIcon192;
    static const int CMPCTheme::W10TitlebarIconPathHeight[5];
    static const int CMPCTheme::W10TitlebarIconPathWidth[5];
    static const float CMPCTheme::W10TitlebarIconPathThickness[5];
    static const int CMPCTheme::W10TitlebarButtonWidth[5];
    static const int CMPCTheme::W10TitlebarButtonSpacing[5];
    static const int CMPCTheme::ToolbarIconPathDimension[5];
    static const int CMPCTheme::ToolbarGripperHeight[5];
    static const int CMPCTheme::ToolbarHideButtonDimensions[5];


    static bool drawThemedControls;

    static CMPCTheme::ModernThemeMode EffectiveThemeMode();

    static void InitializeColors();
};
