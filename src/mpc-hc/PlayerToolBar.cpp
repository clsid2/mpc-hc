/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2016 see Authors.txt
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
#include "mplayerc.h"
#include <cmath>
#include <atlbase.h>
#include <afxpriv.h>
#include "MPCPngImage.h"
#include "PlayerToolBar.h"
#include "MainFrm.h"
#include "PathUtils.h"
#include "SVGImage.h"
#include "ImageGrayer.h"
#include "CMPCTheme.h"
#include "stb/stb_image.h"
#include "stb/stb_image_resize2.h"

// CPlayerToolBar
/*
Each toolbar image contains 4 rows of 26 buttons.
Row 1: buttons for light theme, enabled/active state
Row 2: buttons for light theme, disabled/inactive state
Row 3: buttons for dark theme, enabled/active state
Row 4: buttons for dark theme, disabled/inactive state


Play
Pause
Stop
Previous(file / chapter)
Next(file / chapter)
Audio(stream selection)
Subtitles(stream selection)
Decrease playback speed
Increase playback speed
Framestep
Open(file / url)
Options
Toggle fullscreen
Toggle playlist
Toggle shuffle
Toggle repeat
Seek backwards
Seek forwards
Information
Close player
Generic button 1 (these buttons are reserved for future ability of assigning custom actions)
Generic button 2 (these buttons can have number 1 - 4, letters A - D, or just some other unique icon)
Generic button 3
Generic button 4
Sound enabled(active state) + Sound enabled(low volume) (inactive state)
Sound muted(active state) + Sound unavailable(inactive state)
*/

#define VOLUMEBUTTON_SVG_INDEX 24

std::map<WORD, CPlayerToolBar::svgButtonInfo> CPlayerToolBar::supportedSvgButtons = {
    {ID_PLAY_PLAY, {TBBS_CHECKGROUP, 0, 0, LOCK_LEFT}},
    {ID_PLAY_PAUSE, {TBBS_CHECKGROUP, 1, 0, LOCK_LEFT}},
    {ID_PLAY_STOP, {TBBS_CHECKGROUP, 2, 0, LOCK_LEFT}},
    {ID_NAVIGATE_SKIPBACK, {TBBS_BUTTON, 3}},
    {ID_NAVIGATE_SKIPFORWARD, {TBBS_BUTTON, 4}},
    {ID_AUDIOS, {TBBS_BUTTON, 5, IDS_AUDIOS}},
    {ID_SUBTITLES, {TBBS_BUTTON, 6, IDS_SUBTITLES}},
    {ID_PLAY_DECRATE, {TBBS_BUTTON, 7}},
    {ID_PLAY_INCRATE, {TBBS_BUTTON, 8}},
    {ID_PLAY_FRAMESTEP, {TBBS_BUTTON, 9}},
    {ID_FILE_OPENMEDIA, {TBBS_BUTTON, 10}},
    {ID_VIEW_OPTIONS, {TBBS_BUTTON, 11}},
    {ID_BUTTON_FULLSCREEN, {TBBS_BUTTON, 12, IDS_AG_FULLSCREEN, LOCK_NONE, IDS_AG_EXIT_FULLSCREEN}},
    {ID_BUTTON_PLAYLIST, {TBBS_BUTTON, 13, IDS_AG_SHOW_PLAYLIST, LOCK_NONE, IDS_AG_HIDE_PLAYLIST}},
    {ID_BUTTON_SHUFFLE, {TBBS_BUTTON, 14, IDS_AG_ENABLE_SHUFFLE, LOCK_NONE, IDS_AG_DISABLE_SHUFFLE}},
    {ID_BUTTON_REPEAT, {TBBS_BUTTON, 15, IDS_AG_ENABLE_REPEAT, LOCK_NONE, IDS_AG_DISABLE_REPEAT}},
    {ID_PLAY_SEEKBACKWARDMED, {TBBS_BUTTON, 16}},
    {ID_PLAY_SEEKFORWARDMED, {TBBS_BUTTON, 17}},
    {ID_FILE_PROPERTIES, {TBBS_BUTTON, 18}},
    {ID_FILE_EXIT, {TBBS_BUTTON, 19}},
    {ID_CUSTOM_ACTION1, {TBBS_BUTTON, 20, IDS_CUSTOM_ACTION1}},
    {ID_CUSTOM_ACTION2, {TBBS_BUTTON, 21, IDS_CUSTOM_ACTION2}},
    {ID_CUSTOM_ACTION3, {TBBS_BUTTON, 22, IDS_CUSTOM_ACTION3}},
    {ID_CUSTOM_ACTION4, {TBBS_BUTTON, 23, IDS_CUSTOM_ACTION4}},
    {ID_DUMMYSEPARATOR, {TBBS_SEPARATOR, -1, 0, LOCK_RIGHT}},
    {ID_VOLUME_MUTE, {TBBS_CHECKBOX, VOLUMEBUTTON_SVG_INDEX, 0, LOCK_RIGHT}},
};

static std::vector<int> supportedSvgButtonsSeq;

IMPLEMENT_DYNAMIC(CPlayerToolBar, CToolBar)
CPlayerToolBar::CPlayerToolBar(CMainFrame* pMainFrame)
    : m_pMainFrame(pMainFrame)
    , m_nButtonHeight(16)
    , m_volumeCtrlSize(60)
    , volumeButtonIndex(12)
    , dummySeparatorIndex(11)
    , flexibleSpaceIndex(10)
    , currentlyDraggingButton(-1)
    , toolbarAdjustActive(false)
{
    GetEventd().Connect(m_eventc, {
        MpcEvent::DPI_CHANGED,
        MpcEvent::DEFAULT_TOOLBAR_SIZE_CHANGED,
        MpcEvent::TOOLBAR_THEME_CHANGED,
        MpcEvent::CHANGING_UI_LANGUAGE,

}, std::bind(&CPlayerToolBar::EventCallback, this, std::placeholders::_1));
}

CPlayerToolBar::~CPlayerToolBar()
{
}

bool CPlayerToolBar::LoadExternalToolBar(CImage& image, float svgscale, CStringW resolutionPostfix)
{
    auto& s = AfxGetAppSettings();

    if (s.nToolbarType <= 0 || s.strToolbarName.IsEmpty()) {
        return false;
    }

    // Paths and extensions to try (by order of preference)
    std::vector<CStringW> paths({ PathUtils::GetProgramPath() });
    CStringW appDataPath;
    if (AfxGetMyApp()->GetAppDataPath(appDataPath)) {
        paths.emplace_back(appDataPath);
    }
    CStringW basetbname;
    basetbname = L"buttons" + resolutionPostfix + L".";

    auto isValidToolbarImage = [](const CImage& img) -> bool {
        int height = img.GetHeight();
        int width = img.GetWidth();
        return (height % 4 == 0) && (width % (height / 4) == 0);
    };

    // Try loading the external toolbar
    for (const auto& path : paths) {
        CStringW tbPath = PathUtils::CombinePaths(path, L"toolbars");
        tbPath = PathUtils::CombinePaths(tbPath, s.strToolbarName);
        if (SUCCEEDED(SVGImage::Load(PathUtils::CombinePaths(tbPath, basetbname + "svg"), image, svgscale))) {
            if (isValidToolbarImage(image)) {
                return true;
            }
            image.Destroy();
        }
        CImage src;
        if (SUCCEEDED(src.Load(PathUtils::CombinePaths(tbPath, basetbname + "png")))) {
            if (src.GetBPP() != 32) {
                continue; //only 32 bit png allowed
            }
            if (svgscale != 1.0) {
                int width = src.GetWidth() * svgscale;
                int height = src.GetHeight() * svgscale;

                image.Destroy();
                image.Create(width, height, src.GetBPP(), CImage::createAlphaChannel);

                auto success = stbir_resize(static_cast<BYTE*>(src.GetBits()), src.GetWidth(), src.GetHeight(), src.GetPitch(), static_cast<BYTE*>(image.GetBits()), width, height, image.GetPitch(), STBIR_BGRA, STBIR_TYPE_UINT8, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT);
                if (success && isValidToolbarImage(image)) {
                    return true;
                }
                image.Destroy();
            } else {
                image.Attach(src.Detach());
                if (isValidToolbarImage(image)) {
                    return true;
                }
                image.Destroy();
            }
        }
    }

    return false;
}

void CPlayerToolBar::MakeImageList(bool createCustomizeButtons, int buttonSize, std::unique_ptr<CImageList> &imageList) {
    auto& s = AfxGetAppSettings();

    // We are currently not aware of any cases where the scale factors are different
    float dpiScaling = (float)std::min(m_pMainFrame->m_dpi.ScaleFactorX(), m_pMainFrame->m_dpi.ScaleFactorY());
    int targetsize = int(dpiScaling * buttonSize / 4 + 0.5) * 4; //we need this to be divisible by 4

    float svgscale;

    UINT resourceID;
    CStringW resolutionPostfix;
    if (targetsize < 24) {
        if (s.nToolbarType == CAppSettings::EXTERNAL_TOOLBAR_NO_16) { //force it to internal if they are trying to use a toolbar without 16px for small buttons
            s.nToolbarType = CAppSettings::INTERNAL_TOOLBAR;
            s.strToolbarName = L"";
        }
        resolutionPostfix = L"16";
        resourceID = IDF_SVG_BUTTONS16;
        svgscale = targetsize / 16.0f;
    } else if (targetsize < 32) {
        resolutionPostfix = L"24";
        resourceID = IDF_SVG_BUTTONS24;
        svgscale = targetsize / 24.0f;
    } else if (targetsize < 48) {
        resolutionPostfix = L"32";
        resourceID = IDF_SVG_BUTTONS32;
        svgscale = targetsize / 32.0f;
    } else if (targetsize < 64) {
        resolutionPostfix = L"48";
        resourceID = IDF_SVG_BUTTONS48;
        svgscale = targetsize / 48.0f;
    } else {
        resolutionPostfix = L"64";
        resourceID = IDF_SVG_BUTTONS64;
        svgscale = targetsize / 64.0f;
    }

    CImage image;

    imageList.reset();
    if (!createCustomizeButtons) {
        m_pDisabledButtonsImages.reset();
    }

    bool buttonsImageLoaded = false;
    if (LoadExternalToolBar(image, svgscale, resolutionPostfix)) {
        buttonsImageLoaded = true;
    } else {
        s.nToolbarType = CAppSettings::INTERNAL_TOOLBAR;
        s.strToolbarName = L"";
    }


    if (buttonsImageLoaded || SUCCEEDED(SVGImage::Load(resourceID, image, svgscale))) {
        CImage imageDisabled;
        CBitmap* bmp = CBitmap::FromHandle(image);


        int width = image.GetWidth();
        int height = image.GetHeight() / 4;
        int bpp = image.GetBPP();
        if (width % height == 0) {
            imageList.reset(DEBUG_NEW CImageList());
            imageList->Create(height, height, ILC_COLOR32 | ILC_MASK, 1, 64);
            CImage dynamicToolbar, dynamicToolbarDisabled;

            if (!createCustomizeButtons) {
                // the manual specifies that sizeButton should be sizeImage inflated by (7, 6)
                SetSizes(CSize(height + 7, height + 6), CSize(height, height));

                volumeOn.Destroy();
                volumeOff.Destroy();
                volumeOn.Create(height * 2, height, bpp, CImage::createAlphaChannel);
                volumeOff.Create(height * 2, height, bpp, CImage::createAlphaChannel);

                m_pDisabledButtonsImages.reset(DEBUG_NEW CImageList());
                m_pDisabledButtonsImages->Create(height, height, ILC_COLOR32 | ILC_MASK, 1, 64);
                dynamicToolbar.Create(width*2, height, bpp, CImage::createAlphaChannel);
                dynamicToolbarDisabled.Create(width, height, bpp, CImage::createAlphaChannel);
            } else {
                dynamicToolbar.Create(width * 2, height, bpp, CImage::createAlphaChannel);
            }

            CBitmap* pOldTargetBmp = nullptr;
            CBitmap* pOldSourceBmp = nullptr;

            CDC targetDC;
            CDC sourceDC;
            CDC* pDC = this->GetDC();
            targetDC.CreateCompatibleDC(pDC);
            sourceDC.CreateCompatibleDC(pDC);

            pOldTargetBmp = targetDC.SelectObject(CBitmap::FromHandle(dynamicToolbar));
            pOldSourceBmp = sourceDC.GetCurrentBitmap();

            int imageOffset = 0, imageDisabledOffset = height;
            if (AppIsThemeLoaded() && CMPCTheme::EffectiveThemeMode() == CMPCTheme::ModernThemeMode::DARK) {
                imageOffset = height * 2;
                imageDisabledOffset = height * 3;
            }

            sourceDC.SelectObject(bmp);
            targetDC.BitBlt(0, 0, image.GetWidth(), height, &sourceDC, 0, imageOffset, SRCCOPY);

            if (!createCustomizeButtons) {
                targetDC.SelectObject(CBitmap::FromHandle(dynamicToolbarDisabled));
                targetDC.BitBlt(0, 0, image.GetWidth(), height, &sourceDC, 0, imageDisabledOffset, SRCCOPY);
            }

            //we will add the disabled buttons to the end of the imagelist, for customize page and "toggle" buttons with extra icons (e.g., fullscreen)
            targetDC.SelectObject(CBitmap::FromHandle(dynamicToolbar));
            targetDC.BitBlt(image.GetWidth(), 0, image.GetWidth(), height, &sourceDC, 0, imageDisabledOffset, SRCCOPY);

            sourceDC.SelectObject(pOldSourceBmp);
            targetDC.SelectObject(pOldTargetBmp);

            sourceDC.DeleteDC();
            targetDC.DeleteDC();

            ReleaseDC(pDC);

            imageList->Add(CBitmap::FromHandle(dynamicToolbar), nullptr);
            dynamicToolbar.Destroy();

            if (!createCustomizeButtons) {
                m_pDisabledButtonsImages->Add(CBitmap::FromHandle(dynamicToolbarDisabled), nullptr);
                dynamicToolbarDisabled.Destroy();
                m_nButtonHeight = height;

                GetToolBarCtrl().SetImageList(imageList.get());
                GetToolBarCtrl().SetDisabledImageList(m_pDisabledButtonsImages.get());
            }

        }
    }
    image.Destroy();
}

void CPlayerToolBar::LoadToolbarImage(bool tbArtChanged /* = false */)
{
    MakeImageList(false, AfxGetAppSettings().nDefaultToolbarSize, m_pButtonsImages);
    if (!m_pCustomizeButtonImages || tbArtChanged) {
        MakeImageList(true, 32, m_pCustomizeButtonImages);
    }
}

TBBUTTON CPlayerToolBar::GetStandardButton(int cmdid) {
    auto& svgInfo = supportedSvgButtons[cmdid];
    TBBUTTON button = { 0 };
    button.iBitmap = svgInfo.svgIndex;
    button.idCommand = cmdid;
    button.iString = -1;
    button.fsStyle = svgInfo.style;
    return button;
}

void CPlayerToolBar::LoadButtonStrings() {
    auto& s = AfxGetAppSettings();

    supportedSvgButtonsSeq.clear();
    for (auto& it : supportedSvgButtons) {
        if (it.second.svgIndex != -1) {
            DWORD dwname;
            if (s.CommandIDToWMCMD.count(it.first) > 0) {
                dwname = s.CommandIDToWMCMD[it.first]->dwname;
            } else {
                dwname = it.second.strID;
            }
            it.second.text.LoadStringW(dwname);
            supportedSvgButtonsSeq.push_back(it.first);
        }
    }
}

BOOL CPlayerToolBar::Create(CWnd* pParentWnd)
{
    VERIFY(__super::CreateEx(pParentWnd,
                             TBSTYLE_FLAT | TBSTYLE_TRANSPARENT | TBSTYLE_AUTOSIZE | TBSTYLE_CUSTOMERASE | CCS_ADJUSTABLE,
                             WS_CHILD | WS_VISIBLE | CBRS_BOTTOM /*| CBRS_TOOLTIPS*/,
                             CRect(2, 2, 0, 1)));

    CToolBarCtrl& tb = GetToolBarCtrl();

    dummySeparatorIndex = -1;
    volumeButtonIndex = -1;
    flexibleSpaceIndex = -1;

    LoadButtonStrings();

    PlaceButtons(true);

    // Should never be RTLed
    ModifyStyleEx(WS_EX_LAYOUTRTL, WS_EX_NOINHERITLAYOUT);

    m_volctrl.Create(this);
    m_volctrl.SetRange(0, 100);

    m_nButtonHeight = 16; // reset m_nButtonHeight

    LoadToolbarImage();
    SetMute(AfxGetAppSettings().fMute);

    if (AppIsThemeLoaded()) {
        themedToolTip.enableFlickerHelper(); //avoid flicker on button hover
        themedToolTip.Create(this, TTS_ALWAYSTIP);
        tb.SetToolTips(&themedToolTip);
    } else {
        tb.EnableToolTips();
    }

    return TRUE;
}

void CPlayerToolBar::PlaceButtons(bool loadSavedLayout) {
    CToolBarCtrl& tb = GetToolBarCtrl();

    auto addButton = [&](int cmdid) {
        auto& svgInfo = supportedSvgButtons[cmdid];
        TBBUTTON button = GetStandardButton(cmdid);
        tb.AddButtons(1, &button);
        SetButtonStyle(tb.GetButtonCount() - 1, svgInfo.style | TBBS_DISABLED);
    };

    std::vector<int> buttons(0);
    if (loadSavedLayout) {
        buttons = AfxGetMyApp()->GetProfileVectorInt(IDS_R_PLAYERTOOLBAR, L"ButtonSequence");
    }

    addButton(ID_PLAY_PLAY);
    addButton(ID_PLAY_PAUSE);
    addButton(ID_PLAY_STOP);

    if (buttons.size() >= 5) { //it is required that the toolbar have the 5 standard items, otherwise this is invalid
        for (int i = 3; i < buttons.size() - 2; i++) {
            if (supportedSvgButtons.count(buttons[i])) {
                auto& btn = supportedSvgButtons[buttons[i]];
                if (!btn.positionLocked) {
                    addButton(buttons[i]);
                }
            }
        }
    } else { //add standard dynamic items
        addButton(ID_NAVIGATE_SKIPBACK);
        addButton(ID_PLAY_DECRATE);
        addButton(ID_PLAY_INCRATE);
        addButton(ID_NAVIGATE_SKIPFORWARD);
        addButton(ID_PLAY_FRAMESTEP);
    }

    addButton(ID_DUMMYSEPARATOR);
    addButton(ID_VOLUME_MUTE);
}

void CPlayerToolBar::ArrangeControls() {
    if (!::IsWindow(m_volctrl.m_hWnd)) {
        return;
    }

    CRect r;
    GetClientRect(&r);
    CRect br = GetBorders();
    CRect vr;
    if (AppIsThemeLoaded()) {
        float dpiScaling = (float)std::min(m_pMainFrame->m_dpi.ScaleFactorX(), m_pMainFrame->m_dpi.ScaleFactorY());
        int targetsize = int(dpiScaling * AfxGetAppSettings().nDefaultToolbarSize);
        m_volumeCtrlSize = targetsize * 2.5f;
        vr = CRect(r.right + br.right - m_volumeCtrlSize, r.top + targetsize / 4, r.right + br.right, r.bottom - targetsize / 4);
    } else {
        vr = CRect(r.right + br.right - 58, r.top - 2, r.right + br.right + 6, r.bottom);
        m_volctrl.MoveWindow(vr);
        CRect thumbRect;
        m_volctrl.GetThumbRect(thumbRect);
        m_volctrl.MapWindowPoints(this, thumbRect);
        vr.top += std::max((r.bottom - thumbRect.bottom - 4) / 2, 0l);
        vr.left -= MulDiv(thumbRect.Height(), 50, 19) - 50;
        m_volumeCtrlSize = vr.Width();
    }
    m_volctrl.MoveWindow(vr);

    volumeButtonIndex = GetToolBarCtrl().GetButtonCount() - 1;
    dummySeparatorIndex = volumeButtonIndex - 1;
    flexibleSpaceIndex = dummySeparatorIndex - 1;

    CRect rFlexible, rVolumeButton;
    GetItemRect(flexibleSpaceIndex, &rFlexible);
    GetItemRect(volumeButtonIndex, &rVolumeButton);
    int spacing = vr.left - rFlexible.right - rVolumeButton.Width();
    SetButtonInfo(dummySeparatorIndex, GetItemID(dummySeparatorIndex), TBBS_SEPARATOR, spacing);
}

void CPlayerToolBar::SetMute(bool fMute) {
    CToolBarCtrl& tb = GetToolBarCtrl();
    TBBUTTONINFOW bi = { sizeof(bi) };
    bi.dwMask = TBIF_IMAGE;
    bi.iImage = VOLUMEBUTTON_SVG_INDEX + (fMute ? 1:0);
    tb.SetButtonInfo(ID_VOLUME_MUTE, &bi);
    AfxGetAppSettings().fMute = fMute;
}

void CPlayerToolBar::ToggleButton(int buttonID, bool isActive, std::optional<bool> &lastBool) {
    auto& imgPtr = GetCustomizeButtonImages();
    if (lastBool != isActive && supportedSvgButtons.count(buttonID) > 0 && imgPtr) {
        CToolBarCtrl& tb = GetToolBarCtrl();
        TBBUTTONINFOW bi = { sizeof(bi) };
        bi.dwMask = TBIF_IMAGE;
        bi.iImage = supportedSvgButtons[buttonID].svgIndex + (isActive ? 0 : imgPtr->GetImageCount() / 2);
        if (buttonID == ID_BUTTON_SHUFFLE || buttonID == ID_BUTTON_REPEAT) {
            bi.dwMask |= TBIF_STATE;
            bi.fsState = TBSTATE_ENABLED | (isActive ? TBSTATE_CHECKED : 0);
        }
        tb.SetButtonInfo(buttonID, &bi);
        lastBool = isActive;
    }
}

void CPlayerToolBar::SetFullscreen(bool isFS) {
    ToggleButton(ID_BUTTON_FULLSCREEN, isFS, lastFullscreen);
}

void CPlayerToolBar::SetPlaylist(bool isVisible) {
    ToggleButton(ID_BUTTON_PLAYLIST, isVisible, lastPlaylist);
}

void CPlayerToolBar::SetShuffle(bool isEnabled) {
    ToggleButton(ID_BUTTON_SHUFFLE, isEnabled, lastShuffle);
}

void CPlayerToolBar::SetRepeat(bool isEnabled) {
    ToggleButton(ID_BUTTON_REPEAT, isEnabled, lastRepeat);
}

bool CPlayerToolBar::IsMuted() const
{
    return AfxGetAppSettings().fMute;
}

int CPlayerToolBar::GetVolume() const
{
    int volume = m_volctrl.GetPos(); // [0..100]
    if (IsMuted() || volume <= 0) {
        volume = -10000;
    } else {
        volume = std::min((int)(4000 * log10(volume / 100.0f)), 0); // 4000=2.0*100*20, where 2.0 is a special factor
    }

    return volume;
}

int CPlayerToolBar::GetMinWidth() const
{
    // button widths are inflated by 7px
    // x buttons + spacing + volume
    int buttonCount = GetToolBarCtrl().GetButtonCount() - 2; //minus 2 because of play/pause being combined, dynamic spacer
    return buttonCount * (m_nButtonHeight + 1 + 7) + 4 + m_volumeCtrlSize;
}

void CPlayerToolBar::SetVolume(int volume)
{
    m_volctrl.SetPosInternal(volume);
}

void CPlayerToolBar::EventCallback(MpcEvent ev)
{
    switch (ev) {
        case MpcEvent::DPI_CHANGED:
        case MpcEvent::DEFAULT_TOOLBAR_SIZE_CHANGED:
            LoadToolbarImage();
            break;
        case MpcEvent::TOOLBAR_THEME_CHANGED:
            LoadToolbarImage(true);
            break;
        case MpcEvent::CHANGING_UI_LANGUAGE:
            LoadButtonStrings();
            break;
        default:
            UNREACHABLE_CODE();
    }
}

BEGIN_MESSAGE_MAP(CPlayerToolBar, CToolBar)
    ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, OnCustomDraw)
    ON_WM_SIZE()
    ON_MESSAGE_VOID(WM_INITIALUPDATE, OnInitialUpdate)
    ON_COMMAND_EX(ID_VOLUME_MUTE, OnVolumeMute)
    ON_UPDATE_COMMAND_UI(ID_VOLUME_MUTE, OnUpdateVolumeMute)
    ON_UPDATE_COMMAND_UI(ID_BUTTON_FULLSCREEN, OnUpdateFullscreen)
    ON_UPDATE_COMMAND_UI(ID_BUTTON_PLAYLIST, OnUpdatePlaylist)
    ON_UPDATE_COMMAND_UI(ID_BUTTON_SHUFFLE, OnUpdateShuffle)
    ON_UPDATE_COMMAND_UI(ID_BUTTON_REPEAT, OnUpdateRepeat)
    ON_UPDATE_COMMAND_UI_RANGE(ID_CUSTOM_ACTION1, ID_CUSTOM_ACTION4, OnUpdateCustomAction)
    ON_COMMAND_EX_RANGE(ID_CUSTOM_ACTION1, ID_CUSTOM_ACTION4, OnCustomAction)
    ON_COMMAND_EX(ID_AUDIOS, OnMenuButton)
    ON_COMMAND_EX(ID_SUBTITLES, OnMenuButton)
    ON_COMMAND_EX(ID_VOLUME_UP, OnVolumeUp)
    ON_COMMAND_EX(ID_VOLUME_DOWN, OnVolumeDown)
    ON_COMMAND_EX(ID_BUTTON_FULLSCREEN, OnFullscreenButton)
    ON_COMMAND_EX(ID_BUTTON_PLAYLIST, OnPlaylistButton)
    ON_COMMAND_EX(ID_BUTTON_SHUFFLE, OnShuffleButton)
    ON_COMMAND_EX(ID_BUTTON_REPEAT, OnRepeatButton)
    ON_WM_NCPAINT()
    ON_WM_LBUTTONDOWN()
    ON_WM_RBUTTONDOWN()
    ON_WM_SETCURSOR()
    ON_NOTIFY_EX(TTN_NEEDTEXT, 0, OnToolTipNotify)
    ON_WM_LBUTTONUP()
    ON_WM_RBUTTONUP()
    ON_NOTIFY_REFLECT(TBN_QUERYDELETE, &CPlayerToolBar::OnTbnQueryDelete)
    ON_NOTIFY_REFLECT(TBN_QUERYINSERT, &CPlayerToolBar::OnTbnQueryInsert)
    ON_NOTIFY_REFLECT(TBN_TOOLBARCHANGE, &CPlayerToolBar::OnTbnToolbarChange)
    ON_WM_MOUSEMOVE()
    ON_NOTIFY_REFLECT(TBN_GETBUTTONINFO, &CPlayerToolBar::OnTbnGetButtonInfo)
    ON_NOTIFY_REFLECT(TBN_INITCUSTOMIZE, &CPlayerToolBar::OnTbnInitCustomize)
    ON_NOTIFY_REFLECT(TBN_BEGINADJUST, &CPlayerToolBar::OnTbnBeginAdjust)
    ON_NOTIFY_REFLECT(TBN_ENDADJUST, &CPlayerToolBar::OnTbnEndAdjust)
    ON_WM_LBUTTONDBLCLK()
    ON_NOTIFY_REFLECT(TBN_RESET, &CPlayerToolBar::OnTbnReset)
END_MESSAGE_MAP()

// CPlayerToolBar message handlers

void drawButtonBG(NMCUSTOMDRAW nmcd, COLORREF c)
{
    CDC dc;
    dc.Attach(nmcd.hdc);
    CRect br;
    br.CopyRect(&nmcd.rc);
    br.DeflateRect(0, 0, 1, 1); //we aren't offsetting button when pressed, so try to center better

    dc.FillSolidRect(br, c);

    CBrush fb;
    fb.CreateSolidBrush(CMPCTheme::PlayerButtonBorderColor);
    dc.FrameRect(br, &fb);
    fb.DeleteObject();
    dc.Detach();
}

void CPlayerToolBar::OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMTBCUSTOMDRAW pTBCD = reinterpret_cast<LPNMTBCUSTOMDRAW>(pNMHDR);
    LRESULT lr = CDRF_DODEFAULT;

    switch (pTBCD->nmcd.dwDrawStage) {
        case CDDS_PREERASE:
            m_volctrl.Invalidate();
            lr = CDRF_DODEFAULT;
            break;
        case CDDS_PREPAINT: {
            // paint the control background, this is needed for XP
            CDC dc;
            dc.Attach(pTBCD->nmcd.hdc);
            RECT r;
            GetClientRect(&r);
            if (AppIsThemeLoaded()) {
                dc.FillSolidRect(&r, CMPCTheme::PlayerBGColor);
            } else {
                dc.FillSolidRect(&r, ::GetSysColor(COLOR_BTNFACE));
            }
            dc.Detach();
        }
        lr |= CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
        break;
        case CDDS_ITEMPREPAINT:
            lr |= CDRF_NOTIFYPOSTPAINT | TBCDRF_NOOFFSET;
            {
                if (AppIsThemeLoaded()) {
                    lr |= TBCDRF_NOBACKGROUND;
                    
                    if (pTBCD->nmcd.uItemState & CDIS_CHECKED) {
                        drawButtonBG(pTBCD->nmcd, CMPCTheme::PlayerButtonCheckedColor);
                    } else if (pTBCD->nmcd.uItemState & CDIS_SELECTED) { //pressed
                        drawButtonBG(pTBCD->nmcd, CMPCTheme::PlayerButtonClickedColor);
                    } else if (pTBCD->nmcd.uItemState & CDIS_HOT) {
                        drawButtonBG(pTBCD->nmcd, CMPCTheme::PlayerButtonHotColor);
                    }
                }
            }
            break;
        case CDDS_ITEMPOSTPAINT:
            // paint over the duplicated separator
            CDC dc;
            dc.Attach(pTBCD->nmcd.hdc);
            RECT r;
            GetItemRect(dummySeparatorIndex, &r);
            if (AppIsThemeLoaded()) {
                dc.FillSolidRect(&r, CMPCTheme::PlayerBGColor);
            } else {
                dc.FillSolidRect(&r, GetSysColor(COLOR_BTNFACE));
            }
            dc.Detach();
            lr |= CDRF_SKIPDEFAULT;
            break;
    }

    *pResult = lr;
}

void CPlayerToolBar::OnSize(UINT nType, int cx, int cy)
{
    __super::OnSize(nType, cx, cy);

    ArrangeControls();
}

void CPlayerToolBar::OnInitialUpdate()
{
    //ArrangeControls();
}

BOOL CPlayerToolBar::OnVolumeMute(UINT nID)
{
    SetMute(!IsMuted());
    return FALSE;
}

void CPlayerToolBar::OnUpdateVolumeMute(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(true);
    pCmdUI->SetCheck(IsMuted());
}

//note, this differs from CMainFrame::OnUpdateViewFullscreen in order to avoid "checking" the button state
void CPlayerToolBar::OnUpdateFullscreen(CCmdUI* pCmdUI) {
    pCmdUI->Enable(true);
    SetFullscreen(m_pMainFrame->m_fFullScreen);
}

//note, this differs from CMainFrame::OnUpdateViewPlaylist in order to avoid "checking" the button state
void CPlayerToolBar::OnUpdatePlaylist(CCmdUI* pCmdUI) {
    pCmdUI->Enable(true);
    m_pMainFrame->UpdatePlaylistButton();
}

void CPlayerToolBar::OnUpdateShuffle(CCmdUI* pCmdUI) {
    pCmdUI->Enable(true);
    SetShuffle(AfxGetAppSettings().bShufflePlaylistItems);
}

void CPlayerToolBar::OnUpdateRepeat(CCmdUI* pCmdUI) {
    pCmdUI->Enable(true);
    SetRepeat(AfxGetAppSettings().fLoopForever);
}

void CPlayerToolBar::OnUpdateCustomAction(CCmdUI* pCmdUI) {
    const auto& s = AfxGetAppSettings();

    if (pCmdUI->m_nID == ID_CUSTOM_ACTION1 && s.nToolbarAction1
        || pCmdUI->m_nID == ID_CUSTOM_ACTION2 && s.nToolbarAction2
        || pCmdUI->m_nID == ID_CUSTOM_ACTION3 && s.nToolbarAction3
        || pCmdUI->m_nID == ID_CUSTOM_ACTION4 && s.nToolbarAction4)
    {
        pCmdUI->Enable(true);
    }
}

bool CPlayerToolBar::CmdIsMenu(int cmd) {
    return cmd == ID_MENU_FILTERS
        || cmd == ID_MENU_PLAYER_LONG
        || cmd == ID_MENU_PLAYER_SHORT
        ;
}

UINT CPlayerToolBar::CustomToCMD(UINT nID, int mouseButtonSource) {
    bool left = (mouseButtonSource == VK_LBUTTON);
    const auto& s = AfxGetAppSettings();
    UINT cmd = 0;
    switch (nID) {
    case ID_CUSTOM_ACTION1:
        cmd = left ? s.nToolbarAction1 : s.nToolbarRightAction1;
        break;
    case ID_CUSTOM_ACTION2:
        cmd = left ? s.nToolbarAction2 : s.nToolbarRightAction2;
        break;
    case ID_CUSTOM_ACTION3:
        cmd = left ? s.nToolbarAction3 : s.nToolbarRightAction3;
        break;
    case ID_CUSTOM_ACTION4:
        cmd = left ? s.nToolbarAction4 : s.nToolbarRightAction4;
        break;
    }
    return cmd;
}

BOOL CPlayerToolBar::OnMenuButton(UINT nID) {
    DoCustomContextMenu(nID, nID, leftButtonIndex, VK_LBUTTON);
    return TRUE;
}

BOOL CPlayerToolBar::OnCustomAction(UINT nID) {
    UINT cmd = CustomToCMD(nID, VK_LBUTTON);
    if (CmdIsMenu(cmd)) {
        DoCustomContextMenu(nID, cmd, leftButtonIndex, VK_LBUTTON);
    } else {
        m_pMainFrame->PostMessage(WM_COMMAND, cmd);
    }

    return TRUE;
}

BOOL CPlayerToolBar::OnVolumeUp(UINT nID)
{
    m_volctrl.IncreaseVolume();
    return FALSE;
}

BOOL CPlayerToolBar::OnVolumeDown(UINT nID)
{
    m_volctrl.DecreaseVolume();
    return FALSE;
}

BOOL CPlayerToolBar::OnFullscreenButton(UINT nID) {
    m_pMainFrame->OnViewFullscreen();
    return FALSE;
}

BOOL CPlayerToolBar::OnPlaylistButton(UINT nID) {
    m_pMainFrame->OnViewPlaylist();
    return FALSE;
}

BOOL CPlayerToolBar::OnShuffleButton(UINT nID) {
    m_pMainFrame->OnPlaylistToggleShuffle();
    return FALSE;
}

BOOL CPlayerToolBar::OnRepeatButton(UINT nID) {
    m_pMainFrame->OnPlayRepeatForever();
    return FALSE;
}

void CPlayerToolBar::OnNcPaint() // when using XP styles the NC area isn't drawn for our toolbar...
{
    CRect wr, cr;

    CWindowDC dc(this);
    GetClientRect(&cr);
    ClientToScreen(&cr);
    GetWindowRect(&wr);
    cr.OffsetRect(-wr.left, -wr.top);
    wr.OffsetRect(-wr.left, -wr.top);
    dc.ExcludeClipRect(&cr);

    if (AppIsThemeLoaded()) {
        dc.FillSolidRect(wr, CMPCTheme::PlayerBGColor);
    } else {
        dc.FillSolidRect(wr, ::GetSysColor(COLOR_BTNFACE));
    }

    // Do not call CToolBar::OnNcPaint() for painting messages

    // Invalidate window to force repaint the expanded separator
    Invalidate(FALSE);
}

BOOL CPlayerToolBar::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
    BOOL ret = FALSE;
    if (nHitTest == HTCLIENT) {
        CPoint point;
        VERIFY(GetCursorPos(&point));
        ScreenToClient(&point);

        int i = getHitButtonIdx(point);
        if (i >= 0 && !(GetButtonStyle(i) & (TBBS_SEPARATOR | TBBS_DISABLED))) {
            ::SetCursor(AfxGetApp()->LoadStandardCursor(IDC_HAND));
            ret = TRUE;
        }
    }
    return ret ? ret : __super::OnSetCursor(pWnd, nHitTest, message);
}

void CPlayerToolBar::OnLButtonDown(UINT nFlags, CPoint point)
{
    int i = getHitButtonIdx(point);

    bool isShift = (MK_SHIFT & nFlags);

    if (isShift) {
        currentlyDraggingButton = i;
    }

    DWORD ignoreButtons = TBBS_SEPARATOR | (isShift ? 0 : TBBS_DISABLED); //if shift is pressed, allow dragging

    if (!m_pMainFrame->m_fFullScreen && (i < 0 || (GetButtonStyle(i) & ignoreButtons))) {
        ClientToScreen(&point);
        m_pMainFrame->PostMessage(WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(point.x, point.y));
    } else {
        leftButtonIndex = i;
        __super::OnLButtonDown(nFlags, point);
    }
}

void CPlayerToolBar::OnRButtonDown(UINT nFlags, CPoint point) {
    int i = getHitButtonIdx(point);

    if (!m_pMainFrame->m_fFullScreen && (i < 0 || (GetButtonStyle(i) & (TBBS_SEPARATOR | TBBS_DISABLED)))) {
        ClientToScreen(&point);
        m_pMainFrame->PostMessage(WM_NCRBUTTONDOWN, HTCAPTION, MAKELPARAM(point.x, point.y));
    } else {
        rightButtonIndex = i;
        __super::OnRButtonDown(nFlags, point);
    }
}

int CPlayerToolBar::getHitButtonIdx(CPoint point)
{
    int hit = -1; // -1 means not on any button hit
    CRect r;

    for (int i = 0, j = GetToolBarCtrl().GetButtonCount(); i < j; i++) {
        GetItemRect(i, r);

        if (r.PtInRect(point)) {
            hit = i;
            break;
        }
    }

    return hit;
}

BOOL CPlayerToolBar::OnToolTipNotify(UINT id, NMHDR* pNMHDR, LRESULT* pResult)
{
    TOOLTIPTEXT* pTTT = (TOOLTIPTEXT*)pNMHDR;

    int nID;
    if (pTTT->uFlags & TTF_IDISHWND) {
        nID = ::GetDlgCtrlID((HWND)pNMHDR->idFrom);
    } else {
        nID = (int)pNMHDR->idFrom;
    }
    const auto& s = AfxGetAppSettings();
    if (nID != ID_VOLUME_MUTE && s.CommandIDToWMCMD.count(nID) == 0) {
        if (supportedSvgButtons.count(nID) == 0 || !supportedSvgButtons[nID].strID) {
            return FALSE;
        }
    }
    CToolBarCtrl& tb = GetToolBarCtrl();

    TBBUTTONINFO bi;
    bi.cbSize = sizeof(bi);
    bi.dwMask = TBIF_IMAGE;
    if (-1 == tb.GetButtonInfo(nID, &bi)) { //should only fail if they choose not to show volume button
        return FALSE;
    }

    static CString strTipText;
    if (nID != ID_VOLUME_MUTE) { 
        if (s.CommandIDToWMCMD.count(nID) > 0) {
            strTipText.LoadStringW(s.CommandIDToWMCMD[nID]->dwname);
        } else if (supportedSvgButtons.count(nID) > 0) {
            if (bi.iImage == supportedSvgButtons[nID].svgIndex && supportedSvgButtons[nID].activeStrID) {
                strTipText.LoadStringW(supportedSvgButtons[nID].activeStrID);
            } else {
                strTipText.LoadStringW(supportedSvgButtons[nID].strID);
            }
        } else {
            return FALSE;
        }
    } else if (bi.iImage == VOLUMEBUTTON_SVG_INDEX) {
        strTipText.LoadStringW(ID_VOLUME_MUTE);
    } else if (bi.iImage == VOLUMEBUTTON_SVG_INDEX +1 ) {
        strTipText.LoadStringW(ID_VOLUME_MUTE_OFF);
    } else {
        return FALSE;
    }
    pTTT->lpszText = (LPWSTR)(LPCWSTR)strTipText;

    *pResult = 0;


    return TRUE;    // message was handled
}

void CPlayerToolBar::OnLButtonUp(UINT nFlags, CPoint point)
{
    //rare crash restoring focus after close
    int buttonId = getHitButtonIdx(point);
    bool doRestoreFocus = (buttonId < 0 || buttonId != leftButtonIndex || GetItemID(buttonId) != ID_FILE_EXIT);

    CToolBar::OnLButtonUp(nFlags, point);

    if (doRestoreFocus) {
        m_pMainFrame->RestoreFocus();
    }
}

void CPlayerToolBar::DoCustomContextMenu(int itemId, int cmd, int buttonId, int mouseButtonSource) {
    CToolBarCtrl& tb = GetToolBarCtrl();
    CRect r;
    GetItemRect(buttonId, r);
    ClientToScreen(r);

    tb.PressButton(itemId, TRUE);
    m_pMainFrame->ToolbarContextMenu(cmd, buttonId, r);
    tb.PressButton(itemId, FALSE);

    CPoint p;
    ::GetCursorPos(&p);

    //they clicked a mouse button to open the menu, so we want to swallow clicks on the same button
    if (PtInRect(&r, p)) {
        MSG msg;
        if (mouseButtonSource == VK_LBUTTON) {
            leftButtonIndex = -1;
            while (PeekMessageW(&msg, m_hWnd, WM_LBUTTONDOWN, WM_LBUTTONDOWN, PM_REMOVE) ||
                PeekMessageW(&msg, m_hWnd, WM_LBUTTONDBLCLK, WM_LBUTTONDBLCLK, PM_REMOVE));
        } else {
            rightButtonIndex = -1;
            while (PeekMessageW(&msg, m_hWnd, WM_RBUTTONDOWN, WM_RBUTTONDBLCLK, PM_REMOVE));
        }
    }
}

void CPlayerToolBar::OnRButtonUp(UINT nFlags, CPoint point) {
    CToolBar::OnRButtonUp(nFlags, point);

    const auto& s = AfxGetAppSettings();

    int buttonId = getHitButtonIdx(point);
    if (buttonId >= 0 && rightButtonIndex == buttonId) {
        int itemId = GetItemID(buttonId);

        UINT messageId = 0;

        switch (itemId) {
            case ID_PLAY_PLAY:
                messageId = ID_FILE_OPENMEDIA;
                break;
            case ID_PLAY_FRAMESTEP:
                messageId = ID_PLAY_FRAMESTEP_BACK;
                break;
            case ID_PLAY_STOP:
                messageId = ID_FILE_CLOSE_AND_RESTORE;
                break;
            case ID_NAVIGATE_SKIPFORWARD:
                messageId = ID_NAVIGATE_SKIPFORWARDFILE;
                break;
            case ID_NAVIGATE_SKIPBACK:
                messageId = ID_NAVIGATE_SKIPBACKFILE;
                break;
            case ID_VOLUME_MUTE:
                messageId = ID_STREAM_AUDIO_NEXT;
                break;
            case ID_CUSTOM_ACTION1:
            case ID_CUSTOM_ACTION2:
            case ID_CUSTOM_ACTION3:
            case ID_CUSTOM_ACTION4:
                messageId = CustomToCMD(itemId, VK_RBUTTON);
                break;
        }

        if (messageId > 0) {
            if (CmdIsMenu(messageId)) {
                DoCustomContextMenu(itemId, messageId, buttonId, VK_RBUTTON);
            } else {
                m_pMainFrame->PostMessage(WM_COMMAND, messageId);
            }
        }
    }
}


void CPlayerToolBar::OnTbnQueryDelete(NMHDR* pNMHDR, LRESULT* pResult) {
    LPNMTOOLBAR pNMTB = reinterpret_cast<LPNMTOOLBAR>(pNMHDR);
    if (pNMTB->iItem < 3 || pNMTB->iItem >= dummySeparatorIndex) {
        *pResult = FALSE;
        return;
    }
    *pResult = TRUE;
}


void CPlayerToolBar::OnTbnQueryInsert(NMHDR* pNMHDR, LRESULT* pResult) {
    LPNMTOOLBAR pNMTB = reinterpret_cast<LPNMTOOLBAR>(pNMHDR);

    bool preventSeparatorInsert = false;

    if (!toolbarAdjustActive) {
        int width = LOWORD(GetToolBarCtrl().GetButtonSize());

        CPoint p = mousePosition;
        p.x += width / 2;

        int testButton = GetToolBarCtrl().HitTest(&p);

        //according to https://learn.microsoft.com/en-us/windows/win32/Controls/tb-hittest
        //"The absolute value of the return value is the index of a separator item"
        //but emperical testing shows it is offset by 1
        if (testButton < 0) {
            testButton = -1 - testButton;
        }

        //undocumented toolbar behavior--if the "testpoint" is on the button to the right, a separator is created to the left
        preventSeparatorInsert = (currentlyDraggingButton + 1 == testButton);
    }

    //this code prevents inserting at the given point
    //first test is to prevent inserting buttons outside of the "dynamic" area
    //second test is to prevent the insertion of a separator directly to the left of the dragged button
    if (pNMTB->iItem < 3 || pNMTB->iItem >= volumeButtonIndex //do not allow moving between beginning and ending standard toolbar items
        || preventSeparatorInsert) 
    {
        *pResult = FALSE;
        return;
    }

    *pResult = TRUE;
}

void CPlayerToolBar::SaveToolbarState() {
    if (!toolbarAdjustActive) {
        auto& ctrl = GetToolBarCtrl();
        std::vector<int> buttons;
        for (int i = 0; i < ctrl.GetButtonCount(); i++) {
            TBBUTTON button;
            ctrl.GetButton(i, &button);
            buttons.push_back(button.idCommand);
        }
        AfxGetMyApp()->WriteProfileVectorInt(IDS_R_PLAYERTOOLBAR, L"ButtonSequence", buttons);
    }
}

void CPlayerToolBar::ToolbarChange() {
    //clear these to ensure states are updated for new or moved buttons
    lastFullscreen = std::nullopt;
    lastPlaylist = std::nullopt;
    lastShuffle = std::nullopt;
    lastRepeat = std::nullopt;

    SaveToolbarState();
    m_pMainFrame->RecalcLayout();
    ArrangeControls();

    OnUpdateCmdUI(m_pMainFrame, FALSE); //useful for making sure button states are updated while in the options dialog
    Invalidate();
}

void CPlayerToolBar::OnTbnToolbarChange(NMHDR* pNMHDR, LRESULT* pResult) {
    ToolbarChange();
    *pResult = 0;
}

void CPlayerToolBar::OnMouseMove(UINT nFlags, CPoint point) {
    mousePosition = point;
    CToolBar::OnMouseMove(nFlags, point);
}

LPCWSTR CPlayerToolBar::GetStringFromID(int idCommand) {
    return supportedSvgButtons[idCommand].text.GetString();
}

void CPlayerToolBar::OnTbnGetButtonInfo(NMHDR* pNMHDR, LRESULT* pResult) {
    LPNMTOOLBAR pNMTB = reinterpret_cast<LPNMTOOLBAR>(pNMHDR);
    if (pNMTB->iItem < supportedSvgButtonsSeq.size()) {
        int cmdid = supportedSvgButtonsSeq[pNMTB->iItem];
        TBBUTTON t = GetStandardButton(cmdid);
        t.iString = (INT_PTR)GetStringFromID(t.idCommand);
        pNMTB->tbButton = t;
        *pResult = TRUE;
        return;
    }
    *pResult = 0;
}


void CPlayerToolBar::OnTbnInitCustomize(NMHDR* pNMHDR, LRESULT* pResult) {
    *pResult = TBNRF_HIDEHELP;
}


void CPlayerToolBar::OnTbnBeginAdjust(NMHDR* pNMHDR, LRESULT* pResult) {
    toolbarAdjustActive = true;
    *pResult = 0;
}

void CPlayerToolBar::OnTbnEndAdjust(NMHDR* pNMHDR, LRESULT* pResult) {
    toolbarAdjustActive = false;
    SaveToolbarState();
    *pResult = 0;
}

void CPlayerToolBar::OnLButtonDblClk(UINT nFlags, CPoint point) {
//disabled to avoid the built-in customization dialog
}

void CPlayerToolBar::ToolBarReset() {
    CToolBarCtrl& tb = GetToolBarCtrl();

    int playState = tb.GetState(ID_PLAY_PLAY);
    int pauseState = tb.GetState(ID_PLAY_PAUSE);

    for (int i = tb.GetButtonCount() - 1; i >= 0; i--) {
        tb.DeleteButton(i);
    }
    PlaceButtons(false);

    //this is done to restore the states used for feature 'Hide play or pause button based on playback state'
    tb.SetState(ID_PLAY_PLAY, playState);
    tb.SetState(ID_PLAY_PAUSE, pauseState);

    ArrangeControls();
    Invalidate();
}

void CPlayerToolBar::OnTbnReset(NMHDR* pNMHDR, LRESULT* pResult) {
    ToolBarReset();
    *pResult = 0;
}
