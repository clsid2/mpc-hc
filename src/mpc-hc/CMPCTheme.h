#pragma once

class CMPCTheme
{
public:
    enum class ModernThemeMode {
        DARK,
        LIGHT,
        WINDOWSDEFAULT
    };

    enum class ModernThemeStyle {
        WINDOWSDEFAULT, //follow the windows version
        WINDOWS10,
        WINDOWS11
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

    //accent shades reported by Windows, see ReadAccentColors; used by the Windows 11 style only
    static COLORREF AccentDark3;
    static COLORREF AccentDark2;
    static COLORREF AccentDark1;
    static COLORREF Accent;
    static COLORREF AccentLight1;
    static COLORREF AccentLight2;
    static COLORREF AccentLight3;

    //Windows 11 style only: the playlist's selected row (neutral) and its accent indicator
    static COLORREF PlaylistSelectedColor;
    static COLORREF PlaylistIndicatorColor;
    //Windows 11 style only: fill of a checked check box or radio, and the glyph drawn on it
    static COLORREF CheckboxCheckedColor;
    static COLORREF CheckboxGlyphColor;
    //Windows 11 style only: a disabled check box or radio
    static COLORREF CheckboxDisabledBorderColor;
    static COLORREF CheckboxDisabledCheckedColor;
    static COLORREF CheckboxDisabledGlyphColor;
    //Windows 11 style only: the round slider thumb and its outline
    static COLORREF SliderThumbColor;
    static COLORREF SliderThumbBorderColor;
    //the statistics and status bars: black everywhere except the Windows 11 light style
    static COLORREF InfoBarBGColor;
    static COLORREF InfoBarTextColor;
    static COLORREF InfoBarBorderColor;

    static const int GroupBoxTextIndent;


    static const BYTE GripperBitsH[10];
    static const BYTE GripperBitsV[8];
    static const int gripPatternShort;
    static const int gripPatternLong;

    static wchar_t* const uiTextFont;
    static wchar_t* const uiStaticTextFont;
    static wchar_t* const uiSymbolFont;


    static COLORREF ComboboxArrowColor;
    static COLORREF ComboboxArrowColorDisabled;

    static COLORREF HeaderCtrlSortArrowColor;

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
    static bool isWindows11Style; //set by InitializeColors, so drawing code can test it cheaply

    static CMPCTheme::ModernThemeMode EffectiveThemeMode();
    static CMPCTheme::ModernThemeStyle EffectiveThemeStyle();

    static void InitializeColors();
    static void InitializeWindows10Colors();
    static void InitializeWindows11Colors();
    static void ReadAccentColors();
    static void ApplyAccentColors();
    static COLORREF EnsureContrast(COLORREF fg, COLORREF bg, double minRatio);
};
