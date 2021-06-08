/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2018 see Authors.txt
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
#include "MainFrm.h"
#include "mplayerc.h"

#include "GraphThread.h"
#include "FGFilterLAV.h"
#include "FGManager.h"
#include "FGManagerBDA.h"
#include "ShockwaveGraph.h"
#include "TextPassThruFilter.h"
#include "FakeFilterMapper2.h"

#include "FavoriteAddDlg.h"
#include "FavoriteOrganizeDlg.h"
#include "GoToDlg.h"
#include "MediaTypesDlg.h"
#include "OpenFileDlg.h"
#include "PnSPresetsDlg.h"
#include "SaveDlg.h"
#include "SaveImageDialog.h"
#include "SaveSubtitlesFileDialog.h"
#include "SaveThumbnailsDialog.h"
#include "OpenDirHelper.h"
#include "OpenDlg.h"
#include "TunerScanDlg.h"

#include "ComPropertySheet.h"
#include "PPageAccelTbl.h"
#include "PPageAudioSwitcher.h"
#include "PPageFileInfoSheet.h"
#include "PPageSheet.h"
#include "PPageSubStyle.h"
#include "PPageSubtitles.h"

#include "CoverArt.h"
#include "CrashReporter.h"
#include "KeyProvider.h"
#include "SkypeMoodMsgHandler.h"
#include "Translations.h"
#include "UpdateChecker.h"
#include "WebServer.h"
#include <ISOLang.h>
#include <PathUtils.h>

#include "../DeCSS/VobFile.h"
#include "../Subtitles/PGSSub.h"
#include "../Subtitles/RLECodedSubtitle.h"
#include "../Subtitles/RTS.h"
#include "../Subtitles/STS.h"
#include <SubRenderIntf.h>

#include "../filters/InternalPropertyPage.h"
#include "../filters/PinInfoWnd.h"
#include "../filters/renderer/SyncClock/SyncClock.h"
#include "../filters/transform/BufferFilter/BufferFilter.h"
#include "../filters/transform/VSFilter/IDirectVobSub.h"

#include <AllocatorCommon.h>
#include <NullRenderers.h>
#include <RARFileSource/RFS.h>
#include <SyncAllocatorPresenter.h>

#include "FullscreenWnd.h"
#include "Monitors.h"

#include <WinAPIUtils.h>
#include <WinapiFunc.h>
#include <moreuuids.h>

#include <IBitRateInfo.h>
#include <IChapterInfo.h>
#include <IPinHook.h>

#include <mvrInterfaces.h>

#include <Il21dec.h>
#include <dvdevcod.h>
#include <dvdmedia.h>
#include <strsafe.h>
#include <VersionHelpersInternal.h>

#include <initguid.h>
#include <qnetwork.h>

#include "YoutubeDL.h"
#include "CMPCThemeMenu.h"
#include "CMPCThemeDockBar.h"
#include "CMPCThemeMiniDockFrameWnd.h"

#include <dwmapi.h>
#undef SubclassWindow

// IID_IAMLine21Decoder
DECLARE_INTERFACE_IID_(IAMLine21Decoder_2, IAMLine21Decoder, "6E8D4A21-310C-11d0-B79A-00AA003767A7") {};

#define MIN_LOGO_WIDTH 304
#define MIN_LOGO_HEIGHT 171

#define PREV_CHAP_THRESHOLD 2

static UINT s_uTaskbarRestart = RegisterWindowMessage(_T("TaskbarCreated"));
static UINT WM_NOTIFYICON = RegisterWindowMessage(_T("MYWM_NOTIFYICON"));
static UINT s_uTBBC = RegisterWindowMessage(_T("TaskbarButtonCreated"));

CMainFrame::PlaybackRateMap CMainFrame::filePlaybackRates = {
    { ID_PLAY_PLAYBACKRATE_025,  .25f},
    { ID_PLAY_PLAYBACKRATE_050,  .50f},
    { ID_PLAY_PLAYBACKRATE_075,  .75f},
    { ID_PLAY_PLAYBACKRATE_090,  .90f},
    { ID_PLAY_PLAYBACKRATE_100, 1.00f},
    { ID_PLAY_PLAYBACKRATE_110, 1.10f},
    { ID_PLAY_PLAYBACKRATE_125, 1.25f},
    { ID_PLAY_PLAYBACKRATE_150, 1.50f},
    { ID_PLAY_PLAYBACKRATE_200, 2.00f},
    { ID_PLAY_PLAYBACKRATE_300, 3.00f},
    { ID_PLAY_PLAYBACKRATE_400, 4.00f},
    { ID_PLAY_PLAYBACKRATE_600, 6.00f},
    { ID_PLAY_PLAYBACKRATE_800, 8.00f},
};

CMainFrame::PlaybackRateMap CMainFrame::dvdPlaybackRates = {
    { ID_PLAY_PLAYBACKRATE_025,  .25f},
    { ID_PLAY_PLAYBACKRATE_050,  .50f},
    { ID_PLAY_PLAYBACKRATE_100, 1.00f},
    { ID_PLAY_PLAYBACKRATE_200, 2.00f},
    { ID_PLAY_PLAYBACKRATE_400, 4.00f},
    { ID_PLAY_PLAYBACKRATE_800, 8.00f},
};


#include "MediaInfo/MediaInfoDLL.h"
using namespace MediaInfoDLL;

class CSubClock : public CUnknown, public ISubClock
{
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv) {
        return
            QI(ISubClock)
            CUnknown::NonDelegatingQueryInterface(riid, ppv);
    }

    REFERENCE_TIME m_rt;

public:
    CSubClock() : CUnknown(NAME("CSubClock"), nullptr) {
        m_rt = 0;
    }

    DECLARE_IUNKNOWN;

    // ISubClock
    STDMETHODIMP SetTime(REFERENCE_TIME rt) {
        m_rt = rt;
        return S_OK;
    }
    STDMETHODIMP_(REFERENCE_TIME) GetTime() {
        return m_rt;
    }
};

/////////////////////////////////////////////////////////////////////////////
// CMainFrame

IMPLEMENT_DYNAMIC(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
    ON_WM_NCCREATE()
    ON_WM_CREATE()
    ON_WM_DESTROY()
    ON_WM_CLOSE()
    ON_WM_MEASUREITEM()

    ON_REGISTERED_MESSAGE(s_uTaskbarRestart, OnTaskBarRestart)
    ON_REGISTERED_MESSAGE(WM_NOTIFYICON, OnNotifyIcon)

    ON_REGISTERED_MESSAGE(s_uTBBC, OnTaskBarThumbnailsCreate)

    ON_REGISTERED_MESSAGE(SkypeMoodMsgHandler::uSkypeControlAPIAttach, OnSkypeAttach)

    ON_WM_SETFOCUS()
    ON_WM_GETMINMAXINFO()
    ON_WM_MOVE()
    ON_WM_ENTERSIZEMOVE()
    ON_WM_MOVING()
    ON_WM_SIZE()
    ON_WM_SIZING()
    ON_WM_EXITSIZEMOVE()
    ON_MESSAGE_VOID(WM_DISPLAYCHANGE, OnDisplayChange)
    ON_WM_WINDOWPOSCHANGING()

    ON_MESSAGE(WM_DPICHANGED, OnDpiChanged)

    ON_WM_SYSCOMMAND()
    ON_WM_ACTIVATEAPP()
    ON_MESSAGE(WM_APPCOMMAND, OnAppCommand)
    ON_WM_INPUT()
    ON_MESSAGE(WM_HOTKEY, OnHotKey)

    ON_WM_TIMER()

    ON_MESSAGE(WM_GRAPHNOTIFY, OnGraphNotify)
    ON_MESSAGE(WM_RESET_DEVICE, OnResetDevice)
    ON_MESSAGE(WM_REARRANGERENDERLESS, OnRepaintRenderLess)

    ON_MESSAGE_VOID(WM_SAVESETTINGS, SaveAppSettings)

    ON_WM_NCHITTEST()

    ON_WM_HSCROLL()

    ON_WM_INITMENU()
    ON_WM_INITMENUPOPUP()
    ON_WM_UNINITMENUPOPUP()

    ON_WM_ENTERMENULOOP()

    ON_WM_QUERYENDSESSION()
    ON_WM_ENDSESSION()

    ON_COMMAND(ID_MENU_PLAYER_SHORT, OnMenuPlayerShort)
    ON_COMMAND(ID_MENU_PLAYER_LONG, OnMenuPlayerLong)
    ON_COMMAND(ID_MENU_FILTERS, OnMenuFilters)

    ON_UPDATE_COMMAND_UI(IDC_PLAYERSTATUS, OnUpdatePlayerStatus)

    ON_MESSAGE(WM_POSTOPEN, OnFilePostOpenmedia)
    ON_MESSAGE(WM_OPENFAILED, OnOpenMediaFailed)
    ON_MESSAGE(WM_DVB_EIT_DATA_READY, OnCurrentChannelInfoUpdated)

    ON_COMMAND(ID_BOSS, OnBossKey)

    ON_COMMAND_RANGE(ID_STREAM_AUDIO_NEXT, ID_STREAM_AUDIO_PREV, OnStreamAudio)
    ON_COMMAND_RANGE(ID_STREAM_SUB_NEXT, ID_STREAM_SUB_PREV, OnStreamSub)
    ON_COMMAND(ID_STREAM_SUB_ONOFF, OnStreamSubOnOff)
    ON_COMMAND_RANGE(ID_DVD_ANGLE_NEXT, ID_DVD_ANGLE_PREV, OnDvdAngle)
    ON_COMMAND_RANGE(ID_DVD_AUDIO_NEXT, ID_DVD_AUDIO_PREV, OnDvdAudio)
    ON_COMMAND_RANGE(ID_DVD_SUB_NEXT, ID_DVD_SUB_PREV, OnDvdSub)
    ON_COMMAND(ID_DVD_SUB_ONOFF, OnDvdSubOnOff)

    ON_COMMAND(ID_FILE_OPENQUICK, OnFileOpenQuick)
    ON_UPDATE_COMMAND_UI(ID_FILE_OPENMEDIA, OnUpdateFileOpen)
    ON_COMMAND(ID_FILE_OPENMEDIA, OnFileOpenmedia)
    ON_UPDATE_COMMAND_UI(ID_FILE_OPENMEDIA, OnUpdateFileOpen)
    ON_WM_COPYDATA()
    ON_COMMAND(ID_FILE_OPENDVDBD, OnFileOpendvd)
    ON_UPDATE_COMMAND_UI(ID_FILE_OPENDVDBD, OnUpdateFileOpen)
    ON_COMMAND(ID_FILE_OPENDEVICE, OnFileOpendevice)
    ON_UPDATE_COMMAND_UI(ID_FILE_OPENDEVICE, OnUpdateFileOpen)
    ON_COMMAND_RANGE(ID_FILE_OPEN_OPTICAL_DISK_START, ID_FILE_OPEN_OPTICAL_DISK_END, OnFileOpenOpticalDisk)
    ON_UPDATE_COMMAND_UI_RANGE(ID_FILE_OPEN_OPTICAL_DISK_START, ID_FILE_OPEN_OPTICAL_DISK_END, OnUpdateFileOpen)
    ON_COMMAND(ID_FILE_REOPEN, OnFileReopen)
    ON_COMMAND(ID_FILE_RECYCLE, OnFileRecycle)
    ON_COMMAND(ID_FILE_SAVE_COPY, OnFileSaveAs)
    ON_UPDATE_COMMAND_UI(ID_FILE_SAVE_COPY, OnUpdateFileSaveAs)
    ON_COMMAND(ID_FILE_SAVE_IMAGE, OnFileSaveImage)
    ON_UPDATE_COMMAND_UI(ID_FILE_SAVE_IMAGE, OnUpdateFileSaveImage)
    ON_COMMAND(ID_FILE_SAVE_IMAGE_AUTO, OnFileSaveImageAuto)
    ON_UPDATE_COMMAND_UI(ID_FILE_SAVE_IMAGE_AUTO, OnUpdateFileSaveImage)
    ON_COMMAND(ID_FILE_SAVE_THUMBNAILS, OnFileSaveThumbnails)
    ON_UPDATE_COMMAND_UI(ID_FILE_SAVE_THUMBNAILS, OnUpdateFileSaveThumbnails)
    ON_COMMAND(ID_FILE_SUBTITLES_LOAD, OnFileSubtitlesLoad)
    ON_UPDATE_COMMAND_UI(ID_FILE_SUBTITLES_LOAD, OnUpdateFileSubtitlesLoad)
    ON_COMMAND(ID_FILE_SUBTITLES_SAVE, OnFileSubtitlesSave)
    ON_UPDATE_COMMAND_UI(ID_FILE_SUBTITLES_SAVE, OnUpdateFileSubtitlesSave)
    ON_COMMAND(ID_FILE_SUBTITLES_UPLOAD, OnFileSubtitlesUpload)
    ON_UPDATE_COMMAND_UI(ID_FILE_SUBTITLES_UPLOAD, OnUpdateFileSubtitlesUpload)
    ON_COMMAND(ID_FILE_SUBTITLES_DOWNLOAD, OnFileSubtitlesDownload)
    ON_UPDATE_COMMAND_UI(ID_FILE_SUBTITLES_DOWNLOAD, OnUpdateFileSubtitlesDownload)
    ON_COMMAND(ID_FILE_PROPERTIES, OnFileProperties)
    ON_UPDATE_COMMAND_UI(ID_FILE_PROPERTIES, OnUpdateFileProperties)
    ON_COMMAND(ID_FILE_OPEN_LOCATION, OnFileOpenLocation)
    ON_UPDATE_COMMAND_UI(ID_FILE_OPEN_LOCATION, OnUpdateFileProperties)
    ON_COMMAND(ID_FILE_CLOSE_AND_RESTORE, OnFileCloseAndRestore)
    ON_UPDATE_COMMAND_UI(ID_FILE_CLOSE_AND_RESTORE, OnUpdateFileClose)
    ON_COMMAND(ID_FILE_CLOSEMEDIA, OnFileCloseMedia)
    ON_UPDATE_COMMAND_UI(ID_FILE_CLOSEMEDIA, OnUpdateFileClose)

    ON_COMMAND(ID_VIEW_CAPTIONMENU, OnViewCaptionmenu)
    ON_UPDATE_COMMAND_UI(ID_VIEW_CAPTIONMENU, OnUpdateViewCaptionmenu)
    ON_COMMAND_RANGE(ID_VIEW_SEEKER, ID_VIEW_STATUS, OnViewControlBar)
    ON_UPDATE_COMMAND_UI_RANGE(ID_VIEW_SEEKER, ID_VIEW_STATUS, OnUpdateViewControlBar)
    ON_COMMAND(ID_VIEW_SUBRESYNC, OnViewSubresync)
    ON_UPDATE_COMMAND_UI(ID_VIEW_SUBRESYNC, OnUpdateViewSubresync)
    ON_COMMAND(ID_VIEW_PLAYLIST, OnViewPlaylist)
    ON_UPDATE_COMMAND_UI(ID_VIEW_PLAYLIST, OnUpdateViewPlaylist)
    ON_COMMAND(ID_VIEW_EDITLISTEDITOR, OnViewEditListEditor)
    ON_COMMAND(ID_EDL_IN, OnEDLIn)
    ON_UPDATE_COMMAND_UI(ID_EDL_IN, OnUpdateEDLIn)
    ON_COMMAND(ID_EDL_OUT, OnEDLOut)
    ON_UPDATE_COMMAND_UI(ID_EDL_OUT, OnUpdateEDLOut)
    ON_COMMAND(ID_EDL_NEWCLIP, OnEDLNewClip)
    ON_UPDATE_COMMAND_UI(ID_EDL_NEWCLIP, OnUpdateEDLNewClip)
    ON_COMMAND(ID_EDL_SAVE, OnEDLSave)
    ON_UPDATE_COMMAND_UI(ID_EDL_SAVE, OnUpdateEDLSave)
    ON_COMMAND(ID_VIEW_CAPTURE, OnViewCapture)
    ON_UPDATE_COMMAND_UI(ID_VIEW_CAPTURE, OnUpdateViewCapture)
    ON_COMMAND(ID_VIEW_DEBUGSHADERS, OnViewDebugShaders)
    ON_UPDATE_COMMAND_UI(ID_VIEW_DEBUGSHADERS, OnUpdateViewDebugShaders)
    ON_COMMAND(ID_VIEW_MPCTHEME, OnViewMPCTheme)
    ON_UPDATE_COMMAND_UI(ID_VIEW_MPCTHEME, OnUpdateViewMPCTheme)
    ON_COMMAND(ID_VIEW_PRESETS_MINIMAL, OnViewMinimal)
    ON_UPDATE_COMMAND_UI(ID_VIEW_PRESETS_MINIMAL, OnUpdateViewMinimal)
    ON_COMMAND(ID_VIEW_PRESETS_COMPACT, OnViewCompact)
    ON_UPDATE_COMMAND_UI(ID_VIEW_PRESETS_COMPACT, OnUpdateViewCompact)
    ON_COMMAND(ID_VIEW_PRESETS_NORMAL, OnViewNormal)
    ON_UPDATE_COMMAND_UI(ID_VIEW_PRESETS_NORMAL, OnUpdateViewNormal)
    ON_COMMAND(ID_VIEW_FULLSCREEN, OnViewFullscreen)
    ON_COMMAND(ID_VIEW_FULLSCREEN_SECONDARY, OnViewFullscreenSecondary)
    ON_UPDATE_COMMAND_UI(ID_VIEW_FULLSCREEN, OnUpdateViewFullscreen)
    ON_COMMAND_RANGE(ID_VIEW_ZOOM_50, ID_VIEW_ZOOM_200, OnViewZoom)
    ON_UPDATE_COMMAND_UI_RANGE(ID_VIEW_ZOOM_50, ID_VIEW_ZOOM_200, OnUpdateViewZoom)
    ON_COMMAND_RANGE(ID_VIEW_ZOOM_25, ID_VIEW_ZOOM_25, OnViewZoom)
    ON_UPDATE_COMMAND_UI_RANGE(ID_VIEW_ZOOM_25, ID_VIEW_ZOOM_25, OnUpdateViewZoom)
    ON_COMMAND(ID_VIEW_ZOOM_AUTOFIT, OnViewZoomAutoFit)
    ON_UPDATE_COMMAND_UI(ID_VIEW_ZOOM_AUTOFIT, OnUpdateViewZoom)
    ON_COMMAND(ID_VIEW_ZOOM_AUTOFIT_LARGER, OnViewZoomAutoFitLarger)
    ON_UPDATE_COMMAND_UI(ID_VIEW_ZOOM_AUTOFIT_LARGER, OnUpdateViewZoom)
    ON_COMMAND_RANGE(ID_VIEW_VF_HALF, ID_VIEW_VF_ZOOM2, OnViewDefaultVideoFrame)
    ON_UPDATE_COMMAND_UI_RANGE(ID_VIEW_VF_HALF, ID_VIEW_VF_ZOOM2, OnUpdateViewDefaultVideoFrame)
    ON_COMMAND(ID_VIEW_VF_SWITCHZOOM, OnViewSwitchVideoFrame)
    ON_COMMAND(ID_VIEW_VF_COMPMONDESKARDIFF, OnViewCompMonDeskARDiff)
    ON_UPDATE_COMMAND_UI(ID_VIEW_VF_COMPMONDESKARDIFF, OnUpdateViewCompMonDeskARDiff)
    ON_COMMAND_RANGE(ID_VIEW_RESET, ID_PANSCAN_CENTER, OnViewPanNScan)
    ON_UPDATE_COMMAND_UI_RANGE(ID_VIEW_RESET, ID_PANSCAN_CENTER, OnUpdateViewPanNScan)
    ON_COMMAND_RANGE(ID_PANNSCAN_PRESETS_START, ID_PANNSCAN_PRESETS_END, OnViewPanNScanPresets)
    ON_UPDATE_COMMAND_UI_RANGE(ID_PANNSCAN_PRESETS_START, ID_PANNSCAN_PRESETS_END, OnUpdateViewPanNScanPresets)
    ON_COMMAND_RANGE(ID_PANSCAN_ROTATEXP, ID_PANSCAN_ROTATEZM, OnViewRotate)
    ON_UPDATE_COMMAND_UI_RANGE(ID_PANSCAN_ROTATEXP, ID_PANSCAN_ROTATEZM, OnUpdateViewRotate)
    ON_COMMAND_RANGE(ID_PANSCAN_ROTATEZ270, ID_PANSCAN_ROTATEZ270, OnViewRotate)
    ON_UPDATE_COMMAND_UI_RANGE(ID_PANSCAN_ROTATEZ270, ID_PANSCAN_ROTATEZ270, OnUpdateViewRotate)
    ON_COMMAND_RANGE(ID_ASPECTRATIO_START, ID_ASPECTRATIO_END, OnViewAspectRatio)
    ON_UPDATE_COMMAND_UI_RANGE(ID_ASPECTRATIO_START, ID_ASPECTRATIO_END, OnUpdateViewAspectRatio)
    ON_COMMAND(ID_ASPECTRATIO_NEXT, OnViewAspectRatioNext)
    ON_COMMAND_RANGE(ID_ONTOP_DEFAULT, ID_ONTOP_WHILEPLAYINGVIDEO, OnViewOntop)
    ON_UPDATE_COMMAND_UI_RANGE(ID_ONTOP_DEFAULT, ID_ONTOP_WHILEPLAYINGVIDEO, OnUpdateViewOntop)
    ON_COMMAND(ID_VIEW_OPTIONS, OnViewOptions)

    // Casimir666
    ON_UPDATE_COMMAND_UI(ID_VIEW_TEARING_TEST, OnUpdateViewTearingTest)
    ON_COMMAND(ID_VIEW_TEARING_TEST, OnViewTearingTest)
    ON_UPDATE_COMMAND_UI(ID_VIEW_DISPLAY_RENDERER_STATS, OnUpdateViewDisplayRendererStats)
    ON_COMMAND(ID_VIEW_RESET_RENDERER_STATS, OnViewResetRendererStats)
    ON_COMMAND(ID_VIEW_DISPLAY_RENDERER_STATS, OnViewDisplayRendererStats)
    ON_UPDATE_COMMAND_UI(ID_VIEW_FULLSCREENGUISUPPORT, OnUpdateViewFullscreenGUISupport)
    ON_UPDATE_COMMAND_UI(ID_VIEW_HIGHCOLORRESOLUTION, OnUpdateViewHighColorResolution)
    ON_UPDATE_COMMAND_UI(ID_VIEW_FORCEINPUTHIGHCOLORRESOLUTION, OnUpdateViewForceInputHighColorResolution)
    ON_UPDATE_COMMAND_UI(ID_VIEW_FULLFLOATINGPOINTPROCESSING, OnUpdateViewFullFloatingPointProcessing)
    ON_UPDATE_COMMAND_UI(ID_VIEW_HALFFLOATINGPOINTPROCESSING, OnUpdateViewHalfFloatingPointProcessing)
    ON_UPDATE_COMMAND_UI(ID_VIEW_ENABLEFRAMETIMECORRECTION, OnUpdateViewEnableFrameTimeCorrection)
    ON_UPDATE_COMMAND_UI(ID_VIEW_VSYNC, OnUpdateViewVSync)
    ON_UPDATE_COMMAND_UI(ID_VIEW_VSYNCOFFSET, OnUpdateViewVSyncOffset)
    ON_UPDATE_COMMAND_UI(ID_VIEW_VSYNCACCURATE, OnUpdateViewVSyncAccurate)

    ON_UPDATE_COMMAND_UI(ID_VIEW_SYNCHRONIZEVIDEO, OnUpdateViewSynchronizeVideo)
    ON_UPDATE_COMMAND_UI(ID_VIEW_SYNCHRONIZEDISPLAY, OnUpdateViewSynchronizeDisplay)
    ON_UPDATE_COMMAND_UI(ID_VIEW_SYNCHRONIZENEAREST, OnUpdateViewSynchronizeNearest)

    ON_UPDATE_COMMAND_UI(ID_VIEW_CM_ENABLE, OnUpdateViewColorManagementEnable)
    ON_UPDATE_COMMAND_UI(ID_VIEW_CM_INPUT_AUTO, OnUpdateViewColorManagementInput)
    ON_UPDATE_COMMAND_UI(ID_VIEW_CM_INPUT_HDTV, OnUpdateViewColorManagementInput)
    ON_UPDATE_COMMAND_UI(ID_VIEW_CM_INPUT_SDTV_NTSC, OnUpdateViewColorManagementInput)
    ON_UPDATE_COMMAND_UI(ID_VIEW_CM_INPUT_SDTV_PAL, OnUpdateViewColorManagementInput)
    ON_UPDATE_COMMAND_UI(ID_VIEW_CM_AMBIENTLIGHT_BRIGHT, OnUpdateViewColorManagementAmbientLight)
    ON_UPDATE_COMMAND_UI(ID_VIEW_CM_AMBIENTLIGHT_DIM, OnUpdateViewColorManagementAmbientLight)
    ON_UPDATE_COMMAND_UI(ID_VIEW_CM_AMBIENTLIGHT_DARK, OnUpdateViewColorManagementAmbientLight)
    ON_UPDATE_COMMAND_UI(ID_VIEW_CM_INTENT_PERCEPTUAL, OnUpdateViewColorManagementIntent)
    ON_UPDATE_COMMAND_UI(ID_VIEW_CM_INTENT_RELATIVECOLORIMETRIC, OnUpdateViewColorManagementIntent)
    ON_UPDATE_COMMAND_UI(ID_VIEW_CM_INTENT_SATURATION, OnUpdateViewColorManagementIntent)
    ON_UPDATE_COMMAND_UI(ID_VIEW_CM_INTENT_ABSOLUTECOLORIMETRIC, OnUpdateViewColorManagementIntent)

    ON_UPDATE_COMMAND_UI(ID_VIEW_EVROUTPUTRANGE_0_255, OnUpdateViewEVROutputRange)
    ON_UPDATE_COMMAND_UI(ID_VIEW_EVROUTPUTRANGE_16_235, OnUpdateViewEVROutputRange)

    ON_UPDATE_COMMAND_UI(ID_VIEW_FLUSHGPU_BEFOREVSYNC, OnUpdateViewFlushGPU)
    ON_UPDATE_COMMAND_UI(ID_VIEW_FLUSHGPU_AFTERPRESENT, OnUpdateViewFlushGPU)
    ON_UPDATE_COMMAND_UI(ID_VIEW_FLUSHGPU_WAIT, OnUpdateViewFlushGPU)

    ON_UPDATE_COMMAND_UI(ID_VIEW_D3DFULLSCREEN, OnUpdateViewD3DFullscreen)
    ON_UPDATE_COMMAND_UI(ID_VIEW_DISABLEDESKTOPCOMPOSITION, OnUpdateViewDisableDesktopComposition)
    ON_UPDATE_COMMAND_UI(ID_VIEW_ALTERNATIVEVSYNC, OnUpdateViewAlternativeVSync)

    ON_UPDATE_COMMAND_UI(ID_VIEW_VSYNCOFFSET_INCREASE, OnUpdateViewVSyncOffsetIncrease)
    ON_UPDATE_COMMAND_UI(ID_VIEW_VSYNCOFFSET_DECREASE, OnUpdateViewVSyncOffsetDecrease)
    ON_COMMAND(ID_VIEW_FULLSCREENGUISUPPORT, OnViewFullscreenGUISupport)
    ON_COMMAND(ID_VIEW_HIGHCOLORRESOLUTION, OnViewHighColorResolution)
    ON_COMMAND(ID_VIEW_FORCEINPUTHIGHCOLORRESOLUTION, OnViewForceInputHighColorResolution)
    ON_COMMAND(ID_VIEW_FULLFLOATINGPOINTPROCESSING, OnViewFullFloatingPointProcessing)
    ON_COMMAND(ID_VIEW_HALFFLOATINGPOINTPROCESSING, OnViewHalfFloatingPointProcessing)
    ON_COMMAND(ID_VIEW_ENABLEFRAMETIMECORRECTION, OnViewEnableFrameTimeCorrection)
    ON_COMMAND(ID_VIEW_VSYNC, OnViewVSync)
    ON_COMMAND(ID_VIEW_VSYNCACCURATE, OnViewVSyncAccurate)

    ON_COMMAND(ID_VIEW_SYNCHRONIZEVIDEO, OnViewSynchronizeVideo)
    ON_COMMAND(ID_VIEW_SYNCHRONIZEDISPLAY, OnViewSynchronizeDisplay)
    ON_COMMAND(ID_VIEW_SYNCHRONIZENEAREST, OnViewSynchronizeNearest)

    ON_COMMAND(ID_VIEW_CM_ENABLE, OnViewColorManagementEnable)
    ON_COMMAND(ID_VIEW_CM_INPUT_AUTO, OnViewColorManagementInputAuto)
    ON_COMMAND(ID_VIEW_CM_INPUT_HDTV, OnViewColorManagementInputHDTV)
    ON_COMMAND(ID_VIEW_CM_INPUT_SDTV_NTSC, OnViewColorManagementInputSDTV_NTSC)
    ON_COMMAND(ID_VIEW_CM_INPUT_SDTV_PAL, OnViewColorManagementInputSDTV_PAL)
    ON_COMMAND(ID_VIEW_CM_AMBIENTLIGHT_BRIGHT, OnViewColorManagementAmbientLightBright)
    ON_COMMAND(ID_VIEW_CM_AMBIENTLIGHT_DIM, OnViewColorManagementAmbientLightDim)
    ON_COMMAND(ID_VIEW_CM_AMBIENTLIGHT_DARK, OnViewColorManagementAmbientLightDark)
    ON_COMMAND(ID_VIEW_CM_INTENT_PERCEPTUAL, OnViewColorManagementIntentPerceptual)
    ON_COMMAND(ID_VIEW_CM_INTENT_RELATIVECOLORIMETRIC, OnViewColorManagementIntentRelativeColorimetric)
    ON_COMMAND(ID_VIEW_CM_INTENT_SATURATION, OnViewColorManagementIntentSaturation)
    ON_COMMAND(ID_VIEW_CM_INTENT_ABSOLUTECOLORIMETRIC, OnViewColorManagementIntentAbsoluteColorimetric)

    ON_COMMAND(ID_VIEW_EVROUTPUTRANGE_0_255, OnViewEVROutputRange_0_255)
    ON_COMMAND(ID_VIEW_EVROUTPUTRANGE_16_235, OnViewEVROutputRange_16_235)

    ON_COMMAND(ID_VIEW_FLUSHGPU_BEFOREVSYNC, OnViewFlushGPUBeforeVSync)
    ON_COMMAND(ID_VIEW_FLUSHGPU_AFTERPRESENT, OnViewFlushGPUAfterVSync)
    ON_COMMAND(ID_VIEW_FLUSHGPU_WAIT, OnViewFlushGPUWait)

    ON_COMMAND(ID_VIEW_D3DFULLSCREEN, OnViewD3DFullScreen)
    ON_COMMAND(ID_VIEW_DISABLEDESKTOPCOMPOSITION, OnViewDisableDesktopComposition)
    ON_COMMAND(ID_VIEW_ALTERNATIVEVSYNC, OnViewAlternativeVSync)
    ON_COMMAND(ID_VIEW_RESET_DEFAULT, OnViewResetDefault)
    ON_COMMAND(ID_VIEW_RESET_OPTIMAL, OnViewResetOptimal)

    ON_COMMAND(ID_VIEW_VSYNCOFFSET_INCREASE, OnViewVSyncOffsetIncrease)
    ON_COMMAND(ID_VIEW_VSYNCOFFSET_DECREASE, OnViewVSyncOffsetDecrease)
	ON_UPDATE_COMMAND_UI(ID_PRESIZE_SHADERS_TOGGLE, OnUpdateShaderToggle1)
	ON_COMMAND(ID_PRESIZE_SHADERS_TOGGLE, OnShaderToggle1)
	ON_UPDATE_COMMAND_UI(ID_POSTSIZE_SHADERS_TOGGLE, OnUpdateShaderToggle2)
	ON_COMMAND(ID_POSTSIZE_SHADERS_TOGGLE, OnShaderToggle2)
    ON_UPDATE_COMMAND_UI(ID_VIEW_OSD_DISPLAY_TIME, OnUpdateViewOSDDisplayTime)
    ON_COMMAND(ID_VIEW_OSD_DISPLAY_TIME, OnViewOSDDisplayTime)
    ON_UPDATE_COMMAND_UI(ID_VIEW_OSD_SHOW_FILENAME, OnUpdateViewOSDShowFileName)
    ON_COMMAND(ID_VIEW_OSD_SHOW_FILENAME, OnViewOSDShowFileName)
    ON_COMMAND(ID_D3DFULLSCREEN_TOGGLE, OnD3DFullscreenToggle)
    ON_COMMAND_RANGE(ID_GOTO_PREV_SUB, ID_GOTO_NEXT_SUB, OnGotoSubtitle)
    ON_COMMAND_RANGE(ID_SHIFT_SUB_DOWN, ID_SHIFT_SUB_UP, OnShiftSubtitle)
    ON_COMMAND_RANGE(ID_SUB_DELAY_DOWN, ID_SUB_DELAY_UP, OnSubtitleDelay)

    ON_COMMAND(ID_PLAY_PLAY, OnPlayPlay)
    ON_COMMAND(ID_PLAY_PAUSE, OnPlayPause)
    ON_COMMAND(ID_PLAY_PLAYPAUSE, OnPlayPlaypause)
    ON_COMMAND(ID_PLAY_STOP, OnPlayStop)
    ON_UPDATE_COMMAND_UI(ID_PLAY_PLAY, OnUpdatePlayPauseStop)
    ON_UPDATE_COMMAND_UI(ID_PLAY_PAUSE, OnUpdatePlayPauseStop)
    ON_UPDATE_COMMAND_UI(ID_PLAY_PLAYPAUSE, OnUpdatePlayPauseStop)
    ON_UPDATE_COMMAND_UI(ID_PLAY_STOP, OnUpdatePlayPauseStop)
    ON_COMMAND_RANGE(ID_PLAY_FRAMESTEP, ID_PLAY_FRAMESTEPCANCEL, OnPlayFramestep)
    ON_UPDATE_COMMAND_UI_RANGE(ID_PLAY_FRAMESTEP, ID_PLAY_FRAMESTEPCANCEL, OnUpdatePlayFramestep)
    ON_COMMAND_RANGE(ID_PLAY_SEEKBACKWARDSMALL, ID_PLAY_SEEKFORWARDLARGE, OnPlaySeek)
    ON_COMMAND(ID_PLAY_SEEKSET, OnPlaySeekSet)
    ON_COMMAND_RANGE(ID_PLAY_SEEKKEYBACKWARD, ID_PLAY_SEEKKEYFORWARD, OnPlaySeekKey)
    ON_UPDATE_COMMAND_UI_RANGE(ID_PLAY_SEEKBACKWARDSMALL, ID_PLAY_SEEKFORWARDLARGE, OnUpdatePlaySeek)
    ON_UPDATE_COMMAND_UI(ID_PLAY_SEEKSET, OnUpdatePlaySeek)
    ON_UPDATE_COMMAND_UI_RANGE(ID_PLAY_SEEKKEYBACKWARD, ID_PLAY_SEEKKEYFORWARD, OnUpdatePlaySeek)
    ON_COMMAND_RANGE(ID_PLAY_DECRATE, ID_PLAY_INCRATE, OnPlayChangeRate)
    ON_UPDATE_COMMAND_UI_RANGE(ID_PLAY_DECRATE, ID_PLAY_INCRATE, OnUpdatePlayChangeRate)
    ON_COMMAND_RANGE(ID_PLAY_PLAYBACKRATE_START, ID_PLAY_PLAYBACKRATE_END, OnPlayChangeRate)
    ON_UPDATE_COMMAND_UI_RANGE(ID_PLAY_PLAYBACKRATE_START, ID_PLAY_PLAYBACKRATE_END, OnUpdatePlayChangeRate)
    ON_COMMAND(ID_PLAY_RESETRATE, OnPlayResetRate)
    ON_UPDATE_COMMAND_UI(ID_PLAY_RESETRATE, OnUpdatePlayResetRate)
    ON_COMMAND_RANGE(ID_PLAY_INCAUDDELAY, ID_PLAY_DECAUDDELAY, OnPlayChangeAudDelay)
    ON_UPDATE_COMMAND_UI_RANGE(ID_PLAY_INCAUDDELAY, ID_PLAY_DECAUDDELAY, OnUpdatePlayChangeAudDelay)
    ON_COMMAND(ID_FILTERS_COPY_TO_CLIPBOARD, OnPlayFiltersCopyToClipboard)
    ON_COMMAND_RANGE(ID_FILTERS_SUBITEM_START, ID_FILTERS_SUBITEM_END, OnPlayFilters)
    ON_UPDATE_COMMAND_UI_RANGE(ID_FILTERS_SUBITEM_START, ID_FILTERS_SUBITEM_END, OnUpdatePlayFilters)
    ON_COMMAND(ID_SHADERS_SELECT, OnPlayShadersSelect)
    ON_COMMAND(ID_SHADERS_PRESET_NEXT, OnPlayShadersPresetNext)
    ON_COMMAND(ID_SHADERS_PRESET_PREV, OnPlayShadersPresetPrev)
    ON_COMMAND_RANGE(ID_SHADERS_PRESETS_START, ID_SHADERS_PRESETS_END, OnPlayShadersPresets)
    ON_COMMAND_RANGE(ID_AUDIO_SUBITEM_START, ID_AUDIO_SUBITEM_END, OnPlayAudio)
    ON_COMMAND_RANGE(ID_SUBTITLES_SUBITEM_START, ID_SUBTITLES_SUBITEM_END, OnPlaySubtitles)
    ON_COMMAND(ID_SUBTITLES_DEFAULT_STYLE, OnSubtitlesDefaultStyle)
    ON_COMMAND_RANGE(ID_VIDEO_STREAMS_SUBITEM_START, ID_VIDEO_STREAMS_SUBITEM_END, OnPlayVideoStreams)
    ON_COMMAND_RANGE(ID_FILTERSTREAMS_SUBITEM_START, ID_FILTERSTREAMS_SUBITEM_END, OnPlayFiltersStreams)
    ON_COMMAND_RANGE(ID_VOLUME_UP, ID_VOLUME_MUTE, OnPlayVolume)
    ON_COMMAND_RANGE(ID_VOLUME_BOOST_INC, ID_VOLUME_BOOST_MAX, OnPlayVolumeBoost)
    ON_UPDATE_COMMAND_UI_RANGE(ID_VOLUME_BOOST_INC, ID_VOLUME_BOOST_MAX, OnUpdatePlayVolumeBoost)
    ON_COMMAND(ID_CUSTOM_CHANNEL_MAPPING, OnCustomChannelMapping)
    ON_UPDATE_COMMAND_UI(ID_CUSTOM_CHANNEL_MAPPING, OnUpdateCustomChannelMapping)
    ON_COMMAND_RANGE(ID_NORMALIZE, ID_REGAIN_VOLUME, OnNormalizeRegainVolume)
    ON_UPDATE_COMMAND_UI_RANGE(ID_NORMALIZE, ID_REGAIN_VOLUME, OnUpdateNormalizeRegainVolume)
    ON_COMMAND_RANGE(ID_COLOR_BRIGHTNESS_INC, ID_COLOR_RESET, OnPlayColor)
    ON_UPDATE_COMMAND_UI_RANGE(ID_AFTERPLAYBACK_EXIT, ID_AFTERPLAYBACK_MONITOROFF, OnUpdateAfterplayback)
    ON_COMMAND_RANGE(ID_AFTERPLAYBACK_EXIT, ID_AFTERPLAYBACK_MONITOROFF, OnAfterplayback)
    ON_UPDATE_COMMAND_UI_RANGE(ID_AFTERPLAYBACK_PLAYNEXT, ID_AFTERPLAYBACK_DONOTHING, OnUpdateAfterplayback)
    ON_COMMAND_RANGE(ID_AFTERPLAYBACK_PLAYNEXT, ID_AFTERPLAYBACK_DONOTHING, OnAfterplayback)
    ON_COMMAND_RANGE(ID_PLAY_REPEAT_ONEFILE, ID_PLAY_REPEAT_WHOLEPLAYLIST, OnPlayRepeat)
    ON_UPDATE_COMMAND_UI_RANGE(ID_PLAY_REPEAT_ONEFILE, ID_PLAY_REPEAT_WHOLEPLAYLIST, OnUpdatePlayRepeat)
    ON_COMMAND_RANGE(ID_PLAY_REPEAT_AB, ID_PLAY_REPEAT_AB_MARK_B, OnABRepeat)
    ON_UPDATE_COMMAND_UI_RANGE(ID_PLAY_REPEAT_AB, ID_PLAY_REPEAT_AB_MARK_B, OnUpdateABRepeat)
    ON_COMMAND(ID_PLAY_REPEAT_FOREVER, OnPlayRepeatForever)
    ON_UPDATE_COMMAND_UI(ID_PLAY_REPEAT_FOREVER, OnUpdatePlayRepeatForever)

    ON_COMMAND_RANGE(ID_NAVIGATE_SKIPBACK, ID_NAVIGATE_SKIPFORWARD, OnNavigateSkip)
    ON_UPDATE_COMMAND_UI_RANGE(ID_NAVIGATE_SKIPBACK, ID_NAVIGATE_SKIPFORWARD, OnUpdateNavigateSkip)
    ON_COMMAND_RANGE(ID_NAVIGATE_SKIPBACKFILE, ID_NAVIGATE_SKIPFORWARDFILE, OnNavigateSkipFile)
    ON_UPDATE_COMMAND_UI_RANGE(ID_NAVIGATE_SKIPBACKFILE, ID_NAVIGATE_SKIPFORWARDFILE, OnUpdateNavigateSkipFile)
    ON_COMMAND(ID_NAVIGATE_GOTO, OnNavigateGoto)
    ON_UPDATE_COMMAND_UI(ID_NAVIGATE_GOTO, OnUpdateNavigateGoto)
    ON_COMMAND_RANGE(ID_NAVIGATE_TITLEMENU, ID_NAVIGATE_CHAPTERMENU, OnNavigateMenu)
    ON_UPDATE_COMMAND_UI_RANGE(ID_NAVIGATE_TITLEMENU, ID_NAVIGATE_CHAPTERMENU, OnUpdateNavigateMenu)
    ON_COMMAND_RANGE(ID_NAVIGATE_JUMPTO_SUBITEM_START, ID_NAVIGATE_JUMPTO_SUBITEM_END, OnNavigateJumpTo)
    ON_COMMAND_RANGE(ID_NAVIGATE_MENU_LEFT, ID_NAVIGATE_MENU_LEAVE, OnNavigateMenuItem)
    ON_UPDATE_COMMAND_UI_RANGE(ID_NAVIGATE_MENU_LEFT, ID_NAVIGATE_MENU_LEAVE, OnUpdateNavigateMenuItem)

    ON_COMMAND(ID_NAVIGATE_TUNERSCAN, OnTunerScan)
    ON_UPDATE_COMMAND_UI(ID_NAVIGATE_TUNERSCAN, OnUpdateTunerScan)

    ON_COMMAND(ID_FAVORITES_ADD, OnFavoritesAdd)
    ON_UPDATE_COMMAND_UI(ID_FAVORITES_ADD, OnUpdateFavoritesAdd)
    ON_COMMAND(ID_FAVORITES_QUICKADDFAVORITE, OnFavoritesQuickAddFavorite)
    ON_COMMAND(ID_FAVORITES_ORGANIZE, OnFavoritesOrganize)
    ON_UPDATE_COMMAND_UI(ID_FAVORITES_ORGANIZE, OnUpdateFavoritesOrganize)
    ON_COMMAND_RANGE(ID_FAVORITES_FILE_START, ID_FAVORITES_FILE_END, OnFavoritesFile)
    ON_UPDATE_COMMAND_UI_RANGE(ID_FAVORITES_FILE_START, ID_FAVORITES_FILE_END, OnUpdateFavoritesFile)
    ON_COMMAND_RANGE(ID_FAVORITES_DVD_START, ID_FAVORITES_DVD_END, OnFavoritesDVD)
    ON_UPDATE_COMMAND_UI_RANGE(ID_FAVORITES_DVD_START, ID_FAVORITES_DVD_END, OnUpdateFavoritesDVD)
    ON_COMMAND_RANGE(ID_FAVORITES_DEVICE_START, ID_FAVORITES_DEVICE_END, OnFavoritesDevice)
    ON_UPDATE_COMMAND_UI_RANGE(ID_FAVORITES_DEVICE_START, ID_FAVORITES_DEVICE_END, OnUpdateFavoritesDevice)

    ON_COMMAND(ID_RECENT_FILES_CLEAR, OnRecentFileClear)
    ON_UPDATE_COMMAND_UI(ID_RECENT_FILES_CLEAR, OnUpdateRecentFileClear)
    ON_COMMAND_RANGE(ID_RECENT_FILE_START, ID_RECENT_FILE_END, OnRecentFile)
    ON_UPDATE_COMMAND_UI_RANGE(ID_RECENT_FILE_START, ID_RECENT_FILE_END, OnUpdateRecentFile)

    ON_COMMAND(ID_HELP_HOMEPAGE, OnHelpHomepage)
    ON_COMMAND(ID_HELP_CHECKFORUPDATE, OnHelpCheckForUpdate)
    ON_COMMAND(ID_HELP_TOOLBARIMAGES, OnHelpToolbarImages)
    ON_COMMAND(ID_HELP_DONATE, OnHelpDonate)

    // Open Dir incl. SubDir
    ON_COMMAND(ID_FILE_OPENDIRECTORY, OnFileOpendirectory)
    ON_UPDATE_COMMAND_UI(ID_FILE_OPENDIRECTORY, OnUpdateFileOpen)
    ON_WM_POWERBROADCAST()

    // Navigation panel
    ON_COMMAND(ID_VIEW_NAVIGATION, OnViewNavigation)
    ON_UPDATE_COMMAND_UI(ID_VIEW_NAVIGATION, OnUpdateViewNavigation)

    ON_WM_WTSSESSION_CHANGE()

    ON_MESSAGE(WM_LOADSUBTITLES, OnLoadSubtitles)
    ON_MESSAGE(WM_GETSUBTITLES, OnGetSubtitles)
    ON_WM_DRAWITEM()
    ON_WM_SETTINGCHANGE()
    ON_WM_MOUSEHWHEEL()
END_MESSAGE_MAP()

#ifdef _DEBUG
const TCHAR* GetEventString(LONG evCode)
{
#define UNPACK_VALUE(VALUE) case VALUE: return _T(#VALUE);
    switch (evCode) {
            // System-defined event codes
            UNPACK_VALUE(EC_COMPLETE);
            UNPACK_VALUE(EC_USERABORT);
            UNPACK_VALUE(EC_ERRORABORT);
            //UNPACK_VALUE(EC_TIME);
            UNPACK_VALUE(EC_REPAINT);
            UNPACK_VALUE(EC_STREAM_ERROR_STOPPED);
            UNPACK_VALUE(EC_STREAM_ERROR_STILLPLAYING);
            UNPACK_VALUE(EC_ERROR_STILLPLAYING);
            UNPACK_VALUE(EC_PALETTE_CHANGED);
            UNPACK_VALUE(EC_VIDEO_SIZE_CHANGED);
            UNPACK_VALUE(EC_QUALITY_CHANGE);
            UNPACK_VALUE(EC_SHUTTING_DOWN);
            UNPACK_VALUE(EC_CLOCK_CHANGED);
            UNPACK_VALUE(EC_PAUSED);
            UNPACK_VALUE(EC_OPENING_FILE);
            UNPACK_VALUE(EC_BUFFERING_DATA);
            UNPACK_VALUE(EC_FULLSCREEN_LOST);
            UNPACK_VALUE(EC_ACTIVATE);
            UNPACK_VALUE(EC_NEED_RESTART);
            UNPACK_VALUE(EC_WINDOW_DESTROYED);
            UNPACK_VALUE(EC_DISPLAY_CHANGED);
            UNPACK_VALUE(EC_STARVATION);
            UNPACK_VALUE(EC_OLE_EVENT);
            UNPACK_VALUE(EC_NOTIFY_WINDOW);
            UNPACK_VALUE(EC_STREAM_CONTROL_STOPPED);
            UNPACK_VALUE(EC_STREAM_CONTROL_STARTED);
            UNPACK_VALUE(EC_END_OF_SEGMENT);
            UNPACK_VALUE(EC_SEGMENT_STARTED);
            UNPACK_VALUE(EC_LENGTH_CHANGED);
            UNPACK_VALUE(EC_DEVICE_LOST);
            UNPACK_VALUE(EC_SAMPLE_NEEDED);
            UNPACK_VALUE(EC_PROCESSING_LATENCY);
            UNPACK_VALUE(EC_SAMPLE_LATENCY);
            UNPACK_VALUE(EC_SCRUB_TIME);
            UNPACK_VALUE(EC_STEP_COMPLETE);
            UNPACK_VALUE(EC_TIMECODE_AVAILABLE);
            UNPACK_VALUE(EC_EXTDEVICE_MODE_CHANGE);
            UNPACK_VALUE(EC_STATE_CHANGE);
            UNPACK_VALUE(EC_GRAPH_CHANGED);
            UNPACK_VALUE(EC_CLOCK_UNSET);
            UNPACK_VALUE(EC_VMR_RENDERDEVICE_SET);
            UNPACK_VALUE(EC_VMR_SURFACE_FLIPPED);
            UNPACK_VALUE(EC_VMR_RECONNECTION_FAILED);
            UNPACK_VALUE(EC_PREPROCESS_COMPLETE);
            UNPACK_VALUE(EC_CODECAPI_EVENT);
            UNPACK_VALUE(EC_WMT_INDEX_EVENT);
            UNPACK_VALUE(EC_WMT_EVENT);
            UNPACK_VALUE(EC_BUILT);
            UNPACK_VALUE(EC_UNBUILT);
            UNPACK_VALUE(EC_SKIP_FRAMES);
            UNPACK_VALUE(EC_PLEASE_REOPEN);
            UNPACK_VALUE(EC_STATUS);
            UNPACK_VALUE(EC_MARKER_HIT);
            UNPACK_VALUE(EC_LOADSTATUS);
            UNPACK_VALUE(EC_FILE_CLOSED);
            UNPACK_VALUE(EC_ERRORABORTEX);
            //UNPACK_VALUE(EC_NEW_PIN);
            //UNPACK_VALUE(EC_RENDER_FINISHED);
            UNPACK_VALUE(EC_EOS_SOON);
            UNPACK_VALUE(EC_CONTENTPROPERTY_CHANGED);
            UNPACK_VALUE(EC_BANDWIDTHCHANGE);
            UNPACK_VALUE(EC_VIDEOFRAMEREADY);
            // DVD-Video event codes
            UNPACK_VALUE(EC_DVD_DOMAIN_CHANGE);
            UNPACK_VALUE(EC_DVD_TITLE_CHANGE);
            UNPACK_VALUE(EC_DVD_CHAPTER_START);
            UNPACK_VALUE(EC_DVD_AUDIO_STREAM_CHANGE);
            UNPACK_VALUE(EC_DVD_SUBPICTURE_STREAM_CHANGE);
            UNPACK_VALUE(EC_DVD_ANGLE_CHANGE);
            UNPACK_VALUE(EC_DVD_BUTTON_CHANGE);
            UNPACK_VALUE(EC_DVD_VALID_UOPS_CHANGE);
            UNPACK_VALUE(EC_DVD_STILL_ON);
            UNPACK_VALUE(EC_DVD_STILL_OFF);
            UNPACK_VALUE(EC_DVD_CURRENT_TIME);
            UNPACK_VALUE(EC_DVD_ERROR);
            UNPACK_VALUE(EC_DVD_WARNING);
            UNPACK_VALUE(EC_DVD_CHAPTER_AUTOSTOP);
            UNPACK_VALUE(EC_DVD_NO_FP_PGC);
            UNPACK_VALUE(EC_DVD_PLAYBACK_RATE_CHANGE);
            UNPACK_VALUE(EC_DVD_PARENTAL_LEVEL_CHANGE);
            UNPACK_VALUE(EC_DVD_PLAYBACK_STOPPED);
            UNPACK_VALUE(EC_DVD_ANGLES_AVAILABLE);
            UNPACK_VALUE(EC_DVD_PLAYPERIOD_AUTOSTOP);
            UNPACK_VALUE(EC_DVD_BUTTON_AUTO_ACTIVATED);
            UNPACK_VALUE(EC_DVD_CMD_START);
            UNPACK_VALUE(EC_DVD_CMD_END);
            UNPACK_VALUE(EC_DVD_DISC_EJECTED);
            UNPACK_VALUE(EC_DVD_DISC_INSERTED);
            UNPACK_VALUE(EC_DVD_CURRENT_HMSF_TIME);
            UNPACK_VALUE(EC_DVD_KARAOKE_MODE);
            UNPACK_VALUE(EC_DVD_PROGRAM_CELL_CHANGE);
            UNPACK_VALUE(EC_DVD_TITLE_SET_CHANGE);
            UNPACK_VALUE(EC_DVD_PROGRAM_CHAIN_CHANGE);
            UNPACK_VALUE(EC_DVD_VOBU_Offset);
            UNPACK_VALUE(EC_DVD_VOBU_Timestamp);
            UNPACK_VALUE(EC_DVD_GPRM_Change);
            UNPACK_VALUE(EC_DVD_SPRM_Change);
            UNPACK_VALUE(EC_DVD_BeginNavigationCommands);
            UNPACK_VALUE(EC_DVD_NavigationCommand);
            // Sound device error event codes
            UNPACK_VALUE(EC_SNDDEV_IN_ERROR);
            UNPACK_VALUE(EC_SNDDEV_OUT_ERROR);
            // Custom event codes
            UNPACK_VALUE(EC_BG_AUDIO_CHANGED);
            UNPACK_VALUE(EC_BG_ERROR);
    };
#undef UNPACK_VALUE
    return _T("UNKNOWN");
}
#endif

void CMainFrame::EventCallback(MpcEvent ev)
{
    const auto& s = AfxGetAppSettings();
    switch (ev) {
        case MpcEvent::SHADER_SELECTION_CHANGED:
        case MpcEvent::SHADER_PRERESIZE_SELECTION_CHANGED:
        case MpcEvent::SHADER_POSTRESIZE_SELECTION_CHANGED:
            SetShaders(m_bToggleShader, m_bToggleShaderScreenSpace);
            break;
        case MpcEvent::DISPLAY_MODE_AUTOCHANGING:
            if (GetLoadState() == MLS::LOADED && GetMediaState() == State_Running && s.autoChangeFSMode.uDelay) {
                // pause if the mode is being changed during playback
                OnPlayPause();
                m_bPausedForAutochangeMonitorMode = true;
            }
            break;
        case MpcEvent::DISPLAY_MODE_AUTOCHANGED:
            if (GetLoadState() == MLS::LOADED) {
                if (m_bPausedForAutochangeMonitorMode && s.autoChangeFSMode.uDelay) {
                    // delay play if was paused due to mode change
                    ASSERT(GetMediaState() != State_Stopped);
                    const unsigned uModeChangeDelay = s.autoChangeFSMode.uDelay * 1000;
                    m_timerOneTime.Subscribe(TimerOneTimeSubscriber::DELAY_PLAYPAUSE_AFTER_AUTOCHANGE_MODE,
                                             std::bind(&CMainFrame::OnPlayPlay, this), uModeChangeDelay);
                } else if (m_bDelaySetOutputRect) {
                    ASSERT(GetMediaState() == State_Stopped);
                    // tell OnFilePostOpenmedia() to delay entering play or paused state
                    m_bOpeningInAutochangedMonitorMode = true;
                }
            }
            break;
        case MpcEvent::CHANGING_UI_LANGUAGE:
            UpdateUILanguage();
            break;
        case MpcEvent::STREAM_POS_UPDATE_REQUEST:
            OnTimer(TIMER_STREAMPOSPOLLER);
            OnTimer(TIMER_STREAMPOSPOLLER2);
            break;
        default:
            ASSERT(FALSE);
    }
}

/////////////////////////////////////////////////////////////////////////////
// CMainFrame construction/destruction

CMainFrame::CMainFrame()
    : m_timer32Hz(this, TIMER_32HZ, 32)
    , m_timerOneTime(this, TIMER_ONETIME_START, TIMER_ONETIME_END - TIMER_ONETIME_START + 1)
    , m_bUsingDXVA(false)
    , m_HWAccelType(nullptr)
    , m_posFirstExtSub(nullptr)
    , m_bDelaySetOutputRect(false)
    , m_nJumpToSubMenusCount(0)
    , m_nLoops(0)
    , m_nLastSkipDirection(0)
    , m_fCustomGraph(false)
    , m_fShockwaveGraph(false)
    , m_fFrameSteppingActive(false)
    , m_nStepForwardCount(0)
    , m_rtStepForwardStart(0)
    , m_nVolumeBeforeFrameStepping(0)
    , m_fEndOfStream(false)
    , m_dwLastPause(0)
    , m_bRememberFilePos(false)
    , m_dwLastRun(0)
    , m_bBuffering(false)
    , m_fLiveWM(false)
    , m_rtDurationOverride(-1)
    , m_iPlaybackMode(PM_NONE)
    , m_lCurrentChapter(0)
    , m_lChapterStartTime(0xFFFFFFFF)
    , m_eMediaLoadState(MLS::CLOSED)
    , m_bSettingUpMenus(false)
    , m_bOpenMediaActive(false)
    , streampospoller_active(false)
    , m_fFullScreen(false)
    , m_fFirstFSAfterLaunchOnFS(false)
    , m_fStartInD3DFullscreen(false)
    , m_pLastBar(nullptr)
    , m_bFirstPlay(false)
    , m_bOpeningInAutochangedMonitorMode(false)
    , m_bPausedForAutochangeMonitorMode(false)
    , m_fAudioOnly(true)
    , m_iDVDDomain(DVD_DOMAIN_Stop)
    , m_iDVDTitle(0)
    , m_dSpeedRate(1.0)
    , m_ZoomX(1.0)
    , m_ZoomY(1.0)
    , m_PosX(0.5)
    , m_PosY(0.5)
    , m_AngleX(0)
    , m_AngleY(0)
    , m_AngleZ(0)
    , m_iDefRotation(0)
    , m_pGraphThread(nullptr)
    , m_bOpenedThroughThread(false)
    , m_evOpenPrivateFinished(FALSE, TRUE)
    , m_evClosePrivateFinished(FALSE, TRUE)
    , m_fOpeningAborted(false)
    , m_bWasSnapped(false)
    , m_wndSubtitlesDownloadDialog(this)
    , m_wndSubtitlesUploadDialog(this)
    , m_bTrayIcon(false)
    , m_fCapturing(false)
    , m_controls(this)
    , m_wndView(this)
    , m_wndSeekBar(this)
    , m_wndToolBar(this)
    , m_wndInfoBar(this)
    , m_wndStatsBar(this)
    , m_wndStatusBar(this)
    , m_wndSubresyncBar(this)
    , m_wndPlaylistBar(this)
    , m_wndPreView(this)
    , m_wndCaptureBar(this)
    , m_wndNavigationBar(this)
    , m_pVideoWnd(nullptr)
    , m_pFullscreenWnd(nullptr)
    , m_OSD(this)
    , m_bOSDDisplayTime(false)
    , m_nCurSubtitle(-1)
    , m_lSubtitleShift(0)
    , m_rtCurSubPos(0)
    , m_bScanDlgOpened(false)
    , m_bStopTunerScan(false)
    , m_bLockedZoomVideoWindow(false)
    , m_nLockedZoomVideoWindow(0)
    , m_pActiveContextMenu(nullptr)
    , m_pActiveSystemMenu(nullptr)
    , m_bAltDownClean(false)
    , m_bShowingFloatingMenubar(false)
    , m_bAllowWindowZoom(false)
    , m_dLastVideoScaleFactor(0)
    , m_bExtOnTop(false)
    , m_bIsBDPlay(false)
    , m_bHasBDMeta(false)
    , watchingFileDialog(false)
    , fileDialogHookHelper(nullptr)
    , delayingFullScreen(false)
    , restoringWindowRect(false)
    , abRepeatPositionAEnabled(false)
    , abRepeatPositionBEnabled(false)
    , abRepeatPositionA(0)
    , abRepeatPositionB(0)
    , mediaTypesErrorDlg(nullptr)
    , m_iStreamPosPollerInterval(100)
    , m_BDMeta()
    , currentAudioLang(_T(""))
    , currentSubLang(_T(""))
    , m_bToggleShader(false)
    , m_current_rfe()
    , m_bToggleShaderScreenSpace(false)
    , m_MPLSPlaylist()
    , m_sydlLastProcessURL()
    , m_bUseSeekPreview(false)
    , queuedSeek({0,0,false})
    , lastSeekStart(0)
    , lastSeekFinish(0)
{
    // Don't let CFrameWnd handle automatically the state of the menu items.
    // This means that menu items without handlers won't be automatically
    // disabled but it avoids some unwanted cases where programmatically
    // disabled menu items are always re-enabled by CFrameWnd.
    m_bAutoMenuEnable = FALSE;

    EventRouter::EventSelection receives;
    receives.insert(MpcEvent::SHADER_SELECTION_CHANGED);
    receives.insert(MpcEvent::SHADER_PRERESIZE_SELECTION_CHANGED);
    receives.insert(MpcEvent::SHADER_POSTRESIZE_SELECTION_CHANGED);
    receives.insert(MpcEvent::DISPLAY_MODE_AUTOCHANGING);
    receives.insert(MpcEvent::DISPLAY_MODE_AUTOCHANGED);
    receives.insert(MpcEvent::CHANGING_UI_LANGUAGE);
    receives.insert(MpcEvent::STREAM_POS_UPDATE_REQUEST);
    EventRouter::EventSelection fires;
    fires.insert(MpcEvent::SWITCHING_TO_FULLSCREEN);
    fires.insert(MpcEvent::SWITCHED_TO_FULLSCREEN);
    fires.insert(MpcEvent::SWITCHING_FROM_FULLSCREEN);
    fires.insert(MpcEvent::SWITCHED_FROM_FULLSCREEN);
    fires.insert(MpcEvent::SWITCHING_TO_FULLSCREEN_D3D);
    fires.insert(MpcEvent::SWITCHED_TO_FULLSCREEN_D3D);
    fires.insert(MpcEvent::MEDIA_LOADED);
    fires.insert(MpcEvent::DISPLAY_MODE_AUTOCHANGING);
    fires.insert(MpcEvent::DISPLAY_MODE_AUTOCHANGED);
    fires.insert(MpcEvent::CONTEXT_MENU_POPUP_INITIALIZED);
    fires.insert(MpcEvent::CONTEXT_MENU_POPUP_UNINITIALIZED);
    fires.insert(MpcEvent::SYSTEM_MENU_POPUP_INITIALIZED);
    fires.insert(MpcEvent::SYSTEM_MENU_POPUP_UNINITIALIZED);
    fires.insert(MpcEvent::DPI_CHANGED);
    GetEventd().Connect(m_eventc, receives, std::bind(&CMainFrame::EventCallback, this, std::placeholders::_1), fires);
}

CMainFrame::~CMainFrame()
{
    if (defaultMPCThemeMenu != nullptr) {
        delete defaultMPCThemeMenu;
    }
}

int CMainFrame::OnNcCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (IsWindows10OrGreater()) {
        // Tell Windows to automatically handle scaling of non-client areas
        // such as the caption bar. EnableNonClientDpiScaling was introduced in Windows 10
        const WinapiFunc<BOOL WINAPI(HWND)>
        fnEnableNonClientDpiScaling = { _T("User32.dll"), "EnableNonClientDpiScaling" };

        if (fnEnableNonClientDpiScaling) {
            fnEnableNonClientDpiScaling(m_hWnd);
        }
    }

    return __super::OnNcCreate(lpCreateStruct);
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (__super::OnCreate(lpCreateStruct) == -1) {
        return -1;
    }

    CMPCThemeUtil::enableWindows10DarkFrame(this);

    if (IsWindows8Point1OrGreater()) {
        m_dpi.Override(m_hWnd);
    }

    const WinapiFunc<decltype(ChangeWindowMessageFilterEx)>
    fnChangeWindowMessageFilterEx = { _T("user32.dll"), "ChangeWindowMessageFilterEx" };

    // allow taskbar messages through UIPI
    if (fnChangeWindowMessageFilterEx) {
        VERIFY(fnChangeWindowMessageFilterEx(m_hWnd, s_uTaskbarRestart, MSGFLT_ALLOW, nullptr));
        VERIFY(fnChangeWindowMessageFilterEx(m_hWnd, s_uTBBC, MSGFLT_ALLOW, nullptr));
        VERIFY(fnChangeWindowMessageFilterEx(m_hWnd, WM_COMMAND, MSGFLT_ALLOW, nullptr));
    }

    VERIFY(m_popupMenu.LoadMenu(IDR_POPUP));
    VERIFY(m_mainPopupMenu.LoadMenu(IDR_POPUPMAIN));
    CreateDynamicMenus();

    // create a view to occupy the client area of the frame
    if (!m_wndView.Create(nullptr, nullptr, AFX_WS_DEFAULT_VIEW,
                          CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST, nullptr)) {
        TRACE(_T("Failed to create view window\n"));
        return -1;
    }
    // Should never be RTLed
    m_wndView.ModifyStyleEx(WS_EX_LAYOUTRTL, WS_EX_NOINHERITLAYOUT);

    const CAppSettings& s = AfxGetAppSettings();

    // Create Preview Window
    if (s.fSeekPreview) {
        if (m_wndPreView.CreateEx(0, AfxRegisterWndClass(0), nullptr, WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, CRect(0, 0, 160, 109), this, 0)) {
            m_wndPreView.ShowWindow(SW_HIDE);
            m_wndPreView.SetRelativeSize(AfxGetAppSettings().iSeekPreviewSize);
        } else {
            TRACE(_T("Failed to create Preview Window"));
        }
    }

    // static bars

    BOOL bResult = m_wndStatusBar.Create(this);
    if (bResult) {
        bResult = m_wndStatsBar.Create(this);
    }
    if (bResult) {
        bResult = m_wndInfoBar.Create(this);
    }
    if (bResult) {
        bResult = m_wndToolBar.Create(this);
    }
    if (bResult) {
        bResult = m_wndSeekBar.Create(this);
    }
    if (!bResult) {
        TRACE(_T("Failed to create all control bars\n"));
        return -1;      // fail to create
    }

    m_pFullscreenWnd = DEBUG_NEW CFullscreenWnd(this);

    m_controls.m_toolbars[CMainFrameControls::Toolbar::SEEKBAR] = &m_wndSeekBar;
    m_controls.m_toolbars[CMainFrameControls::Toolbar::CONTROLS] = &m_wndToolBar;
    m_controls.m_toolbars[CMainFrameControls::Toolbar::INFO] = &m_wndInfoBar;
    m_controls.m_toolbars[CMainFrameControls::Toolbar::STATS] = &m_wndStatsBar;
    m_controls.m_toolbars[CMainFrameControls::Toolbar::STATUS] = &m_wndStatusBar;

    // dockable bars

    EnableDocking(CBRS_ALIGN_ANY);

    bResult = m_wndSubresyncBar.Create(this, AFX_IDW_DOCKBAR_TOP, &m_csSubLock);
    if (bResult) {
        m_wndSubresyncBar.SetBarStyle(m_wndSubresyncBar.GetBarStyle() | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC);
        m_wndSubresyncBar.EnableDocking(CBRS_ALIGN_ANY);
        m_wndSubresyncBar.SetHeight(200);
        m_controls.m_panels[CMainFrameControls::Panel::SUBRESYNC] = &m_wndSubresyncBar;
    }
    bResult = bResult && m_wndPlaylistBar.Create(this, AFX_IDW_DOCKBAR_BOTTOM);
    if (bResult) {
        m_wndPlaylistBar.SetBarStyle(m_wndPlaylistBar.GetBarStyle() | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC);
        m_wndPlaylistBar.EnableDocking(CBRS_ALIGN_ANY);
        m_wndPlaylistBar.SetHeight(100);
        m_controls.m_panels[CMainFrameControls::Panel::PLAYLIST] = &m_wndPlaylistBar;
        //m_wndPlaylistBar.LoadPlaylist(GetRecentFile()); //adipose 2019-11-12; do this later after activating the frame
    }
    bResult = bResult && m_wndEditListEditor.Create(this, AFX_IDW_DOCKBAR_RIGHT);
    if (bResult) {
        m_wndEditListEditor.SetBarStyle(m_wndEditListEditor.GetBarStyle() | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC);
        m_wndEditListEditor.EnableDocking(CBRS_ALIGN_ANY);
        m_controls.m_panels[CMainFrameControls::Panel::EDL] = &m_wndEditListEditor;
        m_wndEditListEditor.SetHeight(100);
    }
    bResult = bResult && m_wndCaptureBar.Create(this, AFX_IDW_DOCKBAR_LEFT);
    if (bResult) {
        m_wndCaptureBar.SetBarStyle(m_wndCaptureBar.GetBarStyle() | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC);
        m_wndCaptureBar.EnableDocking(CBRS_ALIGN_LEFT | CBRS_ALIGN_RIGHT);
        m_controls.m_panels[CMainFrameControls::Panel::CAPTURE] = &m_wndCaptureBar;
    }
    bResult = bResult && m_wndNavigationBar.Create(this, AFX_IDW_DOCKBAR_LEFT);
    if (bResult) {
        m_wndNavigationBar.SetBarStyle(m_wndNavigationBar.GetBarStyle() | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC);
        m_wndNavigationBar.EnableDocking(CBRS_ALIGN_LEFT | CBRS_ALIGN_RIGHT);
        m_controls.m_panels[CMainFrameControls::Panel::NAVIGATION] = &m_wndNavigationBar;
    }
    if (!bResult) {
        TRACE(_T("Failed to create all dockable bars\n"));
        return -1;
    }

    // Hide all controls initially
    for (const auto& pair : m_controls.m_toolbars) {
        pair.second->ShowWindow(SW_HIDE);
    }
    for (const auto& pair : m_controls.m_panels) {
        pair.second->ShowWindow(SW_HIDE);
    }

    m_dropTarget.Register(this);

    SetAlwaysOnTop(s.iOnTop);

    ShowTrayIcon(s.fTrayIcon);

    m_Lcd.SetVolumeRange(0, 100);
    m_Lcd.SetVolume(std::max(1, s.nVolume));

    m_pGraphThread = (CGraphThread*)AfxBeginThread(RUNTIME_CLASS(CGraphThread));

    if (m_pGraphThread) {
        m_pGraphThread->SetMainFrame(this);
    }

    m_pSubtitlesProviders = std::make_unique<SubtitlesProviders>(this);
    m_wndSubtitlesDownloadDialog.Create(m_wndSubtitlesDownloadDialog.IDD, this);
    m_wndSubtitlesUploadDialog.Create(m_wndSubtitlesUploadDialog.IDD, this);

    if (s.nCmdlnWebServerPort != 0) {
        if (s.nCmdlnWebServerPort > 0) {
            StartWebServer(s.nCmdlnWebServerPort);
        } else if (s.fEnableWebServer) {
            StartWebServer(s.nWebServerPort);
        }
    }

    m_bToggleShader = s.bToggleShader;
    m_bToggleShaderScreenSpace = s.bToggleShaderScreenSpace;
    OpenSetupWindowTitle(true);

    WTSRegisterSessionNotification();

    UpdateSkypeHandler();

    m_popupMenu.fulfillThemeReqs();
    m_mainPopupMenu.fulfillThemeReqs();
    return 0;
}

void CMainFrame::OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT lpMeasureItemStruct)
{
    if (lpMeasureItemStruct->CtlType == ODT_MENU)  {
        if (CMPCThemeMenu* cm = CMPCThemeMenu::getParentMenu(lpMeasureItemStruct->itemID)) {
            cm->MeasureItem(lpMeasureItemStruct);
            return;
        }
    }

    CFrameWnd::OnMeasureItem(nIDCtl, lpMeasureItemStruct);
}

void CMainFrame::OnDestroy()
{
    WTSUnRegisterSessionNotification();
    ShowTrayIcon(false);
    m_dropTarget.Revoke();

    if (m_pDebugShaders && IsWindow(m_pDebugShaders->m_hWnd)) {
        VERIFY(m_pDebugShaders->DestroyWindow());
    }

    if (m_pGraphThread) {
        CAMMsgEvent e;
        m_pGraphThread->PostThreadMessage(CGraphThread::TM_EXIT, 0, (LPARAM)&e);
        if (!e.Wait(5000)) {
            TRACE(_T("ERROR: Must call TerminateThread() on CMainFrame::m_pGraphThread->m_hThread\n"));
            TerminateThread(m_pGraphThread->m_hThread, DWORD_ERROR);
        }
    }

    if (m_pFullscreenWnd) {
        if (m_pFullscreenWnd->IsWindow()) {
            m_pFullscreenWnd->DestroyWindow();
        }
        delete m_pFullscreenWnd;
    }

    m_wndPreView.DestroyWindow();

    __super::OnDestroy();
}

void CMainFrame::OnClose()
{
    CAppSettings& s = AfxGetAppSettings();

    s.bToggleShader = m_bToggleShader;
    s.bToggleShaderScreenSpace = m_bToggleShaderScreenSpace;
    s.dZoomX = m_ZoomX;
    s.dZoomY = m_ZoomY;

    m_wndPlaylistBar.SavePlaylist();

    m_controls.SaveState();

    ShowWindow(SW_HIDE);

    if (GetLoadState() == MLS::LOADED || GetLoadState() == MLS::LOADING) {
        CloseMedia();
    }

    s.WinLircClient.DisConnect();
    s.UIceClient.DisConnect();

    SendAPICommand(CMD_DISCONNECT, L"\0");  // according to CMD_NOTIFYENDOFSTREAM (ctrl+f it here), you're not supposed to send NULL here

    AfxGetMyApp()->SetClosingState();

    __super::OnClose();
}

LPCTSTR CMainFrame::GetRecentFile() const
{
    auto& MRU = AfxGetAppSettings().MRU;
    MRU.ReadList();
    for (int i = 0; i < MRU.GetSize(); i++) {
        if (MRU[i].fns.GetCount() > 0 && !MRU[i].fns.GetHead().IsEmpty()) {
            return MRU[i].fns.GetHead();
        }
    }
    return nullptr;
}

LRESULT CMainFrame::OnTaskBarRestart(WPARAM, LPARAM)
{
    m_bTrayIcon = false;
    ShowTrayIcon(AfxGetAppSettings().fTrayIcon);
    return 0;
}

LRESULT CMainFrame::OnNotifyIcon(WPARAM wParam, LPARAM lParam)
{
    if ((UINT)wParam != IDR_MAINFRAME) {
        return -1;
    }

    switch ((UINT)lParam) {
        case WM_LBUTTONDOWN:
            if (IsIconic()) {
                ShowWindow(SW_RESTORE);
            }
            CreateThumbnailToolbar();
            MoveVideoWindow();
            SetForegroundWindow();
            break;
        case WM_LBUTTONDBLCLK:
            PostMessage(WM_COMMAND, ID_FILE_OPENMEDIA);
            break;
        case WM_RBUTTONDOWN: {
            POINT p;
            GetCursorPos(&p);
            SetForegroundWindow();
            m_mainPopupMenu.GetSubMenu(0)->TrackPopupMenu(TPM_RIGHTBUTTON | TPM_NOANIMATION, p.x, p.y, GetModalParent());
            PostMessage(WM_NULL);
            break;
        }
        case WM_MOUSEMOVE: {
            CString str;
            GetWindowText(str);
            SetTrayTip(str);
            break;
        }
        default:
            break;
    }

    return 0;
}

LRESULT CMainFrame::OnTaskBarThumbnailsCreate(WPARAM, LPARAM)
{
    return CreateThumbnailToolbar();
}

LRESULT CMainFrame::OnSkypeAttach(WPARAM wParam, LPARAM lParam)
{
    return m_pSkypeMoodMsgHandler ? m_pSkypeMoodMsgHandler->HandleAttach(wParam, lParam) : FALSE;
}

void CMainFrame::ShowTrayIcon(bool bShow)
{
    NOTIFYICONDATA nid = { sizeof(nid), m_hWnd, IDR_MAINFRAME };

    if (bShow) {
        if (!m_bTrayIcon) {
            nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
            nid.uCallbackMessage = WM_NOTIFYICON;
            nid.hIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDR_MAINFRAME),
                                         IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
            StringCchCopy(nid.szTip, _countof(nid.szTip), _T("MPC-HC"));
            Shell_NotifyIcon(NIM_ADD, &nid);
            m_bTrayIcon = true;
        }
    } else {
        if (m_bTrayIcon) {
            Shell_NotifyIcon(NIM_DELETE, &nid);
            m_bTrayIcon = false;
            if (IsIconic()) {
                // if the window was minimized to tray - show it
                ShowWindow(SW_RESTORE);
            }
        }
    }
}

void CMainFrame::SetTrayTip(CString str)
{
    NOTIFYICONDATA tnid;
    tnid.cbSize = sizeof(NOTIFYICONDATA);
    tnid.hWnd = m_hWnd;
    tnid.uID = IDR_MAINFRAME;
    tnid.uFlags = NIF_TIP;
    StringCchCopy(tnid.szTip, _countof(tnid.szTip), str);
    Shell_NotifyIcon(NIM_MODIFY, &tnid);
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!__super::PreCreateWindow(cs)) {
        return FALSE;
    }

    cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
    cs.lpszClass = MPC_WND_CLASS_NAME; //AfxRegisterWndClass(nullptr);

    return TRUE;
}

BOOL CMainFrame::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN) {
        /*if (m_fShockwaveGraph
          && (pMsg->wParam == VK_LEFT || pMsg->wParam == VK_RIGHT
              || pMsg->wParam == VK_UP || pMsg->wParam == VK_DOWN))
              return FALSE;
        */
        if (pMsg->wParam == VK_ESCAPE) {
            bool fEscapeNotAssigned = !AssignedToCmd(VK_ESCAPE, m_fFullScreen, false);

            if (fEscapeNotAssigned) {
                if (m_fFullScreen || IsD3DFullScreenMode()) {
                    OnViewFullscreen();
                    if (GetLoadState() == MLS::LOADED) {
                        PostMessage(WM_COMMAND, ID_PLAY_PAUSE);
                    }
                    return TRUE;
                } else if (IsCaptionHidden()) {
                    PostMessage(WM_COMMAND, ID_VIEW_PRESETS_NORMAL);
                    return TRUE;
                }
            }
        } else if (pMsg->wParam == VK_LEFT && m_pAMTuner) {
            PostMessage(WM_COMMAND, ID_NAVIGATE_SKIPBACK);
            return TRUE;
        } else if (pMsg->wParam == VK_RIGHT && m_pAMTuner) {
            PostMessage(WM_COMMAND, ID_NAVIGATE_SKIPFORWARD);
            return TRUE;
        }
    }

    if ((m_dwMenuBarVisibility & AFX_MBV_DISPLAYONFOCUS) && pMsg->message == WM_SYSKEYUP && pMsg->wParam == VK_F10 &&
            m_dwMenuBarState == AFX_MBS_VISIBLE) {
        // mfc doesn't hide menubar on f10, but we want to
        VERIFY(SetMenuBarState(AFX_MBS_HIDDEN));
        return FALSE;
    }

    if (pMsg->message == WM_KEYDOWN) {
        m_bAltDownClean = false;
    }
    if (pMsg->message == WM_SYSKEYDOWN) {
        m_bAltDownClean = (pMsg->wParam == VK_MENU);
    }
    if ((m_dwMenuBarVisibility & AFX_MBV_DISPLAYONFOCUS) && pMsg->message == WM_SYSKEYUP && pMsg->wParam == VK_MENU &&
            m_dwMenuBarState == AFX_MBS_HIDDEN) {
        // mfc shows menubar when Ctrl->Alt->K is released in reverse order, but we don't want to
        if (m_bAltDownClean) {
            VERIFY(SetMenuBarState(AFX_MBS_VISIBLE));
            return FALSE;
        }
        return TRUE;
    }

    // for compatibility with KatMouse and the like
    if (pMsg->message == WM_MOUSEWHEEL && pMsg->hwnd == m_hWnd) {
        pMsg->hwnd = m_wndView.m_hWnd;
        return FALSE;
    }

    return __super::PreTranslateMessage(pMsg);
}

void CMainFrame::RecalcLayout(BOOL bNotify)
{
    __super::RecalcLayout(bNotify);

    CRect r;
    GetWindowRect(&r);
    MINMAXINFO mmi;
    ZeroMemory(&mmi, sizeof(mmi));
    OnGetMinMaxInfo(&mmi);
    const POINT& min = mmi.ptMinTrackSize;
    if (r.Height() < min.y || r.Width() < min.x) {
        r |= CRect(r.TopLeft(), CSize(min));
        MoveWindow(r);
    }
}

void CMainFrame::EnableDocking(DWORD dwDockStyle)
{
    ASSERT((dwDockStyle & ~(CBRS_ALIGN_ANY | CBRS_FLOAT_MULTI)) == 0);

    m_pFloatingFrameClass = RUNTIME_CLASS(CMPCThemeMiniDockFrameWnd);
    for (int i = 0; i < 4; i++) {
        if (dwDockBarMap[i][1] & dwDockStyle & CBRS_ALIGN_ANY) {
            CMPCThemeDockBar* pDock = (CMPCThemeDockBar*)GetControlBar(dwDockBarMap[i][0]);
            if (pDock == NULL) {
                pDock = new CMPCThemeDockBar;
                if (!pDock->Create(this,
                                   WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_CHILD | WS_VISIBLE |
                                   dwDockBarMap[i][1], dwDockBarMap[i][0])) {
                    AfxThrowResourceException();
                }
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
// CMainFrame diagnostics

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
    __super::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
    __super::Dump(dc);
}

#endif //_DEBUG

typedef HIMC(WINAPI* pfnImmAssociateContext)(HWND, HIMC);
void dynImmAssociateContext(HWND hWnd, HIMC himc) {
    HMODULE hImm32;
    pfnImmAssociateContext pImmAssociateContext;

    hImm32 = LoadLibrary(_T("imm32.dll"));
    if (NULL == hImm32) return; // No East Asian support
    pImmAssociateContext = (pfnImmAssociateContext)GetProcAddress(hImm32, "ImmAssociateContext");
    if (NULL == pImmAssociateContext) {
        FreeLibrary(hImm32);
        return;
    }
    pImmAssociateContext(hWnd, himc);
    FreeLibrary(hImm32);
}

/////////////////////////////////////////////////////////////////////////////
// CMainFrame message handlers
void CMainFrame::OnSetFocus(CWnd* pOldWnd)
{
    // forward focus to the view window
    if (IsWindow(m_wndView.m_hWnd)) {
        m_wndView.SetFocus();
        dynImmAssociateContext(m_wndView.m_hWnd, NULL);
    } else {
        dynImmAssociateContext(m_hWnd, NULL);
    }
}

BOOL CMainFrame::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
    // let the view have first crack at the command
    if (m_wndView.OnCmdMsg(nID, nCode, pExtra, pHandlerInfo)) {
        return TRUE;
    }

    for (const auto& pair : m_controls.m_toolbars) {
        if (pair.second->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo)) {
            return TRUE;
        }
    }

    for (const auto& pair : m_controls.m_panels) {
        if (pair.second->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo)) {
            return TRUE;
        }
    }

    // otherwise, do default handling
    return __super::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

void CMainFrame::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    auto setLarger = [](long & a, long b) {
        a = std::max(a, b);
    };

    const long saneSize = 110;
    const bool bMenuVisible = GetMenuBarVisibility() == AFX_MBV_KEEPVISIBLE || m_bShowingFloatingMenubar;

    // Begin with docked controls
    lpMMI->ptMinTrackSize = CPoint(m_controls.GetDockZonesMinSize(saneSize));

    if (bMenuVisible) {
        // Ensure that menubar will fit horizontally
        MENUBARINFO mbi = { sizeof(mbi) };
        GetMenuBarInfo(OBJID_MENU, 0, &mbi);
        long x = GetSystemMetrics(SM_CYMENU) / 2; // free space after menu
        CRect rect;
        for (int i = 0; GetMenuItemRect(m_hWnd, mbi.hMenu, i, &rect); i++) {
            x += rect.Width();
        }
        setLarger(lpMMI->ptMinTrackSize.x, x);
    }

    if (IsWindow(m_wndToolBar) && m_controls.ControlChecked(CMainFrameControls::Toolbar::CONTROLS)) {
        // Ensure that Controls toolbar will fit
        setLarger(lpMMI->ptMinTrackSize.x, m_wndToolBar.GetMinWidth());
    }

    // Ensure that window decorations will fit
    CRect decorationsRect;
    VERIFY(AdjustWindowRectEx(decorationsRect, GetWindowStyle(m_hWnd), bMenuVisible, GetWindowExStyle(m_hWnd)));
    lpMMI->ptMinTrackSize.x += decorationsRect.Width();
    lpMMI->ptMinTrackSize.y += decorationsRect.Height();

    // Final fence
    setLarger(lpMMI->ptMinTrackSize.x, GetSystemMetrics(SM_CXMIN));
    setLarger(lpMMI->ptMinTrackSize.y, GetSystemMetrics(SM_CYMIN));

    lpMMI->ptMaxTrackSize.x = GetSystemMetrics(SM_CXVIRTUALSCREEN) + decorationsRect.Width();
    lpMMI->ptMaxTrackSize.y = GetSystemMetrics(SM_CYVIRTUALSCREEN)
                              + ((GetStyle() & WS_THICKFRAME) ? GetSystemMetrics(SM_CYSIZEFRAME) : 0);
}

void CMainFrame::OnMove(int x, int y)
{
    __super::OnMove(x, y);

    if (m_bWasSnapped && IsZoomed()) {
        m_bWasSnapped = false;
    }

    WINDOWPLACEMENT wp;
    GetWindowPlacement(&wp);
    if (!m_fFirstFSAfterLaunchOnFS && !m_fFullScreen && IsWindowVisible() && wp.flags != WPF_RESTORETOMAXIMIZED && wp.showCmd != SW_SHOWMINIMIZED) {
        GetWindowRect(AfxGetAppSettings().rcLastWindowPos);
    }
}

void CMainFrame::OnEnterSizeMove()
{
    if (m_bWasSnapped) {
        VERIFY(GetCursorPos(&m_snapStartPoint));
        GetWindowRect(m_snapStartRect);
    }
}

void CMainFrame::OnMoving(UINT fwSide, LPRECT pRect)
{
    if (AfxGetAppSettings().fSnapToDesktopEdges) {
        const CSize threshold(m_dpi.ScaleX(16), m_dpi.ScaleY(16));

        CRect rect(pRect);

        CRect windowRect;
        GetWindowRect(windowRect);

        if (windowRect.Size() != rect.Size()) {
            // aero snap
            return;
        }

        CPoint point;
        VERIFY(GetCursorPos(&point));

        if (m_bWasSnapped) {
            rect.MoveToXY(point - m_snapStartPoint + m_snapStartRect.TopLeft());
        }

        CRect areaRect;
        CMonitors::GetNearestMonitor(this).GetWorkAreaRect(areaRect);
        const CRect invisibleBorderSize = GetInvisibleBorderSize();
        areaRect.InflateRect(invisibleBorderSize);

        bool bSnapping = false;

        if (std::abs(rect.left - areaRect.left) < threshold.cx) {
            bSnapping = true;
            rect.MoveToX(areaRect.left);
        } else if (std::abs(rect.right - areaRect.right) < threshold.cx) {
            bSnapping = true;
            rect.MoveToX(areaRect.right - rect.Width());
        }
        if (std::abs(rect.top - areaRect.top) < threshold.cy) {
            bSnapping = true;
            rect.MoveToY(areaRect.top);
        } else if (std::abs(rect.bottom - areaRect.bottom) < threshold.cy) {
            bSnapping = true;
            rect.MoveToY(areaRect.bottom - rect.Height());
        }

        if (!m_bWasSnapped && bSnapping) {
            m_snapStartPoint = point;
            m_snapStartRect = pRect;
        }

        *pRect = rect;

        m_bWasSnapped = bSnapping;
    } else {
        m_bWasSnapped = false;
    }

    __super::OnMoving(fwSide, pRect);
}

void CMainFrame::OnSize(UINT nType, int cx, int cy)
{
    if (m_bTrayIcon && nType == SIZE_MINIMIZED) {
        ShowWindow(SW_HIDE);
    } else {
        __super::OnSize(nType, cx, cy);

        if (!m_fFirstFSAfterLaunchOnFS && IsWindowVisible() && !m_fFullScreen) {
            CAppSettings& s = AfxGetAppSettings();
            if (nType != SIZE_MAXIMIZED && nType != SIZE_MINIMIZED) {
                GetWindowRect(s.rcLastWindowPos);
            }
            s.nLastWindowType = nType;
        }
    }
}

void CMainFrame::OnSizing(UINT nSide, LPRECT lpRect)
{
    __super::OnSizing(nSide, lpRect);

    const auto& s = AfxGetAppSettings();
    bool bCtrl = GetKeyState(VK_CONTROL) < 0;

    if (GetLoadState() != MLS::LOADED || m_fFullScreen ||
            s.iDefaultVideoSize == DVS_STRETCH || !bCtrl == !s.fLimitWindowProportions || IsAeroSnapped()) {
        return;
    }

    CSize videoSize = GetVideoSize();
    if (videoSize.cx == 0 || videoSize.cy == 0) {
        return;
    }

    CRect currentWindowRect, currentViewRect;
    GetWindowRect(currentWindowRect);
    m_wndView.GetWindowRect(currentViewRect);
    CSize controlsSize(currentWindowRect.Width() - currentViewRect.Width(),
                       currentWindowRect.Height() - currentViewRect.Height());

    const bool bToolbarsOnVideo = m_controls.ToolbarsCoverVideo();
    const bool bPanelsOnVideo = m_controls.PanelsCoverVideo();
    if (bPanelsOnVideo) {
        unsigned uTop, uLeft, uRight, uBottom;
        m_controls.GetVisibleDockZones(uTop, uLeft, uRight, uBottom);
        if (!bToolbarsOnVideo) {
            uBottom -= m_controls.GetVisibleToolbarsHeight();
        }
        controlsSize.cx -= uLeft + uRight;
        controlsSize.cy -= uTop + uBottom;
    } else if (bToolbarsOnVideo) {
        controlsSize.cy -= m_controls.GetVisibleToolbarsHeight();
    }

    CSize newWindowSize(lpRect->right - lpRect->left, lpRect->bottom - lpRect->top);

    newWindowSize -= controlsSize;

    switch (nSide) {
        case WMSZ_TOP:
        case WMSZ_BOTTOM:
            newWindowSize.cx = long(newWindowSize.cy * videoSize.cx / (double)videoSize.cy + 0.5);
            newWindowSize.cy = long(newWindowSize.cx * videoSize.cy / (double)videoSize.cx + 0.5);
            break;
        case WMSZ_TOPLEFT:
        case WMSZ_TOPRIGHT:
        case WMSZ_BOTTOMLEFT:
        case WMSZ_BOTTOMRIGHT:
        case WMSZ_LEFT:
        case WMSZ_RIGHT:
            newWindowSize.cy = long(newWindowSize.cx * videoSize.cy / (double)videoSize.cx + 0.5);
            newWindowSize.cx = long(newWindowSize.cy * videoSize.cx / (double)videoSize.cy + 0.5);
            break;
    }

    newWindowSize += controlsSize;

    switch (nSide) {
        case WMSZ_TOPLEFT:
            lpRect->left = lpRect->right - newWindowSize.cx;
            lpRect->top = lpRect->bottom - newWindowSize.cy;
            break;
        case WMSZ_TOP:
        case WMSZ_TOPRIGHT:
            lpRect->right = lpRect->left + newWindowSize.cx;
            lpRect->top = lpRect->bottom - newWindowSize.cy;
            break;
        case WMSZ_RIGHT:
        case WMSZ_BOTTOM:
        case WMSZ_BOTTOMRIGHT:
            lpRect->right = lpRect->left + newWindowSize.cx;
            lpRect->bottom = lpRect->top + newWindowSize.cy;
            break;
        case WMSZ_LEFT:
        case WMSZ_BOTTOMLEFT:
            lpRect->left = lpRect->right - newWindowSize.cx;
            lpRect->bottom = lpRect->top + newWindowSize.cy;
            break;
    }
}

void CMainFrame::OnExitSizeMove()
{
    if (m_wndView.Dragging()) {
        // HACK: windowed (not renderless) video renderers may not produce WM_MOUSEMOVE message here
        UpdateControlState(CMainFrame::UPDATE_CHILDVIEW_CURSOR_HACK);
    }
}

void CMainFrame::OnDisplayChange() // untested, not sure if it's working...
{
    TRACE(_T("*** CMainFrame::OnDisplayChange()\n"));

    if (GetLoadState() == MLS::LOADED) {
        if (m_pGraphThread) {
            CAMMsgEvent e;
            m_pGraphThread->PostThreadMessage(CGraphThread::TM_DISPLAY_CHANGE, 0, (LPARAM)&e);
            e.WaitMsg();
        } else {
            DisplayChange();
        }
    }

    if (IsD3DFullScreenMode()) {
        MONITORINFO MonitorInfo;
        HMONITOR    hMonitor;

        ZeroMemory(&MonitorInfo, sizeof(MonitorInfo));
        MonitorInfo.cbSize = sizeof(MonitorInfo);

        hMonitor = MonitorFromWindow(m_pFullscreenWnd->m_hWnd, 0);
        if (GetMonitorInfo(hMonitor, &MonitorInfo)) {
            CRect MonitorRect = CRect(MonitorInfo.rcMonitor);
            m_pFullscreenWnd->SetWindowPos(nullptr,
                                           MonitorRect.left,
                                           MonitorRect.top,
                                           MonitorRect.Width(),
                                           MonitorRect.Height(),
                                           SWP_NOZORDER);
            MoveVideoWindow();
        }
    }
}

void CMainFrame::OnWindowPosChanging(WINDOWPOS* lpwndpos)
{
    if (m_fFullScreen && !(lpwndpos->flags & SWP_NOMOVE)) {
        HMONITOR hm = MonitorFromPoint(CPoint(lpwndpos->x, lpwndpos->y), MONITOR_DEFAULTTONULL);
        MONITORINFO mi = { sizeof(mi) };
        if (GetMonitorInfo(hm, &mi)) {
            lpwndpos->flags &= ~SWP_NOSIZE;
            lpwndpos->cx = mi.rcMonitor.right - mi.rcMonitor.left;
            lpwndpos->cy = mi.rcMonitor.bottom - mi.rcMonitor.top;
            lpwndpos->x = mi.rcMonitor.left;
            lpwndpos->y = mi.rcMonitor.top;
        }
    }
    __super::OnWindowPosChanging(lpwndpos);
}

LRESULT CMainFrame::OnDpiChanged(WPARAM wParam, LPARAM lParam)
{
    m_dpi.Override(LOWORD(wParam), HIWORD(wParam));
    m_eventc.FireEvent(MpcEvent::DPI_CHANGED);
    CMPCThemeMenu::clearDimensions();
    if (!restoringWindowRect) { //do not adjust for DPI if restoring saved window position
        MoveWindow(reinterpret_cast<RECT*>(lParam));
    }
    RecalcLayout();
    m_wndPreView.ScaleFont();
    return 0;
}

void CMainFrame::OnSysCommand(UINT nID, LPARAM lParam)
{
    // Only stop screensaver if video playing; allow for audio only
    if ((GetMediaState() == State_Running && !m_fEndOfStream && !m_fAudioOnly)
            && (((nID & 0xFFF0) == SC_SCREENSAVE) || ((nID & 0xFFF0) == SC_MONITORPOWER))) {
        TRACE(_T("SC_SCREENSAVE, nID = %u, lParam = %d\n"), nID, lParam);
        return;
    }

    __super::OnSysCommand(nID, lParam);
}

void CMainFrame::OnActivateApp(BOOL bActive, DWORD dwThreadID)
{
    __super::OnActivateApp(bActive, dwThreadID);

    m_timerOneTime.Unsubscribe(TimerOneTimeSubscriber::PLACE_FULLSCREEN_UNDER_ACTIVE_WINDOW);

    if (m_fFullScreen) {
        if (bActive) {
            // keep the fullscreen window on top while it's active,
            // we don't want notification pop-ups to cover it
            SetWindowPos(&wndTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        } else {
            // don't keep the fullscreen window on top when it's not active,
            // we want to be able to switch to other windows nicely
            struct {
                void operator()() const {
                    CMainFrame* pMainFrame = AfxGetMainFrame();
                    if (!pMainFrame || !pMainFrame->m_fFullScreen || pMainFrame->WindowExpectedOnTop() || pMainFrame->m_bExtOnTop) {
                        return;
                    }
                    // place our window under the new active window
                    // when we can't determine that window, we try later
                    if (CWnd* pActiveWnd = GetForegroundWindow()) {
                        bool bMoved = false;
                        if (CWnd* pActiveRootWnd = pActiveWnd->GetAncestor(GA_ROOT)) {
                            const DWORD dwStyle = pActiveRootWnd->GetStyle();
                            const DWORD dwExStyle = pActiveRootWnd->GetExStyle();
                            if (!(dwStyle & WS_CHILD) && !(dwStyle & WS_POPUP) && !(dwExStyle & WS_EX_TOPMOST)) {
                                if (CWnd* pLastWnd = GetDesktopWindow()->GetTopWindow()) {
                                    while (CWnd* pWnd = pLastWnd->GetNextWindow(GW_HWNDNEXT)) {
                                        if (*pLastWnd == *pActiveRootWnd) {
                                            pMainFrame->SetWindowPos(
                                                pWnd, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                                            bMoved = true;
                                            break;
                                        }
                                        pLastWnd = pWnd;
                                    }
                                } else {
                                    ASSERT(FALSE);
                                }
                            }
                        }
                        if (!bMoved) {
                            pMainFrame->SetWindowPos(
                                &wndNoTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                        }
                    } else {
                        pMainFrame->m_timerOneTime.Subscribe(
                            TimerOneTimeSubscriber::PLACE_FULLSCREEN_UNDER_ACTIVE_WINDOW, *this, 1);
                    }
                }
            } placeUnder;
            placeUnder();
        }
    }
}

LRESULT CMainFrame::OnAppCommand(WPARAM wParam, LPARAM lParam)
{
    UINT cmd  = GET_APPCOMMAND_LPARAM(lParam);
    UINT uDevice = GET_DEVICE_LPARAM(lParam);

    if (uDevice != FAPPCOMMAND_OEM
            || cmd == APPCOMMAND_MEDIA_PLAY
            || cmd == APPCOMMAND_MEDIA_PAUSE
            || cmd == APPCOMMAND_MEDIA_CHANNEL_UP
            || cmd == APPCOMMAND_MEDIA_CHANNEL_DOWN
            || cmd == APPCOMMAND_MEDIA_RECORD
            || cmd == APPCOMMAND_MEDIA_FAST_FORWARD
            || cmd == APPCOMMAND_MEDIA_REWIND) {
        const CAppSettings& s = AfxGetAppSettings();

        BOOL fRet = FALSE;

        POSITION pos = s.wmcmds.GetHeadPosition();
        while (pos) {
            const wmcmd& wc = s.wmcmds.GetNext(pos);
            if (wc.appcmd == cmd && TRUE == SendMessage(WM_COMMAND, wc.cmd)) {
                fRet = TRUE;
            }
        }

        if (fRet) {
            return TRUE;
        }
    }

    return Default();
}

void CMainFrame::OnRawInput(UINT nInputcode, HRAWINPUT hRawInput)
{
    const CAppSettings& s = AfxGetAppSettings();
    UINT nMceCmd = AfxGetMyApp()->GetRemoteControlCode(nInputcode, hRawInput);

    switch (nMceCmd) {
        case MCE_DETAILS:
        case MCE_GUIDE:
        case MCE_TVJUMP:
        case MCE_STANDBY:
        case MCE_OEM1:
        case MCE_OEM2:
        case MCE_MYTV:
        case MCE_MYVIDEOS:
        case MCE_MYPICTURES:
        case MCE_MYMUSIC:
        case MCE_RECORDEDTV:
        case MCE_DVDANGLE:
        case MCE_DVDAUDIO:
        case MCE_DVDMENU:
        case MCE_DVDSUBTITLE:
        case MCE_RED:
        case MCE_GREEN:
        case MCE_YELLOW:
        case MCE_BLUE:
        case MCE_MEDIA_NEXTTRACK:
        case MCE_MEDIA_PREVIOUSTRACK:
            POSITION pos = s.wmcmds.GetHeadPosition();
            while (pos) {
                const wmcmd& wc = s.wmcmds.GetNext(pos);
                if (wc.appcmd == nMceCmd) {
                    SendMessage(WM_COMMAND, wc.cmd);
                    break;
                }
            }
            break;
    }
}

LRESULT CMainFrame::OnHotKey(WPARAM wParam, LPARAM lParam)
{
    const CAppSettings& s = AfxGetAppSettings();
    BOOL fRet = FALSE;

    if (GetActiveWindow() == this || s.fGlobalMedia == TRUE) {
        POSITION pos = s.wmcmds.GetHeadPosition();

        while (pos) {
            const wmcmd& wc = s.wmcmds.GetNext(pos);
            if (wc.appcmd == wParam && TRUE == SendMessage(WM_COMMAND, wc.cmd)) {
                fRet = TRUE;
            }
        }
    }

    return fRet;
}

bool g_bNoDuration = false;
bool g_bExternalSubtitleTime = false;
bool g_bExternalSubtitle = false;
double g_dRate = 1.0;

void CMainFrame::OnTimer(UINT_PTR nIDEvent)
{
    switch (nIDEvent) {
        case TIMER_WINDOW_FULLSCREEN:
            if (AfxGetAppSettings().iFullscreenDelay > 0 && IsWindows8OrGreater()) {//DWMWA_CLOAK not supported on 7
                BOOL setEnabled = FALSE;
                ::DwmSetWindowAttribute(m_hWnd, DWMWA_CLOAK, &setEnabled, sizeof(setEnabled));
            }
            KillTimer(TIMER_WINDOW_FULLSCREEN);
            delayingFullScreen = false;
            break;
        case TIMER_STREAMPOSPOLLER:
            ASSERT(streampospoller_active);
            if (streampospoller_active && GetLoadState() == MLS::LOADED) {
                REFERENCE_TIME rtNow = 0, rtDur = 0;
                switch (GetPlaybackMode()) {
                    case PM_FILE:
                        g_bExternalSubtitleTime = false;
                        if (m_pGB && m_pMS) {
                            m_pMS->GetCurrentPosition(&rtNow);
                            if (!m_pGB || !m_pMS) return; // can happen very rarely due to race condition
                            m_pMS->GetDuration(&rtDur);

                            if (abRepeatPositionBEnabled && rtNow >= abRepeatPositionB) {
                                PerformABRepeat();
                                return;
                            }

                            if (m_bRememberFilePos && !m_fEndOfStream) {
                                CFilePositionList& fp = AfxGetAppSettings().filePositions;
                                FILE_POSITION* filePosition = fp.GetLatestEntry();
                                if (filePosition) {
                                    bool bSave = std::abs(filePosition->llPosition - rtNow) > 300000000;
                                    filePosition->llPosition = rtNow;
                                    if (bSave) {
                                        fp.SaveLatestEntry();
                                    }
                                }
                            }

                            // Casimir666 : autosave subtitle sync after play
                            if (m_nCurSubtitle >= 0 && m_rtCurSubPos != rtNow) {
                                if (m_lSubtitleShift) {
                                    if (m_wndSubresyncBar.SaveToDisk()) {
                                        m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_AG_SUBTITLES_SAVED), 500);
                                    } else {
                                        m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_MAINFRM_4));
                                    }
                                }
                                m_nCurSubtitle = -1;
                                m_lSubtitleShift = 0;
                            }

                            m_wndStatusBar.SetStatusTimer(rtNow, rtDur, IsSubresyncBarVisible(), GetTimeFormat());
                        }
                        break;
                    case PM_DVD:
                        g_bExternalSubtitleTime = true;
                        if (m_pDVDI) {
                            DVD_PLAYBACK_LOCATION2 Location;
                            if (m_pDVDI->GetCurrentLocation(&Location) == S_OK) {
                                double fps = Location.TimeCodeFlags == DVD_TC_FLAG_25fps ? 25.0
                                             : Location.TimeCodeFlags == DVD_TC_FLAG_30fps ? 30.0
                                             : Location.TimeCodeFlags == DVD_TC_FLAG_DropFrame ? 30 / 1.001
                                             : 25.0;

                                rtNow = HMSF2RT(Location.TimeCode, fps);

                                if (abRepeatPositionBEnabled && rtNow >= abRepeatPositionB) {
                                    PerformABRepeat();
                                    return;
                                }

                                DVD_HMSF_TIMECODE tcDur;
                                ULONG ulFlags;
                                if (SUCCEEDED(m_pDVDI->GetTotalTitleTime(&tcDur, &ulFlags))) {
                                    rtDur = HMSF2RT(tcDur, fps);
                                }
                                if (m_pSubClock) {
                                    m_pSubClock->SetTime(rtNow);
                                }
                            }
                        }
                        m_wndStatusBar.SetStatusTimer(rtNow, rtDur, IsSubresyncBarVisible(), GetTimeFormat());
                        break;
                    case PM_ANALOG_CAPTURE:
                        g_bExternalSubtitleTime = true;
                        if (m_fCapturing) {
                            if (m_wndCaptureBar.m_capdlg.m_pMux) {
                                CComQIPtr<IMediaSeeking> pMuxMS = m_wndCaptureBar.m_capdlg.m_pMux;
                                if (!pMuxMS || FAILED(pMuxMS->GetCurrentPosition(&rtNow))) {
                                    if (m_pMS) {
                                        m_pMS->GetCurrentPosition(&rtNow);
                                    }
                                }
                            }
                            if (m_rtDurationOverride >= 0) {
                                rtDur = m_rtDurationOverride;
                            }
                        }
                        break;
                    case PM_DIGITAL_CAPTURE:
                        g_bExternalSubtitleTime = true;
                        m_pMS->GetCurrentPosition(&rtNow);
                        break;
                    default:
                        ASSERT(FALSE);
                        break;
                }

                g_bNoDuration = rtDur <= 0;
                m_wndSeekBar.Enable(!g_bNoDuration);
                m_wndSeekBar.SetRange(0, rtDur);
                m_wndSeekBar.SetPos(rtNow);
                m_OSD.SetRange(0, rtDur);
                m_OSD.SetPos(rtNow);
                m_Lcd.SetMediaRange(0, rtDur);
                m_Lcd.SetMediaPos(rtNow);

                if (m_pCAP) {
                    if (g_bExternalSubtitleTime) {
                        m_pCAP->SetTime(rtNow);
                    }
                    m_wndSubresyncBar.SetTime(rtNow);
                    m_wndSubresyncBar.SetFPS(m_pCAP->GetFPS());
                }
                if (g_bExternalSubtitleTime && (m_iStreamPosPollerInterval > 40)) {
                    AdjustStreamPosPoller(true);
                }
            }
            break;
        case TIMER_STREAMPOSPOLLER2:
            ASSERT(streampospoller_active);
            if (streampospoller_active && GetLoadState() == MLS::LOADED) {
                switch (GetPlaybackMode()) {
                    case PM_FILE:
                    // no break
                    case PM_DVD:
                        if (m_bOSDDisplayTime && m_OSD.CanShowMessage()) {
                            m_OSD.DisplayMessage(OSD_TOPLEFT, m_wndStatusBar.GetStatusTimer());
                        }
                        break;
                    case PM_DIGITAL_CAPTURE: {
                        EventDescriptor& NowNext = m_pDVBState->NowNext;
                        time_t tNow;
                        time(&tNow);
                        if (NowNext.duration > 0 && tNow >= NowNext.startTime && tNow <= NowNext.startTime + NowNext.duration) {
                            REFERENCE_TIME rtNow = REFERENCE_TIME(tNow - NowNext.startTime) * 10000000;
                            REFERENCE_TIME rtDur = REFERENCE_TIME(NowNext.duration) * 10000000;
                            m_wndStatusBar.SetStatusTimer(rtNow, rtDur, false, TIME_FORMAT_MEDIA_TIME);
                            if (m_bOSDDisplayTime && m_OSD.CanShowMessage()) {
                                m_OSD.DisplayMessage(OSD_TOPLEFT, m_wndStatusBar.GetStatusTimer());
                            }
                        } else {
                            m_wndStatusBar.SetStatusTimer(ResStr(IDS_CAPTURE_LIVE));
                        }
                    }
                    break;
                    case PM_ANALOG_CAPTURE:
                        if (!m_fCapturing) {
                            CString str(StrRes(IDS_CAPTURE_LIVE));
                            long lChannel = 0, lVivSub = 0, lAudSub = 0;
                            if (m_pAMTuner
                                    && m_wndCaptureBar.m_capdlg.IsTunerActive()
                                    && SUCCEEDED(m_pAMTuner->get_Channel(&lChannel, &lVivSub, &lAudSub))) {
                                str.AppendFormat(_T(" (ch%ld)"), lChannel);
                            }
                            m_wndStatusBar.SetStatusTimer(str);
                        }
                        break;
                    default:
                        ASSERT(FALSE);
                        break;
                }
            }
            break;
        case TIMER_STATS: {
            CString rate;
            rate.Format(_T("%.2fx"), m_dSpeedRate);
            if (m_pQP) {
                CString info;
                int tmp, tmp1;

                if (SUCCEEDED(m_pQP->get_AvgFrameRate(&tmp))) { // We hang here due to a lock that never gets released.
                    info.Format(_T("%d.%02d (%s)"), tmp / 100, tmp % 100, rate.GetString());
                } else {
                    info = _T("-");
                }
                m_wndStatsBar.SetLine(StrRes(IDS_AG_FRAMERATE), info);

                if (SUCCEEDED(m_pQP->get_AvgSyncOffset(&tmp))
                        && SUCCEEDED(m_pQP->get_DevSyncOffset(&tmp1))) {
                    info.Format(IDS_STATSBAR_SYNC_OFFSET_FORMAT, tmp, tmp1);
                } else {
                    info = _T("-");
                }
                m_wndStatsBar.SetLine(StrRes(IDS_STATSBAR_SYNC_OFFSET), info);

                if (SUCCEEDED(m_pQP->get_FramesDrawn(&tmp))
                        && SUCCEEDED(m_pQP->get_FramesDroppedInRenderer(&tmp1))) {
                    info.Format(IDS_MAINFRM_6, tmp, tmp1);
                } else {
                    info = _T("-");
                }
                m_wndStatsBar.SetLine(StrRes(IDS_AG_FRAMES), info);

                if (SUCCEEDED(m_pQP->get_Jitter(&tmp))) {
                    info.Format(_T("%d ms"), tmp);
                } else {
                    info = _T("-");
                }
                m_wndStatsBar.SetLine(StrRes(IDS_STATSBAR_JITTER), info);
            } else {
                m_wndStatsBar.SetLine(StrRes(IDS_STATSBAR_PLAYBACK_RATE), rate);
            }

            if (m_pBI) {
                CString sInfo;

                for (int i = 0, j = m_pBI->GetCount(); i < j; i++) {
                    int samples, size;
                    if (S_OK == m_pBI->GetStatus(i, samples, size)) {
                        sInfo.AppendFormat(_T("[%d]: %03d/%d KB "), i, samples, size / 1024);
                    }
                }

                if (!sInfo.IsEmpty()) {
                    sInfo.AppendFormat(_T("(p%lu)"), m_pBI->GetPriority());
                    m_wndStatsBar.SetLine(StrRes(IDS_AG_BUFFERS), sInfo);
                }
            }

            {
                // IBitRateInfo
                CString sInfo;
                BeginEnumFilters(m_pGB, pEF, pBF) {
                    unsigned i = 0;
                    BeginEnumPins(pBF, pEP, pPin) {
                        if (CComQIPtr<IBitRateInfo> pBRI = pPin) {
                            DWORD nAvg = pBRI->GetAverageBitRate() / 1000;

                            if (nAvg > 0) {
                                sInfo.AppendFormat(_T("[%u]: %lu/%lu kb/s "), i, nAvg, pBRI->GetCurrentBitRate() / 1000);
                            }
                        }
                        i++;
                    }
                    EndEnumPins;

                    if (!sInfo.IsEmpty()) {
                        m_wndStatsBar.SetLine(StrRes(IDS_STATSBAR_BITRATE), sInfo + ResStr(IDS_STATSBAR_BITRATE_AVG_CUR));
                        sInfo.Empty();
                    }
                }
                EndEnumFilters;
            }

            if (GetPlaybackMode() == PM_DVD) { // we also use this timer to update the info panel for DVD playback
                ULONG ulAvailable, ulCurrent;

                // Location

                CString Location(_T('-'));

                DVD_PLAYBACK_LOCATION2 loc;
                ULONG ulNumOfVolumes, ulVolume;
                DVD_DISC_SIDE Side;
                ULONG ulNumOfTitles;
                ULONG ulNumOfChapters;

                if (SUCCEEDED(m_pDVDI->GetCurrentLocation(&loc))
                        && SUCCEEDED(m_pDVDI->GetNumberOfChapters(loc.TitleNum, &ulNumOfChapters))
                        && SUCCEEDED(m_pDVDI->GetDVDVolumeInfo(&ulNumOfVolumes, &ulVolume, &Side, &ulNumOfTitles))) {
                    Location.Format(IDS_MAINFRM_9,
                                    ulVolume, ulNumOfVolumes,
                                    loc.TitleNum, ulNumOfTitles,
                                    loc.ChapterNum, ulNumOfChapters);
                    ULONG tsec = (loc.TimeCode.bHours * 3600)
                                 + (loc.TimeCode.bMinutes * 60)
                                 + (loc.TimeCode.bSeconds);
                    /* This might not always work, such as on resume */
                    if (loc.ChapterNum != m_lCurrentChapter) {
                        m_lCurrentChapter = loc.ChapterNum;
                        m_lChapterStartTime = tsec;
                    } else {
                        /* If a resume point was used, and the user chapter jumps,
                        then it might do some funky time jumping.  Try to 'fix' the
                        chapter start time if this happens */
                        if (m_lChapterStartTime > tsec) {
                            m_lChapterStartTime = tsec;
                        }
                    }
                }

                m_wndInfoBar.SetLine(StrRes(IDS_INFOBAR_LOCATION), Location);

                // Video

                CString Video(_T('-'));

                DVD_VideoAttributes VATR;

                if (SUCCEEDED(m_pDVDI->GetCurrentAngle(&ulAvailable, &ulCurrent))
                        && SUCCEEDED(m_pDVDI->GetCurrentVideoAttributes(&VATR))) {
                    Video.Format(IDS_MAINFRM_10,
                                 ulCurrent, ulAvailable,
                                 VATR.ulSourceResolutionX, VATR.ulSourceResolutionY, VATR.ulFrameRate,
                                 VATR.ulAspectX, VATR.ulAspectY);
                }

                m_wndInfoBar.SetLine(StrRes(IDS_INFOBAR_VIDEO), Video);

                // Audio

                CString Audio(_T('-'));

                DVD_AudioAttributes AATR;

                if (SUCCEEDED(m_pDVDI->GetCurrentAudio(&ulAvailable, &ulCurrent))
                        && SUCCEEDED(m_pDVDI->GetAudioAttributes(ulCurrent, &AATR))) {
                    CString lang;
                    if (AATR.Language) {
                        GetLocaleString(AATR.Language, LOCALE_SENGLANGUAGE, lang);
                    } else {
                        lang.Format(IDS_AG_UNKNOWN, ulCurrent + 1);
                    }

                    switch (AATR.LanguageExtension) {
                        case DVD_AUD_EXT_NotSpecified:
                        default:
                            break;
                        case DVD_AUD_EXT_Captions:
                            lang += _T(" (Captions)");
                            break;
                        case DVD_AUD_EXT_VisuallyImpaired:
                            lang += _T(" (Visually Impaired)");
                            break;
                        case DVD_AUD_EXT_DirectorComments1:
                            lang += _T(" (Director Comments 1)");
                            break;
                        case DVD_AUD_EXT_DirectorComments2:
                            lang += _T(" (Director Comments 2)");
                            break;
                    }

                    CString format = GetDVDAudioFormatName(AATR);

                    Audio.Format(IDS_MAINFRM_11,
                                 lang.GetString(),
                                 format.GetString(),
                                 AATR.dwFrequency,
                                 AATR.bQuantization,
                                 AATR.bNumberOfChannels,
                                 ResStr(AATR.bNumberOfChannels > 1 ? IDS_MAINFRM_13 : IDS_MAINFRM_12).GetString());

                    m_wndStatusBar.SetStatusBitmap(
                        AATR.bNumberOfChannels == 1 ? IDB_AUDIOTYPE_MONO
                        : AATR.bNumberOfChannels >= 2 ? IDB_AUDIOTYPE_STEREO
                        : IDB_AUDIOTYPE_NOAUDIO);
                }

                m_wndInfoBar.SetLine(StrRes(IDS_INFOBAR_AUDIO), Audio);

                // Subtitles

                CString Subtitles(_T('-'));

                BOOL bIsDisabled;
                DVD_SubpictureAttributes SATR;

                if (SUCCEEDED(m_pDVDI->GetCurrentSubpicture(&ulAvailable, &ulCurrent, &bIsDisabled))
                        && SUCCEEDED(m_pDVDI->GetSubpictureAttributes(ulCurrent, &SATR))) {
                    CString lang;
                    GetLocaleString(SATR.Language, LOCALE_SENGLANGUAGE, lang);

                    switch (SATR.LanguageExtension) {
                        case DVD_SP_EXT_NotSpecified:
                        default:
                            break;
                        case DVD_SP_EXT_Caption_Normal:
                            lang += _T("");
                            break;
                        case DVD_SP_EXT_Caption_Big:
                            lang += _T(" (Big)");
                            break;
                        case DVD_SP_EXT_Caption_Children:
                            lang += _T(" (Children)");
                            break;
                        case DVD_SP_EXT_CC_Normal:
                            lang += _T(" (CC)");
                            break;
                        case DVD_SP_EXT_CC_Big:
                            lang += _T(" (CC Big)");
                            break;
                        case DVD_SP_EXT_CC_Children:
                            lang += _T(" (CC Children)");
                            break;
                        case DVD_SP_EXT_Forced:
                            lang += _T(" (Forced)");
                            break;
                        case DVD_SP_EXT_DirectorComments_Normal:
                            lang += _T(" (Director Comments)");
                            break;
                        case DVD_SP_EXT_DirectorComments_Big:
                            lang += _T(" (Director Comments, Big)");
                            break;
                        case DVD_SP_EXT_DirectorComments_Children:
                            lang += _T(" (Director Comments, Children)");
                            break;
                    }

                    if (bIsDisabled) {
                        lang = _T("-");
                    }

                    Subtitles.Format(_T("%s"),
                                     lang.GetString());
                }

                m_wndInfoBar.SetLine(StrRes(IDS_INFOBAR_SUBTITLES), Subtitles);
            } else if (GetPlaybackMode() == PM_DIGITAL_CAPTURE) {
                if (m_pDVBState->bActive) {
                    CComQIPtr<IBDATuner> pTun = m_pGB;
                    BOOLEAN bPresent, bLocked;
                    LONG lDbStrength, lPercentQuality;
                    CString Signal;

                    if (SUCCEEDED(pTun->GetStats(bPresent, bLocked, lDbStrength, lPercentQuality)) && bPresent) {
                        Signal.Format(IDS_STATSBAR_SIGNAL_FORMAT, (int)lDbStrength, lPercentQuality);
                        m_wndStatsBar.SetLine(StrRes(IDS_STATSBAR_SIGNAL), Signal);
                    }
                } else {
                    m_wndStatsBar.SetLine(StrRes(IDS_STATSBAR_SIGNAL), _T("-"));
                }
            } else if (GetPlaybackMode() == PM_FILE) {
                OpenSetupInfoBar(false);
                const CAppSettings& s = AfxGetAppSettings();
                if (s.iTitleBarTextStyle == 1 && s.fTitleBarTextTitle) {
                    OpenSetupWindowTitle();
                }
                SendNowPlayingToSkype();
                SendNowPlayingToApi();
            }

            if (GetMediaState() == State_Running && !m_fAudioOnly) {
                BOOL fActive = FALSE;
                if (SystemParametersInfo(SPI_GETSCREENSAVEACTIVE, 0, &fActive, 0) && fActive) {
                    SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, FALSE,   nullptr, SPIF_SENDWININICHANGE);
                    SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, fActive, nullptr, SPIF_SENDWININICHANGE);
                }

                // prevent screensaver activate, monitor sleep/turn off after playback
                SetThreadExecutionState(ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);

                UpdateDXVAStatus();
            }
        }
        break;
        case TIMER_UNLOAD_UNUSED_EXTERNAL_OBJECTS: {
            if (GetPlaybackMode() == PM_NONE) {
                if (UnloadUnusedExternalObjects()) {
                    KillTimer(TIMER_UNLOAD_UNUSED_EXTERNAL_OBJECTS);
                }
            }
        }
        break;
        case TIMER_32HZ:
            m_timer32Hz.NotifySubscribers();
            break;
        case TIMER_DELAYEDSEEK:
            KillTimer(TIMER_DELAYEDSEEK);
            if (queuedSeek.seekTime > 0) {
                SeekTo(queuedSeek.rtPos, queuedSeek.bShowOSD);
            }
            break;
        default:
            if (nIDEvent >= TIMER_ONETIME_START && nIDEvent <= TIMER_ONETIME_END) {
                m_timerOneTime.NotifySubscribers(nIDEvent);
            } else {
                ASSERT(FALSE);
            }
    }

    __super::OnTimer(nIDEvent);
}

void CMainFrame::DoAfterPlaybackEvent()
{
    CAppSettings& s = AfxGetAppSettings();
    bool bExitFullScreen = false;
    bool bNoMoreMedia = false;

    if (s.nCLSwitches & CLSW_DONOTHING) {
        // Do nothing
    } else if (s.nCLSwitches & CLSW_CLOSE) {
        SendMessage(WM_COMMAND, ID_FILE_EXIT);
    } else if (s.nCLSwitches & CLSW_MONITOROFF) {
        m_fEndOfStream = true;
        bExitFullScreen = true;
        SetThreadExecutionState(ES_CONTINUOUS);
        SendMessage(WM_SYSCOMMAND, SC_MONITORPOWER, 2);
    } else if (s.nCLSwitches & CLSW_STANDBY) {
        SetPrivilege(SE_SHUTDOWN_NAME);
        SetSystemPowerState(TRUE, FALSE);
        SendMessage(WM_COMMAND, ID_FILE_EXIT); // Recheck if this is still needed after switching to new toolset and SetSuspendState()
    } else if (s.nCLSwitches & CLSW_HIBERNATE) {
        SetPrivilege(SE_SHUTDOWN_NAME);
        SetSystemPowerState(FALSE, FALSE);
        SendMessage(WM_COMMAND, ID_FILE_EXIT);
    } else if (s.nCLSwitches & CLSW_SHUTDOWN) {
        SetPrivilege(SE_SHUTDOWN_NAME);
        InitiateSystemShutdownEx(nullptr, nullptr, 0, TRUE, FALSE,
                                 SHTDN_REASON_MAJOR_APPLICATION | SHTDN_REASON_MINOR_MAINTENANCE | SHTDN_REASON_FLAG_PLANNED);
        SendMessage(WM_COMMAND, ID_FILE_EXIT);
    } else if (s.nCLSwitches & CLSW_LOGOFF) {
        SetPrivilege(SE_SHUTDOWN_NAME);
        ExitWindowsEx(EWX_LOGOFF | EWX_FORCEIFHUNG, 0);
        SendMessage(WM_COMMAND, ID_FILE_EXIT);
    } else if (s.nCLSwitches & CLSW_LOCK) {
        m_fEndOfStream = true;
        bExitFullScreen = true;
        LockWorkStation();
    } else if (s.nCLSwitches & CLSW_PLAYNEXT) {
        if (!SearchInDir(true, (s.fLoopForever || m_nLoops < s.nLoops || s.bLoopFolderOnPlayNextFile))) {
            m_fEndOfStream = true;
            bExitFullScreen = true;
            bNoMoreMedia = true;
        }
    } else {
        switch (s.eAfterPlayback) {
            case CAppSettings::AfterPlayback::PLAY_NEXT:
                if (!SearchInDir(true, s.bLoopFolderOnPlayNextFile)) {
                    m_fEndOfStream = true;
                    bExitFullScreen = true;
                    bNoMoreMedia = true;
                }
                break;
            case CAppSettings::AfterPlayback::REWIND:
                bExitFullScreen = true;
                if (m_wndPlaylistBar.GetCount() > 1) {
                    s.nCLSwitches |= CLSW_OPEN;
                    PostMessage(WM_COMMAND, ID_NAVIGATE_SKIPFORWARD);
                } else {
                    SendMessage(WM_COMMAND, ID_PLAY_STOP);
                }
                break;
            case CAppSettings::AfterPlayback::MONITOROFF:
                m_fEndOfStream = true;
                bExitFullScreen = true;
                SetThreadExecutionState(ES_CONTINUOUS);
                SendMessage(WM_SYSCOMMAND, SC_MONITORPOWER, 2);
                break;
            case CAppSettings::AfterPlayback::CLOSE:
                SendMessage(WM_COMMAND, ID_FILE_CLOSE_AND_RESTORE);
                break;
            case CAppSettings::AfterPlayback::EXIT:
                SendMessage(WM_COMMAND, ID_FILE_EXIT);
                break;
            default:
                m_fEndOfStream = true;
                bExitFullScreen = true;
                break;
        }
    }

    if (AfxGetMyApp()->m_fClosingState) {
        return;
    }

    if (m_fEndOfStream) {
        m_OSD.EnableShowMessage(false);
        SendMessage(WM_COMMAND, ID_PLAY_PAUSE);
        m_OSD.EnableShowMessage();
        if (bNoMoreMedia) {
            m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_NO_MORE_MEDIA));
        }
    }

    if (bExitFullScreen && (m_fFullScreen || IsD3DFullScreenMode()) && s.fExitFullScreenAtTheEnd) {
        OnViewFullscreen();
    }
}

void CMainFrame::OnUpdateABRepeat(CCmdUI* pCmdUI) {
    bool canABRepeat = GetPlaybackMode() == PM_FILE || GetPlaybackMode() == PM_DVD;
    bool abRepeatActive = abRepeatPositionAEnabled || abRepeatPositionBEnabled;

    switch (pCmdUI->m_nID) {
    case ID_PLAY_REPEAT_AB:
        pCmdUI->Enable(canABRepeat && abRepeatActive);
        break;
    case ID_PLAY_REPEAT_AB_MARK_A:
        if (pCmdUI->m_pMenu) {
            pCmdUI->m_pMenu->CheckMenuItem(ID_PLAY_REPEAT_AB_MARK_A, MF_BYCOMMAND | (abRepeatPositionAEnabled ? MF_CHECKED : MF_UNCHECKED));
        }
        pCmdUI->Enable(canABRepeat);
        break;
    case ID_PLAY_REPEAT_AB_MARK_B:
        if (pCmdUI->m_pMenu) {
            pCmdUI->m_pMenu->CheckMenuItem(ID_PLAY_REPEAT_AB_MARK_B, MF_BYCOMMAND | (abRepeatPositionBEnabled ? MF_CHECKED : MF_UNCHECKED));
        }
        pCmdUI->Enable(canABRepeat);
        break;
    default:
        ASSERT(FALSE);
        return;
    }
}


void CMainFrame::OnABRepeat(UINT nID) {
    switch (nID) {
    case ID_PLAY_REPEAT_AB:
        if (abRepeatPositionAEnabled || abRepeatPositionBEnabled) { //only support disabling from the menu
            abRepeatPositionAEnabled = false;
            abRepeatPositionBEnabled = false;
            abRepeatPositionA = 0;
            abRepeatPositionB = 0;
            m_wndSeekBar.Invalidate();
        }
        break;
    case ID_PLAY_REPEAT_AB_MARK_A:
    case ID_PLAY_REPEAT_AB_MARK_B:
        REFERENCE_TIME rtDur = 0;
        int playmode = GetPlaybackMode();

        if (playmode == PM_FILE && m_pMS) {
            m_pMS->GetDuration(&rtDur);
        } else if (playmode == PM_DVD && m_pDVDI) {
            DVD_PLAYBACK_LOCATION2 Location;
            if (m_pDVDI->GetCurrentLocation(&Location) == S_OK) {
                double fps = Location.TimeCodeFlags == DVD_TC_FLAG_25fps ? 25.0
                    : Location.TimeCodeFlags == DVD_TC_FLAG_30fps ? 30.0
                    : Location.TimeCodeFlags == DVD_TC_FLAG_DropFrame ? 30 / 1.001
                    : 25.0;
                DVD_HMSF_TIMECODE tcDur;
                ULONG ulFlags;
                if (SUCCEEDED(m_pDVDI->GetTotalTitleTime(&tcDur, &ulFlags))) {
                    rtDur = HMSF2RT(tcDur, fps);
                }
            }
        } else {
            return;
        }

        if (nID == ID_PLAY_REPEAT_AB_MARK_A) {
            if (abRepeatPositionAEnabled) {
                abRepeatPositionAEnabled = false;
                abRepeatPositionA = 0;
            } else if (SUCCEEDED(m_pMS->GetCurrentPosition(&abRepeatPositionA))) {
                if (abRepeatPositionA < rtDur) {
                    abRepeatPositionAEnabled = true;
                    if (abRepeatPositionBEnabled && abRepeatPositionA >= abRepeatPositionB) {
                        abRepeatPositionBEnabled = false;
                        abRepeatPositionB = 0;
                    }
                } else {
                    abRepeatPositionA = 0;
                }
            }
        } else if (nID == ID_PLAY_REPEAT_AB_MARK_B) {
            if (abRepeatPositionBEnabled) {
                abRepeatPositionBEnabled = false;
                abRepeatPositionB = 0;
            } else if (SUCCEEDED(m_pMS->GetCurrentPosition(&abRepeatPositionB))) {
                if (abRepeatPositionB > 0 && abRepeatPositionB > abRepeatPositionA && rtDur >= abRepeatPositionB) {
                    abRepeatPositionBEnabled = true;
                    if (GetMediaState() == State_Running) {
                        PerformABRepeat(); //we just set loop point B, so we need to repeat right now
                    }
                } else {
                    abRepeatPositionB = 0;
                }
            }
        }

        m_wndSeekBar.Invalidate();
        break;
    }
}

void CMainFrame::PerformABRepeat() {
    SeekTo(abRepeatPositionA);

    if (GetMediaState() == State_Stopped) {
        SendMessage(WM_COMMAND, ID_PLAY_PLAY);
    }
}

void CMainFrame::DisableABRepeat() {
    abRepeatPositionAEnabled = false;
    abRepeatPositionBEnabled = false;
    abRepeatPositionA = 0;
    abRepeatPositionB = 0;

    m_wndSeekBar.Invalidate();
}

bool CMainFrame::CheckABRepeat(REFERENCE_TIME& aPos, REFERENCE_TIME& bPos, bool& aEnabled, bool& bEnabled) {
    if (GetPlaybackMode() == PM_FILE || GetPlaybackMode() == PM_DVD) {
        if (abRepeatPositionAEnabled || abRepeatPositionBEnabled) {
            aPos = abRepeatPositionA;
            bPos = abRepeatPositionB;
            aEnabled = abRepeatPositionAEnabled;
            bEnabled = abRepeatPositionBEnabled;
            return true;
        }
    }
    return false;
}

//
// graph event EC_COMPLETE handler
//
void CMainFrame::GraphEventComplete()
{
    CAppSettings& s = AfxGetAppSettings();

    if (m_bRememberFilePos) {
        FILE_POSITION* filePosition = s.filePositions.GetLatestEntry();

        if (filePosition) {
            filePosition->llPosition = 0;
            s.filePositions.SaveLatestEntry();
        }
    }

    bool bBreak = false;
    if (m_wndPlaylistBar.IsAtEnd() || s.eLoopMode == CAppSettings::LoopMode::FILE) {
        ++m_nLoops;
        bBreak = !!(s.nCLSwitches & CLSW_AFTERPLAYBACK_MASK);
    }

    if (abRepeatPositionAEnabled || abRepeatPositionBEnabled) {
        PerformABRepeat();
    } else if (s.fLoopForever || m_nLoops < s.nLoops) {
        if (bBreak) {
            DoAfterPlaybackEvent();
        } else if ((m_wndPlaylistBar.GetCount() > 1) && (s.eLoopMode == CAppSettings::LoopMode::PLAYLIST)) {
            int nLoops = m_nLoops;
            SendMessage(WM_COMMAND, ID_NAVIGATE_SKIPFORWARDFILE);
            m_nLoops = nLoops;
        } else {
            if (GetMediaState() == State_Stopped) {
                SendMessage(WM_COMMAND, ID_PLAY_PLAY);
            } else {
                REFERENCE_TIME rtPos = 0;
                m_pMS->SetPositions(&rtPos, AM_SEEKING_AbsolutePositioning, nullptr, AM_SEEKING_NoPositioning);
                if (GetMediaState() == State_Paused) {
                    SendMessage(WM_COMMAND, ID_PLAY_PLAY);
                }
            }
        }
    } else {
        DoAfterPlaybackEvent();
    }
}

//
// our WM_GRAPHNOTIFY handler
//

LRESULT CMainFrame::OnGraphNotify(WPARAM wParam, LPARAM lParam)
{
    CAppSettings& s = AfxGetAppSettings();
    HRESULT hr = S_OK;

    LONG evCode = 0;
    LONG_PTR evParam1, evParam2;
    while (!AfxGetMyApp()->m_fClosingState && m_pME && SUCCEEDED(m_pME->GetEvent(&evCode, &evParam1, &evParam2, 0))) {
#ifdef _DEBUG
        if (evCode != EC_DVD_CURRENT_HMSF_TIME) {
            TRACE(_T("--> CMainFrame::OnGraphNotify on thread: %lu; event: 0x%08x (%ws)\n"), GetCurrentThreadId(), evCode, GetEventString(evCode));
        }
#endif
        CString str;

        if (m_fCustomGraph) {
            if (EC_BG_ERROR == evCode) {
                str = CString((char*)evParam1);
            }
        }

        hr = m_pME->FreeEventParams(evCode, evParam1, evParam2);

        CComPtr<IDvdState> pStateData;
        switch (evCode) {
            case EC_COMPLETE:
                GraphEventComplete();
                break;
            case EC_ERRORABORT:
                TRACE(_T("\thr = %08x\n"), (HRESULT)evParam1);
                break;
            case EC_BUFFERING_DATA:
                TRACE(_T("\tBuffering data = %s\n"), evParam1 ? _T("true") : _T("false"));

                m_bBuffering = !!evParam1;
                break;
            case EC_STEP_COMPLETE:
                if (m_fFrameSteppingActive) {
                    m_nStepForwardCount++;
                    m_fFrameSteppingActive = false;
                    m_pBA->put_Volume(m_nVolumeBeforeFrameStepping);
                }
                break;
            case EC_DEVICE_LOST:
                if (evParam2 == 0) {
                    // Device lost
                    if (GetPlaybackMode() == PM_ANALOG_CAPTURE) {
                        CComQIPtr<IBaseFilter> pBF = (IUnknown*)evParam1;
                        if (!m_pVidCap && m_pVidCap == pBF || !m_pAudCap && m_pAudCap == pBF) {
                            SendMessage(WM_COMMAND, ID_FILE_CLOSE_AND_RESTORE);
                        }
                    } else if (GetPlaybackMode() == PM_DIGITAL_CAPTURE) {
                        SendMessage(WM_COMMAND, ID_FILE_CLOSE_AND_RESTORE);
                    }
                }
                break;
            case EC_DVD_TITLE_CHANGE: {
                if (GetPlaybackMode() == PM_FILE) {
                    SetupChapters();
                } else if (GetPlaybackMode() == PM_DVD) {
                    m_iDVDTitle = (DWORD)evParam1;

                    // Save current chapter
                    DVD_POSITION* dvdPosition = s.dvdPositions.GetLatestEntry();
                    if (dvdPosition) {
                        dvdPosition->lTitle = m_iDVDTitle;
                    }

                    if (m_iDVDDomain == DVD_DOMAIN_Title) {
                        CString Domain;
                        Domain.Format(IDS_AG_TITLE, m_iDVDTitle);
                        m_wndInfoBar.SetLine(StrRes(IDS_INFOBAR_DOMAIN), Domain);
                    }

                    SetupDVDChapters();
                }
            }
            break;
            case EC_DVD_DOMAIN_CHANGE: {
                m_iDVDDomain = (DVD_DOMAIN)evParam1;

                OpenDVDData* pDVDData = dynamic_cast<OpenDVDData*>(m_lastOMD.m_p);
                ASSERT(pDVDData);

                CString Domain(_T('-'));

                switch (m_iDVDDomain) {
                    case DVD_DOMAIN_FirstPlay:
                        ULONGLONG llDVDGuid;

                        Domain = _T("First Play");

                        if (s.fShowDebugInfo) {
                            m_OSD.DebugMessage(_T("%s"), Domain.GetString());
                        }

                        if (m_pDVDI && SUCCEEDED(m_pDVDI->GetDiscID(nullptr, &llDVDGuid))) {
                            m_fValidDVDOpen = true;

                            if (s.fShowDebugInfo) {
                                m_OSD.DebugMessage(_T("DVD Title: %lu"), s.lDVDTitle);
                            }

                            if (s.lDVDTitle != 0) {
                                s.dvdPositions.AddEntry(llDVDGuid);
                                // Set command line position
                                hr = m_pDVDC->PlayTitle(s.lDVDTitle, DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, nullptr);
                                if (s.fShowDebugInfo) {
                                    m_OSD.DebugMessage(_T("PlayTitle: 0x%08X"), hr);
                                    m_OSD.DebugMessage(_T("DVD Chapter: %lu"), s.lDVDChapter);
                                }

                                if (s.lDVDChapter > 1) {
                                    hr = m_pDVDC->PlayChapterInTitle(s.lDVDTitle, s.lDVDChapter, DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, nullptr);
                                    if (s.fShowDebugInfo) {
                                        m_OSD.DebugMessage(_T("PlayChapterInTitle: 0x%08X"), hr);
                                    }
                                } else {
                                    // Trick: skip trailers with some DVDs
                                    hr = m_pDVDC->Resume(DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, nullptr);
                                    if (s.fShowDebugInfo) {
                                        m_OSD.DebugMessage(_T("Resume: 0x%08X"), hr);
                                    }

                                    // If the resume call succeeded, then we skip PlayChapterInTitle
                                    // and PlayAtTimeInTitle.
                                    if (hr == S_OK) {
                                        // This might fail if the Title is not available yet?
                                        hr = m_pDVDC->PlayAtTime(&s.DVDPosition,
                                                                 DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, nullptr);
                                        if (s.fShowDebugInfo) {
                                            m_OSD.DebugMessage(_T("PlayAtTime: 0x%08X"), hr);
                                        }
                                    } else {
                                        if (s.fShowDebugInfo)
                                            m_OSD.DebugMessage(_T("Timecode requested: %02d:%02d:%02d.%03d"),
                                                               s.DVDPosition.bHours, s.DVDPosition.bMinutes,
                                                               s.DVDPosition.bSeconds, s.DVDPosition.bFrames);

                                        // Always play chapter 1 (for now, until something else dumb happens)
                                        hr = m_pDVDC->PlayChapterInTitle(s.lDVDTitle, 1,
                                                                         DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, nullptr);
                                        if (s.fShowDebugInfo) {
                                            m_OSD.DebugMessage(_T("PlayChapterInTitle: 0x%08X"), hr);
                                        }

                                        // This might fail if the Title is not available yet?
                                        hr = m_pDVDC->PlayAtTime(&s.DVDPosition,
                                                                 DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, nullptr);
                                        if (s.fShowDebugInfo) {
                                            m_OSD.DebugMessage(_T("PlayAtTime: 0x%08X"), hr);
                                        }

                                        if (hr != S_OK) {
                                            hr = m_pDVDC->PlayAtTimeInTitle(s.lDVDTitle, &s.DVDPosition,
                                                                            DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, nullptr);
                                            if (s.fShowDebugInfo) {
                                                m_OSD.DebugMessage(_T("PlayAtTimeInTitle: 0x%08X"), hr);
                                            }
                                        }
                                    } // Resume

                                    hr = m_pDVDC->PlayAtTime(&s.DVDPosition,
                                                             DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, nullptr);
                                    if (s.fShowDebugInfo) {
                                        m_OSD.DebugMessage(_T("PlayAtTime: %d"), hr);
                                    }
                                }

                                m_iDVDTitle   = s.lDVDTitle;
                                s.lDVDTitle   = 0;
                                s.lDVDChapter = 0;
                            } else if (pDVDData && pDVDData->pDvdState) {
                                // Set position from favorite
                                VERIFY(SUCCEEDED(m_pDVDC->SetState(pDVDData->pDvdState, DVD_CMD_FLAG_Block, nullptr)));
                                // We don't want to restore the position from the favorite
                                // if the playback is reinitialized so we clear the saved state
                                pDVDData->pDvdState.Release();
                            } else if (s.fKeepHistory && s.fRememberDVDPos && !s.dvdPositions.AddEntry(llDVDGuid)) {
                                // Set last remembered position (if found...)
                                DVD_POSITION* dvdPosition = s.dvdPositions.GetLatestEntry();

                                m_pDVDC->PlayTitle(dvdPosition->lTitle, DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, nullptr);
                                m_pDVDC->Resume(DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, nullptr);
#if 1
                                if (SUCCEEDED(hr = m_pDVDC->PlayAtTimeInTitle(
                                                       dvdPosition->lTitle, &dvdPosition->timecode,
                                                       DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, nullptr)))
#else
                                if (SUCCEEDED(hr = m_pDVDC->PlayAtTime(&dvdPosition->timecode,
                                                                       DVD_CMD_FLAG_Flush, nullptr)))
#endif
                                {
                                    m_iDVDTitle = dvdPosition->lTitle;
                                }
                            }

                            if (s.fRememberZoomLevel && !m_fFullScreen && !IsD3DFullScreenMode() && !IsZoomed() && !IsIconic() && !IsAeroSnapped()) { // Hack to the normal initial zoom for DVD + DXVA ...
                                ZoomVideoWindow();
                            }
                        }
                        break;
                    case DVD_DOMAIN_VideoManagerMenu:
                        Domain = _T("Video Manager Menu");
                        if (s.fShowDebugInfo) {
                            m_OSD.DebugMessage(_T("%s"), Domain.GetString());
                        }
                        break;
                    case DVD_DOMAIN_VideoTitleSetMenu:
                        Domain = _T("Video Title Set Menu");
                        if (s.fShowDebugInfo) {
                            m_OSD.DebugMessage(_T("%s"), Domain.GetString());
                        }
                        break;
                    case DVD_DOMAIN_Title:
                        Domain.Format(IDS_AG_TITLE, m_iDVDTitle);
                        if (s.fShowDebugInfo) {
                            m_OSD.DebugMessage(_T("%s"), Domain.GetString());
                        }
                        {
                            DVD_POSITION* dvdPosition = s.dvdPositions.GetLatestEntry();
                            if (dvdPosition) {
                                dvdPosition->lTitle = m_iDVDTitle;
                            }

                            if (!m_fValidDVDOpen && m_pDVDC) {
                                m_fValidDVDOpen = true;
                                m_pDVDC->ShowMenu(DVD_MENU_Title, DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, nullptr);
                            }
                        }

                        break;
                    case DVD_DOMAIN_Stop:
                        Domain.LoadString(IDS_AG_STOP);
                        if (s.fShowDebugInfo) {
                            m_OSD.DebugMessage(_T("%s"), Domain.GetString());
                        }
                        break;
                    default:
                        Domain = _T("-");
                        if (s.fShowDebugInfo) {
                            m_OSD.DebugMessage(_T("%s"), Domain.GetString());
                        }
                        break;
                }

                m_wndInfoBar.SetLine(StrRes(IDS_INFOBAR_DOMAIN), Domain);

                if (GetPlaybackMode() == PM_FILE) {
                    SetupChapters();
                } else if (GetPlaybackMode() == PM_DVD) {
                    SetupDVDChapters();
                }

#if 0   // UOPs debug traces
                if (hr == VFW_E_DVD_OPERATION_INHIBITED) {
                    ULONG UOPfields = 0;
                    pDVDI->GetCurrentUOPS(&UOPfields);
                    CString message;
                    message.Format(_T("UOP bitfield: 0x%08X; domain: %s"), UOPfields, Domain);
                    m_OSD.DisplayMessage(OSD_TOPLEFT, message);
                } else {
                    m_OSD.DisplayMessage(OSD_TOPRIGHT, Domain);
                }
#endif

                MoveVideoWindow(); // AR might have changed
            }
            break;
            case EC_DVD_CURRENT_HMSF_TIME: {
                // Casimir666 : Save current position in the chapter
                DVD_POSITION* dvdPosition = s.dvdPositions.GetLatestEntry();
                if (dvdPosition) {
                    memcpy(&dvdPosition->timecode, (void*)&evParam1, sizeof(DVD_HMSF_TIMECODE));
                }
            }
            break;
            case EC_DVD_ERROR: {
                TRACE(_T("\t%I64d %Id\n"), evParam1, evParam2);

                UINT err;

                switch (evParam1) {
                    case DVD_ERROR_Unexpected:
                    default:
                        err = IDS_MAINFRM_16;
                        break;
                    case DVD_ERROR_CopyProtectFail:
                        err = IDS_MAINFRM_17;
                        break;
                    case DVD_ERROR_InvalidDVD1_0Disc:
                        err = IDS_MAINFRM_18;
                        break;
                    case DVD_ERROR_InvalidDiscRegion:
                        err = IDS_MAINFRM_19;
                        break;
                    case DVD_ERROR_LowParentalLevel:
                        err = IDS_MAINFRM_20;
                        break;
                    case DVD_ERROR_MacrovisionFail:
                        err = IDS_MAINFRM_21;
                        break;
                    case DVD_ERROR_IncompatibleSystemAndDecoderRegions:
                        err = IDS_MAINFRM_22;
                        break;
                    case DVD_ERROR_IncompatibleDiscAndDecoderRegions:
                        err = IDS_MAINFRM_23;
                        break;
                }

                SendMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);

                m_closingmsg.LoadString(err);
            }
            break;
            case EC_DVD_WARNING:
                TRACE(_T("\t%Id %Id\n"), evParam1, evParam2);
                break;
            case EC_VIDEO_SIZE_CHANGED: {
                CSize size((DWORD)evParam1);
                TRACE(_T("\t%ldx%ld\n"), size.cx, size.cy);
                const bool bWasAudioOnly = m_fAudioOnly;
                m_fAudioOnly = (size.cx <= 0 || size.cy <= 0);
                OnVideoSizeChanged(bWasAudioOnly);
            }
            break;
            case EC_LENGTH_CHANGED: {
                REFERENCE_TIME rtDur = 0;
                m_pMS->GetDuration(&rtDur);
                m_wndPlaylistBar.SetCurTime(rtDur);
                OnTimer(TIMER_STREAMPOSPOLLER);
                OnTimer(TIMER_STREAMPOSPOLLER2);
                LoadKeyFrames();
                if (GetPlaybackMode() == PM_FILE) {
                    SetupChapters();
                } else if (GetPlaybackMode() == PM_DVD) {
                    SetupDVDChapters();
                }
            }
            break;
            case EC_BG_AUDIO_CHANGED:
                if (m_fCustomGraph) {
                    int nAudioChannels = (int)evParam1;

                    m_wndStatusBar.SetStatusBitmap(nAudioChannels == 1 ? IDB_AUDIOTYPE_MONO
                                                   : nAudioChannels >= 2 ? IDB_AUDIOTYPE_STEREO
                                                   : IDB_AUDIOTYPE_NOAUDIO);
                }
                break;
            case EC_BG_ERROR:
                if (m_fCustomGraph) {
                    SendMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);
                    m_closingmsg = !str.IsEmpty() ? str : CString(_T("Unspecified graph error"));
                    m_wndPlaylistBar.SetCurValid(false);
                    return hr;
                }
                break;
            case EC_DVD_PLAYBACK_RATE_CHANGE:
                if (m_fCustomGraph && s.autoChangeFSMode.bEnabled &&
                        (m_fFullScreen || IsD3DFullScreenMode()) && m_iDVDDomain == DVD_DOMAIN_Title) {
                    AutoChangeMonitorMode();
                }
                break;
        }
    }

    return hr;
}

LRESULT CMainFrame::OnResetDevice(WPARAM wParam, LPARAM lParam)
{
    m_OSD.HideMessage(true);

    OAFilterState fs = GetMediaState();
    if (fs == State_Running) {
        if (!IsPlaybackCaptureMode()) {
            m_pMC->Pause();
        } else {
            m_pMC->Stop(); // Capture mode doesn't support pause
        }
    }

    if (m_bOpenedThroughThread) {
        CAMMsgEvent e;
        m_pGraphThread->PostThreadMessage(CGraphThread::TM_RESET, 0, (LPARAM)&e);
        e.WaitMsg();
    } else {
        ResetDevice();
    }

    if (fs == State_Running && m_pMC) {
        m_pMC->Run();
        m_dwLastPause = 0;

        // When restarting DVB capture, we need to set again the channel.
        if (GetPlaybackMode() == PM_DIGITAL_CAPTURE) {
            CComQIPtr<IBDATuner> pTun = m_pGB;
            if (pTun) {
                SetChannel(AfxGetAppSettings().nDVBLastChannel);
            }
        }
    }

    if (m_OSD.CanShowMessage()) {
        m_OSD.HideMessage(false);
    }

    return S_OK;
}

LRESULT CMainFrame::OnRepaintRenderLess(WPARAM wParam, LPARAM lParam)
{
    MoveVideoWindow();
    return TRUE;
}

void CMainFrame::SaveAppSettings()
{
    MSG msg;
    if (!PeekMessage(&msg, m_hWnd, WM_SAVESETTINGS, WM_SAVESETTINGS, PM_NOREMOVE | PM_NOYIELD)) {
        AfxGetAppSettings().SaveSettings();
    }
}

LRESULT CMainFrame::OnNcHitTest(CPoint point)
{
    LRESULT nHitTest = __super::OnNcHitTest(point);
    return ((IsCaptionHidden()) && nHitTest == HTCLIENT) ? HTCAPTION : nHitTest;
}

void CMainFrame::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    if (pScrollBar->IsKindOf(RUNTIME_CLASS(CVolumeCtrl))) {
        OnPlayVolume(0);
    } else if (pScrollBar->IsKindOf(RUNTIME_CLASS(CPlayerSeekBar)) && GetLoadState() == MLS::LOADED) {
        SeekTo(m_wndSeekBar.GetPos());
    } else if (*pScrollBar == *m_pVideoWnd) {
        SeekTo(m_OSD.GetPos());
    }

    __super::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CMainFrame::OnInitMenu(CMenu* pMenu)
{
    __super::OnInitMenu(pMenu);

    const UINT uiMenuCount = pMenu->GetMenuItemCount();
    if (uiMenuCount == -1) {
        return;
    }

    MENUITEMINFO mii;
    mii.cbSize = sizeof(mii);

    for (UINT i = 0; i < uiMenuCount; ++i) {
#ifdef _DEBUG
        CString str;
        pMenu->GetMenuString(i, str, MF_BYPOSITION);
        str.Remove('&');
#endif
        UINT itemID = pMenu->GetMenuItemID(i);
        if (itemID == 0xFFFFFFFF) {
            mii.fMask = MIIM_ID;
            pMenu->GetMenuItemInfo(i, &mii, TRUE);
            itemID = mii.wID;
        }

        CMPCThemeMenu* pSubMenu = nullptr;

        if (itemID == ID_FAVORITES) {
            SetupFavoritesSubMenu();
            pSubMenu = &m_favoritesMenu;
        }/*else if (itemID == ID_RECENT_FILES) {
            SetupRecentFilesSubMenu();
            pSubMenu = &m_recentFilesMenu;
        }*/

        if (pSubMenu) {
            mii.fMask = MIIM_STATE | MIIM_SUBMENU;
            mii.fState = (pSubMenu->GetMenuItemCount()) > 0 ? MFS_ENABLED : MFS_DISABLED;
            mii.hSubMenu = *pSubMenu;
            VERIFY(CMPCThemeMenu::SetThemedMenuItemInfo(pMenu, i, &mii, TRUE));
            pSubMenu->fulfillThemeReqs();
        }
    }
}

void CMainFrame::OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu) {
    __super::OnInitMenuPopup(pPopupMenu, nIndex, bSysMenu);

    if (bSysMenu) {
        m_pActiveSystemMenu = pPopupMenu;
        m_eventc.FireEvent(MpcEvent::SYSTEM_MENU_POPUP_INITIALIZED);
        return;
    }

    UINT uiMenuCount = pPopupMenu->GetMenuItemCount();
    if (uiMenuCount == -1) {
        return;
    }

    MENUITEMINFO mii;
    mii.cbSize = sizeof(mii);

    for (UINT i = 0; i < uiMenuCount; ++i) {
#ifdef _DEBUG
        CString str;
        pPopupMenu->GetMenuString(i, str, MF_BYPOSITION);
        str.Remove('&');
#endif
        UINT firstSubItemID = 0;
        CMenu* sm = pPopupMenu->GetSubMenu(i);
        if (sm) {
            firstSubItemID = sm->GetMenuItemID(0);
        }

        if (firstSubItemID == ID_NAVIGATE_SKIPBACK) { // is "Navigate" submenu {
            UINT fState = (GetLoadState() == MLS::LOADED
                && (1/*GetPlaybackMode() == PM_DVD *//*|| (GetPlaybackMode() == PM_FILE && !m_PlayList.IsEmpty())*/))
                ? MF_ENABLED
                : MF_GRAYED;
            pPopupMenu->EnableMenuItem(i, MF_BYPOSITION | fState);
            continue;
        }
        if (firstSubItemID == ID_VIEW_VF_HALF               // is "Video Frame" submenu
            || firstSubItemID == ID_VIEW_INCSIZE        // is "Pan&Scan" submenu
            || firstSubItemID == ID_ASPECTRATIO_START   // is "Override Aspect Ratio" submenu
            || firstSubItemID == ID_VIEW_ZOOM_25) {     // is "Zoom" submenu
            UINT fState = (GetLoadState() == MLS::LOADED && !m_fAudioOnly)
                ? MF_ENABLED
                : MF_GRAYED;
            pPopupMenu->EnableMenuItem(i, MF_BYPOSITION | fState);
            continue;
        }

        // "File -> Subtitles" submenu
        if (firstSubItemID == ID_FILE_SUBTITLES_LOAD) {
            UINT fState = (GetLoadState() == MLS::LOADED && !m_fAudioOnly && m_pCAP)
                ? MF_ENABLED
                : MF_GRAYED;
            pPopupMenu->EnableMenuItem(i, MF_BYPOSITION | fState);
            continue;
        }

        // renderer settings
        if (firstSubItemID == ID_VIEW_TEARING_TEST) {
            UINT fState = MF_GRAYED;
            const CAppSettings& s = AfxGetAppSettings();
            if (s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM || s.iDSVideoRendererType == VIDRNDT_DS_SYNC || s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS) {
                fState = MF_ENABLED;
            }
            pPopupMenu->EnableMenuItem(i, MF_BYPOSITION | fState);
            continue;
        }

        UINT itemID = pPopupMenu->GetMenuItemID(i);
        if (itemID == 0xFFFFFFFF) {
            mii.fMask = MIIM_ID;
            VERIFY(pPopupMenu->GetMenuItemInfo(i, &mii, TRUE));
            itemID = mii.wID;
        }
        CMPCThemeMenu* pSubMenu = nullptr;

        // debug shaders
        if (itemID == ID_VIEW_DEBUGSHADERS) {
            UINT fState = MF_GRAYED;
            if (GetLoadState() == MLS::LOADED && !m_fAudioOnly && m_pCAP2) {
                fState = MF_ENABLED;
            }
            pPopupMenu->EnableMenuItem(i, MF_BYPOSITION | fState);
            continue;
        }

        if (itemID == ID_FILE_OPENDISC) {
            SetupOpenCDSubMenu();
            pSubMenu = &m_openCDsMenu;
        } else if (itemID == ID_FILTERS) {
            SetupFiltersSubMenu();
            pSubMenu = &m_filtersMenu;
        } else if (itemID == ID_AUDIOS) {
            SetupAudioSubMenu();
            pSubMenu = &m_audiosMenu;
        } else if (itemID == ID_SUBTITLES) {
            SetupSubtitlesSubMenu();
            pSubMenu = &m_subtitlesMenu;
        } else if (itemID == ID_VIDEO_STREAMS) {
            CString menuStr;
            menuStr.LoadString(GetPlaybackMode() == PM_DVD ? IDS_MENU_VIDEO_ANGLE : IDS_MENU_VIDEO_STREAM);

            mii.fMask = MIIM_STRING;
            mii.dwTypeData = (LPTSTR)(LPCTSTR)menuStr;
            VERIFY(CMPCThemeMenu::SetThemedMenuItemInfo(pPopupMenu, i, &mii, TRUE));

            SetupVideoStreamsSubMenu();
            pSubMenu = &m_videoStreamsMenu;
        } else if (itemID == ID_NAVIGATE_GOTO) {
            // ID_NAVIGATE_GOTO is just a marker we use to insert the appropriate submenus
            SetupJumpToSubMenus(pPopupMenu, i + 1);
            uiMenuCount = pPopupMenu->GetMenuItemCount(); //SetupJumpToSubMenus could actually reduce the menu count!
        } else if (itemID == ID_FAVORITES) {
            SetupFavoritesSubMenu();
            pSubMenu = &m_favoritesMenu;
        } else if (itemID == ID_RECENT_FILES) {
            SetupRecentFilesSubMenu();
            pSubMenu = &m_recentFilesMenu;
        } else if (itemID == ID_SHADERS) {
            if (SetupShadersSubMenu()) {
                pPopupMenu->EnableMenuItem(ID_SHADERS, MF_BYPOSITION | MF_ENABLED);
            } else {
                pPopupMenu->EnableMenuItem(ID_SHADERS, MF_BYPOSITION | MF_GRAYED);
            }
            pSubMenu = &m_shadersMenu;
        }

        if (pSubMenu) {
            mii.fMask = MIIM_STATE | MIIM_SUBMENU;
            mii.fState = (pSubMenu->GetMenuItemCount() > 0) ? MF_ENABLED : MF_GRAYED;
            mii.hSubMenu = *pSubMenu;
            VERIFY(CMPCThemeMenu::SetThemedMenuItemInfo(pPopupMenu, i, &mii, TRUE));
            pSubMenu->fulfillThemeReqs();
        }
    }

    uiMenuCount = pPopupMenu->GetMenuItemCount();
    if (uiMenuCount == -1) {
        return;
    }

    if (!AppIsThemeLoaded()) { //themed menus draw accelerators already, no need to append
        for (UINT i = 0; i < uiMenuCount; ++i) {
            UINT nID = pPopupMenu->GetMenuItemID(i);
            if (nID == ID_SEPARATOR || nID == -1
                || nID >= ID_FAVORITES_FILE_START && nID <= ID_FAVORITES_FILE_END
                || nID >= ID_RECENT_FILE_START && nID <= ID_RECENT_FILE_END
                || nID >= ID_SUBTITLES_SUBITEM_START && nID <= ID_SUBTITLES_SUBITEM_END
                || nID >= ID_NAVIGATE_JUMPTO_SUBITEM_START && nID <= ID_NAVIGATE_JUMPTO_SUBITEM_END) {
                continue;
            }

            CString str;
            pPopupMenu->GetMenuString(i, str, MF_BYPOSITION);
            int k = str.Find('\t');
            if (k > 0) {
                str = str.Left(k);
            }

            CString key = CPPageAccelTbl::MakeAccelShortcutLabel(nID);
            if (key.IsEmpty() && k < 0) {
                continue;
            }
            str += _T("\t") + key;

            // BUG(?): this disables menu item update ui calls for some reason...
            //pPopupMenu->ModifyMenu(i, MF_BYPOSITION|MF_STRING, nID, str);

            // this works fine
            mii.fMask = MIIM_STRING;
            mii.dwTypeData = (LPTSTR)(LPCTSTR)str;
            VERIFY(pPopupMenu->SetMenuItemInfo(i, &mii, TRUE));
        }
    }

    uiMenuCount = pPopupMenu->GetMenuItemCount();
    if (uiMenuCount == -1) {
        return;
    }

    bool fPnSPresets = false;

    for (UINT i = 0; i < uiMenuCount; ++i) {
        UINT nID = pPopupMenu->GetMenuItemID(i);

        if (nID >= ID_PANNSCAN_PRESETS_START && nID < ID_PANNSCAN_PRESETS_END) {
            do {
                nID = pPopupMenu->GetMenuItemID(i);
                VERIFY(pPopupMenu->DeleteMenu(i, MF_BYPOSITION));
                uiMenuCount--;
            } while (i < uiMenuCount && nID >= ID_PANNSCAN_PRESETS_START && nID < ID_PANNSCAN_PRESETS_END);

            nID = pPopupMenu->GetMenuItemID(i);
        }

        if (nID == ID_VIEW_RESET) {
            fPnSPresets = true;
        }
    }

    if (fPnSPresets) {
        bool usetheme = AppIsThemeLoaded();
        const CAppSettings& s = AfxGetAppSettings();
        INT_PTR i = 0, j = s.m_pnspresets.GetCount();
        for (; i < j; i++) {
            int k = 0;
            CString label = s.m_pnspresets[i].Tokenize(_T(","), k);
            VERIFY(pPopupMenu->InsertMenu(ID_VIEW_RESET, MF_BYCOMMAND, ID_PANNSCAN_PRESETS_START + i, label));
            CMPCThemeMenu::fulfillThemeReqsItem(pPopupMenu, (UINT)(ID_PANNSCAN_PRESETS_START + i), true);
        }
        //if (j > 0)
        {
            VERIFY(pPopupMenu->InsertMenu(ID_VIEW_RESET, MF_BYCOMMAND, ID_PANNSCAN_PRESETS_START + i, ResStr(IDS_PANSCAN_EDIT)));
            VERIFY(pPopupMenu->InsertMenu(ID_VIEW_RESET, MF_BYCOMMAND | MF_SEPARATOR));
            if (usetheme) {
                CMPCThemeMenu::fulfillThemeReqsItem(pPopupMenu, (UINT)(ID_PANNSCAN_PRESETS_START + i), true);
                UINT pos = CMPCThemeMenu::getPosFromID(pPopupMenu, ID_VIEW_RESET); //separator is inserted right before view_reset
                CMPCThemeMenu::fulfillThemeReqsItem(pPopupMenu, pos - 1);
            }
        }
    }

    if (m_pActiveContextMenu == pPopupMenu) {
        m_eventc.FireEvent(MpcEvent::CONTEXT_MENU_POPUP_INITIALIZED);
    }
}

void CMainFrame::OnUnInitMenuPopup(CMenu* pPopupMenu, UINT nFlags)
{
    __super::OnUnInitMenuPopup(pPopupMenu, nFlags);
    if (m_pActiveContextMenu == pPopupMenu) {
        m_pActiveContextMenu = nullptr;
        m_eventc.FireEvent(MpcEvent::CONTEXT_MENU_POPUP_UNINITIALIZED);
    } else if (m_pActiveSystemMenu == pPopupMenu) {
        m_pActiveSystemMenu = nullptr;
        SendMessage(WM_CANCELMODE); // unfocus main menu if system menu was entered with alt+space
        m_eventc.FireEvent(MpcEvent::SYSTEM_MENU_POPUP_UNINITIALIZED);
    }
}

void CMainFrame::OnEnterMenuLoop(BOOL bIsTrackPopupMenu)
{
    if (!bIsTrackPopupMenu && !m_pActiveSystemMenu && GetMenuBarState() == AFX_MBS_HIDDEN) {
        // mfc has problems synchronizing menu visibility with modal loop in certain situations
        ASSERT(!m_pActiveContextMenu);
        VERIFY(SetMenuBarState(AFX_MBS_VISIBLE));
    }
    __super::OnEnterMenuLoop(bIsTrackPopupMenu);
}

BOOL CMainFrame::OnQueryEndSession()
{
    return TRUE;
}

void CMainFrame::OnEndSession(BOOL bEnding)
{
    // do nothing for now
}

BOOL CMainFrame::OnMenu(CMenu* pMenu)
{
    if (!pMenu) {
        return FALSE;
    }

    CPoint point;
    GetCursorPos(&point);

    // Do not show popup menu in D3D fullscreen it has several adverse effects.
    if (IsD3DFullScreenMode()) {
        CWnd* pWnd = WindowFromPoint(point);
        if (pWnd && *pWnd == *m_pFullscreenWnd) {
            return FALSE;
        }
    }

    if (AfxGetMyApp()->m_fClosingState) {
        return FALSE; //prevent crash when player closes with context menu open
    }

    m_pActiveContextMenu = pMenu;

    pMenu->TrackPopupMenu(TPM_RIGHTBUTTON | TPM_NOANIMATION, point.x, point.y, this);

    return TRUE;
}

void CMainFrame::OnMenuPlayerShort()
{
    if (IsMenuHidden() || IsD3DFullScreenMode()) {
        OnMenu(m_mainPopupMenu.GetSubMenu(0));
    } else {
        OnMenu(m_popupMenu.GetSubMenu(0));
    }
}

void CMainFrame::OnMenuPlayerLong()
{
    OnMenu(m_mainPopupMenu.GetSubMenu(0));
}

void CMainFrame::OnMenuFilters()
{
    SetupFiltersSubMenu();
    OnMenu(&m_filtersMenu);
}

void CMainFrame::OnUpdatePlayerStatus(CCmdUI* pCmdUI)
{
    if (GetLoadState() == MLS::LOADING) {
        m_wndStatusBar.SetStatusMessage(StrRes(IDS_CONTROLS_OPENING));
        if (AfxGetAppSettings().bUseEnhancedTaskBar && m_pTaskbarList) {
            m_pTaskbarList->SetProgressState(m_hWnd, TBPF_INDETERMINATE);
        }
    } else if (GetLoadState() == MLS::LOADED) {
        if (!m_tempstatus_msg.IsEmpty()) {
            m_wndStatusBar.SetStatusMessage(m_tempstatus_msg);
            return;
        }
        CString msg;
        if (m_fCapturing) {
            msg.LoadString(IDS_CONTROLS_CAPTURING);

            if (m_pAMDF) {
                long lDropped = 0;
                m_pAMDF->GetNumDropped(&lDropped);
                long lNotDropped = 0;
                m_pAMDF->GetNumNotDropped(&lNotDropped);

                if ((lDropped + lNotDropped) > 0) {
                    msg.AppendFormat(IDS_MAINFRM_37, lDropped + lNotDropped, lDropped);
                }
            }

            CComPtr<IPin> pPin;
            if (m_pCGB && SUCCEEDED(m_pCGB->FindPin(m_wndCaptureBar.m_capdlg.m_pDst, PINDIR_INPUT, nullptr, nullptr, FALSE, 0, &pPin))) {
                LONGLONG size = 0;
                if (CComQIPtr<IStream> pStream = pPin) {
                    pStream->Commit(STGC_DEFAULT);

                    WIN32_FIND_DATA findFileData;
                    HANDLE h = FindFirstFile(m_wndCaptureBar.m_capdlg.m_file, &findFileData);
                    if (h != INVALID_HANDLE_VALUE) {
                        size = ((LONGLONG)findFileData.nFileSizeHigh << 32) | findFileData.nFileSizeLow;

                        if (size < 1024i64 * 1024) {
                            msg.AppendFormat(IDS_MAINFRM_38, size / 1024);
                        } else { //if (size < 1024i64*1024*1024)
                            msg.AppendFormat(IDS_MAINFRM_39, size / 1024 / 1024);
                        }

                        FindClose(h);
                    }
                }

                ULARGE_INTEGER FreeBytesAvailable, TotalNumberOfBytes, TotalNumberOfFreeBytes;
                if (GetDiskFreeSpaceEx(
                            m_wndCaptureBar.m_capdlg.m_file.Left(m_wndCaptureBar.m_capdlg.m_file.ReverseFind('\\') + 1),
                            &FreeBytesAvailable, &TotalNumberOfBytes, &TotalNumberOfFreeBytes)) {
                    if (FreeBytesAvailable.QuadPart < 1024i64 * 1024) {
                        msg.AppendFormat(IDS_MAINFRM_40, FreeBytesAvailable.QuadPart / 1024);
                    } else { //if (FreeBytesAvailable.QuadPart < 1024i64*1024*1024)
                        msg.AppendFormat(IDS_MAINFRM_41, FreeBytesAvailable.QuadPart / 1024 / 1024);
                    }
                }

                if (m_wndCaptureBar.m_capdlg.m_pMux) {
                    __int64 pos = 0;
                    CComQIPtr<IMediaSeeking> pMuxMS = m_wndCaptureBar.m_capdlg.m_pMux;
                    if (pMuxMS && SUCCEEDED(pMuxMS->GetCurrentPosition(&pos)) && pos > 0) {
                        double bytepersec = 10000000.0 * size / pos;
                        if (bytepersec > 0) {
                            m_rtDurationOverride = REFERENCE_TIME(10000000.0 * (FreeBytesAvailable.QuadPart + size) / bytepersec);
                        }
                    }
                }

                if (m_wndCaptureBar.m_capdlg.m_pVidBuffer
                        || m_wndCaptureBar.m_capdlg.m_pAudBuffer) {
                    int nFreeVidBuffers = 0, nFreeAudBuffers = 0;
                    if (CComQIPtr<IBufferFilter> pVB = m_wndCaptureBar.m_capdlg.m_pVidBuffer) {
                        nFreeVidBuffers = pVB->GetFreeBuffers();
                    }
                    if (CComQIPtr<IBufferFilter> pAB = m_wndCaptureBar.m_capdlg.m_pAudBuffer) {
                        nFreeAudBuffers = pAB->GetFreeBuffers();
                    }

                    msg.AppendFormat(IDS_MAINFRM_42, nFreeVidBuffers, nFreeAudBuffers);
                }
            }
        } else if (m_bBuffering) {
            BeginEnumFilters(m_pGB, pEF, pBF) {
                if (CComQIPtr<IAMNetworkStatus, &IID_IAMNetworkStatus> pAMNS = pBF) {
                    long BufferingProgress = 0;
                    if (SUCCEEDED(pAMNS->get_BufferingProgress(&BufferingProgress)) && BufferingProgress > 0) {
                        msg.Format(IDS_CONTROLS_BUFFERING, BufferingProgress);

                        __int64 start = 0, stop = 0;
                        m_wndSeekBar.GetRange(start, stop);
                        m_fLiveWM = (stop == start);
                    }
                    break;
                }
            }
            EndEnumFilters;
        } else if (m_pAMOP) {
            LONGLONG t = 0, c = 0;
            if (SUCCEEDED(m_pAMOP->QueryProgress(&t, &c)) && t > 0 && c < t) {
                msg.Format(IDS_CONTROLS_BUFFERING, c * 100 / t);
            } else {
                m_pAMOP.Release();
            }
        }

        if (msg.IsEmpty()) {
            int msg_id = 0;
            switch (GetMediaState()) {
                case State_Stopped:
                    msg_id = IDS_CONTROLS_STOPPED;
                    break;
                case State_Paused:
                    msg_id = IDS_CONTROLS_PAUSED;
                    break;
                case State_Running:
                    msg_id = IDS_CONTROLS_PLAYING;
                    break;
            }
            if (m_fFrameSteppingActive) {
                msg_id = IDS_CONTROLS_PAUSED;
            }
            if (msg_id) {
                msg.LoadString(msg_id);
            }

            if (m_bUsingDXVA && (msg == ResStr(IDS_CONTROLS_PAUSED) || msg == ResStr(IDS_CONTROLS_PLAYING))) {
                msg.AppendFormat(_T(" %s"), ResStr(IDS_HW_INDICATOR).GetString());
            }
            if (AfxGetAppSettings().bShowLangInStatusbar) {
                if (!currentAudioLang.IsEmpty() || !currentSubLang.IsEmpty()) {
                    msg.Append(_T("\u2001["));
                    if (!currentAudioLang.IsEmpty()) {
                        msg.AppendFormat(_T("AUD: %s"), currentAudioLang.GetString());
                    }
                    if (!currentSubLang.IsEmpty()) {
                        if (!currentAudioLang.IsEmpty()) {
                            msg.Append(_T(", "));
                        }
                        msg.AppendFormat(_T("SUB: %s"), currentSubLang.GetString());
                    }
                    msg.Append(_T("]"));
                }
            }
            if (AfxGetAppSettings().bShowFPSInStatusbar && m_pCAP) {
                msg.AppendFormat(_T("\u2001%.2lf fps (%.2lfx)"), m_pCAP->GetFPS(), m_dSpeedRate);
            }
        }

        m_wndStatusBar.SetStatusMessage(msg);
    } else if (GetLoadState() == MLS::CLOSING) {
        m_wndStatusBar.SetStatusMessage(StrRes(IDS_CONTROLS_CLOSING));
        if (AfxGetAppSettings().bUseEnhancedTaskBar && m_pTaskbarList) {
            m_pTaskbarList->SetProgressState(m_hWnd, TBPF_INDETERMINATE);
        }
    } else {
        m_wndStatusBar.SetStatusMessage(m_closingmsg);
    }
}

LRESULT CMainFrame::OnFilePostOpenmedia(WPARAM wParam, LPARAM lParam)
{
    if (!m_pGB) {
        ASSERT(FALSE);
        return 1;
    }
    ASSERT(GetLoadState() == MLS::LOADING);

    auto& s = AfxGetAppSettings();

    // from this on
    m_bOpenMediaActive = false;
    m_bSettingUpMenus = true;
    SetLoadState(MLS::LOADED);

    // destroy invisible top-level d3dfs window if there is no video renderer
    if (IsD3DFullScreenMode() && !m_pMFVDC && !m_pVMRWC) {
        m_pFullscreenWnd->DestroyWindow();
        m_fStartInD3DFullscreen = true;
    }

    // auto-change monitor mode if requested
    if (s.autoChangeFSMode.bEnabled && (m_fFullScreen || IsD3DFullScreenMode())) {
        AutoChangeMonitorMode();
        // make sure the fullscreen window is positioned properly after the mode change,
        // OnWindowPosChanging() will take care of that
        if (m_bOpeningInAutochangedMonitorMode && m_fFullScreen) {
            CRect rect;
            GetWindowRect(rect);
            MoveWindow(rect);
        }
    }

    // set shader selection
    if (m_pCAP || m_pCAP2) {
        SetShaders(m_bToggleShader, m_bToggleShaderScreenSpace);
    }

    // load keyframes for fast-seek
    if (wParam == PM_FILE) {
        LoadKeyFrames();
    }

    // remember OpenMediaData for later use
    m_lastOMD.Free();
    m_lastOMD.Attach((OpenMediaData*)lParam);

    // the media opened successfully, we don't want to jump trough it anymore
    m_nLastSkipDirection = 0;

    // let the EDL do its magic
    if (s.fEnableEDLEditor) {
        m_wndEditListEditor.OpenFile(m_lastOMD->title);
    }

    // initiate Capture panel with the new media
    if (auto pDeviceData = dynamic_cast<OpenDeviceData*>(m_lastOMD.m_p)) {
        m_wndCaptureBar.m_capdlg.SetVideoInput(pDeviceData->vinput);
        m_wndCaptureBar.m_capdlg.SetVideoChannel(pDeviceData->vchannel);
        m_wndCaptureBar.m_capdlg.SetAudioInput(pDeviceData->ainput);
    }

    // current playlist item was loaded successfully
    m_wndPlaylistBar.SetCurValid(true);

    // set item duration in the playlist
    // TODO: GetDuration() should be refactored out of this place, to some aggregating class
    REFERENCE_TIME rtDur = 0;
    if (m_pMS && m_pMS->GetDuration(&rtDur) == S_OK) {
        m_wndPlaylistBar.SetCurTime(rtDur);
    }

    // process /pns command-line arg, then discard it
    if (!s.strPnSPreset.IsEmpty()) {
        for (int i = 0; i < s.m_pnspresets.GetCount(); i++) {
            int j = 0;
            CString str = s.m_pnspresets[i];
            CString label = str.Tokenize(_T(","), j);
            if (s.strPnSPreset == label) {
                OnViewPanNScanPresets(i + ID_PANNSCAN_PRESETS_START);
            }
        }
        s.strPnSPreset.Empty();
    }

    // initiate toolbars with the new media
    OpenSetupInfoBar();
    OpenSetupStatsBar();
    OpenSetupStatusBar();
    OpenSetupCaptureBar();

    // Load cover-art
    if (m_fAudioOnly) {
        UpdateControlState(CMainFrame::UPDATE_LOGO);
    }

    if (GetPlaybackMode() == PM_DIGITAL_CAPTURE) {
        // show navigation panel when it's available and not disabled
        if (!s.fHideNavigation) {
            m_wndNavigationBar.m_navdlg.UpdateElementList();
            if (!m_controls.ControlChecked(CMainFrameControls::Panel::NAVIGATION)) {
                m_controls.ToggleControl(CMainFrameControls::Panel::NAVIGATION);
            } else {
                ASSERT(FALSE);
            }
        }
    } else if (GetPlaybackMode() == PM_ANALOG_CAPTURE) {
        // show capture bar
        if (!m_controls.ControlChecked(CMainFrameControls::Panel::CAPTURE)) {
            m_controls.ToggleControl(CMainFrameControls::Panel::CAPTURE);
        } else {
            ASSERT(FALSE);
        }
    }

    // we don't want to wait until timers initialize the seekbar and the time counter
    streampospoller_active = true;
    OnTimer(TIMER_STREAMPOSPOLLER);
    OnTimer(TIMER_STREAMPOSPOLLER2);

    if (m_AngleX != 0 || m_AngleY != 0 || m_AngleZ != 0) {
        PerformFlipRotate();
    }

    // auto-zoom if requested
    if (IsWindowVisible() && s.fRememberZoomLevel &&
            !m_fFullScreen && !IsD3DFullScreenMode() && !IsZoomed() && !IsIconic() && !IsAeroSnapped()) {
        ZoomVideoWindow();
    }

    if (s.fLaunchfullscreen && !m_fFullScreen && !IsD3DFullScreenMode() && !m_fAudioOnly) {
        OnViewFullscreen();
    }

    // Add temporary flag to allow EC_VIDEO_SIZE_CHANGED event to stabilize window size
    // for 5 seconds since playback starts
    m_bAllowWindowZoom = true;
    m_timerOneTime.Subscribe(TimerOneTimeSubscriber::AUTOFIT_TIMEOUT, [this]
    { m_bAllowWindowZoom = false; }, 5000);

    // update control bar areas and paint bypassing the message queue
    RecalcLayout();
    UpdateWindow();

    // the window is repositioned and repainted, video renderer rect is ready to be set -
    // OnPlayPlay()/OnPlayPause() will take care of that
    m_bDelaySetOutputRect = false;

    // start playback if requested
    m_bFirstPlay = true;
    const auto uModeChangeDelay = s.autoChangeFSMode.uDelay * 1000;
    if (!(s.nCLSwitches & CLSW_OPEN) && (s.nLoops > 0)) {
        if (m_bOpeningInAutochangedMonitorMode && uModeChangeDelay) {
            m_timerOneTime.Subscribe(TimerOneTimeSubscriber::DELAY_PLAYPAUSE_AFTER_AUTOCHANGE_MODE,
                                     std::bind(&CMainFrame::OnPlayPlay, this), uModeChangeDelay);
        } else {
            OnPlayPlay();
        }
    } else {
        // OnUpdatePlayPauseStop() will decide if we can pause the media
        if (m_bOpeningInAutochangedMonitorMode && uModeChangeDelay) {
            m_timerOneTime.Subscribe(TimerOneTimeSubscriber::DELAY_PLAYPAUSE_AFTER_AUTOCHANGE_MODE,
                                     [this] { OnCommand(ID_PLAY_PAUSE, 0); }, uModeChangeDelay);
        } else {
            OnCommand(ID_PLAY_PAUSE, 0);
        }
    }
    s.nCLSwitches &= ~CLSW_OPEN;

    // Ensure the dynamically added menu items are updated
    SetupFiltersSubMenu();
    SetupAudioSubMenu();
    SetupSubtitlesSubMenu();
    SetupVideoStreamsSubMenu();
    SetupJumpToSubMenus();
    SetupRecentFilesSubMenu();

    // notify listeners
    if (GetPlaybackMode() != PM_DIGITAL_CAPTURE) {
        SendNowPlayingToSkype();
        SendNowPlayingToApi();
    }

    if (CanPreviewUse() && m_wndSeekBar.IsVisible()) {
        CPoint point;
        GetCursorPos(&point);

        CRect rect;
        m_wndSeekBar.GetWindowRect(&rect);
        if (rect.PtInRect(point)) {
            m_wndSeekBar.PreviewWindowShow(point);
        }
    }

    m_bSettingUpMenus = false;

    return 0;
}

LRESULT CMainFrame::OnOpenMediaFailed(WPARAM wParam, LPARAM lParam)
{
    ASSERT(GetLoadState() == MLS::LOADING);
    SetLoadState(MLS::FAILING);

    ASSERT(GetCurrentThreadId() == AfxGetApp()->m_nThreadID);
    const auto& s = AfxGetAppSettings();

    m_lastOMD.Free();
    m_lastOMD.Attach((OpenMediaData*)lParam);

    bool bOpenNextInPlaylist = false;
    bool bAfterPlaybackEvent = false;

    m_bOpenMediaActive = false;

    if (wParam == PM_FILE) {
        auto* pli = m_wndPlaylistBar.GetCur();
        if (pli != nullptr && pli->m_bYoutubeDL && m_sydlLastProcessURL != pli->m_ydlSourceURL) {
            OpenCurPlaylistItem(0, true);  // Try to reprocess if failed first time.
            return 0;
        }
        if (m_wndPlaylistBar.GetCount() == 1) {
            if (m_nLastSkipDirection == ID_NAVIGATE_SKIPBACK) {
                bOpenNextInPlaylist = SearchInDir(false, s.bLoopFolderOnPlayNextFile);
                if (!bOpenNextInPlaylist) {
                    m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_FIRST_IN_FOLDER));
                }
            } else if (m_nLastSkipDirection == ID_NAVIGATE_SKIPFORWARD) {
                bOpenNextInPlaylist = SearchInDir(true, s.bLoopFolderOnPlayNextFile);
                if (!bOpenNextInPlaylist) {
                    m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_LAST_IN_FOLDER));
                }
            }
        } else {
            m_wndPlaylistBar.SetCurValid(false);

            if (m_wndPlaylistBar.IsAtEnd()) {
                m_nLoops++;
            }

            if (s.fLoopForever || m_nLoops < s.nLoops) {
                if (m_nLastSkipDirection == ID_NAVIGATE_SKIPBACK) {
                    bOpenNextInPlaylist = m_wndPlaylistBar.SetPrev();
                } else {
                    bOpenNextInPlaylist = m_wndPlaylistBar.SetNext();
                }
            } else {
                bAfterPlaybackEvent = true;
            }
        }
    }

    CloseMedia(bOpenNextInPlaylist);

    if (bOpenNextInPlaylist) {
        OpenCurPlaylistItem();
    }
    else if (bAfterPlaybackEvent) {
        DoAfterPlaybackEvent();
    }

    return 0;
}

void CMainFrame::OnFilePostClosemedia(bool bNextIsQueued/* = false*/)
{
    SetPlaybackMode(PM_NONE);
    SetLoadState(MLS::CLOSED);

    m_kfs.clear();

    m_nCurSubtitle = -1;
    m_lSubtitleShift = 0;

    CAppSettings& s = AfxGetAppSettings();
    if (!s.fSavePnSZoom) {
        m_AngleX = m_AngleY = m_AngleZ = 0;
        m_ZoomX = m_ZoomY = 1.0;
        m_PosX = m_PosY = 0.5;
    }

    if (m_closingmsg.IsEmpty()) {
        m_closingmsg.LoadString(IDS_CONTROLS_CLOSED);
    }

    m_wndView.SetVideoRect();
    m_wndSeekBar.Enable(false);
    m_wndSeekBar.SetRange(0, 0);
    m_wndSeekBar.SetPos(0);
    m_wndSeekBar.RemoveChapters();
    m_wndInfoBar.RemoveAllLines();
    m_wndStatsBar.RemoveAllLines();
    m_wndStatusBar.Clear();
    m_wndStatusBar.ShowTimer(false);
    currentAudioLang.Empty();
    currentSubLang.Empty();
    m_OSD.SetRange(0, 0);
    m_OSD.SetPos(0);
    m_Lcd.SetMediaRange(0, 0);
    m_Lcd.SetMediaPos(0);

    if (!bNextIsQueued) {
        UpdateControlState(CMainFrame::UPDATE_LOGO);
        RecalcLayout();
    }

    if (s.fEnableEDLEditor) {
        m_wndEditListEditor.CloseFile();
    }

    if (m_controls.ControlChecked(CMainFrameControls::Panel::SUBRESYNC)) {
        m_controls.ToggleControl(CMainFrameControls::Panel::SUBRESYNC);
    }

    if (m_controls.ControlChecked(CMainFrameControls::Panel::CAPTURE)) {
        m_controls.ToggleControl(CMainFrameControls::Panel::CAPTURE);
    }
    m_wndCaptureBar.m_capdlg.SetupVideoControls(_T(""), nullptr, nullptr, nullptr);
    m_wndCaptureBar.m_capdlg.SetupAudioControls(_T(""), nullptr, CInterfaceArray<IAMAudioInputMixer>());

    if (m_controls.ControlChecked(CMainFrameControls::Panel::NAVIGATION)) {
        m_controls.ToggleControl(CMainFrameControls::Panel::NAVIGATION);
    }

    if (!bNextIsQueued) {
        OpenSetupWindowTitle(true);
    }

    SetAlwaysOnTop(s.iOnTop);

    SendNowPlayingToSkype();

    // try to release external objects
    UnloadUnusedExternalObjects();
    SetTimer(TIMER_UNLOAD_UNUSED_EXTERNAL_OBJECTS, 60000, nullptr);

    if (IsD3DFullScreenMode()) {
        m_pFullscreenWnd->DestroyWindow();
        m_fStartInD3DFullscreen = true;
    }
}

void CMainFrame::OnBossKey()
{
    // Disable animation
    ANIMATIONINFO AnimationInfo;
    AnimationInfo.cbSize = sizeof(ANIMATIONINFO);
    ::SystemParametersInfo(SPI_GETANIMATION, sizeof(ANIMATIONINFO), &AnimationInfo, 0);
    int m_WindowAnimationType = AnimationInfo.iMinAnimate;
    AnimationInfo.iMinAnimate = 0;
    ::SystemParametersInfo(SPI_SETANIMATION, sizeof(ANIMATIONINFO), &AnimationInfo, 0);

    SendMessage(WM_COMMAND, ID_PLAY_PAUSE);
    if (m_fFullScreen || IsD3DFullScreenMode()) {
        SendMessage(WM_COMMAND, ID_VIEW_FULLSCREEN);
    }
    SendMessage(WM_SYSCOMMAND, SC_MINIMIZE, -1);

    // Enable animation
    AnimationInfo.iMinAnimate = m_WindowAnimationType;
    ::SystemParametersInfo(SPI_SETANIMATION, sizeof(ANIMATIONINFO), &AnimationInfo, 0);
}

void CMainFrame::OnStreamAudio(UINT nID)
{
    nID -= ID_STREAM_AUDIO_NEXT;

    if (GetLoadState() != MLS::LOADED) {
        return;
    }

    CComQIPtr<IAMStreamSelect> pSS = FindFilter(__uuidof(CAudioSwitcherFilter), m_pGB);

    DWORD cStreams = 0;
    if (pSS && SUCCEEDED(pSS->Count(&cStreams)) && cStreams > 1) {
        for (DWORD i = 0; i < cStreams; i++) {
            DWORD dwFlags = 0;
            LCID lcid = 0;
            DWORD dwGroup = 0;
            CComHeapPtr<WCHAR> pszName;
            if (FAILED(pSS->Info(i, nullptr, &dwFlags, &lcid, &dwGroup, &pszName, nullptr, nullptr))) {
                return;
            }
            if (dwFlags & (AMSTREAMSELECTINFO_ENABLED | AMSTREAMSELECTINFO_EXCLUSIVE)) {
                long stream_index = (i + (nID == 0 ? 1 : cStreams - 1)) % cStreams;
                pSS->Enable(stream_index, AMSTREAMSELECTENABLE_ENABLE);
                if (SUCCEEDED(pSS->Info(stream_index, nullptr, &dwFlags, &lcid, &dwGroup, &pszName, nullptr, nullptr))) {
                    m_OSD.DisplayMessage(OSD_TOPLEFT, GetStreamOSDString(CString(pszName), lcid, 1));
                    if (lcid) {
                        GetLocaleString(lcid, LOCALE_SISO639LANGNAME2, currentAudioLang);
                    } else {
                        currentAudioLang.Empty();
                    }
                }
                break;
            }
        }
    } else if (GetPlaybackMode() == PM_FILE) {
        OnStreamSelect(nID == 0, 1);
    } else if (GetPlaybackMode() == PM_DVD) {
        SendMessage(WM_COMMAND, ID_DVD_AUDIO_NEXT + nID);
    }
}

void CMainFrame::OnStreamSub(UINT nID)
{
    nID -= ID_STREAM_SUB_NEXT;
    if (GetLoadState() != MLS::LOADED) {
        return;
    }

    if (!m_pSubStreams.IsEmpty()) {
        AfxGetAppSettings().fEnableSubtitles = true;
        SetSubtitle(nID == 0 ? 1 : -1, true, true);
        SetFocus();
    } else if (GetPlaybackMode() == PM_FILE) {
        OnStreamSelect(nID == 0, 2);
    } else if (GetPlaybackMode() == PM_DVD) {
        SendMessage(WM_COMMAND, ID_DVD_SUB_NEXT + nID);
    }
}

void CMainFrame::OnStreamSubOnOff()
{
    if (GetLoadState() != MLS::LOADED) {
        return;
    }

    if (m_pCAP && !m_pSubStreams.IsEmpty()) {
        ToggleSubtitleOnOff(true);
        SetFocus();
    } else if (GetPlaybackMode() == PM_DVD) {
        SendMessage(WM_COMMAND, ID_DVD_SUB_ONOFF);
    }
}

void CMainFrame::OnDvdAngle(UINT nID)
{
    if (GetLoadState() != MLS::LOADED) {
        return;
    }

    if (m_pDVDI && m_pDVDC) {
        ULONG ulAnglesAvailable, ulCurrentAngle;
        if (SUCCEEDED(m_pDVDI->GetCurrentAngle(&ulAnglesAvailable, &ulCurrentAngle)) && ulAnglesAvailable > 1) {
            ulCurrentAngle += (nID == ID_DVD_ANGLE_NEXT) ? 1 : -1;
            if (ulCurrentAngle > ulAnglesAvailable) {
                ulCurrentAngle = 1;
            } else if (ulCurrentAngle < 1) {
                ulCurrentAngle = ulAnglesAvailable;
            }
            m_pDVDC->SelectAngle(ulCurrentAngle, DVD_CMD_FLAG_Block, nullptr);

            CString osdMessage;
            osdMessage.Format(IDS_AG_ANGLE, ulCurrentAngle);
            m_OSD.DisplayMessage(OSD_TOPLEFT, osdMessage);
        }
    }
}

void CMainFrame::OnDvdAudio(UINT nID)
{
    nID -= ID_DVD_AUDIO_NEXT;

    if (GetLoadState() != MLS::LOADED) {
        return;
    }

    if (m_pDVDI && m_pDVDC) {
        ULONG nStreamsAvailable, nCurrentStream;
        if (SUCCEEDED(m_pDVDI->GetCurrentAudio(&nStreamsAvailable, &nCurrentStream)) && nStreamsAvailable > 1) {
            DVD_AudioAttributes AATR;
            UINT nNextStream = (nCurrentStream + (nID == 0 ? 1 : nStreamsAvailable - 1)) % nStreamsAvailable;

            HRESULT hr = m_pDVDC->SelectAudioStream(nNextStream, DVD_CMD_FLAG_Block, nullptr);
            if (SUCCEEDED(m_pDVDI->GetAudioAttributes(nNextStream, &AATR))) {
                CString lang;
                CString strMessage;
                if (AATR.Language) {
                    GetLocaleString(AATR.Language, LOCALE_SENGLANGUAGE, lang);
                } else {
                    lang.Format(IDS_AG_UNKNOWN, nNextStream + 1);
                }

                CString format = GetDVDAudioFormatName(AATR);
                CString str;

                if (!format.IsEmpty()) {
                    str.Format(IDS_MAINFRM_11,
                               lang.GetString(),
                               format.GetString(),
                               AATR.dwFrequency,
                               AATR.bQuantization,
                               AATR.bNumberOfChannels,
                               ResStr(AATR.bNumberOfChannels > 1 ? IDS_MAINFRM_13 : IDS_MAINFRM_12).GetString());
                    if (FAILED(hr)) {
                        str += _T(" [") + ResStr(IDS_AG_ERROR) + _T("] ");
                    }
                    strMessage.Format(IDS_AUDIO_STREAM, str.GetString());
                    m_OSD.DisplayMessage(OSD_TOPLEFT, strMessage);
                }
            }
        }
    }
}

void CMainFrame::OnDvdSub(UINT nID)
{
    nID -= ID_DVD_SUB_NEXT;

    if (GetLoadState() != MLS::LOADED) {
        return;
    }

    if (m_pDVDI && m_pDVDC) {
        ULONG ulStreamsAvailable, ulCurrentStream;
        BOOL bIsDisabled;
        if (SUCCEEDED(m_pDVDI->GetCurrentSubpicture(&ulStreamsAvailable, &ulCurrentStream, &bIsDisabled))
                && ulStreamsAvailable > 1) {
            //UINT nNextStream = (ulCurrentStream+(nID==0?1:ulStreamsAvailable-1))%ulStreamsAvailable;
            int nNextStream;

            if (!bIsDisabled) {
                nNextStream = ulCurrentStream + (nID == 0 ? 1 : -1);
            } else {
                nNextStream = (nID == 0 ? 0 : ulStreamsAvailable - 1);
            }

            if (!bIsDisabled && ((nNextStream < 0) || ((ULONG)nNextStream >= ulStreamsAvailable))) {
                m_pDVDC->SetSubpictureState(FALSE, DVD_CMD_FLAG_Block, nullptr);
                m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_SUBTITLE_STREAM_OFF));
            } else {
                HRESULT hr = m_pDVDC->SelectSubpictureStream(nNextStream, DVD_CMD_FLAG_Block, nullptr);

                DVD_SubpictureAttributes SATR;
                m_pDVDC->SetSubpictureState(TRUE, DVD_CMD_FLAG_Block, nullptr);
                if (SUCCEEDED(m_pDVDI->GetSubpictureAttributes(nNextStream, &SATR))) {
                    CString lang;
                    CString strMessage;
                    GetLocaleString(SATR.Language, LOCALE_SENGLANGUAGE, lang);

                    if (FAILED(hr)) {
                        lang += _T(" [") + ResStr(IDS_AG_ERROR) + _T("] ");
                    }
                    strMessage.Format(IDS_SUBTITLE_STREAM, lang.GetString());
                    m_OSD.DisplayMessage(OSD_TOPLEFT, strMessage);
                }
            }
        }
    }
}

void CMainFrame::OnDvdSubOnOff()
{
    if (GetLoadState() != MLS::LOADED) {
        return;
    }

    if (m_pDVDI && m_pDVDC) {
        ULONG ulStreamsAvailable, ulCurrentStream;
        BOOL bIsDisabled;
        if (SUCCEEDED(m_pDVDI->GetCurrentSubpicture(&ulStreamsAvailable, &ulCurrentStream, &bIsDisabled))) {
            m_pDVDC->SetSubpictureState(bIsDisabled, DVD_CMD_FLAG_Block, nullptr);
        }
    }
}

//
// menu item handlers
//

// file

void CMainFrame::OnFileOpenQuick()
{
    if (GetLoadState() == MLS::LOADING || !IsWindow(m_wndPlaylistBar)) {
        return;
    }

    const CAppSettings& s = AfxGetAppSettings();
    CString filter;
    CAtlArray<CString> mask;
    s.m_Formats.GetFilter(filter, mask);

    DWORD dwFlags = OFN_EXPLORER | OFN_ENABLESIZING | OFN_ALLOWMULTISELECT | OFN_ENABLEINCLUDENOTIFY | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    if (!s.fKeepHistory) {
        dwFlags |= OFN_DONTADDTORECENT;
    }

    COpenFileDlg fd(mask, true, nullptr, nullptr, dwFlags, filter, GetModalParent());
    if (fd.DoModal() != IDOK) {
        return;
    }

    CAtlList<CString> fns;

    POSITION pos = fd.GetStartPosition();
    while (pos) {
        fns.AddTail(fd.GetNextPathName(pos));
    }

    bool fMultipleFiles = false;

    if (fns.GetCount() > 1
            || fns.GetCount() == 1
            && (fns.GetHead()[fns.GetHead().GetLength() - 1] == '\\'
                || fns.GetHead()[fns.GetHead().GetLength() - 1] == '*')) {
        fMultipleFiles = true;
    }

    SendMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);

    if (IsIconic()) {
        ShowWindow(SW_RESTORE);
    }
    SetForegroundWindow();

    if (fns.GetCount() == 1) {
        if (OpenBD(fns.GetHead())) {
            return;
        }
    }

    m_wndPlaylistBar.Open(fns, fMultipleFiles);

    OpenCurPlaylistItem();
}

void CMainFrame::OnFileOpenmedia()
{
    if (GetLoadState() == MLS::LOADING || !IsWindow(m_wndPlaylistBar) || IsD3DFullScreenMode()) {
        return;
    }

    static COpenDlg dlg;
    if (IsWindow(dlg.GetSafeHwnd()) && dlg.IsWindowVisible()) {
        dlg.SetForegroundWindow();
        return;
    }
    if (dlg.DoModal() != IDOK || dlg.GetFileNames().IsEmpty()) {
        return;
    }

    if (!dlg.GetAppendToPlaylist()) {
        SendMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);
    }

    if (IsIconic()) {
        ShowWindow(SW_RESTORE);
    }
    SetForegroundWindow();

    CAtlList<CString> filenames;

    if (CanSendToYoutubeDL(dlg.GetFileNames().GetHead())) {
        if (ProcessYoutubeDLURL(dlg.GetFileNames().GetHead(), dlg.GetAppendToPlaylist())) {
            if (!dlg.GetAppendToPlaylist()) {
                OpenCurPlaylistItem();
            }
            return;
        } else if (IsOnYDLWhitelist(dlg.GetFileNames().GetHead())) {
            // don't bother trying to open this website URL directly
            return;
        }
    }

    filenames.AddHeadList(&dlg.GetFileNames());

    if (!dlg.HasMultipleFiles()) {
        if (OpenBD(filenames.GetHead())) {
            return;
        }
    }

    if (dlg.GetAppendToPlaylist()) {
        m_wndPlaylistBar.Append(filenames, dlg.HasMultipleFiles());
    } else {
        m_wndPlaylistBar.Open(filenames, dlg.HasMultipleFiles());

        OpenCurPlaylistItem();
    }
}

void CMainFrame::OnUpdateFileOpen(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(GetLoadState() != MLS::LOADING);
}

BOOL CMainFrame::OnCopyData(CWnd* pWnd, COPYDATASTRUCT* pCDS)
{
    if (AfxGetMyApp()->m_fClosingState) {
        return FALSE;
    }

    CAppSettings& s = AfxGetAppSettings();

    if (m_pSkypeMoodMsgHandler && m_pSkypeMoodMsgHandler->HandleMessage(pWnd->GetSafeHwnd(), pCDS)) {
        return TRUE;
    } else if (pCDS->dwData != 0x6ABE51 || pCDS->cbData < sizeof(DWORD)) {
        if (s.hMasterWnd) {
            ProcessAPICommand(pCDS);
            return TRUE;
        } else {
            return FALSE;
        }
    }

    if (m_bScanDlgOpened) {
        return FALSE;
    }

    DWORD len = *((DWORD*)pCDS->lpData);
    TCHAR* pBuff = (TCHAR*)((DWORD*)pCDS->lpData + 1);
    TCHAR* pBuffEnd = (TCHAR*)((BYTE*)pBuff + pCDS->cbData - sizeof(DWORD));

    CAtlList<CString> cmdln;

    while (len-- > 0 && pBuff < pBuffEnd) {
        CString str(pBuff);
        pBuff += str.GetLength() + 1;

        cmdln.AddTail(str);
    }

    s.ParseCommandLine(cmdln);

    if (s.nCLSwitches & CLSW_SLAVE) {
        SendAPICommand(CMD_CONNECT, L"%d", PtrToInt(GetSafeHwnd()));
        s.nCLSwitches &= ~CLSW_SLAVE;
    }

    POSITION pos = s.slFilters.GetHeadPosition();
    while (pos) {
        CString fullpath = MakeFullPath(s.slFilters.GetNext(pos));

        CPath tmp(fullpath);
        tmp.RemoveFileSpec();
        tmp.AddBackslash();
        CString path = tmp;

        WIN32_FIND_DATA fd;
        ZeroMemory(&fd, sizeof(WIN32_FIND_DATA));
        HANDLE hFind = FindFirstFile(fullpath, &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    continue;
                }

                CFilterMapper2 fm2(false);
                fm2.Register(path + fd.cFileName);
                while (!fm2.m_filters.IsEmpty()) {
                    if (FilterOverride* f = fm2.m_filters.RemoveTail()) {
                        f->fTemporary = true;

                        bool fFound = false;

                        POSITION pos2 = s.m_filters.GetHeadPosition();
                        while (pos2) {
                            FilterOverride* f2 = s.m_filters.GetNext(pos2);
                            if (f2->type == FilterOverride::EXTERNAL && !f2->path.CompareNoCase(f->path)) {
                                fFound = true;
                                break;
                            }
                        }

                        if (!fFound) {
                            CAutoPtr<FilterOverride> p(f);
                            s.m_filters.AddHead(p);
                        }
                    }
                }
            } while (FindNextFile(hFind, &fd));

            FindClose(hFind);
        }
    }

    bool fSetForegroundWindow = false;

    auto applyRandomizeSwitch = [&]() {
        if (s.nCLSwitches & CLSW_RANDOMIZE) {
            m_wndPlaylistBar.Randomize();
            s.nCLSwitches &= ~CLSW_RANDOMIZE;
        }
    };

    if ((s.nCLSwitches & CLSW_DVD) && !s.slFiles.IsEmpty()) {
        SendMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);
        fSetForegroundWindow = true;

        CAutoPtr<OpenDVDData> p(DEBUG_NEW OpenDVDData());
        if (p) {
            p->path = s.slFiles.GetHead();
            p->subs.AddTailList(&s.slSubs);
        }
        OpenMedia(p);
        s.nCLSwitches &= ~CLSW_DVD;
    } else if (s.nCLSwitches & CLSW_CD) {
        SendMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);
        fSetForegroundWindow = true;

        CAtlList<CString> sl;

        if (!s.slFiles.IsEmpty()) {
            GetOpticalDiskType(s.slFiles.GetHead()[0], sl);
        } else {
            CString dir;
            dir.ReleaseBufferSetLength(GetCurrentDirectory(MAX_PATH, dir.GetBuffer(MAX_PATH)));

            GetOpticalDiskType(dir[0], sl);

            for (TCHAR drive = _T('A'); sl.IsEmpty() && drive <= _T('Z'); drive++) {
                GetOpticalDiskType(drive, sl);
            }
        }

        m_wndPlaylistBar.Open(sl, true);
        applyRandomizeSwitch();
        OpenCurPlaylistItem();
        s.nCLSwitches &= ~CLSW_CD;
    } else if (s.nCLSwitches & CLSW_DEVICE) {
        SendMessage(WM_COMMAND, ID_FILE_OPENDEVICE);
        s.nCLSwitches &= ~CLSW_DEVICE;
    } else if (!s.slFiles.IsEmpty()) {
        CAtlList<CString> sl;
        sl.AddTailList(&s.slFiles);

        PathUtils::ParseDirs(sl);

        bool fMulti = sl.GetCount() > 1;

        if (!fMulti) {
            sl.AddTailList(&s.slDubs);
        }

        if (OpenBD(s.slFiles.GetHead())) {
            // Nothing more to do
        } else if (!fMulti && CPath(s.slFiles.GetHead() + _T("\\VIDEO_TS")).IsDirectory()) {
            SendMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);
            fSetForegroundWindow = true;

            CAutoPtr<OpenDVDData> p(DEBUG_NEW OpenDVDData());
            if (p) {
                p->path = s.slFiles.GetHead();
                p->subs.AddTailList(&s.slSubs);
            }
            OpenMedia(p);
        } else {
            if (m_dwLastRun && ((GetTickCount64() - m_dwLastRun) < 1000ULL)) {
                s.nCLSwitches |= CLSW_ADD;
            }
            m_dwLastRun = GetTickCount64();

            if ((s.nCLSwitches & CLSW_ADD) && !IsPlaylistEmpty()) {             
                POSITION pos2 = sl.GetHeadPosition();
                while (pos2) {
                    CString fn = sl.GetNext(pos2);
                    if (!CanSendToYoutubeDL(fn) || !ProcessYoutubeDLURL(fn, true)) {
                        CAtlList<CString> sl2;
                        sl2.AddHead(fn);
                        m_wndPlaylistBar.Append(sl2, false, &s.slSubs);
                    }
                }

                applyRandomizeSwitch();

                if (s.nCLSwitches & (CLSW_OPEN | CLSW_PLAY)) {
                    m_wndPlaylistBar.SetLast();
                    OpenCurPlaylistItem();
                }
            } else {
                //SendMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);
                fSetForegroundWindow = true;

                bool first = true;
                POSITION pos2 = sl.GetHeadPosition();
                while (pos2) {
                    CString fn = sl.GetNext(pos2);
                    if (!CanSendToYoutubeDL(fn) || !ProcessYoutubeDLURL(fn, !first)) {
                        CAtlList<CString> sl2;
                        sl2.AddHead(fn);
                        if (first) {
                            m_wndPlaylistBar.Open(sl2, false, &s.slSubs);
                        } else {
                            m_wndPlaylistBar.Append(sl2, false, &s.slSubs);
                        }
                    }
                    first = false;
                }

                applyRandomizeSwitch();
                m_wndPlaylistBar.SetFirst();
                OpenCurPlaylistItem((s.nCLSwitches & CLSW_STARTVALID) ? s.rtStart : 0);

                s.nCLSwitches &= ~CLSW_STARTVALID;
                s.rtStart = 0;
            }
            s.nCLSwitches &= ~CLSW_ADD;
        }
    } else {
        applyRandomizeSwitch();
    }

    if (s.nCLSwitches & CLSW_PRESET1) {
        SendMessage(WM_COMMAND, ID_VIEW_PRESETS_MINIMAL);
        s.nCLSwitches &= ~CLSW_PRESET1;
    } else if (s.nCLSwitches & CLSW_PRESET2) {
        SendMessage(WM_COMMAND, ID_VIEW_PRESETS_COMPACT);
        s.nCLSwitches &= ~CLSW_PRESET2;
    } else if (s.nCLSwitches & CLSW_PRESET3) {
        SendMessage(WM_COMMAND, ID_VIEW_PRESETS_NORMAL);
        s.nCLSwitches &= ~CLSW_PRESET3;
    }
    if (s.nCLSwitches & CLSW_MUTE) {
        if (!IsMuted()) {
            SendMessage(WM_COMMAND, ID_VOLUME_MUTE);
        }
        s.nCLSwitches &= ~CLSW_MUTE;
    }
    if (s.nCLSwitches & CLSW_VOLUME) {
        m_wndToolBar.SetVolume(s.nCmdVolume);
        s.nCLSwitches &= ~CLSW_VOLUME;
    }

    if (fSetForegroundWindow && !(s.nCLSwitches & CLSW_NOFOCUS)) {
        SetForegroundWindow();
    }

    return TRUE;
}

int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lp, LPARAM pData)
{
    switch (uMsg) {
        case BFFM_INITIALIZED: {
            //Initial directory is set here
            const CAppSettings& s = AfxGetAppSettings();
            if (!s.strDVDPath.IsEmpty()) {
                SendMessage(hwnd, BFFM_SETSELECTION, TRUE, (LPARAM)(LPCTSTR)s.strDVDPath);
            }
            break;
        }
        default:
            break;
    }
    return 0;
}

void CMainFrame::OnFileOpendvd()
{
    if ((GetLoadState() == MLS::LOADING) || IsD3DFullScreenMode()) {
        return;
    }

    CAppSettings& s = AfxGetAppSettings();
    CString strTitle(StrRes(IDS_MAINFRM_46));
    CString path;

    if (s.fUseDVDPath && !s.strDVDPath.IsEmpty()) {
        path = s.strDVDPath;
    } else {
        CFileDialog dlg(TRUE);
        IFileOpenDialog* openDlgPtr = dlg.GetIFileOpenDialog();

        if (openDlgPtr != nullptr) {
            openDlgPtr->SetTitle(strTitle);
            openDlgPtr->SetOptions(FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
            if (FAILED(openDlgPtr->Show(m_hWnd))) {
                openDlgPtr->Release();
                return;
            }
            openDlgPtr->Release();

            path = dlg.GetFolderPath();
        }
    }

    if (!path.IsEmpty()) {
        s.strDVDPath = path;
        if (!OpenBD(path)) {
            CAutoPtr<OpenDVDData> p(DEBUG_NEW OpenDVDData());
            p->path = path;
            p->path.Replace(_T('/'), _T('\\'));
            if (p->path[p->path.GetLength() - 1] != '\\') {
                p->path += _T('\\');
            }

            OpenMedia(p);
        }
    }
}

void CMainFrame::OnFileOpendevice()
{
    const CAppSettings& s = AfxGetAppSettings();

    if (GetLoadState() == MLS::LOADING) {
        return;
    }

    SendMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);
    SetForegroundWindow();

    if (IsIconic()) {
        ShowWindow(SW_RESTORE);
    }

    m_wndPlaylistBar.Empty();

    CAutoPtr<OpenDeviceData> p(DEBUG_NEW OpenDeviceData());
    if (p) {
        p->DisplayName[0] = s.strAnalogVideo;
        p->DisplayName[1] = s.strAnalogAudio;
    }
    OpenMedia(p);
}

void CMainFrame::OnFileOpenOpticalDisk(UINT nID)
{
    nID -= ID_FILE_OPEN_OPTICAL_DISK_START;

    nID++;
    for (TCHAR drive = _T('A'); drive <= _T('Z'); drive++) {
        CAtlList<CString> sl;

        switch (GetOpticalDiskType(drive, sl)) {
            case OpticalDisk_Audio:
            case OpticalDisk_VideoCD:
            case OpticalDisk_DVDVideo:
            case OpticalDisk_BD:
                nID--;
                break;
            default:
                break;
        }

        if (nID == 0) {
            SendMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);
            SetForegroundWindow();

            if (IsIconic()) {
                ShowWindow(SW_RESTORE);
            }

            m_wndPlaylistBar.Open(sl, true);
            OpenCurPlaylistItem();

            break;
        }
    }
}

void CMainFrame::OnFileRecycle()
{
    // check if a file is playing
    if (GetPlaybackMode() != PM_FILE) {
        return;
    }

    m_wndPlaylistBar.DeleteFileInPlaylist(m_wndPlaylistBar.m_pl.GetPos());
}

void CMainFrame::OnFileReopen()
{
    if (!m_LastOpenBDPath.IsEmpty() && OpenBD(m_LastOpenBDPath)) {
        return;
    }
    OpenCurPlaylistItem(0, true);
}

DROPEFFECT CMainFrame::OnDropAccept(COleDataObject* pDataObject, DWORD dwKeyState, CPoint point)
{
    ClientToScreen(&point);
    if (CMouse::CursorOnRootWindow(point, *this)) {
        UpdateControlState(UPDATE_CONTROLS_VISIBILITY);
        return (dwKeyState & MK_CONTROL) ? (DROPEFFECT_COPY | DROPEFFECT_APPEND)
               : (DROPEFFECT_MOVE | DROPEFFECT_LINK | DROPEFFECT_COPY);
    }

    return DROPEFFECT_NONE;
}

BOOL IsSubtitleExtension(CString ext)
{
    return (ext == _T(".srt") || ext == _T(".ssa") || ext == _T(".ass") || ext == _T(".idx") || ext == _T(".sub") || ext == _T(".vtt") || ext == _T(".sup") || ext == _T(".smi") || ext == _T(".psb") || ext == _T(".usf") || ext == _T(".xss") || ext == _T(".rt")|| ext == _T(".txt"));
}

BOOL IsSubtitleFilename(CString filename)
{
    CString ext = CPath(filename).GetExtension().MakeLower();
    return IsSubtitleExtension(ext);
}

BOOL IsAudioFilename(CString filename)
{
    const CMediaFormats& mf = AfxGetAppSettings().m_Formats;
    CString ext = CPath(filename).GetExtension().MakeLower();
    return mf.FindExt(ext, true);
}

void CMainFrame::OnDropFiles(CAtlList<CString>& slFiles, DROPEFFECT dropEffect)
{
    SetForegroundWindow();

    if (slFiles.IsEmpty()) {
        return;
    }

    if (slFiles.GetCount() == 1 && OpenBD(slFiles.GetHead())) {
        return;
    }

    PathUtils::ParseDirs(slFiles);

    bool bAppend = !!(dropEffect & DROPEFFECT_APPEND);

    // Check for subtitle files
    SubtitleInput subInputSelected;
    CString subfile;
    BOOL onlysubs = true;
    POSITION pos = slFiles.GetHeadPosition();
    while (pos) {
        SubtitleInput subInput;
        POSITION curpos = pos;
        subfile = slFiles.GetNext(pos);
        if (IsSubtitleFilename(subfile)) {
            // remove subtitle file from list
            slFiles.RemoveAt(curpos);
            // try to load it
            if (!bAppend && m_pCAP && GetLoadState() == MLS::LOADED && !IsPlaybackCaptureMode() && !m_fAudioOnly && LoadSubtitle(subfile, &subInput, False)) {
                if (onlysubs && !subInputSelected.pSubStream) {
                    // first one
                    subInputSelected = subInput;
                    AfxGetAppSettings().fEnableSubtitles = true;
                    SetSubtitle(subInputSelected);

                    CPath fn(subfile);
                    fn.StripPath();
                    CString statusmsg(static_cast<LPCTSTR>(fn));
                    SendStatusMessage(statusmsg + ResStr(IDS_SUB_LOADED_SUCCESS), 3000);
                }
            }
        }
        else {
            onlysubs = false;
        }
    }

    if (onlysubs && subInputSelected.pSubStream) {
        // subtitles have been loaded, we are done now
        return;
    }

    // list might be empty now if a subtitle file failed to load
    if (slFiles.IsEmpty()) {
        SendStatusMessage(_T("Failed to load subtitle file"), 3000);
        return;
    }

    // load http url with youtube-dl, if available
    if (CanSendToYoutubeDL(slFiles.GetHead())) {
        if (ProcessYoutubeDLURL(slFiles.GetHead(), bAppend)) {
            if (!bAppend) {
                OpenCurPlaylistItem();
            }
            return;
        } else if (IsOnYDLWhitelist(slFiles.GetHead())) {
            // don't bother trying to open this website URL directly
            return;
        }
    }

    // add remaining items
    if (bAppend) {
        m_wndPlaylistBar.Append(slFiles, true);
    } else {
        m_wndPlaylistBar.Open(slFiles, true);
        OpenCurPlaylistItem();
    }
}

void CMainFrame::OnFileSaveAs()
{
    CString in, out, ext;

    CPlaylistItem* pli = m_wndPlaylistBar.GetCur();
    if (pli && !pli->m_fns.IsEmpty()) {
        in = pli->m_fns.GetHead();
    } else {
        return;
    }

    if (pli->m_bYoutubeDL || PathUtils::IsURL(in)) {
        // URL
        if (pli->m_bYoutubeDL) {
            out = _T("%(title)s.%(ext)s");
        } else {
            out = _T("choose_a_filename");
        }
    } else {
        out = PathUtils::StripPathOrUrl(in);
        ext = CPath(out).GetExtension().MakeLower();
        if (ext == _T(".cda")) {
            out = out.Left(out.GetLength() - 4) + _T(".wav");
        } else if (ext == _T(".ifo")) {
            out = out.Left(out.GetLength() - 4) + _T(".vob");
        }
    }

    if (!pli->m_bYoutubeDL || pli->m_ydlSourceURL.IsEmpty() || (AfxGetAppSettings().sYDLCommandLine.Find(_T("-o ")) < 0)) {
        CFileDialog fd(FALSE, 0, out,
                       OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR,
                       ResStr(IDS_ALL_FILES_FILTER), GetModalParent(), 0);
        if (fd.DoModal() != IDOK || !in.CompareNoCase(fd.GetPathName())) {
            return;
        } else {
            out = fd.GetPathName();
        }
    }

    if (pli->m_bYoutubeDL && !pli->m_ydlSourceURL.IsEmpty()) {
        DownloadWithYoutubeDL(pli->m_ydlSourceURL, out);
        return;
    }

    CPath p(out);
    if (!ext.IsEmpty()) {
        p.AddExtension(ext);
    }

    OAFilterState fs = State_Stopped;
    m_pMC->GetState(0, &fs);
    if (fs == State_Running) {
        m_pMC->Pause();
    }

    CSaveDlg dlg(in, p);
    dlg.DoModal();

    if (m_pMC && fs == State_Running) {
        m_pMC->Run();
    }
}

void CMainFrame::OnUpdateFileSaveAs(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(GetLoadState() == MLS::LOADED && GetPlaybackMode() == PM_FILE);
}

bool CMainFrame::GetDIB(BYTE** ppData, long& size, bool fSilent)
{
    if (!ppData) {
        return false;
    }

    *ppData = nullptr;
    size = 0;
    OAFilterState fs = GetMediaState();

    if (!(GetLoadState() == MLS::LOADED && !m_fAudioOnly && (fs == State_Paused || fs == State_Running))) {
        return false;
    }

    if (fs == State_Running && !m_pCAP) {
        m_pMC->Pause();
        GetMediaState(); // wait for completion of the pause command
    }

    HRESULT hr = S_OK;
    CString errmsg;

    do {
        if (m_pCAP) {
            hr = m_pCAP->GetDIB(nullptr, (DWORD*)&size);
            if (FAILED(hr)) {
                errmsg.Format(IDS_GETDIB_FAILED, hr);
                break;
            }

            *ppData = DEBUG_NEW BYTE[size];
            if (!(*ppData)) {
                return false;
            }

            hr = m_pCAP->GetDIB(*ppData, (DWORD*)&size);
            //if (FAILED(hr)) {errmsg.Format(_T("GetDIB failed, hr = %08x"), hr); break;}
            if (FAILED(hr)) {
                OnPlayPause();
                GetMediaState(); // Pause and retry to support ffdshow queuing.
                int retry = 0;
                while (FAILED(hr) && retry < 20) {
                    hr = m_pCAP->GetDIB(*ppData, (DWORD*)&size);
                    if (SUCCEEDED(hr)) {
                        break;
                    }
                    Sleep(1);
                    retry++;
                }
                if (FAILED(hr)) {
                    errmsg.Format(IDS_GETDIB_FAILED, hr);
                    break;
                }
            }
        } else if (m_pMFVDC) {
            // Capture with EVR
            BITMAPINFOHEADER bih = {sizeof(BITMAPINFOHEADER)};
            BYTE* pDib;
            DWORD dwSize;
            REFERENCE_TIME rtImage = 0;
            hr = m_pMFVDC->GetCurrentImage(&bih, &pDib, &dwSize, &rtImage);
            if (FAILED(hr) || dwSize == 0) {
                errmsg.Format(IDS_GETCURRENTIMAGE_FAILED, hr);
                break;
            }

            size = (long)dwSize + sizeof(BITMAPINFOHEADER);
            *ppData = DEBUG_NEW BYTE[size];
            if (!(*ppData)) {
                return false;
            }
            memcpy_s(*ppData, size, &bih, sizeof(BITMAPINFOHEADER));
            memcpy_s(*ppData + sizeof(BITMAPINFOHEADER), size - sizeof(BITMAPINFOHEADER), pDib, dwSize);
            CoTaskMemFree(pDib);
        } else {
            hr = m_pBV->GetCurrentImage(&size, nullptr);
            if (FAILED(hr) || size == 0) {
                errmsg.Format(IDS_GETCURRENTIMAGE_FAILED, hr);
                break;
            }

            *ppData = DEBUG_NEW BYTE[size];
            if (!(*ppData)) {
                return false;
            }

            hr = m_pBV->GetCurrentImage(&size, (long*)*ppData);
            if (FAILED(hr)) {
                errmsg.Format(IDS_GETCURRENTIMAGE_FAILED, hr);
                break;
            }
        }
    } while (0);

    if (!fSilent) {
        if (!errmsg.IsEmpty()) {
            AfxMessageBox(errmsg, MB_OK);
        }
    }

    if (fs == State_Running && GetMediaState() != State_Running) {
        m_pMC->Run();
    }

    if (FAILED(hr)) {
        SAFE_DELETE_ARRAY(*ppData);
        return false;
    }

    return true;
}

void CMainFrame::SaveDIB(LPCTSTR fn, BYTE* pData, long size)
{
    CPath path(fn);

    PBITMAPINFO bi = reinterpret_cast<PBITMAPINFO>(pData);
    PBITMAPINFOHEADER bih = &bi->bmiHeader;
    int bpp = bih->biBitCount;

    if (bpp != 16 && bpp != 24 && bpp != 32) {
        AfxMessageBox(IDS_SCREENSHOT_ERROR, MB_ICONWARNING | MB_OK, 0);
        return;
    }
    int w = bih->biWidth;
    int h = abs(bih->biHeight);
    int srcpitch = w * (bpp >> 3);
    int dstpitch = (w * 3 + 3) / 4 * 4; // round w * 3 to next multiple of 4

    BYTE* p = DEBUG_NEW BYTE[dstpitch * h];

    const BYTE* src = pData + sizeof(*bih);

    BitBltFromRGBToRGB(w, h, p, dstpitch, 24, (BYTE*)src + srcpitch * (h - 1), -srcpitch, bpp);

    {
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        ULONG_PTR gdiplusToken;
        Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

        Gdiplus::Bitmap* bm = new Gdiplus::Bitmap(w, h, dstpitch, PixelFormat24bppRGB, p);

        UINT num;       // number of image encoders
        UINT arraySize; // size, in bytes, of the image encoder array

        // How many encoders are there?
        // How big (in bytes) is the array of all ImageCodecInfo objects?
        Gdiplus::GetImageEncodersSize(&num, &arraySize);

        // Create a buffer large enough to hold the array of ImageCodecInfo
        // objects that will be returned by GetImageEncoders.
        Gdiplus::ImageCodecInfo* pImageCodecInfo = (Gdiplus::ImageCodecInfo*)DEBUG_NEW BYTE[arraySize];

        // GetImageEncoders creates an array of ImageCodecInfo objects
        // and copies that array into a previously allocated buffer.
        // The third argument, imageCodecInfos, is a pointer to that buffer.
        Gdiplus::GetImageEncoders(num, arraySize, pImageCodecInfo);

        Gdiplus::EncoderParameters* pEncoderParameters = nullptr;

        // Find the mime type based on the extension
        CString ext(path.GetExtension());
        CStringW mime;
        if (ext == _T(".jpg")) {
            mime = L"image/jpeg";

            // Set the encoder parameter for jpeg quality
            pEncoderParameters = new Gdiplus::EncoderParameters;
            ULONG quality = AfxGetAppSettings().nJpegQuality;

            pEncoderParameters->Count = 1;
            pEncoderParameters->Parameter[0].Guid = Gdiplus::EncoderQuality;
            pEncoderParameters->Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
            pEncoderParameters->Parameter[0].NumberOfValues = 1;
            pEncoderParameters->Parameter[0].Value = &quality;
        } else if (ext == _T(".bmp")) {
            mime = L"image/bmp";
        } else {
            mime = L"image/png";
        }

        // Get the encoder clsid
        CLSID encoderClsid = CLSID_NULL;
        for (UINT i = 0; i < num && encoderClsid == CLSID_NULL; i++) {
            if (wcscmp(pImageCodecInfo[i].MimeType, mime) == 0) {
                encoderClsid = pImageCodecInfo[i].Clsid;
            }
        }

        Gdiplus::Status s = bm->Save(fn, &encoderClsid, pEncoderParameters);

        // All GDI+ objects must be destroyed before GdiplusShutdown is called
        delete bm;
        delete [] pImageCodecInfo;
        delete pEncoderParameters;
        Gdiplus::GdiplusShutdown(gdiplusToken);
        delete [] p;

        if (s != Gdiplus::Ok) {
            AfxMessageBox(IDS_SCREENSHOT_ERROR, MB_ICONWARNING | MB_OK, 0);
            return;
        }
    }

    path.m_strPath.Replace(_T("\\\\"), _T("\\"));

    SendStatusMessage(m_wndStatusBar.PreparePathStatusMessage(path), 3000);
}

HRESULT GetBasicVideoFrame(IBasicVideo* pBasicVideo, std::vector<BYTE>& dib) {
    // IBasicVideo::GetCurrentImage() gives the original frame

    long size;

    HRESULT hr = pBasicVideo->GetCurrentImage(&size, nullptr);
    if (FAILED(hr)) {
        return hr;
    }
    if (size <= 0) {
        return E_ABORT;
    }

    dib.resize(size);

    hr = pBasicVideo->GetCurrentImage(&size, (long*)dib.data());
    if (FAILED(hr)) {
        dib.clear();
    }

    return hr;
}

HRESULT GetVideoDisplayControlFrame(IMFVideoDisplayControl* pVideoDisplayControl, std::vector<BYTE>& dib) {
    // IMFVideoDisplayControl::GetCurrentImage() gives the displayed frame

    BITMAPINFOHEADER	bih = { sizeof(BITMAPINFOHEADER) };
    BYTE* pDib;
    DWORD				size;
    REFERENCE_TIME		rtImage = 0;

    HRESULT hr = pVideoDisplayControl->GetCurrentImage(&bih, &pDib, &size, &rtImage);
    if (S_OK != hr) {
        return hr;
    }
    if (size == 0) {
        return E_ABORT;
    }

    dib.resize(sizeof(BITMAPINFOHEADER) + size);

    memcpy(dib.data(), &bih, sizeof(BITMAPINFOHEADER));
    memcpy(dib.data() + sizeof(BITMAPINFOHEADER), pDib, size);
    CoTaskMemFree(pDib);

    return hr;
}

HRESULT GetMadVRFrameGrabberFrame(IMadVRFrameGrabber* pMadVRFrameGrabber, std::vector<BYTE>& dib, bool displayed) {
    LPVOID dibImage = nullptr;
    HRESULT hr;

    if (displayed) {
        hr = pMadVRFrameGrabber->GrabFrame(ZOOM_PLAYBACK_SIZE, 0, 0, 0, 0, 0, &dibImage, 0);
    } else {
        hr = pMadVRFrameGrabber->GrabFrame(ZOOM_ENCODED_SIZE, 0, 0, 0, 0, 0, &dibImage, 0);
    }

    if (S_OK != hr) {
        return hr;
    }
    if (!dibImage) {
        return E_ABORT;
    }

    const BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)dibImage;

    dib.resize(sizeof(BITMAPINFOHEADER) + bih->biSizeImage);
    memcpy(dib.data(), dibImage, sizeof(BITMAPINFOHEADER) + bih->biSizeImage);
    LocalFree(dibImage);

    return hr;
}

HRESULT CMainFrame::GetDisplayedImage(std::vector<BYTE>& dib, CString& errmsg) {
    errmsg.Empty();
    HRESULT hr;

	if (m_pCAP) {
		LPVOID dibImage = nullptr;
		hr = m_pCAP->GetDisplayedImage(&dibImage);

		if (S_OK == hr && dibImage) {
			const BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)dibImage;
			dib.resize(sizeof(BITMAPINFOHEADER) + bih->biSizeImage);
			memcpy(dib.data(), dibImage, sizeof(BITMAPINFOHEADER) + bih->biSizeImage);
			LocalFree(dibImage);
		}
	}
	else if (m_pMFVDC) {
        hr = GetVideoDisplayControlFrame(m_pMFVDC, dib);
    } else if (m_pMVRFG) {
        hr = GetMadVRFrameGrabberFrame(m_pMVRFG, dib, true);
    } else {
        hr = E_NOINTERFACE;
    }

    if (FAILED(hr)) {
		errmsg.Format(L"CMainFrame::GetCurrentImage() failed, 0x%08x", hr);
    }

    return hr;
}

HRESULT CMainFrame::GetCurrentFrame(std::vector<BYTE>& dib, CString& errmsg) {
    HRESULT hr = S_OK;
    errmsg.Empty();

    OAFilterState fs = GetMediaState();
    if (m_eMediaLoadState != MLS::LOADED || m_fAudioOnly || (fs != State_Paused && fs != State_Running)) {
        return E_ABORT;
    }

    if (fs == State_Running && !m_pCAP) {
        m_pMC->Pause();
        GetMediaState(); // wait for completion of the pause command
    }

    if (m_pCAP) {
        DWORD size;
        hr = m_pCAP->GetDIB(nullptr, &size);

        if (S_OK == hr) {
            dib.resize(size);
            hr = m_pCAP->GetDIB(dib.data(), &size);
        }

        if (FAILED(hr)) {
            errmsg.Format(L"ISubPicAllocatorPresenter3::GetDIB() failed, 0x%08x", hr);
        }
    } else if (m_pBV) {
        hr = GetBasicVideoFrame(m_pBV, dib);

        if (hr == E_NOINTERFACE && m_pMFVDC) {
            // hmm, EVR is not able to give the original frame, giving the displayed image
            hr = GetDisplayedImage(dib, errmsg);
        } else if (FAILED(hr)) {
            errmsg.Format(L"IBasicVideo::GetCurrentImage() failed, 0x%08x", hr);
        }
    } else {
        hr = E_POINTER;
        errmsg.Format(L"Interface not found!");
    }

    if (fs == State_Running && GetMediaState() != State_Running) {
        m_pMC->Run();
    }

    return hr;
}

HRESULT CMainFrame::GetOriginalFrame(std::vector<BYTE>& dib, CString& errmsg) {
    HRESULT hr = S_OK;
    errmsg.Empty();

    if (m_pMVRFG) {
        hr = GetMadVRFrameGrabberFrame(m_pMVRFG, dib, false);
        if (FAILED(hr)) {
            errmsg.Format(L"IMadVRFrameGrabber::GrabFrame() failed, 0x%08x", hr);
        }
    } else {
        hr = GetCurrentFrame(dib, errmsg);
    }

    return hr;
}

HRESULT CMainFrame::RenderCurrentSubtitles(BYTE* pData) {
    ASSERT(AfxGetAppSettings().bSnapShotSubtitles && !m_pMVRFG && AfxGetAppSettings().fEnableSubtitles && AfxGetAppSettings().IsISRAutoLoadEnabled());
    CheckPointer(pData, E_FAIL);
    HRESULT hr = S_FALSE;

    if (CComQIPtr<ISubPicProvider> pSubPicProvider = m_pCurrentSubInput.pSubStream) {
        const PBITMAPINFOHEADER bih = (PBITMAPINFOHEADER)pData;
        const int width = bih->biWidth;
        const int height = bih->biHeight;

        SubPicDesc spdRender;
		spdRender.type    = MSP_RGB32;
        spdRender.w = width;
        spdRender.h = abs(height);
        spdRender.bpp = 32;
        spdRender.pitch = width * 4;
        spdRender.vidrect = { 0, 0, width, height };
        spdRender.bits = DEBUG_NEW BYTE[spdRender.pitch * spdRender.h];

        REFERENCE_TIME rtNow = 0;
        m_pMS->GetCurrentPosition(&rtNow);

        CComPtr<CMemSubPicAllocator> pSubPicAllocator = DEBUG_NEW CMemSubPicAllocator(spdRender.type, CSize(spdRender.w, spdRender.h));

        CMemSubPic memSubPic(spdRender, pSubPicAllocator);
        memSubPic.ClearDirtyRect(0xFF000000);

        RECT bbox = {};
        hr = pSubPicProvider->Render(spdRender, rtNow, m_pCAP->GetFPS(), bbox);
        if (S_OK == hr) {
            SubPicDesc spdTarget;
			spdTarget.type    = MSP_RGB32;
            spdTarget.w = width;
            spdTarget.h = height;
            spdTarget.bpp = 32;
            spdTarget.pitch = -width * 4;
            spdTarget.vidrect = { 0, 0, width, height };
            spdTarget.bits = (BYTE*)(bih + 1) + (width * 4) * (height - 1);

            hr = memSubPic.AlphaBlt(&spdRender.vidrect, &spdTarget.vidrect, &spdTarget);
        }
    }

    return hr;
}

void CMainFrame::SaveImage(LPCWSTR fn, bool displayed, bool includeSubtitles) {
    std::vector<BYTE> dib;
    CString errmsg;
    HRESULT hr;
    if (displayed) {
        hr = GetDisplayedImage(dib, errmsg);
    } else {
        hr = GetCurrentFrame(dib, errmsg);
        if (includeSubtitles && hr == S_OK) {
            RenderCurrentSubtitles(dib.data());
        }
    }

    if (hr == S_OK) {
        SaveDIB(fn, dib.data(), (long)dib.size());
        m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_OSD_IMAGE_SAVED), 3000);
    } else {
        m_OSD.DisplayMessage(OSD_TOPLEFT, errmsg, 3000);
    }
}

void CMainFrame::SaveThumbnails(LPCTSTR fn)
{
    if (!m_pMC || !m_pMS || GetPlaybackMode() != PM_FILE /*&& GetPlaybackMode() != PM_DVD*/) {
        return;
    }

    REFERENCE_TIME rtPos = GetPos();
    REFERENCE_TIME rtDur = GetDur();

    if (rtDur <= 0) {
        AfxMessageBox(IDS_THUMBNAILS_NO_DURATION, MB_ICONWARNING | MB_OK, 0);
        return;
    }

    OAFilterState filterState = GetMediaState();
    bool bWasStopped = (filterState == State_Stopped);
    if (filterState != State_Paused) {
        OnPlayPause();
        GetMediaState(); // wait for completion of the pause command
    }

    CSize szVideoARCorrected, szVideo, szAR;

    if (m_pCAP) {
        szVideo = m_pCAP->GetVideoSize(false);
        szAR = m_pCAP->GetVideoSize(true);
    } else if (m_pMFVDC) {
        VERIFY(SUCCEEDED(m_pMFVDC->GetNativeVideoSize(&szVideo, &szAR)));
    } else {
        VERIFY(SUCCEEDED(m_pBV->GetVideoSize(&szVideo.cx, &szVideo.cy)));

        CComQIPtr<IBasicVideo2> pBV2 = m_pBV;
        long lARx = 0, lARy = 0;
        if (pBV2 && SUCCEEDED(pBV2->GetPreferredAspectRatio(&lARx, &lARy)) && lARx > 0 && lARy > 0) {
            szAR.SetSize(lARx, lARy);
        }
    }

    if (szVideo.cx <= 0 || szVideo.cy <= 0) {
        AfxMessageBox(IDS_THUMBNAILS_NO_FRAME_SIZE, MB_ICONWARNING | MB_OK, 0);
        return;
    }

    // with the overlay mixer IBasicVideo2 won't tell the new AR when changed dynamically
    DVD_VideoAttributes VATR;
    if (GetPlaybackMode() == PM_DVD && SUCCEEDED(m_pDVDI->GetCurrentVideoAttributes(&VATR))) {
        szAR.SetSize(VATR.ulAspectX, VATR.ulAspectY);
    }

    szVideoARCorrected = (szAR.cx <= 0 || szAR.cy <= 0) ? szVideo : CSize(MulDiv(szVideo.cy, szAR.cx, szAR.cy), szVideo.cy);

    const CAppSettings& s = AfxGetAppSettings();

    int cols = std::max(1, std::min(16, s.iThumbCols));
    int rows = std::max(1, std::min(40, s.iThumbRows));

    const int margin = 5;
    const int infoheight = 70;
    int width = std::max(256, std::min(3840, s.iThumbWidth));
    int height = width * szVideoARCorrected.cy / szVideoARCorrected.cx * rows / cols + infoheight;

    int dibsize = sizeof(BITMAPINFOHEADER) + width * height * 4;

    CAutoVectorPtr<BYTE> dib;
    if (!dib.Allocate(dibsize)) {
        AfxMessageBox(IDS_OUT_OF_MEMORY, MB_ICONWARNING | MB_OK, 0);
        return;
    }

    BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)(BYTE*)dib;
    ZeroMemory(bih, sizeof(BITMAPINFOHEADER));
    bih->biSize = sizeof(BITMAPINFOHEADER);
    bih->biWidth = width;
    bih->biHeight = height;
    bih->biPlanes = 1;
    bih->biBitCount = 32;
    bih->biCompression = BI_RGB;
    bih->biSizeImage = width * height * 4;
    memsetd(bih + 1, 0xffffff, bih->biSizeImage);

    SubPicDesc spd;
    spd.w = width;
    spd.h = height;
    spd.bpp = 32;
    spd.pitch = -width * 4;
    spd.vidrect = CRect(0, 0, width, height);
    spd.bits = (BYTE*)(bih + 1) + (width * 4) * (height - 1);

    // Paint the background
    {
        BYTE* p = (BYTE*)spd.bits;
        for (int y = 0; y < spd.h; y++, p += spd.pitch) {
            for (int x = 0; x < spd.w; x++) {
                ((DWORD*)p)[x] = 0x010101 * (0xe0 + 0x08 * y / spd.h + 0x18 * (spd.w - x) / spd.w);
            }
        }
    }

    CCritSec csSubLock;
    RECT bbox;
    CSize szThumbnail((width - margin * 2) / cols - margin * 2, (height - margin * 2 - infoheight) / rows - margin * 2);
    // Ensure the thumbnails aren't ridiculously small so that the time indication can at least fit
    if (szThumbnail.cx < 60 || szThumbnail.cy < 20) {
        AfxMessageBox(IDS_THUMBNAIL_TOO_SMALL, MB_ICONWARNING | MB_OK, 0);
        return;
    }

    // Draw the thumbnails
    for (int i = 1, pics = cols * rows; i <= pics; i++) {
        REFERENCE_TIME rt = rtDur * i / (pics + 1);
        DVD_HMSF_TIMECODE hmsf = RT2HMS_r(rt);

        DoSeekTo(rt, false);
        UpdateWindow();

        m_nVolumeBeforeFrameStepping = m_wndToolBar.Volume;
        m_pBA->put_Volume(-10000);

        HRESULT hr = m_pFS ? m_pFS->Step(1, nullptr) : E_FAIL;

        if (FAILED(hr)) {
            m_pBA->put_Volume(m_nVolumeBeforeFrameStepping);
            AfxMessageBox(IDS_FRAME_STEP_ERROR_RENDERER, MB_ICONEXCLAMATION | MB_OK, 0);
            return;
        }

        HANDLE hGraphEvent = nullptr;
        m_pME->GetEventHandle((OAEVENT*)&hGraphEvent);

        while (hGraphEvent && WaitForSingleObject(hGraphEvent, INFINITE) == WAIT_OBJECT_0) {
            LONG evCode = 0;
            LONG_PTR evParam1, evParam2;
            while (m_pME && SUCCEEDED(m_pME->GetEvent(&evCode, &evParam1, &evParam2, 0))) {
                m_pME->FreeEventParams(evCode, evParam1, evParam2);
                if (EC_STEP_COMPLETE == evCode) {
                    hGraphEvent = nullptr;
                }
            }
        }

        m_pBA->put_Volume(m_nVolumeBeforeFrameStepping);

        int col = (i - 1) % cols;
        int row = (i - 1) / cols;

        CPoint p(2 * margin + col * (szThumbnail.cx + 2 * margin), infoheight + 2 * margin + row * (szThumbnail.cy + 2 * margin));
        CRect r(p, szThumbnail);

        CRenderedTextSubtitle rts(&csSubLock);
        rts.CreateDefaultStyle(0);
        rts.m_dstScreenSize.SetSize(width, height);
        STSStyle* style = DEBUG_NEW STSStyle();
        style->marginRect.SetRectEmpty();
        rts.AddStyle(_T("thumbs"), style);

        CStringW str;
        str.Format(L"{\\an7\\1c&Hffffff&\\4a&Hb0&\\bord1\\shad4\\be1}{\\p1}m %d %d l %d %d %d %d %d %d{\\p}",
                   r.left, r.top, r.right, r.top, r.right, r.bottom, r.left, r.bottom);
        rts.Add(str, true, MS2RT(0), MS2RT(1), _T("thumbs")); // Thumbnail background
        str.Format(L"{\\an3\\1c&Hffffff&\\3c&H000000&\\alpha&H80&\\fs16\\b1\\bord2\\shad0\\pos(%d,%d)}%02u:%02u:%02u",
                   r.right - 5, r.bottom - 3, hmsf.bHours, hmsf.bMinutes, hmsf.bSeconds);
        rts.Add(str, true, MS2RT(1), MS2RT(2), _T("thumbs")); // Thumbnail time

        rts.Render(spd, 0, 25, bbox); // Draw the thumbnail background

        BYTE* pData = nullptr;
        long size = 0;
        if (!GetDIB(&pData, size)) {
            return;
        }

        BITMAPINFO* bi = (BITMAPINFO*)pData;

        if (bi->bmiHeader.biBitCount != 32) {
            CString strTemp;
            strTemp.Format(IDS_THUMBNAILS_INVALID_FORMAT, bi->bmiHeader.biBitCount);
            AfxMessageBox(strTemp);
            delete [] pData;
            return;
        }

        int sw = bi->bmiHeader.biWidth;
        int sh = abs(bi->bmiHeader.biHeight);
        int sp = sw * 4;
        const BYTE* src = pData + sizeof(bi->bmiHeader);
        if (bi->bmiHeader.biHeight >= 0) {
            src += sp * (sh - 1);
            sp = -sp;
        }

        int dp = spd.pitch;
        BYTE* dst = spd.bits + spd.pitch * r.top + r.left * 4;

        for (DWORD h = szThumbnail.cy, y = 0, yd = (sh << 8) / h; h > 0; y += yd, h--) {
            DWORD yf = y & 0xff;
            DWORD yi = y >> 8;

            DWORD* s0 = (DWORD*)(src + (int)yi * sp);
            DWORD* s1 = (DWORD*)(src + (int)yi * sp + sp);
            DWORD* d = (DWORD*)dst;

            for (DWORD w = szThumbnail.cx, x = 0, xd = (sw << 8) / w; w > 0; x += xd, w--) {
                DWORD xf = x & 0xff;
                DWORD xi = x >> 8;

                DWORD c0 = s0[xi];
                DWORD c1 = s0[xi + 1];
                DWORD c2 = s1[xi];
                DWORD c3 = s1[xi + 1];

                c0 = ((c0 & 0xff00ff) + ((((c1 & 0xff00ff) - (c0 & 0xff00ff)) * xf) >> 8)) & 0xff00ff
                     | ((c0 & 0x00ff00) + ((((c1 & 0x00ff00) - (c0 & 0x00ff00)) * xf) >> 8)) & 0x00ff00;

                c2 = ((c2 & 0xff00ff) + ((((c3 & 0xff00ff) - (c2 & 0xff00ff)) * xf) >> 8)) & 0xff00ff
                     | ((c2 & 0x00ff00) + ((((c3 & 0x00ff00) - (c2 & 0x00ff00)) * xf) >> 8)) & 0x00ff00;

                c0 = ((c0 & 0xff00ff) + ((((c2 & 0xff00ff) - (c0 & 0xff00ff)) * yf) >> 8)) & 0xff00ff
                     | ((c0 & 0x00ff00) + ((((c2 & 0x00ff00) - (c0 & 0x00ff00)) * yf) >> 8)) & 0x00ff00;

                *d++ = c0;
            }

            dst += dp;
        }

        rts.Render(spd, 10000, 25, bbox); // Draw the thumbnail time

        delete [] pData;
    }

    // Draw the file information
    {
        CRenderedTextSubtitle rts(&csSubLock);
        rts.CreateDefaultStyle(0);
        rts.m_dstScreenSize.SetSize(width, height);
        STSStyle* style = DEBUG_NEW STSStyle();
        // Use System UI font.
        CFont tempFont;
        CMPCThemeUtil::getFontByType(tempFont, nullptr, nullptr, CMPCThemeUtil::MessageFont);
        LOGFONT lf;
        if (tempFont.GetLogFont(&lf)) {
            CString fontName(lf.lfFaceName);
            style->fontName = fontName;
        }
        style->marginRect.SetRect(margin * 2, margin * 2, margin * 2, height - infoheight - margin);
        rts.AddStyle(_T("thumbs"), style);

        CStringW str;
        str.Format(L"{\\an9\\fs%d\\b1\\bord0\\shad0\\1c&Hffffff&}%s", infoheight - 10, L"MPC-HC");

        rts.Add(str, true, 0, 1, _T("thumbs"), _T(""), _T(""), CRect(0, 0, 0, 0), -1);

        DVD_HMSF_TIMECODE hmsf = RT2HMS_r(rtDur);

        CString title;

        CPlaylistItem* pli = m_wndPlaylistBar.GetCur();
        if (pli && pli->m_bYoutubeDL && !pli->m_fns.IsEmpty() && pli->m_label && !pli->m_label.IsEmpty()) {
            title = pli->m_label;
        } else {
            title = GetFileName();
        }

        CStringW fs;
        WIN32_FIND_DATA wfd;
        HANDLE hFind = FindFirstFile(m_wndPlaylistBar.GetCurFileName(), &wfd);
        if (hFind != INVALID_HANDLE_VALUE) {
            FindClose(hFind);

            __int64 size = (__int64(wfd.nFileSizeHigh) << 32) | wfd.nFileSizeLow;
            const int MAX_FILE_SIZE_BUFFER = 65;
            WCHAR szFileSize[MAX_FILE_SIZE_BUFFER];
            StrFormatByteSizeW(size, szFileSize, MAX_FILE_SIZE_BUFFER);
            CString szByteSize;
            szByteSize.Format(_T("%I64d"), size);
            fs.Format(IDS_THUMBNAILS_INFO_FILESIZE, szFileSize, FormatNumber(szByteSize).GetString());
        }

        CStringW ar;
        if (szAR.cx > 0 && szAR.cy > 0 && szAR.cx != szVideo.cx && szAR.cy != szVideo.cy) {
            ar.Format(L"(%ld:%ld)", szAR.cx, szAR.cy);
        }

        str.Format(IDS_THUMBNAILS_INFO_HEADER,
                   title.GetString(), fs.GetString(), szVideo.cx, szVideo.cy, ar.GetString(), hmsf.bHours, hmsf.bMinutes, hmsf.bSeconds);
        rts.Add(str, true, 0, 1, _T("thumbs"));

        rts.Render(spd, 0, 25, bbox);
    }

    SaveDIB(fn, (BYTE*)dib, dibsize);

    if (bWasStopped) {
        OnPlayStop();
    } else {
        DoSeekTo(rtPos, false);
    }

    m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_OSD_THUMBS_SAVED), 3000);
}

CString CMainFrame::MakeSnapshotFileName(BOOL thumbnails)
{
    CAppSettings& s = AfxGetAppSettings();
    CString prefix;
    CString fn;

    ASSERT(!thumbnails || GetPlaybackMode() == PM_FILE);

    auto videoFn = GetFileName();
    if (!s.bSnapShotKeepVideoExtension) {
        int nPos = videoFn.ReverseFind('.');
        if (nPos != -1) {
            videoFn = videoFn.Left(nPos);
        }
    }

    if (GetPlaybackMode() == PM_FILE) {
        if (thumbnails) {
            prefix.Format(_T("%s_thumbs"), videoFn.GetString());
        } else {
            if (s.bSaveImagePosition) {
                prefix.Format(_T("%s_snapshot_%s"), videoFn.GetString(), GetVidPos().GetString());
            } else {
                prefix.Format(_T("%s"), videoFn.GetString());
            }
        }
    } else if (GetPlaybackMode() == PM_DVD) {
        if (s.bSaveImagePosition) {
            prefix.Format(_T("dvd_snapshot_%s"), GetVidPos().GetString());
        } else {
            prefix = _T("dvd_snapshot");
        }
    } else if (GetPlaybackMode() == PM_DIGITAL_CAPTURE) {
        prefix.Format(_T("%s_snapshot"), m_pDVBState->sChannelName.GetString());
    } else {
        prefix = _T("snapshot");
    }

    if (!thumbnails && s.bSaveImageCurrentTime) {
        CTime t = CTime::GetCurrentTime();
        fn.Format(_T("%s_[%s]%s"), PathUtils::FilterInvalidCharsFromFileName(prefix).GetString(), t.Format(_T("%Y.%m.%d_%H.%M.%S")).GetString(), s.strSnapshotExt.GetString());
    } else {
        fn.Format(_T("%s%s"), PathUtils::FilterInvalidCharsFromFileName(prefix).GetString(), s.strSnapshotExt.GetString());
    }
    return fn;
}

BOOL CMainFrame::IsRendererCompatibleWithSaveImage()
{
    BOOL result = TRUE;
    const CAppSettings& s = AfxGetAppSettings();

    if (m_fShockwaveGraph) {
        AfxMessageBox(IDS_SCREENSHOT_ERROR_SHOCKWAVE, MB_ICONEXCLAMATION | MB_OK, 0);
        result = FALSE;
    } else if (s.iDSVideoRendererType == VIDRNDT_DS_OVERLAYMIXER) {
        AfxMessageBox(IDS_SCREENSHOT_ERROR_OVERLAY, MB_ICONEXCLAMATION | MB_OK, 0);
        result = FALSE;
    }

    return result;
}

CString CMainFrame::GetVidPos() const
{
    CString posstr = _T("");
    if ((GetPlaybackMode() == PM_FILE) || (GetPlaybackMode() == PM_DVD)) {
        __int64 start, stop, pos;
        m_wndSeekBar.GetRange(start, stop);
        pos = m_wndSeekBar.GetPos();

        DVD_HMSF_TIMECODE tcNow = RT2HMSF(pos);
        DVD_HMSF_TIMECODE tcDur = RT2HMSF(stop);

        if (tcDur.bHours > 0 || tcNow.bHours > 0) {
            posstr.Format(_T("%02u.%02u.%02u.%03u"), tcNow.bHours, tcNow.bMinutes, tcNow.bSeconds, (pos / 10000) % 1000);
        } else {
            posstr.Format(_T("%02u.%02u.%03u"), tcNow.bMinutes, tcNow.bSeconds, (pos / 10000) % 1000);
        }
    }

    return posstr;
}

void CMainFrame::OnFileSaveImage()
{
    CAppSettings& s = AfxGetAppSettings();

    /* Check if a compatible renderer is being used */
    if (!IsRendererCompatibleWithSaveImage()) {
        return;
    }

    CPath psrc(s.strSnapshotPath);
    psrc.Combine(s.strSnapshotPath.GetString(), MakeSnapshotFileName(FALSE));

    bool subtitleOptionSupported = !m_pMVRFG && s.fEnableSubtitles && s.IsISRAutoLoadEnabled();

    CSaveImageDialog fd(s.nJpegQuality, nullptr, (LPCTSTR)psrc,
                        _T("BMP - Windows Bitmap (*.bmp)|*.bmp|JPG - JPEG Image (*.jpg)|*.jpg|PNG - Portable Network Graphics (*.png)|*.png||"), GetModalParent(), subtitleOptionSupported);

    if (s.strSnapshotExt == _T(".bmp")) {
        fd.m_pOFN->nFilterIndex = 1;
    } else if (s.strSnapshotExt == _T(".jpg")) {
        fd.m_pOFN->nFilterIndex = 2;
    } else if (s.strSnapshotExt == _T(".png")) {
        fd.m_pOFN->nFilterIndex = 3;
    }

    if (fd.DoModal() != IDOK) {
        return;
    }

    if (fd.m_pOFN->nFilterIndex == 1) {
        s.strSnapshotExt = _T(".bmp");
    } else if (fd.m_pOFN->nFilterIndex == 2) {
        s.strSnapshotExt = _T(".jpg");
        s.nJpegQuality = fd.m_nJpegQuality;
    } else {
        fd.m_pOFN->nFilterIndex = 3;
        s.strSnapshotExt = _T(".png");
    }

    CPath pdst(fd.GetPathName());
    CString ext(pdst.GetExtension().MakeLower());
    if (ext != s.strSnapshotExt) {
        if (ext == _T(".bmp") || ext == _T(".jpg") || ext == _T(".png")) {
            ext = s.strSnapshotExt;
        } else {
            ext += s.strSnapshotExt;
        }
        pdst.RenameExtension(ext);
    }
    CString path = (LPCTSTR)pdst;
    pdst.RemoveFileSpec();
    s.strSnapshotPath = (LPCTSTR)pdst;

    bool includeSubtitles = subtitleOptionSupported && s.bSnapShotSubtitles;

    SaveImage(path, false, includeSubtitles);
}

void CMainFrame::OnFileSaveImageAuto()
{
    const CAppSettings& s = AfxGetAppSettings();

    // If path doesn't exist, Save Image instead
    if (!PathUtils::IsDir(s.strSnapshotPath)) {
        AfxMessageBox(IDS_SCREENSHOT_ERROR, MB_ICONWARNING | MB_OK, 0);
        OnFileSaveImage();
        return;
    }

    /* Check if a compatible renderer is being used */
    if (!IsRendererCompatibleWithSaveImage()) {
        return;
    }

    bool includeSubtitles = s.bSnapShotSubtitles && !m_pMVRFG && s.fEnableSubtitles && s.IsISRAutoLoadEnabled();

    CString fn;
    fn.Format(_T("%s\\%s"), s.strSnapshotPath.GetString(), MakeSnapshotFileName(FALSE).GetString());
    SaveImage(fn.GetString(), false, includeSubtitles);
}

void CMainFrame::OnUpdateFileSaveImage(CCmdUI* pCmdUI)
{
    OAFilterState fs = GetMediaState();
    pCmdUI->Enable(GetLoadState() == MLS::LOADED && !m_fAudioOnly && (fs == State_Paused || fs == State_Running));
}

void CMainFrame::OnFileSaveThumbnails()
{
    CAppSettings& s = AfxGetAppSettings();

    /* Check if a compatible renderer is being used */
    if (!IsRendererCompatibleWithSaveImage()) {
        return;
    }

    CPath psrc(s.strSnapshotPath);
    psrc.Combine(s.strSnapshotPath, MakeSnapshotFileName(TRUE));

    CSaveThumbnailsDialog fd(s.nJpegQuality, s.iThumbRows, s.iThumbCols, s.iThumbWidth, nullptr, (LPCTSTR)psrc,
                             _T("BMP - Windows Bitmap (*.bmp)|*.bmp|JPG - JPEG Image (*.jpg)|*.jpg|PNG - Portable Network Graphics (*.png)|*.png||"), GetModalParent());

    if (s.strSnapshotExt == _T(".bmp")) {
        fd.m_pOFN->nFilterIndex = 1;
    } else if (s.strSnapshotExt == _T(".jpg")) {
        fd.m_pOFN->nFilterIndex = 2;
    } else if (s.strSnapshotExt == _T(".png")) {
        fd.m_pOFN->nFilterIndex = 3;
    }

    if (fd.DoModal() != IDOK) {
        return;
    }

    if (fd.m_pOFN->nFilterIndex == 1) {
        s.strSnapshotExt = _T(".bmp");
    } else if (fd.m_pOFN->nFilterIndex == 2) {
        s.strSnapshotExt = _T(".jpg");
        s.nJpegQuality = fd.m_nJpegQuality;
    } else {
        fd.m_pOFN->nFilterIndex = 3;
        s.strSnapshotExt = _T(".png");
    }

    s.iThumbRows = fd.m_rows;
    s.iThumbCols = fd.m_cols;
    s.iThumbWidth = fd.m_width;

    CPath pdst(fd.GetPathName());
    CString ext(pdst.GetExtension().MakeLower());
    if (ext != s.strSnapshotExt) {
        if (ext == _T(".bmp") || ext == _T(".jpg") || ext == _T(".png")) {
            ext = s.strSnapshotExt;
        } else {
            ext += s.strSnapshotExt;
        }
        pdst.RenameExtension(ext);
    }
    CString path = (LPCTSTR)pdst;
    pdst.RemoveFileSpec();
    s.strSnapshotPath = (LPCTSTR)pdst;

    SaveThumbnails(path);
}

void CMainFrame::OnUpdateFileSaveThumbnails(CCmdUI* pCmdUI)
{
    OAFilterState fs = GetMediaState();
    UNREFERENCED_PARAMETER(fs);
    pCmdUI->Enable(GetLoadState() == MLS::LOADED && !m_fAudioOnly && (GetPlaybackMode() == PM_FILE /*|| GetPlaybackMode() == PM_DVD*/));
}

void CMainFrame::OnFileSubtitlesLoad()
{
    if (!m_pCAP) {
        AfxMessageBox(IDS_CANNOT_LOAD_SUB, MB_ICONINFORMATION | MB_OK, 0);
        return;
    }

    DWORD dwFlags = OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_ENABLESIZING | OFN_NOCHANGEDIR;
    if (!AfxGetAppSettings().fKeepHistory) {
        dwFlags |= OFN_DONTADDTORECENT;
    }
    CString filters;
    filters.Format(_T("%s|*.srt;*.sub;*.ssa;*.ass;*.smi;*.psb;*.txt;*.idx;*.usf;*.xss;*.rt;*.sup;*.vtt|%s"),
                   ResStr(IDS_SUBTITLE_FILES_FILTER).GetString(), ResStr(IDS_ALL_FILES_FILTER).GetString());

    CFileDialog fd(TRUE, nullptr, nullptr, dwFlags, filters, GetModalParent());

    OPENFILENAME& ofn = fd.GetOFN();
    // Provide a buffer big enough to hold 16 paths (which should be more than enough)
    const int nBufferSize = 16 * (MAX_PATH + 1) + 1;
    CString filenames;
    ofn.lpstrFile = filenames.GetBuffer(nBufferSize);
    ofn.nMaxFile = nBufferSize;
    // Set the current file directory as default folder
    CPath defaultDir(m_wndPlaylistBar.GetCurFileName());
    defaultDir.RemoveFileSpec();
    if (!defaultDir.m_strPath.IsEmpty()) {
        ofn.lpstrInitialDir = defaultDir;
    }

    if (fd.DoModal() == IDOK) {
        bool bFirstFile = true;
        POSITION pos = fd.GetStartPosition();
        while (pos) {
            SubtitleInput subInput;
            if (LoadSubtitle(fd.GetNextPathName(pos), &subInput) && bFirstFile) {
                bFirstFile = false;
                // Use the subtitles file that was just added
                AfxGetAppSettings().fEnableSubtitles = true;
                SetSubtitle(subInput);
            }
        }
    }
}

void CMainFrame::OnUpdateFileSubtitlesLoad(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(GetLoadState() == MLS::LOADED && !IsPlaybackCaptureMode() && !m_fAudioOnly && m_pCAP);
}

void CMainFrame::OnFileSubtitlesSave()
{
    int i = 0;
    SubtitleInput* pSubInput = GetSubtitleInput(i, true);

    if (pSubInput) {
        CLSID clsid;
        if (FAILED(pSubInput->pSubStream->GetClassID(&clsid))) {
            return;
        }

        // exclude the extension, it will be auto-completed
        CString suggestedFileName(PathUtils::FileName(GetFileName()));

        // Try to detect the directory of the current playlist item, and prepend it if it's valid
        CPath path(m_wndPlaylistBar.GetCurFileName());
        if (path.RemoveFileSpec() && path.IsDirectory()) {
            path.AddBackslash();
            suggestedFileName = CString(path) + suggestedFileName;
        }

        if (clsid == __uuidof(CVobSubFile)) {
            CVobSubFile* pVSF = (CVobSubFile*)(ISubStream*)pSubInput->pSubStream;

            // remember to set lpszDefExt to the first extension in the filter so that the save dialog autocompletes the extension
            // and tracks attempts to overwrite in a graceful manner
            CSaveSubtitlesFileDialog fd(m_pCAP->GetSubtitleDelay(), _T("idx"), suggestedFileName,
                                        _T("VobSub (*.idx, *.sub)|*.idx;*.sub||"), GetModalParent());

            if (fd.DoModal() == IDOK) {
                CAutoLock cAutoLock(&m_csSubLock);
                pVSF->Save(fd.GetPathName(), fd.GetDelay());
            }
        } else if (clsid == __uuidof(CRenderedTextSubtitle)) {
            CRenderedTextSubtitle* pRTS = (CRenderedTextSubtitle*)(ISubStream*)pSubInput->pSubStream;

            const std::vector<Subtitle::SubType> types = {
                Subtitle::SRT,
                Subtitle::SUB,
                Subtitle::SMI,
                Subtitle::PSB,
                Subtitle::SSA,
                Subtitle::ASS
            };

            CString filter;
            filter += _T("SubRip (*.srt)|*.srt|");
            filter += _T("MicroDVD (*.sub)|*.sub|");
            filter += _T("SAMI (*.smi)|*.smi|");
            filter += _T("PowerDivX (*.psb)|*.psb|");
            filter += _T("SubStation Alpha (*.ssa)|*.ssa|");
            filter += _T("Advanced SubStation Alpha (*.ass)|*.ass|");
            filter += _T("|");

            CAppSettings& s = AfxGetAppSettings();

            if (s.bAddLangCodeWhenSaveSubtitles && pRTS->m_lcid && pRTS->m_lcid != LCID(-1)) {
                CString str;
                GetLocaleString(pRTS->m_lcid, LOCALE_SISO639LANGNAME, str);
                suggestedFileName += _T('.') + str;

                if (pRTS->m_eHearingImpaired == Subtitle::HI_YES) {
                    suggestedFileName += _T(".hi");
                }
            }

            // same thing as in the case of CVobSubFile above for lpszDefExt
            CSaveSubtitlesFileDialog fd(pRTS->m_encoding, m_pCAP->GetSubtitleDelay(), s.bSubSaveExternalStyleFile,
                                        _T("srt"), suggestedFileName, filter, types, GetModalParent());

            if (fd.DoModal() == IDOK) {
                CAutoLock cAutoLock(&m_csSubLock);
                s.bSubSaveExternalStyleFile = fd.GetSaveExternalStyleFile();
                pRTS->SaveAs(fd.GetPathName(), types[fd.m_ofn.nFilterIndex - 1], m_pCAP->GetFPS(), fd.GetDelay(), fd.GetEncoding(), fd.GetSaveExternalStyleFile());
            }
        } else {
            AfxMessageBox(_T("This operation is not supported.\r\nThe selected subtitles cannot be saved."), MB_ICONEXCLAMATION | MB_OK);
        }
    }
}

void CMainFrame::OnUpdateFileSubtitlesSave(CCmdUI* pCmdUI)
{
    bool bEnable = false;

    if (!m_pCurrentSubInput.pSourceFilter) {
        if (auto pRTS = dynamic_cast<CRenderedTextSubtitle*>((ISubStream*)m_pCurrentSubInput.pSubStream)) {
            bEnable = !pRTS->IsEmpty();
        } else if (dynamic_cast<CVobSubFile*>((ISubStream*)m_pCurrentSubInput.pSubStream)) {
            bEnable = true;
        }
    }

    pCmdUI->Enable(bEnable);
}

void CMainFrame::OnFileSubtitlesUpload()
{
    m_wndSubtitlesUploadDialog.ShowWindow(SW_SHOW);
}

void CMainFrame::OnUpdateFileSubtitlesUpload(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    pCmdUI->Enable(!m_pSubStreams.IsEmpty() && s.fEnableSubtitles);
}

void CMainFrame::OnFileSubtitlesDownload()
{
    m_wndSubtitlesDownloadDialog.ShowWindow(SW_SHOW);
}

void CMainFrame::OnUpdateFileSubtitlesDownload(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(GetLoadState() == MLS::LOADED && !IsPlaybackCaptureMode() && m_pCAP && !m_fAudioOnly);
}

void CMainFrame::OnFileProperties()
{
    CPPageFileInfoSheet fileinfo(m_pDVBState ? m_pDVBState->sChannelName : m_wndPlaylistBar.GetCurFileName(), this, GetModalParent());
    fileinfo.DoModal();
}

void CMainFrame::OnUpdateFileProperties(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(GetLoadState() == MLS::LOADED && GetPlaybackMode() != PM_ANALOG_CAPTURE);
}

void CMainFrame::OnFileOpenLocation() {
    CString filePath = m_wndPlaylistBar.GetCurFileName();
    ExploreToFile(filePath);
}

void CMainFrame::OnFileCloseMedia()
{
    CloseMedia();
}

void CMainFrame::OnUpdateViewTearingTest(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    bool supported = (s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS
                      || s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM
                      || s.iDSVideoRendererType == VIDRNDT_DS_SYNC);

    pCmdUI->Enable(supported && GetLoadState() == MLS::LOADED && !m_fAudioOnly);
    pCmdUI->SetCheck(supported && AfxGetMyApp()->m_Renderers.m_bTearingTest);
}

void CMainFrame::OnViewTearingTest()
{
    AfxGetMyApp()->m_Renderers.m_bTearingTest = !AfxGetMyApp()->m_Renderers.m_bTearingTest;
}

void CMainFrame::OnUpdateViewDisplayRendererStats(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    bool supported = (s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS
                      || s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM
                      || s.iDSVideoRendererType == VIDRNDT_DS_SYNC
                      || s.iDSVideoRendererType == VIDRNDT_DS_MPCVR);

    pCmdUI->Enable(supported && GetLoadState() == MLS::LOADED && !m_fAudioOnly);
    pCmdUI->SetCheck(supported && AfxGetMyApp()->m_Renderers.m_iDisplayStats);
}

void CMainFrame::OnViewResetRendererStats()
{
    AfxGetMyApp()->m_Renderers.m_bResetStats = true; // Reset by "consumer"
}

void CMainFrame::OnViewDisplayRendererStats()
{
    const CAppSettings& s = AfxGetAppSettings();
    bool supported = (s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS
                      || s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM
                      || s.iDSVideoRendererType == VIDRNDT_DS_SYNC
                      || s.iDSVideoRendererType == VIDRNDT_DS_MPCVR);

    if (supported) {
        if (m_pCAP3) {
            m_pCAP3->ToggleStats();
            return;
        }

        if (!AfxGetMyApp()->m_Renderers.m_iDisplayStats) {
            AfxGetMyApp()->m_Renderers.m_bResetStats = true; // to reset statistics on first call ...
        }

        ++AfxGetMyApp()->m_Renderers.m_iDisplayStats;
        if (AfxGetMyApp()->m_Renderers.m_iDisplayStats > 3) {
            AfxGetMyApp()->m_Renderers.m_iDisplayStats = 0;
        }
        RepaintVideo();
    }
}

void CMainFrame::OnUpdateViewVSync(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS
                       || s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D);

    pCmdUI->Enable(supported);
    pCmdUI->SetCheck(!supported || r.m_AdvRendSets.bVMR9VSync);
}

void CMainFrame::OnUpdateViewVSyncOffset(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS
                       || s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D
                      && r.m_AdvRendSets.bVMR9AlterativeVSync);

    pCmdUI->Enable(supported);
    CString Temp;
    Temp.Format(L"%d", r.m_AdvRendSets.iVMR9VSyncOffset);
    pCmdUI->SetText(Temp);
    CMPCThemeMenu::updateItem(pCmdUI);
}

void CMainFrame::OnUpdateViewVSyncAccurate(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS
                       || s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D);

    pCmdUI->Enable(supported);
    pCmdUI->SetCheck(r.m_AdvRendSets.bVMR9VSyncAccurate);
}

void CMainFrame::OnUpdateViewSynchronizeVideo(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_SYNC) && GetPlaybackMode() == PM_NONE);

    pCmdUI->Enable(supported);
    pCmdUI->SetCheck(r.m_AdvRendSets.bSynchronizeVideo);
}

void CMainFrame::OnUpdateViewSynchronizeDisplay(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_SYNC) && GetPlaybackMode() == PM_NONE);

    pCmdUI->Enable(supported);
    pCmdUI->SetCheck(r.m_AdvRendSets.bSynchronizeDisplay);
}

void CMainFrame::OnUpdateViewSynchronizeNearest(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    bool supported = (s.iDSVideoRendererType == VIDRNDT_DS_SYNC);

    pCmdUI->Enable(supported);
    pCmdUI->SetCheck(r.m_AdvRendSets.bSynchronizeNearest);
}

void CMainFrame::OnUpdateViewColorManagementEnable(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    const CRenderersData& rd = AfxGetMyApp()->m_Renderers;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS
                       || s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D)
                     && rd.m_bFP16Support;

    pCmdUI->Enable(supported);
    pCmdUI->SetCheck(r.m_AdvRendSets.bVMR9ColorManagementEnable);
}

void CMainFrame::OnUpdateViewColorManagementInput(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    const CRenderersData& rd = AfxGetMyApp()->m_Renderers;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS
                       || s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D)
                     && rd.m_bFP16Support
                     && r.m_AdvRendSets.bVMR9ColorManagementEnable;

    pCmdUI->Enable(supported);

    switch (pCmdUI->m_nID) {
        case ID_VIEW_CM_INPUT_AUTO:
            pCmdUI->SetCheck(r.m_AdvRendSets.iVMR9ColorManagementInput == VIDEO_SYSTEM_UNKNOWN);
            break;

        case ID_VIEW_CM_INPUT_HDTV:
            pCmdUI->SetCheck(r.m_AdvRendSets.iVMR9ColorManagementInput == VIDEO_SYSTEM_HDTV);
            break;

        case ID_VIEW_CM_INPUT_SDTV_NTSC:
            pCmdUI->SetCheck(r.m_AdvRendSets.iVMR9ColorManagementInput == VIDEO_SYSTEM_SDTV_NTSC);
            break;

        case ID_VIEW_CM_INPUT_SDTV_PAL:
            pCmdUI->SetCheck(r.m_AdvRendSets.iVMR9ColorManagementInput == VIDEO_SYSTEM_SDTV_PAL);
            break;
    }
}

void CMainFrame::OnUpdateViewColorManagementAmbientLight(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    const CRenderersData& rd = AfxGetMyApp()->m_Renderers;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS
                       || s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D)
                     && rd.m_bFP16Support &&
                     r.m_AdvRendSets.bVMR9ColorManagementEnable;

    pCmdUI->Enable(supported);

    switch (pCmdUI->m_nID) {
        case ID_VIEW_CM_AMBIENTLIGHT_BRIGHT:
            pCmdUI->SetCheck(r.m_AdvRendSets.iVMR9ColorManagementAmbientLight == AMBIENT_LIGHT_BRIGHT);
            break;
        case ID_VIEW_CM_AMBIENTLIGHT_DIM:
            pCmdUI->SetCheck(r.m_AdvRendSets.iVMR9ColorManagementAmbientLight == AMBIENT_LIGHT_DIM);
            break;
        case ID_VIEW_CM_AMBIENTLIGHT_DARK:
            pCmdUI->SetCheck(r.m_AdvRendSets.iVMR9ColorManagementAmbientLight == AMBIENT_LIGHT_DARK);
            break;
    }
}

void CMainFrame::OnUpdateViewColorManagementIntent(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    const CRenderersData& rd = AfxGetMyApp()->m_Renderers;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS
                       || s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D)
                     && rd.m_bFP16Support
                     && r.m_AdvRendSets.bVMR9ColorManagementEnable;

    pCmdUI->Enable(supported);

    switch (pCmdUI->m_nID) {
        case ID_VIEW_CM_INTENT_PERCEPTUAL:
            pCmdUI->SetCheck(r.m_AdvRendSets.iVMR9ColorManagementIntent == COLOR_RENDERING_INTENT_PERCEPTUAL);
            break;
        case ID_VIEW_CM_INTENT_RELATIVECOLORIMETRIC:
            pCmdUI->SetCheck(r.m_AdvRendSets.iVMR9ColorManagementIntent == COLOR_RENDERING_INTENT_RELATIVE_COLORIMETRIC);
            break;
        case ID_VIEW_CM_INTENT_SATURATION:
            pCmdUI->SetCheck(r.m_AdvRendSets.iVMR9ColorManagementIntent == COLOR_RENDERING_INTENT_SATURATION);
            break;
        case ID_VIEW_CM_INTENT_ABSOLUTECOLORIMETRIC:
            pCmdUI->SetCheck(r.m_AdvRendSets.iVMR9ColorManagementIntent == COLOR_RENDERING_INTENT_ABSOLUTE_COLORIMETRIC);
            break;
    }
}

void CMainFrame::OnUpdateViewEVROutputRange(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM
                       || s.iDSVideoRendererType == VIDRNDT_DS_SYNC)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D);

    pCmdUI->Enable(supported);

    if (pCmdUI->m_nID == ID_VIEW_EVROUTPUTRANGE_0_255) {
        pCmdUI->SetCheck(r.m_AdvRendSets.iEVROutputRange == 0);
    } else if (pCmdUI->m_nID == ID_VIEW_EVROUTPUTRANGE_16_235) {
        pCmdUI->SetCheck(r.m_AdvRendSets.iEVROutputRange == 1);
    }
}

void CMainFrame::OnUpdateViewFlushGPU(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS
                       || s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D);

    pCmdUI->Enable(supported);

    if (pCmdUI->m_nID == ID_VIEW_FLUSHGPU_BEFOREVSYNC) {
        pCmdUI->SetCheck(r.m_AdvRendSets.bVMRFlushGPUBeforeVSync);
    } else if (pCmdUI->m_nID == ID_VIEW_FLUSHGPU_AFTERPRESENT) {
        pCmdUI->SetCheck(r.m_AdvRendSets.bVMRFlushGPUAfterPresent);
    } else if (pCmdUI->m_nID == ID_VIEW_FLUSHGPU_WAIT) {
        pCmdUI->SetCheck(r.m_AdvRendSets.bVMRFlushGPUWait);
    }

}

void CMainFrame::OnUpdateViewD3DFullscreen(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS
                       || s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM
                       || s.iDSVideoRendererType == VIDRNDT_DS_SYNC)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D);

    pCmdUI->Enable(supported);
    pCmdUI->SetCheck(s.fD3DFullscreen);
}

void CMainFrame::OnUpdateViewDisableDesktopComposition(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS
                       || s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM
                       || s.iDSVideoRendererType == VIDRNDT_DS_SYNC)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D
                      && !IsWindows8OrGreater());

    pCmdUI->Enable(supported);
    pCmdUI->SetCheck(supported && r.m_AdvRendSets.bVMRDisableDesktopComposition);
}

void CMainFrame::OnUpdateViewAlternativeVSync(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS
                       || s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D);

    pCmdUI->Enable(supported);
    pCmdUI->SetCheck(r.m_AdvRendSets.bVMR9AlterativeVSync);
}

void CMainFrame::OnUpdateViewFullscreenGUISupport(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS
                       || s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D);

    pCmdUI->Enable(supported);
    pCmdUI->SetCheck(r.m_AdvRendSets.bVMR9FullscreenGUISupport);
}

void CMainFrame::OnUpdateViewHighColorResolution(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    const CRenderersData& rd = AfxGetMyApp()->m_Renderers;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM
                       || s.iDSVideoRendererType == VIDRNDT_DS_SYNC)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D)
                     && rd.m_b10bitSupport;

    pCmdUI->Enable(supported);
    pCmdUI->SetCheck(r.m_AdvRendSets.bEVRHighColorResolution);
}

void CMainFrame::OnUpdateViewForceInputHighColorResolution(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    const CRenderersData& rd = AfxGetMyApp()->m_Renderers;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D)
                     && rd.m_b10bitSupport;

    pCmdUI->Enable(supported);
    pCmdUI->SetCheck(r.m_AdvRendSets.bEVRForceInputHighColorResolution);
}

void CMainFrame::OnUpdateViewFullFloatingPointProcessing(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    const CRenderersData& rd = AfxGetMyApp()->m_Renderers;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS
                       || s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D)
                     && rd.m_bFP32Support;

    pCmdUI->Enable(supported);
    pCmdUI->SetCheck(r.m_AdvRendSets.bVMR9FullFloatingPointProcessing);
}

void CMainFrame::OnUpdateViewHalfFloatingPointProcessing(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    const CRenderersData& rd = AfxGetMyApp()->m_Renderers;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS
                       || s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D)
                     && rd.m_bFP16Support;

    pCmdUI->Enable(supported);
    pCmdUI->SetCheck(r.m_AdvRendSets.bVMR9HalfFloatingPointProcessing);
}

void CMainFrame::OnUpdateViewEnableFrameTimeCorrection(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    bool supported = ((s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM)
                      && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D);

    pCmdUI->Enable(supported);
    pCmdUI->SetCheck(r.m_AdvRendSets.bEVREnableFrameTimeCorrection);
}

void CMainFrame::OnUpdateViewVSyncOffsetIncrease(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    bool supported = s.iDSVideoRendererType == VIDRNDT_DS_SYNC
                     || (((s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS
                           || s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM)
                          && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D)
                         && r.m_AdvRendSets.bVMR9AlterativeVSync);

    pCmdUI->Enable(supported);
}

void CMainFrame::OnUpdateViewVSyncOffsetDecrease(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    bool supported = s.iDSVideoRendererType == VIDRNDT_DS_SYNC
                     || (((s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS
                           || s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM)
                          && r.iAPSurfaceUsage == VIDRNDT_AP_TEXTURE3D)
                         && r.m_AdvRendSets.bVMR9AlterativeVSync);

    pCmdUI->Enable(supported);
}

void CMainFrame::OnViewVSync()
{
    CRenderersSettings& r = AfxGetAppSettings().m_RenderersSettings;
    r.m_AdvRendSets.bVMR9VSync = !r.m_AdvRendSets.bVMR9VSync;
    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(r.m_AdvRendSets.bVMR9VSync
                                              ? IDS_OSD_RS_VSYNC_ON : IDS_OSD_RS_VSYNC_OFF));
}

void CMainFrame::OnViewVSyncAccurate()
{
    CRenderersSettings& r = AfxGetAppSettings().m_RenderersSettings;
    r.m_AdvRendSets.bVMR9VSyncAccurate = !r.m_AdvRendSets.bVMR9VSyncAccurate;
    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(r.m_AdvRendSets.bVMR9VSyncAccurate
                                              ? IDS_OSD_RS_ACCURATE_VSYNC_ON : IDS_OSD_RS_ACCURATE_VSYNC_OFF));
}

void CMainFrame::OnViewSynchronizeVideo()
{
    CRenderersSettings& r = AfxGetAppSettings().m_RenderersSettings;
    r.m_AdvRendSets.bSynchronizeVideo = !r.m_AdvRendSets.bSynchronizeVideo;
    if (r.m_AdvRendSets.bSynchronizeVideo) {
        r.m_AdvRendSets.bSynchronizeDisplay = false;
        r.m_AdvRendSets.bSynchronizeNearest = false;
        r.m_AdvRendSets.bVMR9VSync = false;
        r.m_AdvRendSets.bVMR9VSyncAccurate = false;
        r.m_AdvRendSets.bVMR9AlterativeVSync = false;
    }

    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(r.m_AdvRendSets.bSynchronizeVideo
                                              ? IDS_OSD_RS_SYNC_TO_DISPLAY_ON : IDS_OSD_RS_SYNC_TO_DISPLAY_ON));
}

void CMainFrame::OnViewSynchronizeDisplay()
{
    CRenderersSettings& r = AfxGetAppSettings().m_RenderersSettings;
    r.m_AdvRendSets.bSynchronizeDisplay = !r.m_AdvRendSets.bSynchronizeDisplay;
    if (r.m_AdvRendSets.bSynchronizeDisplay) {
        r.m_AdvRendSets.bSynchronizeVideo = false;
        r.m_AdvRendSets.bSynchronizeNearest = false;
        r.m_AdvRendSets.bVMR9VSync = false;
        r.m_AdvRendSets.bVMR9VSyncAccurate = false;
        r.m_AdvRendSets.bVMR9AlterativeVSync = false;
    }

    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(r.m_AdvRendSets.bSynchronizeDisplay
                                              ? IDS_OSD_RS_SYNC_TO_VIDEO_ON : IDS_OSD_RS_SYNC_TO_VIDEO_ON));
}

void CMainFrame::OnViewSynchronizeNearest()
{
    CRenderersSettings& r = AfxGetAppSettings().m_RenderersSettings;
    r.m_AdvRendSets.bSynchronizeNearest = !r.m_AdvRendSets.bSynchronizeNearest;
    if (r.m_AdvRendSets.bSynchronizeNearest) {
        r.m_AdvRendSets.bSynchronizeVideo = false;
        r.m_AdvRendSets.bSynchronizeDisplay = false;
        r.m_AdvRendSets.bVMR9VSync = false;
        r.m_AdvRendSets.bVMR9VSyncAccurate = false;
        r.m_AdvRendSets.bVMR9AlterativeVSync = false;
    }

    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(r.m_AdvRendSets.bSynchronizeNearest
                                              ? IDS_OSD_RS_PRESENT_NEAREST_ON : IDS_OSD_RS_PRESENT_NEAREST_OFF));
}

void CMainFrame::OnViewColorManagementEnable()
{
    CRenderersSettings& r = AfxGetAppSettings().m_RenderersSettings;
    r.m_AdvRendSets.bVMR9ColorManagementEnable = !r.m_AdvRendSets.bVMR9ColorManagementEnable;
    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(r.m_AdvRendSets.bVMR9ColorManagementEnable
                                              ? IDS_OSD_RS_COLOR_MANAGEMENT_ON : IDS_OSD_RS_COLOR_MANAGEMENT_OFF));
}

void CMainFrame::OnViewColorManagementInputAuto()
{
    CRenderersSettings& r = AfxGetAppSettings().m_RenderersSettings;
    r.m_AdvRendSets.iVMR9ColorManagementInput = VIDEO_SYSTEM_UNKNOWN;
    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_OSD_RS_INPUT_TYPE_AUTO));
}

void CMainFrame::OnViewColorManagementInputHDTV()
{
    CRenderersSettings& r = AfxGetAppSettings().m_RenderersSettings;
    r.m_AdvRendSets.iVMR9ColorManagementInput = VIDEO_SYSTEM_HDTV;
    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_OSD_RS_INPUT_TYPE_HDTV));
}

void CMainFrame::OnViewColorManagementInputSDTV_NTSC()
{
    CRenderersSettings& r = AfxGetAppSettings().m_RenderersSettings;
    r.m_AdvRendSets.iVMR9ColorManagementInput = VIDEO_SYSTEM_SDTV_NTSC;
    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_OSD_RS_INPUT_TYPE_SD_NTSC));
}

void CMainFrame::OnViewColorManagementInputSDTV_PAL()
{
    CRenderersSettings& r = AfxGetAppSettings().m_RenderersSettings;
    r.m_AdvRendSets.iVMR9ColorManagementInput = VIDEO_SYSTEM_SDTV_PAL;
    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_OSD_RS_INPUT_TYPE_SD_PAL));
}

void CMainFrame::OnViewColorManagementAmbientLightBright()
{
    CRenderersSettings& r = AfxGetAppSettings().m_RenderersSettings;
    r.m_AdvRendSets.iVMR9ColorManagementAmbientLight = AMBIENT_LIGHT_BRIGHT;
    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_OSD_RS_AMBIENT_LIGHT_BRIGHT));
}

void CMainFrame::OnViewColorManagementAmbientLightDim()
{
    CRenderersSettings& r = AfxGetAppSettings().m_RenderersSettings;
    r.m_AdvRendSets.iVMR9ColorManagementAmbientLight = AMBIENT_LIGHT_DIM;
    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_OSD_RS_AMBIENT_LIGHT_DIM));
}

void CMainFrame::OnViewColorManagementAmbientLightDark()
{
    CRenderersSettings& r = AfxGetAppSettings().m_RenderersSettings;
    r.m_AdvRendSets.iVMR9ColorManagementAmbientLight = AMBIENT_LIGHT_DARK;
    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_OSD_RS_AMBIENT_LIGHT_DARK));
}

void CMainFrame::OnViewColorManagementIntentPerceptual()
{
    CRenderersSettings& r = AfxGetAppSettings().m_RenderersSettings;
    r.m_AdvRendSets.iVMR9ColorManagementIntent = COLOR_RENDERING_INTENT_PERCEPTUAL;
    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_OSD_RS_REND_INTENT_PERCEPT));
}

void CMainFrame::OnViewColorManagementIntentRelativeColorimetric()
{
    CRenderersSettings& r = AfxGetAppSettings().m_RenderersSettings;
    r.m_AdvRendSets.iVMR9ColorManagementIntent = COLOR_RENDERING_INTENT_RELATIVE_COLORIMETRIC;
    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_OSD_RS_REND_INTENT_RELATIVE));
}

void CMainFrame::OnViewColorManagementIntentSaturation()
{
    CRenderersSettings& r = AfxGetAppSettings().m_RenderersSettings;
    r.m_AdvRendSets.iVMR9ColorManagementIntent = COLOR_RENDERING_INTENT_SATURATION;
    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_OSD_RS_REND_INTENT_SATUR));
}

void CMainFrame::OnViewColorManagementIntentAbsoluteColorimetric()
{
    CRenderersSettings& r = AfxGetAppSettings().m_RenderersSettings;
    r.m_AdvRendSets.iVMR9ColorManagementIntent = COLOR_RENDERING_INTENT_ABSOLUTE_COLORIMETRIC;
    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_OSD_RS_REND_INTENT_ABSOLUTE));
}

void CMainFrame::OnViewEVROutputRange_0_255()
{
    CRenderersSettings& r = AfxGetAppSettings().m_RenderersSettings;
    r.m_AdvRendSets.iEVROutputRange = 0;
    r.UpdateData(true);
    CString strOSD;
    strOSD.Format(IDS_OSD_RS_OUTPUT_RANGE, _T("0 - 255"));
    m_OSD.DisplayMessage(OSD_TOPRIGHT, strOSD);
}

void CMainFrame::OnViewEVROutputRange_16_235()
{
    CRenderersSettings& r = AfxGetAppSettings().m_RenderersSettings;
    r.m_AdvRendSets.iEVROutputRange = 1;
    r.UpdateData(true);
    CString strOSD;
    strOSD.Format(IDS_OSD_RS_OUTPUT_RANGE, _T("16 - 235"));
    m_OSD.DisplayMessage(OSD_TOPRIGHT, strOSD);
}

void CMainFrame::OnViewFlushGPUBeforeVSync()
{
    CRenderersSettings& r = AfxGetAppSettings().m_RenderersSettings;
    r.m_AdvRendSets.bVMRFlushGPUBeforeVSync = !r.m_AdvRendSets.bVMRFlushGPUBeforeVSync;
    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(r.m_AdvRendSets.bVMRFlushGPUBeforeVSync
                                              ? IDS_OSD_RS_FLUSH_BEF_VSYNC_ON : IDS_OSD_RS_FLUSH_BEF_VSYNC_OFF));
}

void CMainFrame::OnViewFlushGPUAfterVSync()
{
    CRenderersSettings& r = AfxGetAppSettings().m_RenderersSettings;
    r.m_AdvRendSets.bVMRFlushGPUAfterPresent = !r.m_AdvRendSets.bVMRFlushGPUAfterPresent;
    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(r.m_AdvRendSets.bVMRFlushGPUAfterPresent
                                              ? IDS_OSD_RS_FLUSH_AFT_PRES_ON : IDS_OSD_RS_FLUSH_AFT_PRES_OFF));
}

void CMainFrame::OnViewFlushGPUWait()
{
    CRenderersSettings& r = AfxGetAppSettings().m_RenderersSettings;
    r.m_AdvRendSets.bVMRFlushGPUWait = !r.m_AdvRendSets.bVMRFlushGPUWait;
    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(r.m_AdvRendSets.bVMRFlushGPUWait
                                              ? IDS_OSD_RS_WAIT_ON : IDS_OSD_RS_WAIT_OFF));
}

void CMainFrame::OnViewD3DFullScreen()
{
    CAppSettings& r = AfxGetAppSettings();
    r.fD3DFullscreen = !r.fD3DFullscreen;
    r.SaveSettings();
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(r.fD3DFullscreen
                                              ? IDS_OSD_RS_D3D_FULLSCREEN_ON : IDS_OSD_RS_D3D_FULLSCREEN_OFF));
}

void CMainFrame::OnViewDisableDesktopComposition()
{
    CRenderersSettings& r = AfxGetAppSettings().m_RenderersSettings;
    r.m_AdvRendSets.bVMRDisableDesktopComposition = !r.m_AdvRendSets.bVMRDisableDesktopComposition;
    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(r.m_AdvRendSets.bVMRDisableDesktopComposition
                                              ? IDS_OSD_RS_NO_DESKTOP_COMP_ON : IDS_OSD_RS_NO_DESKTOP_COMP_OFF));
}

void CMainFrame::OnViewAlternativeVSync()
{
    CRenderersSettings& r = AfxGetAppSettings().m_RenderersSettings;
    r.m_AdvRendSets.bVMR9AlterativeVSync = !r.m_AdvRendSets.bVMR9AlterativeVSync;
    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(r.m_AdvRendSets.bVMR9AlterativeVSync
                                              ? IDS_OSD_RS_ALT_VSYNC_ON : IDS_OSD_RS_ALT_VSYNC_OFF));
}

void CMainFrame::OnViewResetDefault()
{
    CRenderersSettings& r = AfxGetAppSettings().m_RenderersSettings;
    r.m_AdvRendSets.SetDefault();
    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_OSD_RS_RESET_DEFAULT));
}

void CMainFrame::OnViewResetOptimal()
{
    CRenderersSettings& r = AfxGetAppSettings().m_RenderersSettings;
    r.m_AdvRendSets.SetOptimal();
    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_OSD_RS_RESET_OPTIMAL));
}

void CMainFrame::OnViewFullscreenGUISupport()
{
    CRenderersSettings& r = AfxGetAppSettings().m_RenderersSettings;
    r.m_AdvRendSets.bVMR9FullscreenGUISupport = !r.m_AdvRendSets.bVMR9FullscreenGUISupport;
    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(r.m_AdvRendSets.bVMR9FullscreenGUISupport
                                              ? IDS_OSD_RS_D3D_FS_GUI_SUPP_ON : IDS_OSD_RS_D3D_FS_GUI_SUPP_OFF));
}

void CMainFrame::OnViewHighColorResolution()
{
    CRenderersSettings& r = AfxGetAppSettings().m_RenderersSettings;
    r.m_AdvRendSets.bEVRHighColorResolution = !r.m_AdvRendSets.bEVRHighColorResolution;
    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(r.m_AdvRendSets.bEVRHighColorResolution
                                              ? IDS_OSD_RS_10BIT_RBG_OUT_ON : IDS_OSD_RS_10BIT_RBG_OUT_OFF));
}

void CMainFrame::OnViewForceInputHighColorResolution()
{
    CRenderersSettings& r = AfxGetAppSettings().m_RenderersSettings;
    r.m_AdvRendSets.bEVRForceInputHighColorResolution = !r.m_AdvRendSets.bEVRForceInputHighColorResolution;
    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(r.m_AdvRendSets.bEVRForceInputHighColorResolution
                                              ? IDS_OSD_RS_10BIT_RBG_IN_ON : IDS_OSD_RS_10BIT_RBG_IN_OFF));
}

void CMainFrame::OnViewFullFloatingPointProcessing()
{
    CRenderersSettings& r = AfxGetAppSettings().m_RenderersSettings;
    if (!r.m_AdvRendSets.bVMR9FullFloatingPointProcessing) {
        if (AfxMessageBox(_T("WARNING: Full Floating Point processing can sometimes cause problems. With some videos it can cause the player to freeze, crash, or display corrupted video. This happens mostly with Intel GPUs.\n\nAre you really sure that you want to enable this setting?"), MB_YESNO) == IDNO) {
            return;
        }
    }
    r.m_AdvRendSets.bVMR9FullFloatingPointProcessing = !r.m_AdvRendSets.bVMR9FullFloatingPointProcessing;
    if (r.m_AdvRendSets.bVMR9FullFloatingPointProcessing) {
        r.m_AdvRendSets.bVMR9HalfFloatingPointProcessing = false;
    }
    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(r.m_AdvRendSets.bVMR9FullFloatingPointProcessing
                                              ? IDS_OSD_RS_FULL_FP_PROCESS_ON : IDS_OSD_RS_FULL_FP_PROCESS_OFF));
}

void CMainFrame::OnViewHalfFloatingPointProcessing()
{
    CRenderersSettings& r = AfxGetAppSettings().m_RenderersSettings;
    r.m_AdvRendSets.bVMR9HalfFloatingPointProcessing = !r.m_AdvRendSets.bVMR9HalfFloatingPointProcessing;
    if (r.m_AdvRendSets.bVMR9HalfFloatingPointProcessing) {
        r.m_AdvRendSets.bVMR9FullFloatingPointProcessing = false;
    }
    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(r.m_AdvRendSets.bVMR9HalfFloatingPointProcessing
                                              ? IDS_OSD_RS_HALF_FP_PROCESS_ON : IDS_OSD_RS_HALF_FP_PROCESS_OFF));
}

void CMainFrame::OnViewEnableFrameTimeCorrection()
{
    CRenderersSettings& r = AfxGetAppSettings().m_RenderersSettings;
    r.m_AdvRendSets.bEVREnableFrameTimeCorrection = !r.m_AdvRendSets.bEVREnableFrameTimeCorrection;
    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(r.m_AdvRendSets.bEVREnableFrameTimeCorrection
                                              ? IDS_OSD_RS_FT_CORRECTION_ON : IDS_OSD_RS_FT_CORRECTION_OFF));
}

void CMainFrame::OnViewVSyncOffsetIncrease()
{
    CAppSettings& s = AfxGetAppSettings();
    CRenderersSettings& r = s.m_RenderersSettings;
    CString strOSD;
    if (s.iDSVideoRendererType == VIDRNDT_DS_SYNC) {
        r.m_AdvRendSets.fTargetSyncOffset = r.m_AdvRendSets.fTargetSyncOffset - 0.5; // Yeah, it should be a "-"
        strOSD.Format(IDS_OSD_RS_TARGET_VSYNC_OFFSET, r.m_AdvRendSets.fTargetSyncOffset);
    } else {
        ++r.m_AdvRendSets.iVMR9VSyncOffset;
        strOSD.Format(IDS_OSD_RS_VSYNC_OFFSET, r.m_AdvRendSets.iVMR9VSyncOffset);
    }
    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, strOSD);
}

void CMainFrame::OnViewVSyncOffsetDecrease()
{
    CAppSettings& s = AfxGetAppSettings();
    CRenderersSettings& r = s.m_RenderersSettings;
    CString strOSD;
    if (s.iDSVideoRendererType == VIDRNDT_DS_SYNC) {
        r.m_AdvRendSets.fTargetSyncOffset = r.m_AdvRendSets.fTargetSyncOffset + 0.5;
        strOSD.Format(IDS_OSD_RS_TARGET_VSYNC_OFFSET, r.m_AdvRendSets.fTargetSyncOffset);
    } else {
        --r.m_AdvRendSets.iVMR9VSyncOffset;
        strOSD.Format(IDS_OSD_RS_VSYNC_OFFSET, r.m_AdvRendSets.iVMR9VSyncOffset);
    }
    r.UpdateData(true);
    m_OSD.DisplayMessage(OSD_TOPRIGHT, strOSD);
}

void CMainFrame::OnUpdateViewOSDDisplayTime(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    pCmdUI->Enable(s.fShowOSD && GetLoadState() != MLS::CLOSED);
    pCmdUI->SetCheck(m_bOSDDisplayTime);
}

void CMainFrame::OnViewOSDDisplayTime()
{
    m_bOSDDisplayTime = !m_bOSDDisplayTime;

    if (!m_bOSDDisplayTime) {
        m_OSD.ClearMessage();
    }

    OnTimer(TIMER_STREAMPOSPOLLER2);
}

void CMainFrame::OnUpdateViewOSDShowFileName(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    pCmdUI->Enable(s.fShowOSD && GetLoadState() != MLS::CLOSED);
}

void CMainFrame::OnViewOSDShowFileName()
{
    CString strOSD;
    switch (GetPlaybackMode()) {
        case PM_FILE:
            strOSD = GetFileName();
            break;
        case PM_DVD:
            strOSD = _T("DVD");
            if (m_pDVDI) {
                CString path;
                ULONG len = 0;
                if (SUCCEEDED(m_pDVDI->GetDVDDirectory(path.GetBuffer(MAX_PATH), MAX_PATH, &len)) && len) {
                    path.ReleaseBuffer();
                    if (path.Find(_T("\\VIDEO_TS")) == 2) {
                        strOSD.AppendFormat(_T(" - %s"), GetDriveLabel(CPath(path)).GetString());
                    } else {
                        strOSD.AppendFormat(_T(" - %s"), path.GetString());
                    }
                }
            }
            break;
        case PM_ANALOG_CAPTURE:
            strOSD = GetCaptureTitle();
            break;
        case PM_DIGITAL_CAPTURE:
            UpdateCurrentChannelInfo(true, false);
            break;
        default: // Shouldn't happen
            ASSERT(FALSE);
            return;
    }
    if (!strOSD.IsEmpty()) {
        m_OSD.DisplayMessage(OSD_TOPLEFT, strOSD);
    }
}

void CMainFrame::OnUpdateShaderToggle1(CCmdUI* pCmdUI)
{
	if (AfxGetAppSettings().m_Shaders.GetCurrentPreset().GetPreResize().empty()) {
		pCmdUI->Enable(FALSE);
		pCmdUI->SetCheck (0);
	} else {
		pCmdUI->Enable(TRUE);
		pCmdUI->SetCheck (m_bToggleShader);
	}
}

void CMainFrame::OnUpdateShaderToggle2(CCmdUI* pCmdUI)
{
	CAppSettings& s = AfxGetAppSettings();

	if (s.m_Shaders.GetCurrentPreset().GetPostResize().empty()) {
		pCmdUI->Enable(FALSE);
		pCmdUI->SetCheck(0);
	} else {
		pCmdUI->Enable(TRUE);
		pCmdUI->SetCheck(m_bToggleShaderScreenSpace);
	}
}

void CMainFrame::OnShaderToggle1()
{
	m_bToggleShader = !m_bToggleShader;
    SetShaders(m_bToggleShader, m_bToggleShaderScreenSpace);
    if (m_bToggleShader) {
		m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_PRESIZE_SHADERS_ENABLED));
	} else {
		m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_PRESIZE_SHADERS_DISABLED));
	}

	if (m_pCAP) {
		RepaintVideo();
	}
}

void CMainFrame::OnShaderToggle2()
{
	m_bToggleShaderScreenSpace = !m_bToggleShaderScreenSpace;
    SetShaders(m_bToggleShader, m_bToggleShaderScreenSpace);
    if (m_bToggleShaderScreenSpace) {
        m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_POSTSIZE_SHADERS_ENABLED));
	} else {
        m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_POSTSIZE_SHADERS_DISABLED));
	}

	if (m_pCAP) {
		RepaintVideo();
	}
}

void CMainFrame::OnD3DFullscreenToggle()
{
    CAppSettings& s = AfxGetAppSettings();
    CString strMsg;

    s.fD3DFullscreen = !s.fD3DFullscreen;
    strMsg.LoadString(s.fD3DFullscreen ? IDS_OSD_RS_D3D_FULLSCREEN_ON : IDS_OSD_RS_D3D_FULLSCREEN_OFF);
    if (GetLoadState() == MLS::CLOSED) {
        m_closingmsg = strMsg;
    } else {
        m_OSD.DisplayMessage(OSD_TOPRIGHT, strMsg);
    }
}

void CMainFrame::OnFileCloseAndRestore()
{
    SendMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);
    RestoreDefaultWindowRect();
}

void CMainFrame::OnUpdateFileClose(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(GetLoadState() == MLS::LOADED || GetLoadState() == MLS::LOADING);
}

// view

void CMainFrame::SetCaptionState(MpcCaptionState eState)
{
    auto& s = AfxGetAppSettings();

    if (eState == s.eCaptionMenuMode) {
        return;
    }

    const auto eOldState = s.eCaptionMenuMode;
    s.eCaptionMenuMode = eState;

    if (m_fFullScreen) {
        return;
    }

    DWORD dwRemove = 0, dwAdd = 0;
    DWORD dwMenuFlags = GetMenuBarVisibility();

    CRect windowRect;

    const bool bZoomed = !!IsZoomed();

    if (!bZoomed) {
        GetWindowRect(&windowRect);
        CRect decorationsRect;
        VERIFY(AdjustWindowRectEx(decorationsRect, GetWindowStyle(m_hWnd), dwMenuFlags == AFX_MBV_KEEPVISIBLE, GetWindowExStyle(m_hWnd)));
        windowRect.bottom -= decorationsRect.bottom;
        windowRect.right  -= decorationsRect.right;
        windowRect.top    -= decorationsRect.top;
        windowRect.left   -= decorationsRect.left;
    }

    const int base = MpcCaptionState::MODE_COUNT;
    for (int i = eOldState; i != eState; i = (i + 1) % base) {
        switch (static_cast<MpcCaptionState>(i)) {
            case MpcCaptionState::MODE_BORDERLESS:
                dwMenuFlags = AFX_MBV_KEEPVISIBLE;
                dwAdd |= (WS_CAPTION | WS_THICKFRAME);
                dwRemove &= ~(WS_CAPTION | WS_THICKFRAME);
                break;
            case MpcCaptionState::MODE_SHOWCAPTIONMENU:
                dwMenuFlags = AFX_MBV_DISPLAYONFOCUS;
                break;
            case MpcCaptionState::MODE_HIDEMENU:
                dwMenuFlags = AFX_MBV_DISPLAYONFOCUS;
                dwAdd &= ~WS_CAPTION;
                dwRemove |= WS_CAPTION;
                break;
            case MpcCaptionState::MODE_FRAMEONLY:
                dwMenuFlags = AFX_MBV_DISPLAYONFOCUS;
                dwAdd &= ~WS_THICKFRAME;
                dwRemove |= WS_THICKFRAME;
                break;
            default:
                ASSERT(FALSE);
        }
    }

    UINT uFlags = SWP_NOZORDER;
    if (dwRemove != dwAdd) {
        uFlags |= SWP_FRAMECHANGED;
        VERIFY(SetWindowLong(m_hWnd, GWL_STYLE, (GetWindowLong(m_hWnd, GWL_STYLE) | dwAdd) & ~dwRemove));
    }

    SetMenuBarVisibility(dwMenuFlags);
    if (bZoomed) {
        CMonitors::GetNearestMonitor(this).GetWorkAreaRect(windowRect);
    } else {
        VERIFY(AdjustWindowRectEx(windowRect, GetWindowStyle(m_hWnd), dwMenuFlags == AFX_MBV_KEEPVISIBLE, GetWindowExStyle(m_hWnd)));
    }

    VERIFY(SetWindowPos(nullptr, windowRect.left, windowRect.top, windowRect.Width(), windowRect.Height(), uFlags));
}

void CMainFrame::OnViewCaptionmenu()
{
    const auto& s = AfxGetAppSettings();
    SetCaptionState(static_cast<MpcCaptionState>((s.eCaptionMenuMode + 1) % MpcCaptionState::MODE_COUNT));
}

void CMainFrame::OnUpdateViewCaptionmenu(CCmdUI* pCmdUI)
{
    const auto& s = AfxGetAppSettings();
    const UINT next[] = { IDS_VIEW_HIDEMENU, IDS_VIEW_FRAMEONLY, IDS_VIEW_BORDERLESS, IDS_VIEW_CAPTIONMENU };
    pCmdUI->SetText(ResStr(next[s.eCaptionMenuMode % MpcCaptionState::MODE_COUNT]));
    CMPCThemeMenu::updateItem(pCmdUI);
}

void CMainFrame::OnViewControlBar(UINT nID)
{
    nID -= ID_VIEW_SEEKER;
    m_controls.ToggleControl(static_cast<CMainFrameControls::Toolbar>(nID));
}

void CMainFrame::OnUpdateViewControlBar(CCmdUI* pCmdUI)
{
    const UINT nID = pCmdUI->m_nID - ID_VIEW_SEEKER;
    pCmdUI->SetCheck(m_controls.ControlChecked(static_cast<CMainFrameControls::Toolbar>(nID)));
    if (pCmdUI->m_nID == ID_VIEW_SEEKER) {
        pCmdUI->Enable(!IsPlaybackCaptureMode());
    }
}

void CMainFrame::OnViewSubresync()
{
    m_controls.ToggleControl(CMainFrameControls::Panel::SUBRESYNC);
    AdjustStreamPosPoller(true);
}

void CMainFrame::OnUpdateViewSubresync(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(m_controls.ControlChecked(CMainFrameControls::Panel::SUBRESYNC));
    pCmdUI->Enable(m_pCAP && !m_pSubStreams.IsEmpty() && !IsPlaybackCaptureMode());
}

void CMainFrame::OnViewPlaylist()
{
    m_controls.ToggleControl(CMainFrameControls::Panel::PLAYLIST);
    m_wndPlaylistBar.SetHiddenDueToFullscreen(false);
}

void CMainFrame::OnUpdateViewPlaylist(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(m_controls.ControlChecked(CMainFrameControls::Panel::PLAYLIST));
}

void CMainFrame::OnViewEditListEditor()
{
    CAppSettings& s = AfxGetAppSettings();

    if (s.fEnableEDLEditor || (AfxMessageBox(IDS_MB_SHOW_EDL_EDITOR, MB_ICONQUESTION | MB_YESNO, 0) == IDYES)) {
        s.fEnableEDLEditor = true;
        m_controls.ToggleControl(CMainFrameControls::Panel::EDL);
    }
}

void CMainFrame::OnEDLIn()
{
    if (AfxGetAppSettings().fEnableEDLEditor && (GetLoadState() == MLS::LOADED) && (GetPlaybackMode() == PM_FILE)) {
        REFERENCE_TIME rt;

        m_pMS->GetCurrentPosition(&rt);
        m_wndEditListEditor.SetIn(rt);
    }
}

void CMainFrame::OnUpdateEDLIn(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_controls.ControlChecked(CMainFrameControls::Panel::EDL));
}

void CMainFrame::OnEDLOut()
{
    if (AfxGetAppSettings().fEnableEDLEditor && (GetLoadState() == MLS::LOADED) && (GetPlaybackMode() == PM_FILE)) {
        REFERENCE_TIME rt;

        m_pMS->GetCurrentPosition(&rt);
        m_wndEditListEditor.SetOut(rt);
    }
}

void CMainFrame::OnUpdateEDLOut(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_controls.ControlChecked(CMainFrameControls::Panel::EDL));
}

void CMainFrame::OnEDLNewClip()
{
    if (AfxGetAppSettings().fEnableEDLEditor && (GetLoadState() == MLS::LOADED) && (GetPlaybackMode() == PM_FILE)) {
        REFERENCE_TIME rt;

        m_pMS->GetCurrentPosition(&rt);
        m_wndEditListEditor.NewClip(rt);
    }
}

void CMainFrame::OnUpdateEDLNewClip(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_controls.ControlChecked(CMainFrameControls::Panel::EDL));
}

void CMainFrame::OnEDLSave()
{
    if (AfxGetAppSettings().fEnableEDLEditor && (GetLoadState() == MLS::LOADED) && (GetPlaybackMode() == PM_FILE)) {
        m_wndEditListEditor.Save();
    }
}

void CMainFrame::OnUpdateEDLSave(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_controls.ControlChecked(CMainFrameControls::Panel::EDL));
}

// Navigation menu
void CMainFrame::OnViewNavigation()
{
    const bool bHiding = m_controls.ControlChecked(CMainFrameControls::Panel::NAVIGATION);
    m_controls.ToggleControl(CMainFrameControls::Panel::NAVIGATION);
    if (!bHiding) {
        m_wndNavigationBar.m_navdlg.UpdateElementList();
    }
    AfxGetAppSettings().fHideNavigation = bHiding;
}

void CMainFrame::OnUpdateViewNavigation(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(m_controls.ControlChecked(CMainFrameControls::Panel::NAVIGATION));
    pCmdUI->Enable(GetLoadState() == MLS::LOADED && GetPlaybackMode() == PM_DIGITAL_CAPTURE);
}

void CMainFrame::OnViewCapture()
{
    m_controls.ToggleControl(CMainFrameControls::Panel::CAPTURE);
}

void CMainFrame::OnUpdateViewCapture(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(m_controls.ControlChecked(CMainFrameControls::Panel::CAPTURE));
    pCmdUI->Enable(GetLoadState() == MLS::LOADED && GetPlaybackMode() == PM_ANALOG_CAPTURE);
}

void CMainFrame::OnViewDebugShaders()
{
    auto& dlg = m_pDebugShaders;
    if (dlg && !dlg->m_hWnd) {
        // something has destroyed the dialog and we didn't know about it
        dlg = nullptr;
    }
    if (!dlg) {
        // dialog doesn't exist - create and show it
        dlg = std::make_unique<CDebugShadersDlg>();
        dlg->ShowWindow(SW_SHOW);
    } else if (dlg->IsWindowVisible()) {
        if (dlg->IsIconic()) {
            // dialog is visible but iconic - restore it
            VERIFY(dlg->ShowWindow(SW_RESTORE));
        } else {
            // dialog is visible and not iconic - destroy it
            VERIFY(dlg->DestroyWindow());
            ASSERT(!dlg->m_hWnd);
            dlg = nullptr;
        }
    } else {
        // dialog is not visible - show it
        VERIFY(!dlg->ShowWindow(SW_SHOW));
    }
}

void CMainFrame::OnUpdateViewDebugShaders(CCmdUI* pCmdUI)
{
    const auto& dlg = m_pDebugShaders;
    pCmdUI->SetCheck(dlg && dlg->m_hWnd && dlg->IsWindowVisible());
}

void CMainFrame::OnUpdateViewMPCTheme(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    pCmdUI->SetCheck(s.bMPCTheme);
}

void CMainFrame::OnViewMPCTheme()
{
    CAppSettings& s = AfxGetAppSettings();
    s.bMPCTheme = !s.bMPCTheme;
}

void CMainFrame::OnViewMinimal()
{
    SetCaptionState(MODE_BORDERLESS);
    m_controls.SetToolbarsSelection(CS_NONE, true);
}

void CMainFrame::OnUpdateViewMinimal(CCmdUI* pCmdUI)
{
}

void CMainFrame::OnViewCompact()
{
    SetCaptionState(MODE_FRAMEONLY);
    m_controls.SetToolbarsSelection(CS_SEEKBAR, true);
}

void CMainFrame::OnUpdateViewCompact(CCmdUI* pCmdUI)
{
}

void CMainFrame::OnViewNormal()
{
    SetCaptionState(MODE_SHOWCAPTIONMENU);
    m_controls.SetToolbarsSelection(CS_SEEKBAR | CS_TOOLBAR | CS_STATUSBAR, true);
}

void CMainFrame::OnUpdateViewNormal(CCmdUI* pCmdUI)
{
}

void CMainFrame::OnViewFullscreen()
{
    const CAppSettings& s = AfxGetAppSettings();

    if (IsD3DFullScreenMode() || (s.IsD3DFullscreen() && !m_fFullScreen && !m_fAudioOnly)) {
        ToggleD3DFullscreen(true);
    } else {
        ToggleFullscreen(true, true);
    }
}

void CMainFrame::OnViewFullscreenSecondary()
{
    const CAppSettings& s = AfxGetAppSettings();

    if (IsD3DFullScreenMode() || (s.IsD3DFullscreen() && !m_fFullScreen && !m_fAudioOnly)) {
        ToggleD3DFullscreen(false);
    } else {
        ToggleFullscreen(true, false);
    }
}

void CMainFrame::OnUpdateViewFullscreen(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(GetLoadState() == MLS::LOADED || m_fFullScreen);
    pCmdUI->SetCheck(m_fFullScreen);
}

void CMainFrame::OnViewZoom(UINT nID)
{
    double scale = (nID == ID_VIEW_ZOOM_25) ? 0.25 : (nID == ID_VIEW_ZOOM_50) ? 0.5 : (nID == ID_VIEW_ZOOM_200) ? 2.0 : 1.0;

    ZoomVideoWindow(scale);

    CString strODSMessage;
    strODSMessage.Format(IDS_OSD_ZOOM, scale * 100);
    m_OSD.DisplayMessage(OSD_TOPLEFT, strODSMessage, 3000);
}

void CMainFrame::OnUpdateViewZoom(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(GetLoadState() == MLS::LOADED && !m_fAudioOnly);
}

void CMainFrame::OnViewZoomAutoFit()
{
    ZoomVideoWindow(ZOOM_AUTOFIT);
    m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_OSD_ZOOM_AUTO), 3000);
}

void CMainFrame::OnViewZoomAutoFitLarger()
{
    ZoomVideoWindow(ZOOM_AUTOFIT_LARGER);
    m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_OSD_ZOOM_AUTO_LARGER), 3000);
}

void CMainFrame::OnViewDefaultVideoFrame(UINT nID)
{
    AfxGetAppSettings().iDefaultVideoSize = nID - ID_VIEW_VF_HALF;
    m_ZoomX = m_ZoomY = 1;
    m_PosX = m_PosY = 0.5;
    MoveVideoWindow();
}

void CMainFrame::OnUpdateViewDefaultVideoFrame(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();

    pCmdUI->Enable(GetLoadState() == MLS::LOADED && !m_fAudioOnly && s.iDSVideoRendererType != VIDRNDT_DS_EVR);

    int dvs = pCmdUI->m_nID - ID_VIEW_VF_HALF;
    if (s.iDefaultVideoSize == dvs && pCmdUI->m_pMenu) {
        pCmdUI->m_pMenu->CheckMenuRadioItem(ID_VIEW_VF_HALF, ID_VIEW_VF_ZOOM2, pCmdUI->m_nID, MF_BYCOMMAND);
    }
}

void CMainFrame::OnViewSwitchVideoFrame()
{
    CAppSettings& s = AfxGetAppSettings();

    int vs = s.iDefaultVideoSize;
    if (vs <= DVS_DOUBLE || vs == DVS_FROMOUTSIDE) {
        vs = DVS_STRETCH;
    } else if (vs == DVS_FROMINSIDE) {
        vs = DVS_ZOOM1;
    } else if (vs == DVS_ZOOM2) {
        vs = DVS_FROMOUTSIDE;
    } else {
        vs++;
    }
    switch (vs) { // TODO: Read messages from resource file
        case DVS_STRETCH:
            m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_STRETCH_TO_WINDOW));
            break;
        case DVS_FROMINSIDE:
            m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_TOUCH_WINDOW_FROM_INSIDE));
            break;
        case DVS_ZOOM1:
            m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_ZOOM1));
            break;
        case DVS_ZOOM2:
            m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_ZOOM2));
            break;
        case DVS_FROMOUTSIDE:
            m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_TOUCH_WINDOW_FROM_OUTSIDE));
            break;
    }
    s.iDefaultVideoSize = vs;
    m_ZoomX = m_ZoomY = 1;
    m_PosX = m_PosY = 0.5;
    MoveVideoWindow();
}

void CMainFrame::OnViewCompMonDeskARDiff()
{
    CAppSettings& s = AfxGetAppSettings();

    s.fCompMonDeskARDiff = !s.fCompMonDeskARDiff;
    OnVideoSizeChanged();
}

void CMainFrame::OnUpdateViewCompMonDeskARDiff(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();

    pCmdUI->Enable(GetLoadState() == MLS::LOADED
                   && !m_fAudioOnly
                   && s.iDSVideoRendererType != VIDRNDT_DS_EVR
                   // This doesn't work with madVR due to the fact that it positions video itself.
                   // And it has exactly the same option built in.
                   && s.iDSVideoRendererType != VIDRNDT_DS_MADVR);
    pCmdUI->SetCheck(s.fCompMonDeskARDiff);
}

void CMainFrame::OnViewPanNScan(UINT nID)
{
    if (GetLoadState() != MLS::LOADED) {
        return;
    }

    int x = 0, y = 0;
    int dx = 0, dy = 0;

    switch (nID) {
        case ID_VIEW_RESET:
            m_ZoomX = m_ZoomY = 1.0;
            m_PosX = m_PosY = 0.5;
            m_AngleX = m_AngleY = m_AngleZ = 0;
            PerformFlipRotate();
            break;
        case ID_VIEW_INCSIZE:
            x = y = 1;
            break;
        case ID_VIEW_DECSIZE:
            x = y = -1;
            break;
        case ID_VIEW_INCWIDTH:
            x = 1;
            break;
        case ID_VIEW_DECWIDTH:
            x = -1;
            break;
        case ID_VIEW_INCHEIGHT:
            y = 1;
            break;
        case ID_VIEW_DECHEIGHT:
            y = -1;
            break;
        case ID_PANSCAN_CENTER:
            m_PosX = m_PosY = 0.5;
            break;
        case ID_PANSCAN_MOVELEFT:
            dx = -1;
            break;
        case ID_PANSCAN_MOVERIGHT:
            dx = 1;
            break;
        case ID_PANSCAN_MOVEUP:
            dy = -1;
            break;
        case ID_PANSCAN_MOVEDOWN:
            dy = 1;
            break;
        case ID_PANSCAN_MOVEUPLEFT:
            dx = dy = -1;
            break;
        case ID_PANSCAN_MOVEUPRIGHT:
            dx = 1;
            dy = -1;
            break;
        case ID_PANSCAN_MOVEDOWNLEFT:
            dx = -1;
            dy = 1;
            break;
        case ID_PANSCAN_MOVEDOWNRIGHT:
            dx = dy = 1;
            break;
        default:
            break;
    }

    if (x > 0 && m_ZoomX < 5.0) {
        m_ZoomX = std::min(m_ZoomX * 1.02, 5.0);
    } else if (x < 0 && m_ZoomX > 0.2) {
        m_ZoomX = std::max(m_ZoomX / 1.02, 0.2);
    }

    if (y > 0 && m_ZoomY < 5.0) {
        m_ZoomY = std::min(m_ZoomY * 1.02, 5.0);
    } else if (y < 0 && m_ZoomY > 0.2) {
        m_ZoomY = std::max(m_ZoomY / 1.02, 0.2);
    }

    if (dx < 0 && m_PosX > -0.5) {
        m_PosX = std::max(m_PosX - 0.005 * m_ZoomX, -0.5);
    } else if (dx > 0 && m_PosX < 1.5) {
        m_PosX = std::min(m_PosX + 0.005 * m_ZoomX, 1.5);
    }

    if (dy < 0 && m_PosY > -0.5) {
        m_PosY = std::max(m_PosY - 0.005 * m_ZoomY, -0.5);
    } else if (dy > 0 && m_PosY < 1.5) {
        m_PosY = std::min(m_PosY + 0.005 * m_ZoomY, 1.5);
    }

    MoveVideoWindow(true);
}

void CMainFrame::OnUpdateViewPanNScan(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(GetLoadState() == MLS::LOADED && !m_fAudioOnly && AfxGetAppSettings().iDSVideoRendererType != VIDRNDT_DS_EVR);
}

void CMainFrame::OnViewPanNScanPresets(UINT nID)
{
    if (GetLoadState() != MLS::LOADED) {
        return;
    }

    CAppSettings& s = AfxGetAppSettings();

    nID -= ID_PANNSCAN_PRESETS_START;

    if ((INT_PTR)nID == s.m_pnspresets.GetCount()) {
        CPnSPresetsDlg dlg;
        dlg.m_pnspresets.Copy(s.m_pnspresets);
        if (dlg.DoModal() == IDOK) {
            s.m_pnspresets.Copy(dlg.m_pnspresets);
            s.SaveSettings();
        }
        return;
    }

    m_PosX = 0.5;
    m_PosY = 0.5;
    m_ZoomX = 1.0;
    m_ZoomY = 1.0;

    CString str = s.m_pnspresets[nID];

    int i = 0, j = 0;
    for (CString token = str.Tokenize(_T(","), i); !token.IsEmpty(); token = str.Tokenize(_T(","), i), j++) {
        float f = 0;
        if (_stscanf_s(token, _T("%f"), &f) != 1) {
            continue;
        }

        switch (j) {
            case 0:
                break;
            case 1:
                m_PosX = f;
                break;
            case 2:
                m_PosY = f;
                break;
            case 3:
                m_ZoomX = f;
                break;
            case 4:
                m_ZoomY = f;
                break;
            default:
                break;
        }
    }

    if (j != 5) {
        return;
    }

    m_PosX = std::min(std::max(m_PosX, -0.5), 1.5);
    m_PosY = std::min(std::max(m_PosY, -0.5), 1.5);
    m_ZoomX = std::min(std::max(m_ZoomX, 0.2), 5.0);
    m_ZoomY = std::min(std::max(m_ZoomY, 0.2), 5.0);

    MoveVideoWindow(true);
}

void CMainFrame::OnUpdateViewPanNScanPresets(CCmdUI* pCmdUI)
{
    int nID = pCmdUI->m_nID - ID_PANNSCAN_PRESETS_START;
    const CAppSettings& s = AfxGetAppSettings();
    pCmdUI->Enable(GetLoadState() == MLS::LOADED && !m_fAudioOnly && nID >= 0 && nID <= s.m_pnspresets.GetCount() && s.iDSVideoRendererType != VIDRNDT_DS_EVR);
}

bool CMainFrame::PerformFlipRotate()
{
    HRESULT hr = E_NOTIMPL;
    // Note: m_AngleZ is counterclockwise, so value 270 means rotated 90 degrees clockwise
    if (m_pCAP3) {
        bool isFlip   = m_AngleX == 180;
        bool isMirror = m_AngleY == 180;
        int rotation = (360 - m_AngleZ + m_iDefRotation) % 360;
        if (m_pMVRS) {
            // MadVR: does not support mirror, instead of flip we rotate 180 degrees
            hr = m_pCAP3->SetRotation(isFlip ? (rotation + 180) % 360 : rotation);
        } else {
            // MPCVR: instead of flip, we mirror plus rotate 180 degrees
            hr = m_pCAP3->SetRotation(isFlip ? (rotation + 180) % 360 : rotation);
            if (SUCCEEDED(hr)) {
                // SetFlip actually mirrors instead of doing vertical flip
                hr = m_pCAP3->SetFlip(isFlip || isMirror);
            }
        }
    } else if (m_pCAP) {
        // default angle has already been set, so only apply custom angle
        hr = m_pCAP->SetVideoAngle(Vector(Vector::DegToRad(m_AngleX), Vector::DegToRad(m_AngleY), Vector::DegToRad(m_AngleZ)));
    }

    if (FAILED(hr)) {
        m_AngleX = m_AngleY = m_AngleZ = 0;
        return false;
    }
    return true;
}

void CMainFrame::OnViewRotate(UINT nID)
{
    switch (nID) {
    case ID_PANSCAN_ROTATEXP:
        if (!m_pCAP3) {
            m_AngleX += 2;
            break;
        }
        // fall through for m_pCAP3
    case ID_PANSCAN_ROTATEXM:
        if (m_AngleX >= 180) {
            m_AngleX = 0;
        } else {
            m_AngleX = 180;
        }
        break;
    case ID_PANSCAN_ROTATEYP:
        if (!m_pCAP3) {
            m_AngleY += 2;
            break;
        }
    case ID_PANSCAN_ROTATEYM:
        if (m_AngleY >= 180) {
            m_AngleY = 0;
        } else {
            m_AngleY = 180;
        }
        break;
    case ID_PANSCAN_ROTATEZM:
        if (m_AngleZ == 0 || m_AngleZ > 270) {
            m_AngleZ = 270;
        } else if (m_AngleZ > 180) {
            m_AngleZ = 180;
        } else if (m_AngleZ > 90) {
            m_AngleZ = 90;
        } else if (m_AngleZ > 0) {
            m_AngleZ = 0;
        }
        break;
    case ID_PANSCAN_ROTATEZP:
        if (!m_pCAP3) {
            m_AngleZ += 2;
            break;
        }
    case ID_PANSCAN_ROTATEZ270:
        if (m_AngleZ < 90) {
            m_AngleZ = 90;
        } else if (m_AngleZ >= 270) {
            m_AngleZ = 0;
        } else if (m_AngleZ >= 180) {
            m_AngleZ = 270;
        } else if (m_AngleZ >= 90) {
            m_AngleZ = 180;
        }
        break;
    default:
        return;
    }

    m_AngleX %= 360;
    m_AngleY %= 360;
    if (m_AngleX == 180 && m_AngleY == 180) {
        m_AngleX = m_AngleY = 0;
        m_AngleZ += 180;
    }
    m_AngleZ %= 360;

    ASSERT(m_AngleX >= 0);
    ASSERT(m_AngleY >= 0);
    ASSERT(m_AngleZ >= 0);

    if (PerformFlipRotate()) {
        // FIXME: do proper resizing of the window after rotate
        if (!m_pMVRC) {
            MoveVideoWindow();
        }

        CString info;
        info.Format(_T("x: %d, y: %d, z: %d"), m_AngleX, m_AngleY, m_AngleZ);
        SendStatusMessage(info, 3000);
    }
}

void CMainFrame::OnUpdateViewRotate(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(GetLoadState() == MLS::LOADED && !m_fAudioOnly && (m_pCAP || m_pCAP3));
}

// FIXME
const static SIZE s_ar[] = {{0, 0}, {4, 3}, {5, 4}, {16, 9}, {235, 100}, {185, 100}};

void CMainFrame::OnViewAspectRatio(UINT nID)
{
    auto& s = AfxGetAppSettings();

    CString info;
    if (nID == ID_ASPECTRATIO_SAR) {
        s.fKeepAspectRatio = false;
        info.LoadString(IDS_ASPECT_RATIO_SAR);
    } else {
        s.fKeepAspectRatio = true;
        CSize ar = s_ar[nID - ID_ASPECTRATIO_START];
        s.SetAspectRatioOverride(ar);
        if (ar.cx && ar.cy) {
            info.Format(IDS_MAINFRM_68, ar.cx, ar.cy);
        } else {
            info.LoadString(IDS_MAINFRM_69);
        }
    }

    SendStatusMessage(info, 3000);
    m_OSD.DisplayMessage(OSD_TOPLEFT, info, 3000);

    OnVideoSizeChanged();
}

void CMainFrame::OnUpdateViewAspectRatio(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();

    bool bSelected;
    if (pCmdUI->m_nID == ID_ASPECTRATIO_SAR) {
        bSelected = s.fKeepAspectRatio == false;
    } else {
        bSelected = s.fKeepAspectRatio == true && s.GetAspectRatioOverride() == s_ar[pCmdUI->m_nID - ID_ASPECTRATIO_START];
    }
    if (bSelected && pCmdUI->m_pMenu) {
        pCmdUI->m_pMenu->CheckMenuRadioItem(ID_ASPECTRATIO_START, ID_ASPECTRATIO_END, pCmdUI->m_nID, MF_BYCOMMAND);
    }

    pCmdUI->Enable(GetLoadState() == MLS::LOADED && !m_fAudioOnly && s.iDSVideoRendererType != VIDRNDT_DS_EVR);
}

void CMainFrame::OnViewAspectRatioNext()
{
    static_assert(ID_ASPECTRATIO_SAR - ID_ASPECTRATIO_START == _countof(s_ar) && ID_ASPECTRATIO_SAR == ID_ASPECTRATIO_END,
                  "ID_ASPECTRATIO_SAR needs to be last item in the menu.");

    const auto& s = AfxGetAppSettings();
    UINT nID = ID_ASPECTRATIO_START;
    if (s.fKeepAspectRatio) {
        const CSize ar = s.GetAspectRatioOverride();
        for (int i = 0; i < _countof(s_ar); i++) {
            if (ar == s_ar[i]) {
                nID += (i + 1) % ((ID_ASPECTRATIO_END - ID_ASPECTRATIO_START) + 1);
                break;
            }
        }
    }

    OnViewAspectRatio(nID);
}

void CMainFrame::OnViewOntop(UINT nID)
{
    nID -= ID_ONTOP_DEFAULT;
    if (AfxGetAppSettings().iOnTop == (int)nID) {
        nID = !nID;
    }
    SetAlwaysOnTop(nID);
}

void CMainFrame::OnUpdateViewOntop(CCmdUI* pCmdUI)
{
    int onTop = pCmdUI->m_nID - ID_ONTOP_DEFAULT;
    if (AfxGetAppSettings().iOnTop == onTop && pCmdUI->m_pMenu) {
        pCmdUI->m_pMenu->CheckMenuRadioItem(ID_ONTOP_DEFAULT, ID_ONTOP_WHILEPLAYINGVIDEO, pCmdUI->m_nID, MF_BYCOMMAND);
    }
}

void CMainFrame::OnViewOptions()
{
    ShowOptions();
}

// play

void CMainFrame::OnPlayPlay()
{
    const CAppSettings& s = AfxGetAppSettings();

    m_timerOneTime.Unsubscribe(TimerOneTimeSubscriber::DELAY_PLAYPAUSE_AFTER_AUTOCHANGE_MODE);
    m_bOpeningInAutochangedMonitorMode = false;
    m_bPausedForAutochangeMonitorMode = false;

    if (GetLoadState() == MLS::CLOSED) {
        m_bFirstPlay = false;
        OpenCurPlaylistItem();
        return;
    }

    if (GetLoadState() == MLS::LOADING) {
        return;
    }

    if (GetLoadState() == MLS::LOADED) {
        // If playback was previously stopped or ended, we need to reset the window size
        bool bVideoWndNeedReset = GetMediaState() == State_Stopped || m_fEndOfStream;

        KillTimersStop();

        if (GetPlaybackMode() == PM_FILE) {
            if (m_fEndOfStream) {
                SendMessage(WM_COMMAND, ID_PLAY_STOP);
            } else {
                // after long pause (10min) or hibernation, perform a seek to current position first, to prevent potential hang on some systems (due to gpu driver issues?)
                if (m_dwLastPause && m_wndSeekBar.HasDuration() && (GetTickCount64() - m_dwLastPause >= 10*60*1000)) {
                    streampospoller_active = true;
                    DoSeekTo(m_wndSeekBar.GetPos(), False);
                }
            }
            m_pMS->SetRate(m_dSpeedRate);
        } else if (GetPlaybackMode() == PM_DVD) {
            m_dSpeedRate = 1.0;
            m_pDVDC->PlayForwards(m_dSpeedRate, DVD_CMD_FLAG_Block, nullptr);
            m_pDVDC->Pause(FALSE);
        } else if (GetPlaybackMode() == PM_ANALOG_CAPTURE) {
            m_pMC->Stop(); // audio preview won't be in sync if we run it from paused state
        } else if (GetPlaybackMode() == PM_DIGITAL_CAPTURE) {
            CComQIPtr<IBDATuner> pTun = m_pGB;
            if (pTun) {
                bVideoWndNeedReset = false; // SetChannel deals with MoveVideoWindow
                SetChannel(s.nDVBLastChannel);
            } else {
                ASSERT(FALSE);
            }
        } else {
            ASSERT(FALSE);
        }

        if (bVideoWndNeedReset) {
            MoveVideoWindow(false, true);
        }

        // Restart playback
        m_dwLastPause = 0;
        m_pMC->Run();

        if (m_fFrameSteppingActive) {
            m_pFS->CancelStep();
            m_fFrameSteppingActive = false;
            m_pBA->put_Volume(m_nVolumeBeforeFrameStepping);
        } else {
            m_pBA->put_Volume(m_wndToolBar.Volume);
        }
        m_nStepForwardCount = 0;

        SetAlwaysOnTop(s.iOnTop);

        SetTimersPlay();
    }

    m_Lcd.SetStatusMessage(ResStr(IDS_CONTROLS_PLAYING), 3000);
    SetPlayState(PS_PLAY);

    OnTimer(TIMER_STREAMPOSPOLLER);

    SetupEVRColorControl(); // can be configured when streaming begins

    if (m_OSD.CanShowMessage()) {
        CString strOSD;
        CString strPlay(StrRes(ID_PLAY_PLAY));
        int i = strPlay.Find(_T("\n"));
        if (i > 0) {
            strPlay.Delete(i, strPlay.GetLength() - i);
        }

        if (m_bFirstPlay) {
            if (GetPlaybackMode() == PM_FILE) {
                if (!m_LastOpenBDPath.IsEmpty()) {
                    strOSD.LoadString(IDS_PLAY_BD);
                } else {
                    strOSD = GetFileName();
                    if (!strOSD.IsEmpty() && !m_wndPlaylistBar.GetCur()->m_bYoutubeDL) {
                        strOSD.TrimRight('/');
                        strOSD.Replace('\\', '/');
                        strOSD = strOSD.Mid(strOSD.ReverseFind('/') + 1);
                    }
                }
            } else if (GetPlaybackMode() == PM_DVD) {
                strOSD.LoadString(IDS_PLAY_DVD);
            }
        }

        if (strOSD.IsEmpty()) {
            strOSD = strPlay;
        }
        if (GetPlaybackMode() != PM_DIGITAL_CAPTURE) {
            m_OSD.DisplayMessage(OSD_TOPLEFT, strOSD, 3000);
        }
    }

    m_bFirstPlay = false;
}

void CMainFrame::OnPlayPauseI()
{
    m_timerOneTime.Unsubscribe(TimerOneTimeSubscriber::DELAY_PLAYPAUSE_AFTER_AUTOCHANGE_MODE);
    m_bOpeningInAutochangedMonitorMode = false;

    if (GetLoadState() == MLS::LOADED && GetMediaState() == State_Stopped) {
        MoveVideoWindow(false, true);
    }

    if (GetLoadState() == MLS::LOADED) {

        if (GetPlaybackMode() == PM_FILE
                || GetPlaybackMode() == PM_DVD
                || GetPlaybackMode() == PM_ANALOG_CAPTURE) {
            m_pMC->Pause();
            m_dwLastPause = GetTickCount64();
        } else {
            ASSERT(FALSE);
        }

        KillTimer(TIMER_STATS);
        SetAlwaysOnTop(AfxGetAppSettings().iOnTop);
    }

    CString strOSD(StrRes(ID_PLAY_PAUSE));
    int i = strOSD.Find(_T("\n"));
    if (i > 0) {
        strOSD.Delete(i, strOSD.GetLength() - i);
    }
    m_OSD.DisplayMessage(OSD_TOPLEFT, strOSD, 3000);
    m_Lcd.SetStatusMessage(ResStr(IDS_CONTROLS_PAUSED), 3000);
    SetPlayState(PS_PAUSE);
}

void CMainFrame::OnPlayPause()
{
    // Support ffdshow queuing.
    // To avoid black out on pause, we have to lock g_ffdshowReceive to synchronize with ReceiveMine.
    if (queue_ffdshow_support) {
        CAutoLock lck(&g_ffdshowReceive);
        return OnPlayPauseI();
    }
    OnPlayPauseI();
}

void CMainFrame::OnPlayPlaypause()
{
    OAFilterState fs = GetMediaState();
    if (fs == State_Running) {
        SendMessage(WM_COMMAND, ID_PLAY_PAUSE);
    } else if (fs == State_Stopped || fs == State_Paused) {
        SendMessage(WM_COMMAND, ID_PLAY_PLAY);
    }
}

void CMainFrame::OnApiPause()
{
    OAFilterState fs = GetMediaState();
    if (fs == State_Running) {
        SendMessage(WM_COMMAND, ID_PLAY_PAUSE);
    }
}
void CMainFrame::OnApiPlay()
{
    OAFilterState fs = GetMediaState();
    if (fs == State_Stopped || fs == State_Paused) {
        SendMessage(WM_COMMAND, ID_PLAY_PLAY);
    }
}

void CMainFrame::OnPlayStop()
{
    m_timerOneTime.Unsubscribe(TimerOneTimeSubscriber::DELAY_PLAYPAUSE_AFTER_AUTOCHANGE_MODE);
    m_bOpeningInAutochangedMonitorMode = false;
    m_bPausedForAutochangeMonitorMode = false;

    KillTimersStop();

    m_wndSeekBar.SetPos(0);
    if (GetLoadState() == MLS::LOADED) {
        if (GetPlaybackMode() == PM_FILE) {
            LONGLONG pos = 0;
            m_pMS->SetPositions(&pos, AM_SEEKING_AbsolutePositioning, nullptr, AM_SEEKING_NoPositioning);
            m_pMC->Stop();

            // BUG: after pause or stop the netshow url source filter won't continue
            // on the next play command, unless we cheat it by setting the file name again.
            //
            // Note: WMPx may be using some undocumented interface to restart streaming.

            BeginEnumFilters(m_pGB, pEF, pBF) {
                CComQIPtr<IAMNetworkStatus, &IID_IAMNetworkStatus> pAMNS = pBF;
                CComQIPtr<IFileSourceFilter> pFSF = pBF;
                if (pAMNS && pFSF) {
                    WCHAR* pFN = nullptr;
                    AM_MEDIA_TYPE mt;
                    if (SUCCEEDED(pFSF->GetCurFile(&pFN, &mt)) && pFN && *pFN) {
                        pFSF->Load(pFN, nullptr);
                        CoTaskMemFree(pFN);
                    }
                    break;
                }
            }
            EndEnumFilters;
        } else if (GetPlaybackMode() == PM_DVD) {
            m_pDVDC->SetOption(DVD_ResetOnStop, TRUE);
            m_pMC->Stop();
            m_pDVDC->SetOption(DVD_ResetOnStop, FALSE);

            if (m_bUseSeekPreview && m_pDVDC_preview) {
                m_pDVDC_preview->SetOption(DVD_ResetOnStop, TRUE);
                m_pMC_preview->Stop();
                m_pDVDC_preview->SetOption(DVD_ResetOnStop, FALSE);
            }
        } else if (GetPlaybackMode() == PM_DIGITAL_CAPTURE) {
            m_pMC->Stop();
            m_pDVBState->bActive = false;
            OpenSetupWindowTitle();
            m_wndStatusBar.SetStatusTimer(StrRes(IDS_CAPTURE_LIVE));
        } else if (GetPlaybackMode() == PM_ANALOG_CAPTURE) {
            m_pMC->Stop();
        }

        m_dSpeedRate = 1.0;

        if (m_fFrameSteppingActive) {
            m_pFS->CancelStep();
            m_fFrameSteppingActive = false;
            m_nStepForwardCount = 0;
            m_pBA->put_Volume(m_nVolumeBeforeFrameStepping);
        }
        m_nStepForwardCount = 0;
    } else if (GetLoadState() == MLS::CLOSING) {
        if (m_pMC) {
            VERIFY(m_pMC->Stop() == S_OK);
        }
    }

    m_nLoops = 0;

    if (m_hWnd) {
        MoveVideoWindow();

        if (GetLoadState() == MLS::LOADED) {
            __int64 start, stop;
            m_wndSeekBar.GetRange(start, stop);
            if (!IsPlaybackCaptureMode()) {
                m_wndStatusBar.SetStatusTimer(m_wndSeekBar.GetPos(), stop, IsSubresyncBarVisible(), GetTimeFormat());
            }

            SetAlwaysOnTop(AfxGetAppSettings().iOnTop);
        }
    }

    if (!m_fEndOfStream && GetLoadState() == MLS::LOADED) {
        CString strOSD(StrRes(ID_PLAY_STOP));
        int i = strOSD.Find(_T("\n"));
        if (i > 0) {
            strOSD.Delete(i, strOSD.GetLength() - i);
        }
        m_OSD.DisplayMessage(OSD_TOPLEFT, strOSD, 3000);
        m_Lcd.SetStatusMessage(ResStr(IDS_CONTROLS_STOPPED), 3000);
    } else {
        m_fEndOfStream = false;
    }

    DisableABRepeat();

    SetPlayState(PS_STOP);
}

void CMainFrame::OnUpdatePlayPauseStop(CCmdUI* pCmdUI)
{
    OAFilterState fs = m_fFrameSteppingActive ? State_Paused : GetMediaState();

    pCmdUI->SetCheck(fs == State_Running && pCmdUI->m_nID == ID_PLAY_PLAY
                     || fs == State_Paused && pCmdUI->m_nID == ID_PLAY_PAUSE
                     || fs == State_Stopped && pCmdUI->m_nID == ID_PLAY_STOP
                     || (fs == State_Paused || fs == State_Running) && pCmdUI->m_nID == ID_PLAY_PLAYPAUSE);

    bool fEnable = false;

    if (fs >= 0) {
        if (GetPlaybackMode() == PM_FILE || IsPlaybackCaptureMode()) {
            fEnable = true;

            if (m_fCapturing) {
                fEnable = false;
            } else if (m_fLiveWM && pCmdUI->m_nID == ID_PLAY_PAUSE) {
                fEnable = false;
            } else if (GetPlaybackMode() == PM_DIGITAL_CAPTURE && pCmdUI->m_nID == ID_PLAY_PAUSE) {
                fEnable = false; // Disable pause for digital capture mode to avoid accidental playback stop. We don't support time shifting yet.
            }
        } else if (GetPlaybackMode() == PM_DVD) {
            fEnable = m_iDVDDomain != DVD_DOMAIN_VideoManagerMenu
                      && m_iDVDDomain != DVD_DOMAIN_VideoTitleSetMenu;

            if (fs == State_Stopped && pCmdUI->m_nID == ID_PLAY_PAUSE) {
                fEnable = false;
            }
        }
    } else if (pCmdUI->m_nID == ID_PLAY_PLAY && !IsPlaylistEmpty()) {
        fEnable = true;
    }

    pCmdUI->Enable(fEnable);
}

void CMainFrame::OnPlayFramestep(UINT nID)
{
    if (!m_pFS) {
        return;
    }

    KillTimerDelayedSeek();

    m_OSD.EnableShowMessage(false);
    if (nID == ID_PLAY_FRAMESTEP) {
        if (GetMediaState() != State_Paused && !queue_ffdshow_support) {
            SendMessage(WM_COMMAND, ID_PLAY_PAUSE);
        }

        // To support framestep back, store the initial position when
        // stepping forward
        if (m_nStepForwardCount == 0) {
            if (GetPlaybackMode() == PM_DVD) {
                OnTimer(TIMER_STREAMPOSPOLLER);
                m_rtStepForwardStart = m_wndSeekBar.GetPos();
            } else {
                m_pMS->GetCurrentPosition(&m_rtStepForwardStart);
            }
        }

        m_fFrameSteppingActive = true;

        m_nVolumeBeforeFrameStepping = m_wndToolBar.Volume;
        m_pBA->put_Volume(-10000);

        m_pFS->Step(1, nullptr);
    } else if (S_OK == m_pMS->IsFormatSupported(&TIME_FORMAT_FRAME)) {
        if (GetMediaState() != State_Paused) {
            SendMessage(WM_COMMAND, ID_PLAY_PAUSE);
        }

        if (SUCCEEDED(m_pMS->SetTimeFormat(&TIME_FORMAT_FRAME))) {
            REFERENCE_TIME rtCurPos;

            if (SUCCEEDED(m_pMS->GetCurrentPosition(&rtCurPos))) {
                rtCurPos += (nID == ID_PLAY_FRAMESTEP) ? 1 : -1;

                m_pMS->SetPositions(&rtCurPos, AM_SEEKING_AbsolutePositioning, nullptr, AM_SEEKING_NoPositioning);
            }
            m_pMS->SetTimeFormat(&TIME_FORMAT_MEDIA_TIME);
        }
    } else { //if (s.iDSVideoRendererType != VIDRNDT_DS_VMR9WINDOWED && s.iDSVideoRendererType != VIDRNDT_DS_VMR9RENDERLESS)
        if (GetMediaState() != State_Paused) {
            SendMessage(WM_COMMAND, ID_PLAY_PAUSE);
        }

        const REFERENCE_TIME rtAvgTimePerFrame = std::llround(GetAvgTimePerFrame() * 10000000i64);
        REFERENCE_TIME rtCurPos = 0;

        // Exit of framestep forward : calculate the initial position
        if (m_nStepForwardCount) {
            m_pFS->CancelStep();
            rtCurPos = m_rtStepForwardStart + m_nStepForwardCount * rtAvgTimePerFrame;
            m_nStepForwardCount = 0;
        } else if (GetPlaybackMode() == PM_DVD) {
            // IMediaSeeking doesn't work well with DVD Navigator
            OnTimer(TIMER_STREAMPOSPOLLER);
            rtCurPos = m_wndSeekBar.GetPos();
        } else {
            m_pMS->GetCurrentPosition(&rtCurPos);
        }

        rtCurPos += (nID == ID_PLAY_FRAMESTEP) ? rtAvgTimePerFrame : -rtAvgTimePerFrame;
        DoSeekTo(rtCurPos, false);
    }
    m_OSD.EnableShowMessage();
}

void CMainFrame::OnUpdatePlayFramestep(CCmdUI* pCmdUI)
{
    bool fEnable = false;

    if (GetLoadState() == MLS::LOADED && !m_fAudioOnly && !m_fLiveWM
            && (GetPlaybackMode() == PM_FILE || (GetPlaybackMode() == PM_DVD && m_iDVDDomain == DVD_DOMAIN_Title))) {
        if (m_pFS && (S_OK == m_pMS->IsFormatSupported(&TIME_FORMAT_FRAME))) {
            fEnable = true;
        } else if (pCmdUI->m_nID == ID_PLAY_FRAMESTEP) {
            fEnable = true;
        } else if (pCmdUI->m_nID == ID_PLAY_FRAMESTEPCANCEL && m_pFS && m_pFS->CanStep(0, nullptr) == S_OK) {
            fEnable = true;
        }
    }

    pCmdUI->Enable(fEnable);
}

void CMainFrame::OnPlaySeek(UINT nID)
{
    const auto& s = AfxGetAppSettings();

    REFERENCE_TIME rtJumpDiff =
        nID == ID_PLAY_SEEKBACKWARDSMALL ? -10000i64 * s.nJumpDistS :
        nID == ID_PLAY_SEEKFORWARDSMALL  ? +10000i64 * s.nJumpDistS :
        nID == ID_PLAY_SEEKBACKWARDMED   ? -10000i64 * s.nJumpDistM :
        nID == ID_PLAY_SEEKFORWARDMED    ? +10000i64 * s.nJumpDistM :
        nID == ID_PLAY_SEEKBACKWARDLARGE ? -10000i64 * s.nJumpDistL :
        nID == ID_PLAY_SEEKFORWARDLARGE  ? +10000i64 * s.nJumpDistL :
        0;

    if (rtJumpDiff == 0) {
        ASSERT(FALSE);
        return;
    }

    if (m_fShockwaveGraph) {
        // HACK: the custom graph should support frame based seeking instead
        rtJumpDiff /= 10000i64 * 100;
    }

    const REFERENCE_TIME rtPos = m_wndSeekBar.GetPos();
    REFERENCE_TIME rtSeekTo = rtPos + rtJumpDiff;
    if (rtSeekTo < 0) {
        rtSeekTo = 0;
    }

    if (s.bFastSeek && !m_kfs.empty() && m_wndSeekBar.GetDuration() >= 600000000LL) {
        REFERENCE_TIME rtMaxForwardDiff;
        REFERENCE_TIME rtMaxBackwardDiff;
        if (s.bAllowInaccurateFastseek && (nID != ID_PLAY_SEEKBACKWARDSMALL) && (nID != ID_PLAY_SEEKFORWARDSMALL)) {
            if (rtJumpDiff > 0) {
                rtMaxForwardDiff  = 200000000LL;
                rtMaxBackwardDiff = rtJumpDiff / 2;
            } else {
                rtMaxForwardDiff  = -rtJumpDiff / 2;
                rtMaxBackwardDiff = 200000000LL;
            }
        } else {
            rtMaxForwardDiff = rtMaxBackwardDiff = std::min(100000000LL, abs(rtJumpDiff) * 3 / 10);
        }
        rtSeekTo = GetClosestKeyFrame(rtSeekTo, rtMaxForwardDiff, rtMaxBackwardDiff);
    }

    SeekTo(rtSeekTo);
}

void CMainFrame::OnPlaySeekSet()
{
    const REFERENCE_TIME rtPos = m_wndSeekBar.GetPos();
    REFERENCE_TIME rtStart, rtStop;
    m_wndSeekBar.GetRange(rtStart, rtStop);
    if (rtPos != rtStart) {
        SeekTo(rtStart);
    }
}

void CMainFrame::AdjustStreamPosPoller(bool restart)
{
    if (streampospoller_active) {
        int current_value = m_iStreamPosPollerInterval;

        if (g_bExternalSubtitleTime || IsSubresyncBarVisible()) {
            m_iStreamPosPollerInterval = 40;
        } else {
            m_iStreamPosPollerInterval = AfxGetAppSettings().nStreamPosPollerInterval;
        }

        if (restart && current_value != m_iStreamPosPollerInterval) {
            SetTimer(TIMER_STREAMPOSPOLLER, m_iStreamPosPollerInterval, nullptr);
        }
    }
}

void CMainFrame::SetTimersPlay()
{
    streampospoller_active = true;
    AdjustStreamPosPoller(false);

    SetTimer(TIMER_STREAMPOSPOLLER, m_iStreamPosPollerInterval, nullptr);
    SetTimer(TIMER_STREAMPOSPOLLER2, 500, nullptr);
    SetTimer(TIMER_STATS, 1000, nullptr);
}

void CMainFrame::KillTimerDelayedSeek()
{
    KillTimer(TIMER_DELAYEDSEEK);
    queuedSeek = { 0, 0, false };
}

void CMainFrame::KillTimersStop()
{
    KillTimerDelayedSeek();
    KillTimer(TIMER_STREAMPOSPOLLER2);
    if (KillTimer(TIMER_STREAMPOSPOLLER)) {
        ASSERT(streampospoller_active);
    }
    KillTimer(TIMER_STATS);
    m_timerOneTime.Unsubscribe(TimerOneTimeSubscriber::DVBINFO_UPDATE);
    streampospoller_active = false;
}

void CMainFrame::OnPlaySeekKey(UINT nID)
{
    if (!m_kfs.empty()) {
        bool bSeekingForward = (nID == ID_PLAY_SEEKKEYFORWARD);
        const REFERENCE_TIME rtPos = m_wndSeekBar.GetPos();
        REFERENCE_TIME rtKeyframe;
        REFERENCE_TIME rtTarget;
        REFERENCE_TIME rtMin;
        REFERENCE_TIME rtMax;
        if (bSeekingForward) {
            rtMin = rtPos + 10000LL; // at least one millisecond later
            rtMax = GetDur();
            rtTarget = rtMin;
        } else {
            rtMin = 0;
            if (GetMediaState() == State_Paused) {
                rtMax = rtPos - 10000LL;
            } else {
                rtMax = rtPos - 5000000LL;
            }
            rtTarget = rtMax;
        }

        if (GetKeyFrame(rtTarget, rtMin, rtMax, false, rtKeyframe)) {
            SeekTo(rtKeyframe);
        }
    }
}

void CMainFrame::OnUpdatePlaySeek(CCmdUI* pCmdUI)
{
    bool fEnable = false;

    if (GetLoadState() == MLS::LOADED) {
        fEnable = true;
        if (GetPlaybackMode() == PM_DVD && m_iDVDDomain != DVD_DOMAIN_Title) {
            fEnable = false;
        } else if (IsPlaybackCaptureMode()) {
            fEnable = false;
        }
    }

    pCmdUI->Enable(fEnable);
}

void CMainFrame::SetPlayingRate(double rate)
{
    if (GetLoadState() != MLS::LOADED) {
        return;
    }
    HRESULT hr = E_FAIL;
    if (GetPlaybackMode() == PM_FILE) {
        if (rate < 0.05) {
            if (m_dSpeedRate > 0.05) {
                rate = 0.05;
            } else {
                return;
            }
        }
        if (GetMediaState() != State_Running) {
            SendMessage(WM_COMMAND, ID_PLAY_PLAY);
        }
        hr = m_pMS->SetRate(rate);
    } else if (GetPlaybackMode() == PM_DVD) {
        if (GetMediaState() != State_Running) {
            SendMessage(WM_COMMAND, ID_PLAY_PLAY);
        }
        if (rate > 0) {
            hr = m_pDVDC->PlayForwards(rate, DVD_CMD_FLAG_Block, nullptr);
        } else {
            hr = m_pDVDC->PlayBackwards(-rate, DVD_CMD_FLAG_Block, nullptr);
        }
    }
    if (SUCCEEDED(hr)) {
        m_dSpeedRate = rate;
        CString strODSMessage;
        strODSMessage.Format(IDS_OSD_SPEED, rate);
        m_OSD.DisplayMessage(OSD_TOPRIGHT, strODSMessage);
    }
}

void CMainFrame::OnPlayChangeRate(UINT nID)
{
    if (GetLoadState() != MLS::LOADED) {
        return;
    }

    if (GetPlaybackMode() == PM_FILE) {
        const CAppSettings& s = AfxGetAppSettings();
        double dSpeedStep = s.nSpeedStep / 100.0;

        if (nID == ID_PLAY_INCRATE) {
            if (s.nSpeedStep > 0) {
                SetPlayingRate(m_dSpeedRate + dSpeedStep);
            } else {
                SetPlayingRate(m_dSpeedRate * 2.0);
            }
        } else if (nID == ID_PLAY_DECRATE) {
            if (s.nSpeedStep > 0) {
                SetPlayingRate(m_dSpeedRate - dSpeedStep);
            } else {
                SetPlayingRate(m_dSpeedRate / 2.0);
            }
        } else if (nID > ID_PLAY_PLAYBACKRATE_START && nID < ID_PLAY_PLAYBACKRATE_END) {
            if (filePlaybackRates.count(nID) != 0) {
                SetPlayingRate(filePlaybackRates[nID]);
            } else if (nID == ID_PLAY_PLAYBACKRATE_FPS24 || nID == ID_PLAY_PLAYBACKRATE_FPS25) {
                if (m_pCAP) {
                    float target = (nID == ID_PLAY_PLAYBACKRATE_FPS24 ? 24.0f : 25.0f);
                    SetPlayingRate(target / m_pCAP->GetFPS());
                }
            }
        }
    } else if (GetPlaybackMode() == PM_DVD) {
        if (nID == ID_PLAY_INCRATE) {
            if (m_dSpeedRate > 0) {
                SetPlayingRate(m_dSpeedRate * 2.0);
            } else if (m_dSpeedRate >= -1) {
                SetPlayingRate(1);
            } else {
                SetPlayingRate(m_dSpeedRate / 2.0);
            }
        } else if (nID == ID_PLAY_DECRATE) {
            if (m_dSpeedRate < 0) {
                SetPlayingRate(m_dSpeedRate * 2.0);
            } else if (m_dSpeedRate <= 1) {
                SetPlayingRate(-1);
            } else {
                SetPlayingRate(m_dSpeedRate / 2.0);
            }
        } else if (nID > ID_PLAY_PLAYBACKRATE_START && nID < ID_PLAY_PLAYBACKRATE_END) {
            if (dvdPlaybackRates.count(nID) != 0) {
                SetPlayingRate(dvdPlaybackRates[nID]);
            }
        }
    } else if (GetPlaybackMode() == PM_ANALOG_CAPTURE) {
        if (GetMediaState() != State_Running) {
            SendMessage(WM_COMMAND, ID_PLAY_PLAY);
        }

        long lChannelMin = 0, lChannelMax = 0;
        m_pAMTuner->ChannelMinMax(&lChannelMin, &lChannelMax);
        long lChannel = 0, lVivSub = 0, lAudSub = 0;
        m_pAMTuner->get_Channel(&lChannel, &lVivSub, &lAudSub);

        long lFreqOrg = 0, lFreqNew = -1;
        m_pAMTuner->get_VideoFrequency(&lFreqOrg);

        //long lSignalStrength;
        do {
            if (nID == ID_PLAY_DECRATE) {
                lChannel--;
            } else if (nID == ID_PLAY_INCRATE) {
                lChannel++;
            }

            //if (lChannel < lChannelMin) lChannel = lChannelMax;
            //if (lChannel > lChannelMax) lChannel = lChannelMin;

            if (lChannel < lChannelMin || lChannel > lChannelMax) {
                break;
            }

            if (FAILED(m_pAMTuner->put_Channel(lChannel, AMTUNER_SUBCHAN_DEFAULT, AMTUNER_SUBCHAN_DEFAULT))) {
                break;
            }

            long flFoundSignal;
            m_pAMTuner->AutoTune(lChannel, &flFoundSignal);

            m_pAMTuner->get_VideoFrequency(&lFreqNew);
        } while (FALSE);
        /*SUCCEEDED(m_pAMTuner->SignalPresent(&lSignalStrength))
        && (lSignalStrength != AMTUNER_SIGNALPRESENT || lFreqNew == lFreqOrg));*/
    } else {
        ASSERT(FALSE);
    }
}

void CMainFrame::OnUpdatePlayChangeRate(CCmdUI* pCmdUI)
{
    bool fEnable = false;

    if (GetLoadState() == MLS::LOADED) {
        if (pCmdUI->m_nID > ID_PLAY_PLAYBACKRATE_START && pCmdUI->m_nID < ID_PLAY_PLAYBACKRATE_END && pCmdUI->m_pMenu) {
            fEnable = false;
            if (GetPlaybackMode() == PM_FILE) {
                if (filePlaybackRates.count(pCmdUI->m_nID) != 0) {
                    fEnable = true;
                    if (filePlaybackRates[pCmdUI->m_nID] == m_dSpeedRate) {
                        pCmdUI->m_pMenu->CheckMenuRadioItem(ID_PLAY_PLAYBACKRATE_START, ID_PLAY_PLAYBACKRATE_END, pCmdUI->m_nID, MF_BYCOMMAND);
                    }
                } else if (pCmdUI->m_nID == ID_PLAY_PLAYBACKRATE_FPS24 || pCmdUI->m_nID == ID_PLAY_PLAYBACKRATE_FPS25) {
                    fEnable = true;
                    if (m_pCAP) {
                        float target = (pCmdUI->m_nID == ID_PLAY_PLAYBACKRATE_FPS24 ? 24.0f : 25.0f);
                        if (target / m_pCAP->GetFPS() == m_dSpeedRate) {
                            bool found = false;
                            for (auto const& [key, rate] : filePlaybackRates) { //make sure it wasn't a standard rate already
                                if (rate == m_dSpeedRate) {
                                    found = true;
                                }
                            }
                            if (!found) { //must have used fps, as it didn't match a standard rate
                                pCmdUI->m_pMenu->CheckMenuRadioItem(ID_PLAY_PLAYBACKRATE_START, ID_PLAY_PLAYBACKRATE_END, pCmdUI->m_nID, MF_BYCOMMAND);
                            }
                        }
                    }
                }
            } else if (GetPlaybackMode() == PM_DVD) {
                if (dvdPlaybackRates.count(pCmdUI->m_nID) != 0) {
                    fEnable = true;
                    if (dvdPlaybackRates[pCmdUI->m_nID] == m_dSpeedRate) {
                        pCmdUI->m_pMenu->CheckMenuRadioItem(ID_PLAY_PLAYBACKRATE_START, ID_PLAY_PLAYBACKRATE_END, pCmdUI->m_nID, MF_BYCOMMAND);
                    }
                }
            }
        } else {
            bool fInc = pCmdUI->m_nID == ID_PLAY_INCRATE;

            fEnable = true;
            if (fInc && m_dSpeedRate >= 128.0) {
                fEnable = false;
            } else if (!fInc && GetPlaybackMode() == PM_FILE && m_dSpeedRate <= 0.05) {
                fEnable = false;
            } else if (!fInc && GetPlaybackMode() == PM_DVD && m_dSpeedRate <= -128.0) {
                fEnable = false;
            } else if (GetPlaybackMode() == PM_DVD && m_iDVDDomain != DVD_DOMAIN_Title) {
                fEnable = false;
            } else if (m_fShockwaveGraph) {
                fEnable = false;
            } else if (GetPlaybackMode() == PM_ANALOG_CAPTURE && (!m_wndCaptureBar.m_capdlg.IsTunerActive() || m_fCapturing)) {
                fEnable = false;
            } else if (GetPlaybackMode() == PM_DIGITAL_CAPTURE) {
                fEnable = false;
            } else if (m_fLiveWM) {
                fEnable = false;
            }
        }
    }

    pCmdUI->Enable(fEnable);
}

void CMainFrame::OnPlayResetRate()
{
    if (GetLoadState() != MLS::LOADED) {
        return;
    }

    HRESULT hr = E_FAIL;

    if (GetMediaState() != State_Running) {
        SendMessage(WM_COMMAND, ID_PLAY_PLAY);
    }

    if (GetPlaybackMode() == PM_FILE) {
        hr = m_pMS->SetRate(1.0);
    } else if (GetPlaybackMode() == PM_DVD) {
        hr = m_pDVDC->PlayForwards(1.0, DVD_CMD_FLAG_Block, nullptr);
    }

    if (SUCCEEDED(hr)) {
        m_dSpeedRate = 1.0;

        CString strODSMessage;
        strODSMessage.Format(IDS_OSD_SPEED, m_dSpeedRate);
        m_OSD.DisplayMessage(OSD_TOPRIGHT, strODSMessage);
    }
}

void CMainFrame::OnUpdatePlayResetRate(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(GetLoadState() == MLS::LOADED);
}

void CMainFrame::SetAudioDelay(REFERENCE_TIME rtShift)
{
    if (CComQIPtr<IAudioSwitcherFilter> pASF = FindFilter(__uuidof(CAudioSwitcherFilter), m_pGB)) {
        pASF->SetAudioTimeShift(rtShift);

        if (GetLoadState() == MLS::LOADED) {
            CString str;
            str.Format(IDS_MAINFRM_70, rtShift / 10000);
            SendStatusMessage(str, 3000);
            m_OSD.DisplayMessage(OSD_TOPLEFT, str);
        }
    }
}

void CMainFrame::SetSubtitleDelay(int delay_ms)
{
    if (m_pCAP) {
        m_pCAP->SetSubtitleDelay(delay_ms);

        CString strSubDelay;
        strSubDelay.Format(IDS_MAINFRM_139, delay_ms);
        SendStatusMessage(strSubDelay, 3000);
        m_OSD.DisplayMessage(OSD_TOPLEFT, strSubDelay);
    }
}

void CMainFrame::OnPlayChangeAudDelay(UINT nID)
{
    if (CComQIPtr<IAudioSwitcherFilter> pASF = FindFilter(__uuidof(CAudioSwitcherFilter), m_pGB)) {
        REFERENCE_TIME rtShift = pASF->GetAudioTimeShift();
        rtShift +=
            nID == ID_PLAY_INCAUDDELAY ? 100000 :
            nID == ID_PLAY_DECAUDDELAY ? -100000 :
            0;

        SetAudioDelay(rtShift);
    }
}

void CMainFrame::OnUpdatePlayChangeAudDelay(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(!!m_pGB /*&& !!FindFilter(__uuidof(CAudioSwitcherFilter), m_pGB)*/);
}

void CMainFrame::OnPlayFiltersCopyToClipboard()
{
    // Don't translate that output since it's mostly for debugging purpose
    CString filtersList = _T("Filters currently loaded:\r\n");
    // Skip the first two entries since they are the "Copy to clipboard" menu entry and a separator
    for (int i = 2, count = m_filtersMenu.GetMenuItemCount(); i < count; i++) {
        CString filterName;
        m_filtersMenu.GetMenuString(i, filterName, MF_BYPOSITION);
        filtersList.AppendFormat(_T("  - %s\r\n"), filterName.GetString());
    }

    // Allocate a global memory object for the text
    int len = filtersList.GetLength() + 1;
    HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, len * sizeof(WCHAR));
    if (hGlob) {
        // Lock the handle and copy the text to the buffer
        LPVOID pData = GlobalLock(hGlob);
        if (pData) {
            wcscpy_s((WCHAR*)pData, len, (LPCWSTR)filtersList);
            GlobalUnlock(hGlob);

            if (OpenClipboard()) {
                // Place the handle on the clipboard, if the call succeeds
                // the system will take care of the allocated memory
                if (::EmptyClipboard() && ::SetClipboardData(CF_UNICODETEXT, hGlob)) {
                    hGlob = nullptr;
                }

                ::CloseClipboard();
            }
        }

        if (hGlob) {
            GlobalFree(hGlob);
        }
    }
}

void CMainFrame::OnPlayFilters(UINT nID)
{
    //ShowPPage(m_spparray[nID - ID_FILTERS_SUBITEM_START], m_hWnd);

    CComPtr<IUnknown> pUnk = m_pparray[nID - ID_FILTERS_SUBITEM_START];

    CComPropertySheet ps(IDS_PROPSHEET_PROPERTIES, GetModalParent());

    // Find out if we are opening the property page for an internal filter
    CComQIPtr<IBaseFilter> pBF = pUnk;
    bool bIsInternalFilter = false;
    CFGFilterLAV::LAVFILTER_TYPE LAVFilterType = CFGFilterLAV::INVALID;
    if (pBF) {
        bIsInternalFilter = CFGFilterLAV::IsInternalInstance(pBF, &LAVFilterType);
    }

    if (CComQIPtr<ISpecifyPropertyPages> pSPP = pUnk) {
        ULONG uIgnoredPage = ULONG(-1);
        // If we are dealing with an internal filter, we want to ignore the "Formats" page.
        if (bIsInternalFilter) {
            uIgnoredPage = (LAVFilterType != CFGFilterLAV::AUDIO_DECODER) ? 1 : 2;
        }
        ps.AddPages(pSPP, uIgnoredPage);
    }

    if (pBF) {
        HRESULT hr;
        CComPtr<IPropertyPage> pPP = DEBUG_NEW CInternalPropertyPageTempl<CPinInfoWnd>(nullptr, &hr);
        ps.AddPage(pPP, pBF);
    }

    if (ps.GetPageCount() > 0) {
        ps.DoModal();
        OpenSetupStatusBar();

        if (bIsInternalFilter) {
            if (CComQIPtr<ILAVFSettings> pLAVFSettings = pBF) {
                CFGFilterLAVSplitterBase::Settings settings;
                if (settings.GetSettings(pLAVFSettings)) { // Get current settings from LAVSplitter
                    settings.SaveSettings(); // Save them to the registry/ini
                }
            } else if (CComQIPtr<ILAVVideoSettings> pLAVVideoSettings = pBF) {
                CFGFilterLAVVideo::Settings settings;
                if (settings.GetSettings(pLAVVideoSettings)) { // Get current settings from LAVVideo
                    settings.SaveSettings(); // Save them to the registry/ini
                }
            } else if (CComQIPtr<ILAVAudioSettings> pLAVAudioSettings = pBF) {
                CFGFilterLAVAudio::Settings settings;
                if (settings.GetSettings(pLAVAudioSettings)) { // Get current settings from LAVAudio
                    settings.SaveSettings(); // Save them to the registry/ini
                }
            }
        }
    }
}

void CMainFrame::OnUpdatePlayFilters(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(!m_fCapturing);
}

void CMainFrame::OnPlayShadersSelect()
{
    ShowOptions(IDD_PPAGESHADERS);
}

void CMainFrame::OnPlayShadersPresetNext()
{
    auto& s = AfxGetAppSettings();
    if (s.m_Shaders.NextPreset()) {
        CString name;
        if (s.m_Shaders.GetCurrentPresetName(name)) {
            CString msg;
            msg.Format(IDS_OSD_SHADERS_PRESET, name.GetString());
            m_OSD.DisplayMessage(OSD_TOPLEFT, msg);
        }
    }
}

void CMainFrame::OnPlayShadersPresetPrev()
{
    auto& s = AfxGetAppSettings();
    if (s.m_Shaders.PrevPreset()) {
        CString name;
        if (s.m_Shaders.GetCurrentPresetName(name)) {
            CString msg;
            msg.Format(IDS_OSD_SHADERS_PRESET, name.GetString());
            m_OSD.DisplayMessage(OSD_TOPLEFT, msg);
        }
    }
}

void CMainFrame::OnPlayShadersPresets(UINT nID)
{
    ASSERT((nID >= ID_SHADERS_PRESETS_START) && (nID <= ID_SHADERS_PRESETS_END));
    auto& s = AfxGetAppSettings();
    int num = (int)nID - ID_SHADERS_PRESETS_START;
    auto presets = s.m_Shaders.GetPresets();
    ASSERT(num < (int)presets.size());
    for (const auto& pair : presets) {
        if (num-- == 0) {
            s.m_Shaders.SetCurrentPreset(pair.first);
            break;
        }
    }
}

// Called from GraphThread
void CMainFrame::OnPlayAudio(UINT nID)
{
    int i = (int)nID - ID_AUDIO_SUBITEM_START;

    CComQIPtr<IAMStreamSelect> pSS = FindFilter(__uuidof(CAudioSwitcherFilter), m_pGB);

    DWORD cStreams = 0;
    if (i != 0) {
        currentAudioLang = _T("");
    }

    if (GetPlaybackMode() == PM_DVD) {
        m_pDVDC->SelectAudioStream(i, DVD_CMD_FLAG_Block, nullptr);
        LCID lcid = 0;
        if (SUCCEEDED(m_pDVDI->GetAudioLanguage(i, &lcid)) && lcid != 0) {
            GetLocaleString(lcid, LOCALE_SISO639LANGNAME2, currentAudioLang);
        }

    } else if (pSS && SUCCEEDED(pSS->Count(&cStreams)) && cStreams > 0) {
        if (i == 0) {
            ShowOptions(CPPageAudioSwitcher::IDD);
        } else {
            pSS->Enable(i - 1, AMSTREAMSELECTENABLE_ENABLE);
            LCID lcid = 0;
            if (SUCCEEDED(pSS->Info(i - 1, nullptr, nullptr, &lcid, nullptr, nullptr, nullptr, nullptr)) && lcid != 0) {
                GetLocaleString(lcid, LOCALE_SISO639LANGNAME2, currentAudioLang);
            }
        }
    } else if (GetPlaybackMode() == PM_FILE) {
        OnNavStreamSelectSubMenu(i, 1);
    } else if (GetPlaybackMode() == PM_DIGITAL_CAPTURE) {
        if (CBDAChannel* pChannel = m_pDVBState->pChannel) {
            OnNavStreamSelectSubMenu(i, 1);
            pChannel->SetDefaultAudio(i);
        }
    }
}

void CMainFrame::OnSubtitlesDefaultStyle()
{
    CAppSettings& s = AfxGetAppSettings();
    if (!m_pSubStreams.IsEmpty()) {
        s.fUseDefaultSubtitlesStyle = !s.fUseDefaultSubtitlesStyle;
        UpdateSubDefaultStyle();
    }
}

void CMainFrame::OnPlaySubtitles(UINT nID)
{
    CAppSettings& s = AfxGetAppSettings();
    int i = (int)nID - ID_SUBTITLES_SUBITEM_START;

    if (GetPlaybackMode() == PM_DVD) {
        ULONG ulStreamsAvailable, ulCurrentStream;
        BOOL bIsDisabled;
        if (SUCCEEDED(m_pDVDI->GetCurrentSubpicture(&ulStreamsAvailable, &ulCurrentStream, &bIsDisabled))
                && ulStreamsAvailable) {
            if (i == 0) {
                m_pDVDC->SetSubpictureState(bIsDisabled, DVD_CMD_FLAG_Block, nullptr);
            } else if (i <= int(ulStreamsAvailable)) {
                m_pDVDC->SelectSubpictureStream(i - 1, DVD_CMD_FLAG_Block, nullptr);
                m_pDVDC->SetSubpictureState(TRUE, DVD_CMD_FLAG_Block, nullptr);
            }
            i -= ulStreamsAvailable + 1;
        }
    }

    if (GetPlaybackMode() == PM_DIGITAL_CAPTURE) {
        if (CBDAChannel* pChannel = m_pDVBState->pChannel) {
            OnNavStreamSelectSubMenu(i, 2);
            pChannel->SetDefaultSubtitle(i);
        }
    } else if (!m_pSubStreams.IsEmpty()) {
        // Currently the subtitles menu contains 5 items apart from the actual subtitles list when the ISR is used
        i -= 5;

        if (i == -5) {
            // options
            ShowOptions(CPPageSubtitles::IDD);
        } else if (i == -4) {
            // styles
            int j = 0;
            SubtitleInput* pSubInput = GetSubtitleInput(j, true);
            CLSID clsid;

            if (pSubInput && SUCCEEDED(pSubInput->pSubStream->GetClassID(&clsid))) {
                if (clsid == __uuidof(CRenderedTextSubtitle)) {
                    CRenderedTextSubtitle* pRTS = (CRenderedTextSubtitle*)(ISubStream*)pSubInput->pSubStream;

                    CAutoPtrArray<CPPageSubStyle> pages;
                    CAtlArray<STSStyle*> styles;

                    POSITION pos = pRTS->m_styles.GetStartPosition();
                    for (int k = 0; pos; k++) {
                        CString key;
                        STSStyle* val;
                        pRTS->m_styles.GetNextAssoc(pos, key, val);

                        CAutoPtr<CPPageSubStyle> page(DEBUG_NEW CPPageSubStyle());
                        page->InitStyle(key, *val);
                        pages.Add(page);
                        styles.Add(val);
                    }

                    CMPCThemePropertySheet dlg(IDS_SUBTITLES_STYLES_CAPTION, GetModalParent());
                    for (size_t l = 0; l < pages.GetCount(); l++) {
                        dlg.AddPage(pages[l]);
                    }

                    if (dlg.DoModal() == IDOK) {
                        {
                            CAutoLock cAutoLock(&m_csSubLock);

                            for (size_t l = 0; l < pages.GetCount(); l++) {
                                pages[l]->GetStyle(*styles[l]);
                            }
                            pRTS->Deinit();
                        }
                        InvalidateSubtitle();
                    }
                }
            }
        } else if (i == -3) {
            // reload
            ReloadSubtitle();
        } else if (i == -2) {
            // enable
            ToggleSubtitleOnOff();
        } else if (i == -1) {
            // override default style
            // TODO: default subtitles style toggle here
            s.fUseDefaultSubtitlesStyle = !s.fUseDefaultSubtitlesStyle;
            UpdateSubDefaultStyle();
        } else if (i >= 0) {
            // this is an actual item from the subtitles list
            s.fEnableSubtitles = true;
            SetSubtitle(i);
        }
    } else if (GetPlaybackMode() == PM_FILE) {
        OnNavStreamSelectSubMenu(i, 2);
    }
}

void CMainFrame::OnPlayVideoStreams(UINT nID)
{
    nID -= ID_VIDEO_STREAMS_SUBITEM_START;

    if (GetPlaybackMode() == PM_FILE) {
        OnNavStreamSelectSubMenu(nID, 0);
    } else if (GetPlaybackMode() == PM_DVD) {
        m_pDVDC->SelectAngle(nID + 1, DVD_CMD_FLAG_Block, nullptr);

        CString osdMessage;
        osdMessage.Format(IDS_AG_ANGLE, nID + 1);
        m_OSD.DisplayMessage(OSD_TOPLEFT, osdMessage);
    }
}

void CMainFrame::OnPlayFiltersStreams(UINT nID)
{
    nID -= ID_FILTERSTREAMS_SUBITEM_START;
    CComPtr<IAMStreamSelect> pAMSS = m_ssarray[nID];
    UINT i = nID;

    while (i > 0 && pAMSS == m_ssarray[i - 1]) {
        i--;
    }

    if (FAILED(pAMSS->Enable(nID - i, AMSTREAMSELECTENABLE_ENABLE))) {
        MessageBeep(UINT_MAX);
    }

    OpenSetupStatusBar();
}

void CMainFrame::OnPlayVolume(UINT nID)
{
    if (GetLoadState() == MLS::LOADED) {
        CString strVolume;
        m_pBA->put_Volume(m_wndToolBar.Volume);

        //strVolume.Format (L"Vol : %d dB", m_wndToolBar.Volume / 100);
        if (m_wndToolBar.Volume == -10000) {
            strVolume.Format(IDS_VOLUME_OSD, 0);
        } else {
            strVolume.Format(IDS_VOLUME_OSD, m_wndToolBar.m_volctrl.GetPos());
        }
        m_OSD.DisplayMessage(OSD_TOPLEFT, strVolume);
        //SendStatusMessage(strVolume, 3000); // Now the volume is displayed in three places at once.
    }

    m_Lcd.SetVolume((m_wndToolBar.Volume > -10000 ? m_wndToolBar.m_volctrl.GetPos() : 1));
}

void CMainFrame::OnPlayVolumeBoost(UINT nID)
{
    CAppSettings& s = AfxGetAppSettings();

    switch (nID) {
        case ID_VOLUME_BOOST_INC:
            s.nAudioBoost += s.nVolumeStep;
            if (s.nAudioBoost > 300) {
                s.nAudioBoost = 300;
            }
            break;
        case ID_VOLUME_BOOST_DEC:
            if (s.nAudioBoost > s.nVolumeStep) {
                s.nAudioBoost -= s.nVolumeStep;
            } else {
                s.nAudioBoost = 0;
            }
            break;
        case ID_VOLUME_BOOST_MIN:
            s.nAudioBoost = 0;
            break;
        case ID_VOLUME_BOOST_MAX:
            s.nAudioBoost = 300;
            break;
    }

    SetVolumeBoost(s.nAudioBoost);
}

void CMainFrame::SetVolumeBoost(UINT nAudioBoost)
{
    if (CComQIPtr<IAudioSwitcherFilter> pASF = FindFilter(__uuidof(CAudioSwitcherFilter), m_pGB)) {
        bool fNormalize, fNormalizeRecover;
        UINT nMaxNormFactor, nBoost;
        pASF->GetNormalizeBoost2(fNormalize, nMaxNormFactor, fNormalizeRecover, nBoost);

        CString strBoost;
        strBoost.Format(IDS_BOOST_OSD, nAudioBoost);
        pASF->SetNormalizeBoost2(fNormalize, nMaxNormFactor, fNormalizeRecover, nAudioBoost);
        m_OSD.DisplayMessage(OSD_TOPLEFT, strBoost);
    }
}

void CMainFrame::OnUpdatePlayVolumeBoost(CCmdUI* pCmdUI)
{
    pCmdUI->Enable();
}

void CMainFrame::OnCustomChannelMapping()
{
    if (CComQIPtr<IAudioSwitcherFilter> pASF = FindFilter(__uuidof(CAudioSwitcherFilter), m_pGB)) {
        CAppSettings& s = AfxGetAppSettings();
        s.fCustomChannelMapping = !s.fCustomChannelMapping;
        pASF->SetSpeakerConfig(s.fCustomChannelMapping, s.pSpeakerToChannelMap);
        m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(s.fCustomChannelMapping ? IDS_OSD_CUSTOM_CH_MAPPING_ON : IDS_OSD_CUSTOM_CH_MAPPING_OFF));
    }
}

void CMainFrame::OnUpdateCustomChannelMapping(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    pCmdUI->Enable(s.fEnableAudioSwitcher);
}

void CMainFrame::OnNormalizeRegainVolume(UINT nID)
{
    if (CComQIPtr<IAudioSwitcherFilter> pASF = FindFilter(__uuidof(CAudioSwitcherFilter), m_pGB)) {
        CAppSettings& s = AfxGetAppSettings();
        WORD osdMessage = 0;

        switch (nID) {
            case ID_NORMALIZE:
                s.fAudioNormalize = !s.fAudioNormalize;
                osdMessage = s.fAudioNormalize ? IDS_OSD_NORMALIZE_ON : IDS_OSD_NORMALIZE_OFF;
                break;
            case ID_REGAIN_VOLUME:
                s.fAudioNormalizeRecover = !s.fAudioNormalizeRecover;
                osdMessage = s.fAudioNormalizeRecover ? IDS_OSD_REGAIN_VOLUME_ON : IDS_OSD_REGAIN_VOLUME_OFF;
                break;
        }

        pASF->SetNormalizeBoost2(s.fAudioNormalize, s.nAudioMaxNormFactor, s.fAudioNormalizeRecover, s.nAudioBoost);
        m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(osdMessage));
    }
}

void CMainFrame::OnUpdateNormalizeRegainVolume(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    pCmdUI->Enable(s.fEnableAudioSwitcher);
}

void CMainFrame::OnPlayColor(UINT nID)
{
    if (m_pVMRMC || m_pMFVP) {
        CAppSettings& s = AfxGetAppSettings();
        //ColorRanges* crs = AfxGetMyApp()->ColorControls;
        int& brightness = s.iBrightness;
        int& contrast   = s.iContrast;
        int& hue        = s.iHue;
        int& saturation = s.iSaturation;
        CString tmp, str;
        switch (nID) {

            case ID_COLOR_BRIGHTNESS_INC:
                brightness += 2;
            // no break
            case ID_COLOR_BRIGHTNESS_DEC:
                brightness -= 1;
                SetColorControl(ProcAmp_Brightness, brightness, contrast, hue, saturation);
                tmp.Format(brightness ? _T("%+d") : _T("%d"), brightness);
                str.Format(IDS_OSD_BRIGHTNESS, tmp.GetString());
                break;

            case ID_COLOR_CONTRAST_INC:
                contrast += 2;
            // no break
            case ID_COLOR_CONTRAST_DEC:
                contrast -= 1;
                SetColorControl(ProcAmp_Contrast, brightness, contrast, hue, saturation);
                tmp.Format(contrast ? _T("%+d") : _T("%d"), contrast);
                str.Format(IDS_OSD_CONTRAST, tmp.GetString());
                break;

            case ID_COLOR_HUE_INC:
                hue += 2;
            // no break
            case ID_COLOR_HUE_DEC:
                hue -= 1;
                SetColorControl(ProcAmp_Hue, brightness, contrast, hue, saturation);
                tmp.Format(hue ? _T("%+d") : _T("%d"), hue);
                str.Format(IDS_OSD_HUE, tmp.GetString());
                break;

            case ID_COLOR_SATURATION_INC:
                saturation += 2;
            // no break
            case ID_COLOR_SATURATION_DEC:
                saturation -= 1;
                SetColorControl(ProcAmp_Saturation, brightness, contrast, hue, saturation);
                tmp.Format(saturation ? _T("%+d") : _T("%d"), saturation);
                str.Format(IDS_OSD_SATURATION, tmp.GetString());
                break;

            case ID_COLOR_RESET:
                brightness = AfxGetMyApp()->GetColorControl(ProcAmp_Brightness)->DefaultValue;
                contrast   = AfxGetMyApp()->GetColorControl(ProcAmp_Contrast)->DefaultValue;
                hue        = AfxGetMyApp()->GetColorControl(ProcAmp_Hue)->DefaultValue;
                saturation = AfxGetMyApp()->GetColorControl(ProcAmp_Saturation)->DefaultValue;
                SetColorControl(ProcAmp_All, brightness, contrast, hue, saturation);
                str.LoadString(IDS_OSD_RESET_COLOR);
                break;
        }
        m_OSD.DisplayMessage(OSD_TOPLEFT, str);
    } else {
        m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_OSD_NO_COLORCONTROL));
    }
}

void CMainFrame::OnAfterplayback(UINT nID)
{
    CAppSettings& s = AfxGetAppSettings();
    WORD osdMsg = 0;
    bool bDisable = false;

    auto toggleOption = [&](UINT64 nID) {
        bDisable = !!(s.nCLSwitches & nID);
        s.nCLSwitches &= ~CLSW_AFTERPLAYBACK_MASK | nID;
        s.nCLSwitches ^= nID;
    };

    switch (nID) {
        case ID_AFTERPLAYBACK_EXIT:
            toggleOption(CLSW_CLOSE);
            osdMsg = IDS_AFTERPLAYBACK_EXIT;
            break;
        case ID_AFTERPLAYBACK_STANDBY:
            toggleOption(CLSW_STANDBY);
            osdMsg = IDS_AFTERPLAYBACK_STANDBY;
            break;
        case ID_AFTERPLAYBACK_HIBERNATE:
            toggleOption(CLSW_HIBERNATE);
            osdMsg = IDS_AFTERPLAYBACK_HIBERNATE;
            break;
        case ID_AFTERPLAYBACK_SHUTDOWN:
            toggleOption(CLSW_SHUTDOWN);
            osdMsg = IDS_AFTERPLAYBACK_SHUTDOWN;
            break;
        case ID_AFTERPLAYBACK_LOGOFF:
            toggleOption(CLSW_LOGOFF);
            osdMsg = IDS_AFTERPLAYBACK_LOGOFF;
            break;
        case ID_AFTERPLAYBACK_LOCK:
            toggleOption(CLSW_LOCK);
            osdMsg = IDS_AFTERPLAYBACK_LOCK;
            break;
        case ID_AFTERPLAYBACK_MONITOROFF:
            toggleOption(CLSW_MONITOROFF);
            osdMsg = IDS_AFTERPLAYBACK_MONITOROFF;
            break;
        case ID_AFTERPLAYBACK_PLAYNEXT:
            toggleOption(CLSW_PLAYNEXT);
            osdMsg = IDS_AFTERPLAYBACK_PLAYNEXT;
            break;
        case ID_AFTERPLAYBACK_DONOTHING:
            toggleOption(CLSW_DONOTHING);
            osdMsg = IDS_AFTERPLAYBACK_DONOTHING;
            break;
    }
    if (bDisable) {
        switch (s.eAfterPlayback) {
            case CAppSettings::AfterPlayback::PLAY_NEXT:
                osdMsg = IDS_AFTERPLAYBACK_PLAYNEXT;
                break;
            case CAppSettings::AfterPlayback::REWIND:
                osdMsg = IDS_AFTERPLAYBACK_REWIND;
                break;
            case CAppSettings::AfterPlayback::MONITOROFF:
                osdMsg = IDS_AFTERPLAYBACK_MONITOROFF;
                break;
            case CAppSettings::AfterPlayback::CLOSE:
                osdMsg = IDS_AFTERPLAYBACK_CLOSE;
                break;
            case CAppSettings::AfterPlayback::EXIT:
                osdMsg = IDS_AFTERPLAYBACK_EXIT;
                break;
            default:
                ASSERT(FALSE);
            // no break
            case CAppSettings::AfterPlayback::DO_NOTHING:
                osdMsg = IDS_AFTERPLAYBACK_DONOTHING;
                break;
        }
    }

    m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(osdMsg));
}

void CMainFrame::OnUpdateAfterplayback(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    bool bChecked;
    bool bRadio = false;

    switch (pCmdUI->m_nID) {
        case ID_AFTERPLAYBACK_EXIT:
            bChecked = !!(s.nCLSwitches & CLSW_CLOSE);
            bRadio = s.eAfterPlayback == CAppSettings::AfterPlayback::EXIT;
            break;
        case ID_AFTERPLAYBACK_STANDBY:
            bChecked = !!(s.nCLSwitches & CLSW_STANDBY);
            break;
        case ID_AFTERPLAYBACK_HIBERNATE:
            bChecked = !!(s.nCLSwitches & CLSW_HIBERNATE);
            break;
        case ID_AFTERPLAYBACK_SHUTDOWN:
            bChecked = !!(s.nCLSwitches & CLSW_SHUTDOWN);
            break;
        case ID_AFTERPLAYBACK_LOGOFF:
            bChecked = !!(s.nCLSwitches & CLSW_LOGOFF);
            break;
        case ID_AFTERPLAYBACK_LOCK:
            bChecked = !!(s.nCLSwitches & CLSW_LOCK);
            break;
        case ID_AFTERPLAYBACK_MONITOROFF:
            bChecked = !!(s.nCLSwitches & CLSW_MONITOROFF);
            bRadio = s.eAfterPlayback == CAppSettings::AfterPlayback::MONITOROFF;
            break;
        case ID_AFTERPLAYBACK_PLAYNEXT:
            bChecked = !!(s.nCLSwitches & CLSW_PLAYNEXT);
            bRadio = s.eAfterPlayback == CAppSettings::AfterPlayback::PLAY_NEXT;
            break;
        case ID_AFTERPLAYBACK_DONOTHING:
            bChecked = !!(s.nCLSwitches & CLSW_DONOTHING);
            bRadio = s.eAfterPlayback == CAppSettings::AfterPlayback::DO_NOTHING;
            break;
        default:
            ASSERT(FALSE);
            return;
    }

    if (IsMenu(*pCmdUI->m_pMenu)) {
        MENUITEMINFO mii, cii;
        ZeroMemory(&cii, sizeof(MENUITEMINFO));
        cii.cbSize = sizeof(cii);
        cii.fMask = MIIM_FTYPE;
        pCmdUI->m_pMenu->GetMenuItemInfo(pCmdUI->m_nID, &cii);

        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_FTYPE | MIIM_STATE;
        mii.fType = (bRadio ? MFT_RADIOCHECK : 0) | (cii.fType & MFT_OWNERDRAW); //preserve owner draw flag
        mii.fState = (bRadio ? MFS_DISABLED : 0) | (bChecked || bRadio ? MFS_CHECKED : 0);
        VERIFY(CMPCThemeMenu::SetThemedMenuItemInfo(pCmdUI->m_pMenu, pCmdUI->m_nID, &mii));
    }
}

void CMainFrame::OnPlayRepeat(UINT nID)
{
    CAppSettings& s = AfxGetAppSettings();
    WORD osdMsg = 0;

    switch (nID) {
        case ID_PLAY_REPEAT_ONEFILE:
            s.eLoopMode = CAppSettings::LoopMode::FILE;
            osdMsg = IDS_PLAYLOOPMODE_FILE;
            break;
        case ID_PLAY_REPEAT_WHOLEPLAYLIST:
            s.eLoopMode = CAppSettings::LoopMode::PLAYLIST;
            osdMsg = IDS_PLAYLOOPMODE_PLAYLIST;
            break;
        default:
            ASSERT(FALSE);
            return;
    }

    m_nLoops = 0;
    m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(osdMsg));
}

void CMainFrame::OnUpdatePlayRepeat(CCmdUI* pCmdUI)
{
    CAppSettings::LoopMode loopmode;

    switch (pCmdUI->m_nID) {
        case ID_PLAY_REPEAT_ONEFILE:
            loopmode = CAppSettings::LoopMode::FILE;
            break;
        case ID_PLAY_REPEAT_WHOLEPLAYLIST:
            loopmode = CAppSettings::LoopMode::PLAYLIST;
            break;
        default:
            ASSERT(FALSE);
            return;
    }
    if (AfxGetAppSettings().eLoopMode == loopmode && pCmdUI->m_pMenu) {
        pCmdUI->m_pMenu->CheckMenuRadioItem(ID_PLAY_REPEAT_ONEFILE, ID_PLAY_REPEAT_WHOLEPLAYLIST, pCmdUI->m_nID, MF_BYCOMMAND);
    }
}

void CMainFrame::OnPlayRepeatForever()
{
    CAppSettings& s = AfxGetAppSettings();

    s.fLoopForever = !s.fLoopForever;

    m_nLoops = 0;
    m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(s.fLoopForever ? IDS_PLAYLOOP_FOREVER_ON : IDS_PLAYLOOP_FOREVER_OFF));
}

void CMainFrame::OnUpdatePlayRepeatForever(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(AfxGetAppSettings().fLoopForever);
}

bool CMainFrame::SeekToFileChapter(int iChapter, bool bRelative /*= false*/)
{
    if (GetPlaybackMode() != PM_FILE) {
        return false;
    }

    bool ret = false;

    if (DWORD nChapters = m_pCB->ChapGetCount()) {
        REFERENCE_TIME rt;

        if (bRelative) {
            if (SUCCEEDED(m_pMS->GetCurrentPosition(&rt))) {
                if (iChapter < 0) {
                    // Add a small threshold to jump back at least that amount of time
                    // This is needed when rt is near start of current chapter
                    rt -= PREV_CHAP_THRESHOLD * 10000000;
                    iChapter = 0;
                    iChapter = m_pCB->ChapLookupPrevious(&rt, nullptr);
                    // seek to start if there is no previous chapter
                    if (iChapter == -1 && rt >= 0) {
                        REFERENCE_TIME rtStart, rtStop;
                        m_wndSeekBar.GetRange(rtStart, rtStop);
                        DoSeekTo(rtStart, false);
                        return true;
                    }
                } else {
                    iChapter = m_pCB->ChapLookupNext(&rt, nullptr);
                }
            } else {
                return false;
            }
        }

        CComBSTR name;
        REFERENCE_TIME rtStart, rtStop;
        m_wndSeekBar.GetRange(rtStart, rtStop);
        if (iChapter >= 0 && DWORD(iChapter) < nChapters && SUCCEEDED(m_pCB->ChapGet(iChapter, &rt, &name)) && rt < rtStop) {
            DoSeekTo(rt, false);
            SendStatusMessage(ResStr(IDS_AG_CHAPTER2) + CString(name), 3000);
            ret = true;

            REFERENCE_TIME rtDur;
            if (SUCCEEDED(m_pMS->GetDuration(&rtDur))) {
                const CAppSettings& s = AfxGetAppSettings();
                CString strOSD;

                strOSD.Format(_T("%s%s/%s %s%d/%u - \"%s\""),
                              s.fRemainingTime ? _T("- ") : _T(""), ReftimeToString2(s.fRemainingTime ? rtDur - rt : rt).GetString(), ReftimeToString2(rtDur).GetString(),
                              ResStr(IDS_AG_CHAPTER2).GetString(), iChapter + 1, nChapters, static_cast<LPCTSTR>(name));
                m_OSD.DisplayMessage(OSD_TOPLEFT, strOSD, 3000);
            }
        }
    }

    return ret;
}

bool CMainFrame::SeekToDVDChapter(int iChapter, bool bRelative /*= false*/)
{
    if (GetPlaybackMode() != PM_DVD) {
        return false;
    }

    ULONG ulNumOfVolumes, ulVolume;
    DVD_DISC_SIDE Side;
    ULONG ulNumOfTitles = 0;
    CheckNoLogBool(m_pDVDI->GetDVDVolumeInfo(&ulNumOfVolumes, &ulVolume, &Side, &ulNumOfTitles));

    DVD_PLAYBACK_LOCATION2 Location;
    ULONG ulNumOfChapters = 0;
    ULONG uTitle = 0, uChapter = 0;
    if (bRelative) {
        CheckNoLogBool(m_pDVDI->GetCurrentLocation(&Location));

        CheckNoLogBool(m_pDVDI->GetNumberOfChapters(Location.TitleNum, &ulNumOfChapters));

        uTitle = Location.TitleNum;
        uChapter = Location.ChapterNum;

        if (iChapter < 0) {
            ULONG tsec = (Location.TimeCode.bHours * 3600)
                         + (Location.TimeCode.bMinutes * 60)
                         + (Location.TimeCode.bSeconds);
            ULONG diff = 0;
            if (m_lChapterStartTime != 0xFFFFFFFF && tsec > m_lChapterStartTime) {
                diff = tsec - m_lChapterStartTime;
            }
            // Go the previous chapter only if more than PREV_CHAP_THRESHOLD seconds
            // have passed since the beginning of the current chapter else restart it
            if (diff <= PREV_CHAP_THRESHOLD) {
                // If we are at the first chapter of a volume that isn't the first
                // one, we skip to the last chapter of the previous volume.
                if (uChapter == 1 && uTitle > 1) {
                    uTitle--;
                    CheckNoLogBool(m_pDVDI->GetNumberOfChapters(uTitle, &uChapter));
                } else if (uChapter > 1) {
                    uChapter--;
                }
            }
        } else {
            // If we are at the last chapter of a volume that isn't the last
            // one, we skip to the first chapter of the next volume.
            if (uChapter == ulNumOfChapters && uTitle < ulNumOfTitles) {
                uTitle++;
                uChapter = 1;
            } else if (uChapter < ulNumOfChapters) {
                uChapter++;
            }
        }
    } else if (iChapter > 0) {
        uChapter = ULONG(iChapter);
        if (uChapter <= ulNumOfTitles) {
            uTitle = uChapter;
            uChapter = 1;
        } else {
            CheckNoLogBool(m_pDVDI->GetCurrentLocation(&Location));
            uTitle = Location.TitleNum;
            uChapter -= ulNumOfTitles;
        }
    }

    if (uTitle && uChapter
            && SUCCEEDED(m_pDVDC->PlayChapterInTitle(uTitle, uChapter, DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, nullptr))) {
        if (SUCCEEDED(m_pDVDI->GetCurrentLocation(&Location))
                && SUCCEEDED(m_pDVDI->GetNumberOfChapters(Location.TitleNum, &ulNumOfChapters))) {
            CString strTitle;
            strTitle.Format(IDS_AG_TITLE2, Location.TitleNum, ulNumOfTitles);
            __int64 start, stop;
            m_wndSeekBar.GetRange(start, stop);

            CString strOSD;
            if (stop > 0) {
                const CAppSettings& s = AfxGetAppSettings();

                DVD_HMSF_TIMECODE currentHMSF = s.fRemainingTime ? RT2HMS_r(stop - HMSF2RT(Location.TimeCode)) : Location.TimeCode;
                DVD_HMSF_TIMECODE stopHMSF = RT2HMS_r(stop);
                strOSD.Format(_T("%s%s/%s %s, %s%02u/%02lu"),
                              s.fRemainingTime ? _T("- ") : _T(""), DVDtimeToString(currentHMSF, stopHMSF.bHours > 0).GetString(), DVDtimeToString(stopHMSF).GetString(),
                              strTitle.GetString(), ResStr(IDS_AG_CHAPTER2).GetString(), Location.ChapterNum, ulNumOfChapters);
            } else {
                strOSD.Format(_T("%s, %s%02u/%02lu"), strTitle.GetString(), ResStr(IDS_AG_CHAPTER2).GetString(), Location.ChapterNum, ulNumOfChapters);
            }

            m_OSD.DisplayMessage(OSD_TOPLEFT, strOSD, 3000);
        }

        return true;
    }

    return false;
}

// navigate
void CMainFrame::OnNavigateSkip(UINT nID)
{
    const CAppSettings& s = AfxGetAppSettings();

    if (GetPlaybackMode() == PM_FILE || CanSkipFromClosedFile()) {
        m_nLastSkipDirection = nID;

        if (!SeekToFileChapter((nID == ID_NAVIGATE_SKIPBACK) ? -1 : 1, true)) {
            if (nID == ID_NAVIGATE_SKIPBACK) {
                SendMessage(WM_COMMAND, ID_NAVIGATE_SKIPBACKFILE);
            } else if (nID == ID_NAVIGATE_SKIPFORWARD) {
                SendMessage(WM_COMMAND, ID_NAVIGATE_SKIPFORWARDFILE);
            }
        }
    } else if (GetPlaybackMode() == PM_DVD) {
        m_dSpeedRate = 1.0;

        if (GetMediaState() != State_Running) {
            SendMessage(WM_COMMAND, ID_PLAY_PLAY);
        }

        SeekToDVDChapter((nID == ID_NAVIGATE_SKIPBACK) ? -1 : 1, true);
    } else if (GetPlaybackMode() == PM_DIGITAL_CAPTURE) {
        CComQIPtr<IBDATuner> pTun = m_pGB;
        if (pTun) {
            int nCurrentChannel = s.nDVBLastChannel;

            if (nID == ID_NAVIGATE_SKIPBACK) {
                if (SUCCEEDED(SetChannel(nCurrentChannel - 1))) {
                    if (m_controls.ControlChecked(CMainFrameControls::Panel::NAVIGATION)) {
                        m_wndNavigationBar.m_navdlg.UpdatePos(nCurrentChannel - 1);
                    }
                }
            } else if (nID == ID_NAVIGATE_SKIPFORWARD) {
                if (SUCCEEDED(SetChannel(nCurrentChannel + 1))) {
                    if (m_controls.ControlChecked(CMainFrameControls::Panel::NAVIGATION)) {
                        m_wndNavigationBar.m_navdlg.UpdatePos(nCurrentChannel + 1);
                    }
                }
            }
        }
    }
}

bool CMainFrame::CanSkipFromClosedFile() {
    if (GetPlaybackMode() == PM_NONE && AfxGetAppSettings().fUseSearchInFolder) {
        if (m_wndPlaylistBar.GetCount() == 1) {
            CPlaylistItem* pli = m_wndPlaylistBar.GetCur();
            if (pli && !pli->m_fns.IsEmpty()) {
                CString in = pli->m_fns.GetHead();
                if (!(pli->m_bYoutubeDL || PathUtils::IsURL(in))) {
                    return true;
                }
            }
        } else if (m_wndPlaylistBar.GetCount() == 0 && !lastOpenFile.IsEmpty()) {
            return true;
        }
    }
    return false;
}

void CMainFrame::OnUpdateNavigateSkip(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();

    pCmdUI->Enable(
        (GetLoadState() == MLS::LOADED
        && ((GetPlaybackMode() == PM_DVD
            && m_iDVDDomain != DVD_DOMAIN_VideoManagerMenu
            && m_iDVDDomain != DVD_DOMAIN_VideoTitleSetMenu)
            || (GetPlaybackMode() == PM_FILE  && s.fUseSearchInFolder)
            || (GetPlaybackMode() == PM_FILE  && !s.fUseSearchInFolder && (m_wndPlaylistBar.GetCount() > 1 || m_pCB->ChapGetCount() > 1))
            || (GetPlaybackMode() == PM_DIGITAL_CAPTURE && !m_pDVBState->bSetChannelActive)))
        || (GetLoadState() == MLS::CLOSED && CanSkipFromClosedFile()) //to support skipping from broken file
    );
}

void CMainFrame::OnNavigateSkipFile(UINT nID)
{
    if (GetPlaybackMode() == PM_FILE || GetPlaybackMode() == PM_ANALOG_CAPTURE || CanSkipFromClosedFile()) {
        if (m_wndPlaylistBar.GetCount() == 1 || CanSkipFromClosedFile()) {
            CAppSettings& s = AfxGetAppSettings();
            if (GetPlaybackMode() == PM_ANALOG_CAPTURE || !s.fUseSearchInFolder) {
                SendMessage(WM_COMMAND, ID_PLAY_STOP); // do not remove this, unless you want a circular call with OnPlayPlay()
                SendMessage(WM_COMMAND, ID_PLAY_PLAY);
            } else {
                if (nID == ID_NAVIGATE_SKIPBACKFILE) {
                    if (!SearchInDir(false, s.bLoopFolderOnPlayNextFile)) {
                        m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_FIRST_IN_FOLDER));
                    }
                } else if (nID == ID_NAVIGATE_SKIPFORWARDFILE) {
                    if (!SearchInDir(true, s.bLoopFolderOnPlayNextFile)) {
                        m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_LAST_IN_FOLDER));
                    }
                }
            }
        } else {
            if (nID == ID_NAVIGATE_SKIPBACKFILE) {
                m_wndPlaylistBar.SetPrev();
            } else if (nID == ID_NAVIGATE_SKIPFORWARDFILE) {
                m_wndPlaylistBar.SetNext();
            }

            OpenCurPlaylistItem();
        }
    }
}

void CMainFrame::OnUpdateNavigateSkipFile(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    pCmdUI->Enable(
        (GetLoadState() == MLS::LOADED
        && ((GetPlaybackMode() == PM_FILE && (m_wndPlaylistBar.GetCount() > 1 || s.fUseSearchInFolder))
            || (GetPlaybackMode() == PM_ANALOG_CAPTURE && !m_fCapturing && m_wndPlaylistBar.GetCount() > 1)))
        || (GetLoadState() == MLS::CLOSED && CanSkipFromClosedFile())
    );
}

void CMainFrame::OnNavigateGoto()
{
    if ((GetLoadState() != MLS::LOADED) || IsD3DFullScreenMode()) {
        return;
    }

    const REFTIME atpf = GetAvgTimePerFrame();

    REFERENCE_TIME start, dur = -1;
    m_wndSeekBar.GetRange(start, dur);
    CGoToDlg dlg(m_wndSeekBar.GetPos(), dur, atpf > 0.0 ? (1.0 / atpf) : 0.0);
    if (IDOK != dlg.DoModal() || dlg.m_time < 0) {
        return;
    }

    DoSeekTo(dlg.m_time);
}

void CMainFrame::OnUpdateNavigateGoto(CCmdUI* pCmdUI)
{
    bool fEnable = false;

    if (GetLoadState() == MLS::LOADED) {
        fEnable = true;
        if (GetPlaybackMode() == PM_DVD && m_iDVDDomain != DVD_DOMAIN_Title) {
            fEnable = false;
        } else if (IsPlaybackCaptureMode()) {
            fEnable = false;
        }
    }

    pCmdUI->Enable(fEnable);
}

void CMainFrame::OnNavigateMenu(UINT nID)
{
    nID -= ID_NAVIGATE_TITLEMENU;

    if (GetLoadState() != MLS::LOADED || GetPlaybackMode() != PM_DVD) {
        return;
    }

    m_dSpeedRate = 1.0;

    if (GetMediaState() != State_Running) {
        SendMessage(WM_COMMAND, ID_PLAY_PLAY);
    }

    m_pDVDC->ShowMenu((DVD_MENU_ID)(nID + 2), DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, nullptr);
}

void CMainFrame::OnUpdateNavigateMenu(CCmdUI* pCmdUI)
{
    UINT nID = pCmdUI->m_nID - ID_NAVIGATE_TITLEMENU;
    ULONG ulUOPs;

    if (GetLoadState() != MLS::LOADED || GetPlaybackMode() != PM_DVD
            || FAILED(m_pDVDI->GetCurrentUOPS(&ulUOPs))) {
        pCmdUI->Enable(FALSE);
        return;
    }

    pCmdUI->Enable(!(ulUOPs & (UOP_FLAG_ShowMenu_Title << nID)));
}

void CMainFrame::OnNavigateJumpTo(UINT nID)
{
    if (nID < ID_NAVIGATE_JUMPTO_SUBITEM_START) {
        return;
    }

    const CAppSettings& s = AfxGetAppSettings();

    if (GetPlaybackMode() == PM_FILE) {
        int id = nID - ID_NAVIGATE_JUMPTO_SUBITEM_START;

        if (id < (int)m_MPLSPlaylist.GetCount() && m_MPLSPlaylist.GetCount() > 1) {
            POSITION pos = m_MPLSPlaylist.GetHeadPosition();
            int idx = 0;
            while (pos) {
                CHdmvClipInfo::PlaylistItem Item = m_MPLSPlaylist.GetNext(pos);
                if (idx == id) {
                    m_bIsBDPlay = true;
                    m_wndPlaylistBar.Empty();
                    CAtlList<CString> sl;
                    sl.AddTail(CString(Item.m_strFileName));
                    m_wndPlaylistBar.Append(sl, false);
                    OpenCurPlaylistItem();
                    return;
                }
                idx++;
            }
        }

        if (m_MPLSPlaylist.GetCount() > 1) {
            id -= (int)m_MPLSPlaylist.GetCount();
        }

        if (m_pCB->ChapGetCount() > 1) {
            if (SeekToFileChapter(id)) {
                return;
            }

            id -= m_pCB->ChapGetCount();
        }

        if (id >= 0 && id < m_wndPlaylistBar.GetCount() && m_wndPlaylistBar.GetSelIdx() != id) {
            m_wndPlaylistBar.SetSelIdx(id);
            OpenCurPlaylistItem();
        }
    } else if (GetPlaybackMode() == PM_DVD) {
        SeekToDVDChapter(nID - ID_NAVIGATE_JUMPTO_SUBITEM_START + 1);
    } else if (GetPlaybackMode() == PM_DIGITAL_CAPTURE) {
        CComQIPtr<IBDATuner> pTun = m_pGB;
        if (pTun) {
            int nChannel = nID - ID_NAVIGATE_JUMPTO_SUBITEM_START;

            if (s.nDVBLastChannel != nChannel) {
                if (SUCCEEDED(SetChannel(nChannel))) {
                    if (m_controls.ControlChecked(CMainFrameControls::Panel::NAVIGATION)) {
                        m_wndNavigationBar.m_navdlg.UpdatePos(nChannel);
                    }
                }
            }
        }
    }
}

void CMainFrame::OnNavigateMenuItem(UINT nID)
{
    nID -= ID_NAVIGATE_MENU_LEFT;

    if (GetPlaybackMode() == PM_DVD) {
        switch (nID) {
            case 0:
                m_pDVDC->SelectRelativeButton(DVD_Relative_Left);
                break;
            case 1:
                m_pDVDC->SelectRelativeButton(DVD_Relative_Right);
                break;
            case 2:
                m_pDVDC->SelectRelativeButton(DVD_Relative_Upper);
                break;
            case 3:
                m_pDVDC->SelectRelativeButton(DVD_Relative_Lower);
                break;
            case 4:
                if (m_iDVDDomain == DVD_DOMAIN_Title || m_iDVDDomain == DVD_DOMAIN_VideoTitleSetMenu || m_iDVDDomain == DVD_DOMAIN_VideoManagerMenu) {
                    m_pDVDC->ActivateButton();
                } else {
                    OnPlayPlay();
                }
                break;
            case 5:
                m_pDVDC->ReturnFromSubmenu(DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, nullptr);
                break;
            case 6:
                m_pDVDC->Resume(DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, nullptr);
                break;
            default:
                break;
        }
    } else if (GetPlaybackMode() == PM_FILE) {
        OnPlayPlay();
    }
}

void CMainFrame::OnUpdateNavigateMenuItem(CCmdUI* pCmdUI)
{
    pCmdUI->Enable((GetLoadState() == MLS::LOADED) && ((GetPlaybackMode() == PM_DVD) || (GetPlaybackMode() == PM_FILE)));
}

void CMainFrame::OnTunerScan()
{
    m_bScanDlgOpened = true;
    CTunerScanDlg dlg(this);
    dlg.DoModal();
    m_bScanDlgOpened = false;
}

void CMainFrame::OnUpdateTunerScan(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(GetLoadState() == MLS::LOADED && GetPlaybackMode() == PM_DIGITAL_CAPTURE);
}

// favorites

class CDVDStateStream : public CUnknown, public IStream
{
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv) {
        return
            QI(IStream)
            CUnknown::NonDelegatingQueryInterface(riid, ppv);
    }

    size_t m_pos;

public:
    CDVDStateStream() : CUnknown(NAME("CDVDStateStream"), nullptr) {
        m_pos = 0;
    }

    DECLARE_IUNKNOWN;

    CAtlArray<BYTE> m_data;

    // ISequentialStream
    STDMETHODIMP Read(void* pv, ULONG cb, ULONG* pcbRead) {
        size_t cbRead = std::min(m_data.GetCount() - m_pos, size_t(cb));
        cbRead = std::max(cbRead, size_t(0));
        if (cbRead) {
            memcpy(pv, &m_data[m_pos], cbRead);
        }
        if (pcbRead) {
            *pcbRead = (ULONG)cbRead;
        }
        m_pos += cbRead;
        return S_OK;
    }
    STDMETHODIMP Write(const void* pv, ULONG cb, ULONG* pcbWritten) {
        BYTE* p = (BYTE*)pv;
        ULONG cbWritten = (ULONG) - 1;
        while (++cbWritten < cb) {
            m_data.Add(*p++);
        }
        if (pcbWritten) {
            *pcbWritten = cbWritten;
        }
        return S_OK;
    }

    // IStream
    STDMETHODIMP Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition) {
        return E_NOTIMPL;
    }

    STDMETHODIMP SetSize(ULARGE_INTEGER libNewSize) {
        return E_NOTIMPL;
    }

    STDMETHODIMP CopyTo(IStream* pstm, ULARGE_INTEGER cb, ULARGE_INTEGER* pcbRead, ULARGE_INTEGER* pcbWritten) {
        return E_NOTIMPL;
    }

    STDMETHODIMP Commit(DWORD grfCommitFlags) {
        return E_NOTIMPL;
    }

    STDMETHODIMP Revert() {
        return E_NOTIMPL;
    }

    STDMETHODIMP LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) {
        return E_NOTIMPL;
    }

    STDMETHODIMP UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) {
        return E_NOTIMPL;
    }

    STDMETHODIMP Stat(STATSTG* pstatstg, DWORD grfStatFlag) {
        return E_NOTIMPL;
    }

    STDMETHODIMP Clone(IStream** ppstm) {
        return E_NOTIMPL;
    }
};

void CMainFrame::AddFavorite(bool fDisplayMessage, bool fShowDialog)
{
    CAppSettings& s = AfxGetAppSettings();
    CAtlList<CString> args;
    WORD osdMsg = 0;
    const TCHAR sep = _T(';');

    if (GetPlaybackMode() == PM_FILE) {
        bool is_BD = false;
        CString fn = m_wndPlaylistBar.GetCurFileName();
        if (fn.IsEmpty()) {
            BeginEnumFilters(m_pGB, pEF, pBF) {
                CComQIPtr<IFileSourceFilter> pFSF = pBF;
                if (pFSF) {
                    CComHeapPtr<OLECHAR> pFN;
                    AM_MEDIA_TYPE mt;
                    if (SUCCEEDED(pFSF->GetCurFile(&pFN, &mt)) && pFN && *pFN) {
                        fn = CStringW(pFN);
                    }
                    break;
                }
            }
            EndEnumFilters;
            if (fn.IsEmpty()) {
                return;
            }
            is_BD = true;
        }

        CString desc = GetFileName();

        // Name
        CString name;
        if (fShowDialog) {
            CFavoriteAddDlg dlg(desc, fn);
            if (dlg.DoModal() != IDOK) {
                return;
            }
            name = dlg.m_name;
        } else {
            name = desc;
        }
        args.AddTail(name);

        // RememberPos
        CString posStr = _T("0");
        if (s.bFavRememberPos) {
            posStr.Format(_T("%I64d"), GetPos());
        }
        args.AddTail(posStr);

        // RelativeDrive
        CString relativeDrive;
        relativeDrive.Format(_T("%d"), s.bFavRelativeDrive);

        args.AddTail(relativeDrive);

        // Paths
        if (is_BD) {
            args.AddTail(fn);
        } else {
            CPlaylistItem pli;
            if (m_wndPlaylistBar.GetCur(pli)) {
                if (pli.m_bYoutubeDL) {
                    args.AddTail(pli.m_ydlSourceURL);
                } else {
                    POSITION pos = pli.m_fns.GetHeadPosition();
                    while (pos) {
                        args.AddTail(pli.m_fns.GetNext(pos));
                    }
                }
            }
        }

        CString str = ImplodeEsc(args, sep);
        s.AddFav(FAV_FILE, str);
        osdMsg = IDS_FILE_FAV_ADDED;
    } else if (GetPlaybackMode() == PM_DVD) {
        WCHAR path[MAX_PATH];
        ULONG len = 0;
        if (SUCCEEDED(m_pDVDI->GetDVDDirectory(path, MAX_PATH, &len))) {
            CString fn = path;
            fn.TrimRight(_T("/\\"));

            DVD_PLAYBACK_LOCATION2 Location;
            CString desc;
            if (SUCCEEDED(m_pDVDI->GetCurrentLocation(&Location))) {
                desc.Format(_T("%s - T%02u C%02u - %02u:%02u:%02u"), fn.GetString(), Location.TitleNum, Location.ChapterNum,
                            Location.TimeCode.bHours, Location.TimeCode.bMinutes, Location.TimeCode.bSeconds);
            } else {
                desc = fn;
            }
            // Name
            CString name;
            if (fShowDialog) {
                CFavoriteAddDlg dlg(fn, desc);
                if (dlg.DoModal() != IDOK) {
                    return;
                }
                name = dlg.m_name;
            } else {
                name = s.bFavRememberPos ? desc : fn;
            }
            args.AddTail(name);

            // RememberPos
            CString pos(_T("0"));
            if (s.bFavRememberPos) {
                CDVDStateStream stream;
                stream.AddRef();

                CComPtr<IDvdState> pStateData;
                CComQIPtr<IPersistStream> pPersistStream;
                if (SUCCEEDED(m_pDVDI->GetState(&pStateData))
                        && (pPersistStream = pStateData)
                        && SUCCEEDED(OleSaveToStream(pPersistStream, (IStream*)&stream))) {
                    pos = BinToCString(stream.m_data.GetData(), stream.m_data.GetCount());
                }
            }

            args.AddTail(pos);

            // Paths
            args.AddTail(fn);

            CString str = ImplodeEsc(args, sep);
            s.AddFav(FAV_DVD, str);
            osdMsg = IDS_DVD_FAV_ADDED;
        }
    } // TODO: PM_ANALOG_CAPTURE and PM_DIGITAL_CAPTURE

    if (fDisplayMessage && osdMsg) {
        CString osdMsgStr(StrRes(osdMsg));
        SendStatusMessage(osdMsgStr, 3000);
        m_OSD.DisplayMessage(OSD_TOPLEFT, osdMsgStr, 3000);
    }
}

void CMainFrame::OnFavoritesAdd()
{
    AddFavorite();
}

void CMainFrame::OnUpdateFavoritesAdd(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(GetPlaybackMode() == PM_FILE || GetPlaybackMode() == PM_DVD);
}

void CMainFrame::OnFavoritesQuickAddFavorite()
{
    AddFavorite(true, false);
}

void CMainFrame::OnFavoritesOrganize()
{
    CFavoriteOrganizeDlg dlg;
    dlg.DoModal();
}

void CMainFrame::OnUpdateFavoritesOrganize(CCmdUI* pCmdUI)
{
    const CAppSettings& s = AfxGetAppSettings();
    CAtlList<CString> sl;
    s.GetFav(FAV_FILE, sl);
    bool enable = !sl.IsEmpty();

    if (!enable) {
        s.GetFav(FAV_DVD, sl);
        enable = !sl.IsEmpty();
    }

    pCmdUI->Enable(enable);
}

void CMainFrame::OnRecentFileClear()
{
    if (IDYES != AfxMessageBox(IDS_RECENT_FILES_QUESTION, MB_ICONQUESTION | MB_YESNO, 0)) {
        return;
    }

    CAppSettings& s = AfxGetAppSettings();

    // Empty MPC-HC's recent menu (iterating reverse because the indexes change)
    for (int i = s.MRU.GetSize() - 1; i >= 0; i--) {
        s.MRU.Remove(i);
    }
    for (int i = s.MRUDub.GetSize() - 1; i >= 0; i--) {
        s.MRUDub.Remove(i);
    }
    s.MRU.WriteList();
    s.MRUDub.WriteList();

    // Empty the "Recent" jump list
    CComPtr<IApplicationDestinations> pDests;
    HRESULT hr = pDests.CoCreateInstance(CLSID_ApplicationDestinations, nullptr, CLSCTX_INPROC_SERVER);
    if (SUCCEEDED(hr)) {
        pDests->RemoveAllDestinations();
    }

    // Remove the saved positions in media
    s.filePositions.Empty();
    s.dvdPositions.Empty();
}

void CMainFrame::OnUpdateRecentFileClear(CCmdUI* pCmdUI)
{
    // TODO: Add your command update UI handler code here
}

void CMainFrame::OnFavoritesFile(UINT nID)
{
    nID -= ID_FAVORITES_FILE_START;
    CAtlList<CString> sl;
    AfxGetAppSettings().GetFav(FAV_FILE, sl);

    if (POSITION pos = sl.FindIndex(nID)) {
        PlayFavoriteFile(sl.GetAt(pos));
    }
}

void CMainFrame::PlayFavoriteFile(CString fav)
{
    CAtlList<CString> args;
    REFERENCE_TIME rtStart = 0;
    BOOL bRelativeDrive = FALSE;

    ExplodeEsc(fav, args, _T(';'));
    args.RemoveHeadNoReturn(); // desc / name
    _stscanf_s(args.RemoveHead(), _T("%I64d"), &rtStart);    // pos
    _stscanf_s(args.RemoveHead(), _T("%d"), &bRelativeDrive);    // relative drive
    rtStart = std::max(rtStart, 0ll);

    // NOTE: This is just for the favorites but we could add a global settings that does this always when on. Could be useful when using removable devices.
    //       All you have to do then is plug in your 500 gb drive, full with movies and/or music, start MPC-HC (from the 500 gb drive) with a preloaded playlist and press play.
    if (bRelativeDrive) {
        // Get the drive MPC-HC is on and apply it to the path list
        CString exePath = PathUtils::GetProgramPath(true);

        CPath exeDrive(exePath);

        if (exeDrive.StripToRoot()) {
            POSITION pos = args.GetHeadPosition();

            while (pos != nullptr) {
                CString& stringPath = args.GetNext(pos);
                CPath path(stringPath);

                int rootLength = path.SkipRoot();

                if (path.StripToRoot()) {
                    if (_tcsicmp(exeDrive, path) != 0) {   // Do we need to replace the drive letter ?
                        // Replace drive letter
                        CString newPath(exeDrive);

                        newPath += stringPath.Mid(rootLength);  //newPath += stringPath.Mid( 3 );

                        stringPath = newPath;
                    }
                }
            }
        }
    }

    SendMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);

    if (!m_wndPlaylistBar.SelectFileInPlaylist(args.GetHead()) &&
        (!CanSendToYoutubeDL(args.GetHead()) ||
         !ProcessYoutubeDLURL(args.GetHead(), false))) {
        m_wndPlaylistBar.Open(args, false);
    }

    if (GetPlaybackMode() == PM_FILE && args.GetHead() == m_lastOMD->title) {
        m_pMS->SetPositions(&rtStart, AM_SEEKING_AbsolutePositioning, nullptr, AM_SEEKING_NoPositioning);
        OnPlayPlay();
    } else {
        OpenCurPlaylistItem(rtStart);
    }
}

void CMainFrame::OnUpdateFavoritesFile(CCmdUI* pCmdUI)
{
    //UINT nID = pCmdUI->m_nID - ID_FAVORITES_FILE_START;
}

void CMainFrame::OnRecentFile(UINT nID)
{
    CAtlList<CString> fns;
    auto& MRU = AfxGetAppSettings().MRU;
    MRU.ReadList();
    RecentFileEntry r;

    // find corresponding item in MRU list, we can't directly use string from menu because it may have been shortened
    nID -= ID_RECENT_FILE_START;
    for (int i = 0; i < MRU.GetSize(); i++) {
        if (MRU[i].fns.GetCount() > 0 && !MRU[i].fns.GetHead().IsEmpty()) {
            if (nID > 0) {
                nID--;
            }
            else {
                r = MRU[i];
                fns.AddHeadList(&r.fns);
                break;
            }
        }
    }
    if (fns.GetCount() < 1 || fns.GetHead().IsEmpty()) {
        ASSERT(false);
        return;
    }

    if (fns.GetCount() == 1 && CanSendToYoutubeDL(r.fns.GetHead())) {
        SendMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);
        if (ProcessYoutubeDLURL(fns.GetHead(), false)) {
            OpenCurPlaylistItem();
            return;
        } else if (IsOnYDLWhitelist(fns.GetHead())) {
            // don't bother trying to open this website URL directly
            return;
        }
    }

    CAtlList<CString> fnsl;
    fnsl.AddHeadList(&fns);
    CAtlList<CString> subs;
    subs.AddHeadList(&r.subs);

    if (!m_wndPlaylistBar.SelectFileInPlaylist(fns.GetHead())) {
        m_wndPlaylistBar.Open(fnsl, false, &subs, r.title, _T(""), r.cue);
    }
    else {
        m_wndPlaylistBar.ReplaceCurrentItem(fnsl, &subs, r.title, _T(""), r.cue);
    }

    OpenCurPlaylistItem();
}

void CMainFrame::OnUpdateRecentFile(CCmdUI* pCmdUI)
{
    //UINT nID = pCmdUI->m_nID - ID_RECENT_FILE_START;
}

void CMainFrame::OnFavoritesDVD(UINT nID)
{
    nID -= ID_FAVORITES_DVD_START;

    CAtlList<CString> sl;
    AfxGetAppSettings().GetFav(FAV_DVD, sl);

    if (POSITION pos = sl.FindIndex(nID)) {
        PlayFavoriteDVD(sl.GetAt(pos));
    }
}

void CMainFrame::PlayFavoriteDVD(CString fav)
{
    CAtlList<CString> args;
    CString fn;
    CDVDStateStream stream;

    stream.AddRef();

    ExplodeEsc(fav, args, _T(';'), 3);
    args.RemoveHeadNoReturn(); // desc / name
    CString state = args.RemoveHead(); // state
    if (state != _T("0")) {
        CStringToBin(state, stream.m_data);
    }
    fn = args.RemoveHead(); // path

    SendMessage(WM_COMMAND, ID_FILE_CLOSEMEDIA);

    CComPtr<IDvdState> pDvdState;
    HRESULT hr = OleLoadFromStream((IStream*)&stream, IID_PPV_ARGS(&pDvdState));
    UNREFERENCED_PARAMETER(hr);

    CAutoPtr<OpenDVDData> p(DEBUG_NEW OpenDVDData());
    if (p) {
        p->path = fn;
        p->pDvdState = pDvdState;
    }
    OpenMedia(p);
}

void CMainFrame::OnUpdateFavoritesDVD(CCmdUI* pCmdUI)
{
    //UINT nID = pCmdUI->m_nID - ID_FAVORITES_DVD_START;
}

void CMainFrame::OnFavoritesDevice(UINT nID)
{
    //nID -= ID_FAVORITES_DEVICE_START;
}

void CMainFrame::OnUpdateFavoritesDevice(CCmdUI* pCmdUI)
{
    //UINT nID = pCmdUI->m_nID - ID_FAVORITES_DEVICE_START;
}

// help

void CMainFrame::OnHelpHomepage()
{
    ShellExecute(m_hWnd, _T("open"), WEBSITE_URL, nullptr, nullptr, SW_SHOWDEFAULT);
}

void CMainFrame::OnHelpCheckForUpdate()
{
    UpdateChecker::CheckForUpdate();
}

void CMainFrame::OnHelpToolbarImages()
{
    ShellExecute(m_hWnd, _T("open"), _T("https://github.com/clsid2/mpc-hc/issues/382"), nullptr, nullptr, SW_SHOWDEFAULT);
}

void CMainFrame::OnHelpDonate()
{
    ShellExecute(m_hWnd, _T("open"), _T("https://github.com/clsid2/mpc-hc/issues/383"), nullptr, nullptr, SW_SHOWDEFAULT);
}

//////////////////////////////////

static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
    CAtlArray<HMONITOR>* ml = (CAtlArray<HMONITOR>*)dwData;
    ml->Add(hMonitor);
    return TRUE;
}

void CMainFrame::SetDefaultWindowRect(int iMonitor)
{
    const CAppSettings& s = AfxGetAppSettings();
    const CRect rcLastWindowPos = s.rcLastWindowPos;

    if (s.eCaptionMenuMode != MODE_SHOWCAPTIONMENU) {
        if (s.eCaptionMenuMode == MODE_FRAMEONLY) {
            ModifyStyle(WS_CAPTION, 0, SWP_NOZORDER);
        } else if (s.eCaptionMenuMode == MODE_BORDERLESS) {
            ModifyStyle(WS_CAPTION | WS_THICKFRAME, 0, SWP_NOZORDER);
        }
        SetMenuBarVisibility(AFX_MBV_DISPLAYONFOCUS);
        SetWindowPos(nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    CSize windowSize;
    if (s.HasFixedWindowSize()) {
        windowSize = s.sizeFixedWindow;
    } else if (s.fRememberWindowSize) {
        windowSize = rcLastWindowPos.Size();
    } else {
        CRect windowRect;
        GetWindowRect(&windowRect);
        CRect clientRect;
        GetClientRect(&clientRect);

        CSize logoSize = m_wndView.GetLogoSize();
        logoSize.cx = std::max<LONG>(logoSize.cx, m_dpi.ScaleX(MIN_LOGO_WIDTH));
        logoSize.cy = std::max<LONG>(logoSize.cy, m_dpi.ScaleY(MIN_LOGO_HEIGHT));

        unsigned uTop, uLeft, uRight, uBottom;
        m_controls.GetDockZones(uTop, uLeft, uRight, uBottom);

        windowSize.cx = windowRect.Width() - clientRect.Width() + logoSize.cx + uLeft + uRight;
        windowSize.cy = windowRect.Height() - clientRect.Height() + logoSize.cy + uTop + uBottom;
    }

    CMonitors monitors;
    CMonitor monitor;
    if (iMonitor > 0 && iMonitor <= monitors.GetCount()) {
        monitor = monitors.GetMonitor(iMonitor - 1);
    } else {
        monitor = CMonitors::GetNearestMonitor(this);
    }

    bool bRestoredWindowPosition = false;
    if (s.fRememberWindowPos) {
        CRect windowRect(rcLastWindowPos.TopLeft(), windowSize);
        if ((!iMonitor && CMonitors::IsOnScreen(windowRect))
                || (iMonitor && monitor.IsOnMonitor(windowRect))) {
            restoringWindowRect = true;
            MoveWindow(windowRect);
            restoringWindowRect = false;
            bRestoredWindowPosition = true;
        }
    }

    if (!bRestoredWindowPosition) {
        MINMAXINFO mmi;
        OnGetMinMaxInfo(&mmi);
        CRect windowRect(0, 0, std::max(windowSize.cx, mmi.ptMinTrackSize.x), std::max(windowSize.cy, mmi.ptMinTrackSize.y));
        monitor.CenterRectToMonitor(windowRect, TRUE);
        SetWindowPos(nullptr, windowRect.left, windowRect.top, windowSize.cx, windowSize.cy, SWP_NOZORDER | SWP_NOACTIVATE);
    }

    if (s.fSavePnSZoom) {
        m_ZoomX = s.dZoomX;
        m_ZoomY = s.dZoomY;
    }
}

void CMainFrame::SetDefaultFullscreenState()
{
    CAppSettings& s = AfxGetAppSettings();

    bool clGoFullscreen = !(s.nCLSwitches & CLSW_ADD) && (s.nCLSwitches & CLSW_FULLSCREEN);

    if (clGoFullscreen && !s.slFiles.IsEmpty()) {
        // ignore fullscreen if all files are audio
        clGoFullscreen = false;
        const CMediaFormats& mf = AfxGetAppSettings().m_Formats;
        POSITION pos = s.slFiles.GetHeadPosition();
        while (pos) {
            CString fpath = s.slFiles.GetNext(pos);
            CString ext = fpath.Mid(fpath.ReverseFind('.') + 1);
            if (!mf.FindExt(ext, true)) {
                clGoFullscreen = true;
                break;
            }
        }
    }

    if (clGoFullscreen) {
        if (s.IsD3DFullscreen()) {
            m_fStartInD3DFullscreen = true;
        } else {
            ToggleFullscreen(true, true);
            m_fFirstFSAfterLaunchOnFS = true;
        }
        s.nCLSwitches &= ~CLSW_FULLSCREEN;
    } else if (s.fRememberWindowSize && s.fRememberWindowPos && !m_fFullScreen && s.fLastFullScreen) {
        // Casimir666 : if fullscreen was on, put it on back
        if (s.IsD3DFullscreen()) {
            m_fStartInD3DFullscreen = true;
        } else {
            ToggleFullscreen(true, true);
            m_fFirstFSAfterLaunchOnFS = true;
        }
    }
}

void CMainFrame::RestoreDefaultWindowRect()
{
    const CAppSettings& s = AfxGetAppSettings();

    if (!m_fFullScreen && !IsZoomed() && !IsIconic() && !IsAeroSnapped()) {
        CSize windowSize;

        if (s.HasFixedWindowSize()) {
            windowSize = s.sizeFixedWindow;
        } else if (s.fRememberWindowSize) {
            windowSize = s.rcLastWindowPos.Size();
        } else {
            CRect windowRect;
            GetWindowRect(&windowRect);
            CRect clientRect;
            GetClientRect(&clientRect);

            CSize logoSize = m_wndView.GetLogoSize();
            logoSize.cx = std::max<LONG>(logoSize.cx, m_dpi.ScaleX(MIN_LOGO_WIDTH));
            logoSize.cy = std::max<LONG>(logoSize.cy, m_dpi.ScaleY(MIN_LOGO_HEIGHT));

            unsigned uTop, uLeft, uRight, uBottom;
            m_controls.GetDockZones(uTop, uLeft, uRight, uBottom);

            windowSize.cx = windowRect.Width() - clientRect.Width() + logoSize.cx + uLeft + uRight;
            windowSize.cy = windowRect.Height() - clientRect.Height() + logoSize.cy + uTop + uBottom;
        }

        if (s.fRememberWindowPos) {
            MoveWindow(CRect(s.rcLastWindowPos.TopLeft(), windowSize));
        } else {
            SetWindowPos(nullptr, 0, 0, windowSize.cx, windowSize.cy, SWP_NOMOVE | SWP_NOZORDER);
            CenterWindow();
        }
    }
}

CRect CMainFrame::GetInvisibleBorderSize() const
{
    CRect invisibleBorders;

    if (IsWindows10OrGreater()) {
        static const WinapiFunc<decltype(DwmGetWindowAttribute)>
        fnDwmGetWindowAttribute = { _T("Dwmapi.dll"), "DwmGetWindowAttribute" };

        if (fnDwmGetWindowAttribute) {
            if (SUCCEEDED(fnDwmGetWindowAttribute(GetSafeHwnd(), DWMWA_EXTENDED_FRAME_BOUNDS, &invisibleBorders, sizeof(RECT)))) {
                CRect windowRect;
                GetWindowRect(windowRect);

                invisibleBorders.TopLeft() = invisibleBorders.TopLeft() - windowRect.TopLeft();
                invisibleBorders.BottomRight() = windowRect.BottomRight() - invisibleBorders.BottomRight();
            } else {
                ASSERT(false);
            }
        }
    }

    return invisibleBorders;
}

OAFilterState CMainFrame::GetMediaState() const
{
    OAFilterState ret = -1;
    if (GetLoadState() == MLS::LOADED) {
        m_pMC->GetState(0, &ret);
    }
    return ret;
}

void CMainFrame::SetPlaybackMode(int iNewStatus)
{
    m_iPlaybackMode = iNewStatus;
}

CSize CMainFrame::GetVideoSize() const
{
    CSize ret;
    if (GetLoadState() != MLS::LOADED || m_fAudioOnly) {
        return ret;
    }

    const CAppSettings& s = AfxGetAppSettings();
    CSize videoSize, preferedAR;

    if (m_pCAP) {
        videoSize = m_pCAP->GetVideoSize(false);
        preferedAR = m_pCAP->GetVideoSize(s.fKeepAspectRatio);
    } else if (m_pMFVDC) {
        m_pMFVDC->GetNativeVideoSize(&videoSize, &preferedAR);   // TODO : check AR !!
    } else if (m_pBV) {
        m_pBV->GetVideoSize(&videoSize.cx, &videoSize.cy);

        long arx = 0, ary = 0;
        CComQIPtr<IBasicVideo2> pBV2 = m_pBV;
        // FIXME: It can hang here, for few seconds (CPU goes to 100%), after the window have been moving over to another screen,
        // due to GetPreferredAspectRatio, if it happens before CAudioSwitcherFilter::DeliverEndFlush, it seems.
        if (pBV2 && SUCCEEDED(pBV2->GetPreferredAspectRatio(&arx, &ary)) && arx > 0 && ary > 0) {
            preferedAR.SetSize(arx, ary);
        }
    }

    if (videoSize.cx <= 0 || videoSize.cy <= 0) {
        return ret;
    }

    if (s.fKeepAspectRatio) {
        CSize overrideAR = s.GetAspectRatioOverride();
        DVD_VideoAttributes VATR;
        if ((!overrideAR.cx || !overrideAR.cy) && GetPlaybackMode() == PM_DVD
                && SUCCEEDED(m_pDVDI->GetCurrentVideoAttributes(&VATR))) {
            overrideAR.SetSize(VATR.ulAspectX, VATR.ulAspectY);
        }
        if (overrideAR.cx > 0 && overrideAR.cy > 0) {
            if (m_pMVRC && SUCCEEDED(m_pMVRC->SendCommandDouble("setArOverride", double(overrideAR.cx) / overrideAR.cy))) {
                ret = m_pCAP->GetVideoSize(false);
            } else {
                ret = CSize(MulDiv(videoSize.cy, overrideAR.cx, overrideAR.cy), videoSize.cy);
            }
        } else {
            if (m_pMVRC && SUCCEEDED(m_pMVRC->SendCommandDouble("setArOverride", 0.0))) {
                ret = m_pCAP->GetVideoSize(false);
            } else {
                ret = CSize(MulDiv(videoSize.cy, preferedAR.cx, preferedAR.cy), videoSize.cy);
            }
        }
    } else {
        CSize originalVideoSize(0, 1);
        if (m_pMVRI) {
            m_pMVRI->GetSize("originalVideoSize", &originalVideoSize);
        }
        if (m_pMVRC && SUCCEEDED(m_pMVRC->SendCommandDouble("setArOverride",
                                                            double(originalVideoSize.cx) / originalVideoSize.cy))) {
            ret = m_pCAP->GetVideoSize(false);
        } else {
            ret = videoSize;
        }
    }

    if (s.fCompMonDeskARDiff
            && s.iDSVideoRendererType != VIDRNDT_DS_EVR
            && s.iDSVideoRendererType != VIDRNDT_DS_MADVR)
        if (HDC hDC = ::GetDC(nullptr)) {
            int _HORZSIZE = GetDeviceCaps(hDC, HORZSIZE);
            int _VERTSIZE = GetDeviceCaps(hDC, VERTSIZE);
            int _HORZRES = GetDeviceCaps(hDC, HORZRES);
            int _VERTRES = GetDeviceCaps(hDC, VERTRES);

            if (_HORZSIZE > 0 && _VERTSIZE > 0 && _HORZRES > 0 && _VERTRES > 0) {
                double a = 1.0 * _HORZSIZE / _VERTSIZE;
                double b = 1.0 * _HORZRES / _VERTRES;

                if (b < a) {
                    ret.cy = (DWORD)(1.0 * ret.cy * a / b);
                } else if (a < b) {
                    ret.cx = (DWORD)(1.0 * ret.cx * b / a);
                }
            }

            ::ReleaseDC(nullptr, hDC);
        }

    return ret;
}

void CMainFrame::ToggleFullscreen(bool fToNearest, bool fSwitchScreenResWhenHasTo)
{
    if (IsD3DFullScreenMode()) {
        ASSERT(FALSE);
        return;
    }

    CAppSettings& s = AfxGetAppSettings();

    if (delayingFullScreen) {
        return; //swallow request if we are in the delay period
    }

    CMonitor currentMonitor = CMonitors::GetNearestMonitor(this);
    const CWnd* pInsertAfter = nullptr;
    CRect windowRect;
    DWORD dwRemove = 0, dwAdd = 0;

    if (s.iFullscreenDelay > 0 && IsWindows8OrGreater()) {//DWMWA_CLOAK not supported on 7
        BOOL setEnabled = TRUE;
        ::DwmSetWindowAttribute(m_hWnd, DWMWA_CLOAK, &setEnabled, sizeof(setEnabled));
    }

    if (!m_fFullScreen) {
        SetCursor(nullptr); // prevents cursor flickering when our window is not under the cursor
        m_eventc.FireEvent(MpcEvent::SWITCHING_TO_FULLSCREEN);

        if (s.bHidePlaylistFullScreen && m_controls.ControlChecked(CMainFrameControls::Panel::PLAYLIST)) {
            m_wndPlaylistBar.SetHiddenDueToFullscreen(true);
            ShowControlBar(&m_wndPlaylistBar, FALSE, FALSE);
        }

        GetWindowRect(&m_lastWindowRect);

        if (s.autoChangeFSMode.bEnabled && fSwitchScreenResWhenHasTo && GetPlaybackMode() != PM_NONE) {
            AutoChangeMonitorMode();
        }

        CMonitor monitor;
        if (s.strFullScreenMonitor != _T("Current")) {
            CMonitors monitors;
            for (int i = 0; i < monitors.GetCount(); i++) {
                monitor = monitors.GetMonitor(i);
                if (!monitor.IsMonitor()) {
                    continue;
                }
                CString str;
                monitor.GetName(str);
                if (s.strFullScreenMonitor == str) {
                    break;
                }
                monitor.Detach();
            }
        }
        if (!monitor.IsMonitor()) {
            monitor = currentMonitor;
        }

        dwRemove |= WS_CAPTION | WS_THICKFRAME;
        if (s.fPreventMinimize && monitor != currentMonitor) {
            dwRemove |= WS_MINIMIZEBOX;
        }

        m_bExtOnTop = !s.iOnTop && (GetExStyle() & WS_EX_TOPMOST);
        pInsertAfter = &wndTopMost;

        if (fToNearest) {
            monitor.GetMonitorRect(windowRect);
        } else {
            GetDesktopWindow()->GetWindowRect(windowRect);
        }
    } else {
        m_eventc.FireEvent(MpcEvent::SWITCHING_FROM_FULLSCREEN);

        if (s.autoChangeFSMode.bEnabled && s.autoChangeFSMode.bApplyDefaultModeAtFSExit && !s.autoChangeFSMode.modes.empty() && s.autoChangeFSMode.modes[0].bChecked) {
            SetDispMode(s.strFullScreenMonitor, s.autoChangeFSMode.modes[0].dm, s.fAudioTimeShift ? s.iAudioTimeShift : 0); // Restore default time shift
        }

        windowRect = m_lastWindowRect;
        if (!CMonitors::IsOnScreen(windowRect)) {
            currentMonitor.CenterRectToMonitor(windowRect, TRUE);
        }

        dwAdd |= WS_MINIMIZEBOX;
        if (s.eCaptionMenuMode != MODE_BORDERLESS) {
            dwAdd |= WS_THICKFRAME;
            if (s.eCaptionMenuMode != MODE_FRAMEONLY) {
                dwAdd |= WS_CAPTION;
            }
        }

        if (m_wndPlaylistBar.IsHiddenDueToFullscreen() && !m_controls.ControlChecked(CMainFrameControls::Panel::PLAYLIST)) {
            m_wndPlaylistBar.SetHiddenDueToFullscreen(false);
            m_controls.ToggleControl(CMainFrameControls::Panel::PLAYLIST);
        }

        // If MPC-HC wasn't previously set "on top" by an external tool,
        // we restore the current internal on top state. (1/2)
        if (!m_bExtOnTop) {
            pInsertAfter = &wndNoTopMost;
        }
    }

    bool bZoomVideoWindow = false;
    if (m_fFirstFSAfterLaunchOnFS) {
        ASSERT(m_fFullScreen);
        bZoomVideoWindow = s.fRememberZoomLevel;
        m_fFirstFSAfterLaunchOnFS = false;
    }

    m_fFullScreen = !m_fFullScreen;
    s.fLastFullScreen = m_fFullScreen;

    // Temporarily hide the OSD message if there is one, it will
    // be restored after. This avoid positioning problems.
    m_OSD.HideMessage(true);

    ModifyStyle(dwRemove, dwAdd, SWP_NOZORDER);
    SetWindowPos(pInsertAfter, windowRect.left, windowRect.top, windowRect.Width(), windowRect.Height(),
                 SWP_NOSENDCHANGING | SWP_FRAMECHANGED);

    // If MPC-HC wasn't previously set "on top" by an external tool,
    // we restore the current internal on top state. (2/2)
    if (!m_fFullScreen && !m_bExtOnTop) {
        SetAlwaysOnTop(s.iOnTop);
    }

    SetMenuBarVisibility((!m_fFullScreen && s.eCaptionMenuMode == MODE_SHOWCAPTIONMENU)
                         ? AFX_MBV_KEEPVISIBLE : AFX_MBV_DISPLAYONFOCUS);

    UpdateControlState(UPDATE_CONTROLS_VISIBILITY);

    if (bZoomVideoWindow) {
        ZoomVideoWindow();
    }
    MoveVideoWindow();

    m_OSD.HideMessage(false);

    if (m_fFullScreen) {
        m_eventc.FireEvent(MpcEvent::SWITCHED_TO_FULLSCREEN);
    } else {
        m_eventc.FireEvent(MpcEvent::SWITCHED_FROM_FULLSCREEN);
    }

    if (s.iFullscreenDelay > 0 && IsWindows8OrGreater()) {//DWMWA_CLOAK not supported on 7
        UINT_PTR timerID = 0;
        timerID = SetTimer(TIMER_WINDOW_FULLSCREEN, s.iFullscreenDelay, nullptr);
        if (0 == timerID) {
            BOOL setEnabled = FALSE;
            ::DwmSetWindowAttribute(m_hWnd, DWMWA_CLOAK, &setEnabled, sizeof(setEnabled));
        } else {
            delayingFullScreen = true;
        }
        RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE);
    }
}

void CMainFrame::ToggleD3DFullscreen(bool fSwitchScreenResWhenHasTo)
{
    CComQIPtr<ID3DFullscreenControl> pD3DFS;
    if (m_pMFVDC) {
        pD3DFS = m_pMFVDC;
    } else {
        pD3DFS = m_pVMRWC;
    }

    if (pD3DFS) {
        CAppSettings& s = AfxGetAppSettings();

        bool bIsFullscreen = false;
        pD3DFS->GetD3DFullscreen(&bIsFullscreen);
        s.fLastFullScreen = !bIsFullscreen;

        if (bIsFullscreen) {
            // Turn off D3D Fullscreen
            m_OSD.EnableShowSeekBar(false);
            m_OSD.EnableShowMessage(false);
            pD3DFS->SetD3DFullscreen(false);

            // Assign the windowed video frame and pass it to the relevant classes.
            m_pVideoWnd = &m_wndView;
            m_OSD.SetVideoWindow(m_pVideoWnd);
            if (m_pMFVDC) {
                m_pMFVDC->SetVideoWindow(m_pVideoWnd->m_hWnd);
            } else {
                m_pVMRWC->SetVideoClippingWindow(m_pVideoWnd->m_hWnd);
            }

            if (s.autoChangeFSMode.bEnabled && s.autoChangeFSMode.bApplyDefaultModeAtFSExit && !s.autoChangeFSMode.modes.empty() && s.autoChangeFSMode.modes[0].bChecked) {
                SetDispMode(s.strFullScreenMonitor, s.autoChangeFSMode.modes[0].dm, s.fAudioTimeShift ? s.iAudioTimeShift : 0); // Restore default time shift
            }

            // Destroy the D3D Fullscreen window and zoom the windowed video frame
            m_pFullscreenWnd->DestroyWindow();
            if (m_fFirstFSAfterLaunchOnFS) {
                if (s.fRememberZoomLevel) {
                    ZoomVideoWindow();
                }
                m_fFirstFSAfterLaunchOnFS = false;
            }
            MoveVideoWindow();
            m_OSD.EnableShowMessage(true);
        } else {
            // Set the fullscreen display mode
            if (s.autoChangeFSMode.bEnabled && fSwitchScreenResWhenHasTo) {
                AutoChangeMonitorMode();
            }

            // Create a new D3D Fullscreen window
            CreateFullScreenWindow();

            m_eventc.FireEvent(MpcEvent::SWITCHING_TO_FULLSCREEN_D3D);

            // Turn on D3D Fullscreen
            m_OSD.EnableShowSeekBar(true);
            pD3DFS->SetD3DFullscreen(true);

            // Assign the windowed video frame and pass it to the relevant classes.
            m_pVideoWnd = m_pFullscreenWnd;
            m_OSD.SetVideoWindow(m_pVideoWnd);
            if (m_pMFVDC) {
                m_pMFVDC->SetVideoWindow(m_pVideoWnd->m_hWnd);
            } else {
                m_pVMRWC->SetVideoClippingWindow(m_pVideoWnd->m_hWnd);
            }

            MoveVideoWindow();

            m_eventc.FireEvent(MpcEvent::SWITCHED_TO_FULLSCREEN_D3D);
        }
    }
}

bool CMainFrame::GetCurDispMode(const CString& displayName, DisplayMode& dm)
{
    return GetDispMode(displayName, ENUM_CURRENT_SETTINGS, dm);
}

bool CMainFrame::GetDispMode(CString displayName, int i, DisplayMode& dm)
{
    DEVMODE devmode;
    devmode.dmSize = sizeof(DEVMODE);
    if (displayName == _T("Current") || displayName.IsEmpty()) {
        CMonitor monitor = CMonitors::GetNearestMonitor(AfxGetApp()->m_pMainWnd);
        monitor.GetName(displayName);
    }

    dm.bValid = !!EnumDisplaySettings(displayName, i, &devmode);

    if (dm.bValid) {
        dm.size = CSize(devmode.dmPelsWidth, devmode.dmPelsHeight);
        dm.bpp = devmode.dmBitsPerPel;
        dm.freq = devmode.dmDisplayFrequency;
        dm.dwDisplayFlags = devmode.dmDisplayFlags;
    }

    return dm.bValid;
}

void CMainFrame::SetDispMode(CString displayName, const DisplayMode& dm, int msAudioDelay)
{
    DisplayMode dmCurrent;
    if (!GetCurDispMode(displayName, dmCurrent)) {
        return;
    }

    CComQIPtr<IAudioSwitcherFilter> pASF = FindFilter(__uuidof(CAudioSwitcherFilter), m_pGB);
    if (dm.size == dmCurrent.size && dm.bpp == dmCurrent.bpp && dm.freq == dmCurrent.freq) {
        if (pASF) {
            pASF->SetAudioTimeShift(msAudioDelay * 10000i64);
        }
        return;
    }

    DEVMODE dmScreenSettings;
    ZeroMemory(&dmScreenSettings, sizeof(dmScreenSettings));
    dmScreenSettings.dmSize = sizeof(dmScreenSettings);
    dmScreenSettings.dmPelsWidth = dm.size.cx;
    dmScreenSettings.dmPelsHeight = dm.size.cy;
    dmScreenSettings.dmBitsPerPel = dm.bpp;
    dmScreenSettings.dmDisplayFrequency = dm.freq;
    dmScreenSettings.dmDisplayFlags = dm.dwDisplayFlags;
    dmScreenSettings.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_DISPLAYFREQUENCY | DM_DISPLAYFLAGS;

    if (displayName == _T("Current") || displayName.IsEmpty()) {
        CMonitor monitor = CMonitors::GetNearestMonitor(AfxGetApp()->m_pMainWnd);
        monitor.GetName(displayName);
    }

    const auto& s = AfxGetAppSettings();
    LONG ret;

    m_eventc.FireEvent(MpcEvent::DISPLAY_MODE_AUTOCHANGING);
    if (AfxGetAppSettings().autoChangeFSMode.bRestoreResAfterProgExit) {
        ret = ChangeDisplaySettingsEx(displayName, &dmScreenSettings, nullptr, CDS_FULLSCREEN, nullptr);
    } else {
        ret = ChangeDisplaySettingsEx(displayName, &dmScreenSettings, nullptr, 0, nullptr);
    }
    m_eventc.FireEvent(MpcEvent::DISPLAY_MODE_AUTOCHANGED);

    msAudioDelay = ret == DISP_CHANGE_SUCCESSFUL ? msAudioDelay : (s.fAudioTimeShift ? s.iAudioTimeShift  : 0);
    if (pASF) {
        pASF->SetAudioTimeShift(msAudioDelay * 10000i64);
    }
}

void CMainFrame::AutoChangeMonitorMode()
{
    const CAppSettings& s = AfxGetAppSettings();
    if (s.autoChangeFSMode.modes.empty()) {
        return;
    }

    double dMediaFPS = 0.0;

    if (GetPlaybackMode() == PM_FILE) {
        REFERENCE_TIME m_rtTimePerFrame = 1;
        // if ExtractAvgTimePerFrame isn't executed then MediaFPS=10000000.0,
        // (int)(MediaFPS + 0.5)=10000000 and SetDispMode is executed to Default.
        BeginEnumFilters(m_pGB, pEF, pBF) {
            BeginEnumPins(pBF, pEP, pPin) {
                CMediaTypeEx mt;
                PIN_DIRECTION dir;
                if (SUCCEEDED(pPin->QueryDirection(&dir)) && dir == PINDIR_OUTPUT
                        && SUCCEEDED(pPin->ConnectionMediaType(&mt))) {
                    ExtractAvgTimePerFrame(&mt, m_rtTimePerFrame);
                    if (m_rtTimePerFrame == 0) {
                        m_rtTimePerFrame = 1;
                    }
                }
            }
            EndEnumPins;
        }
        EndEnumFilters;
        dMediaFPS = 10000000.0 / m_rtTimePerFrame;
    } else if (GetPlaybackMode() == PM_DVD) {
        DVD_PLAYBACK_LOCATION2 Location;
        if (m_pDVDI->GetCurrentLocation(&Location) == S_OK) {
            dMediaFPS = Location.TimeCodeFlags == DVD_TC_FLAG_25fps ? 25.0
                        : Location.TimeCodeFlags == DVD_TC_FLAG_30fps ? 30.0
                        : Location.TimeCodeFlags == DVD_TC_FLAG_DropFrame ? 29.97
                        : 25.0;
        }
    }

    for (const auto& mode : s.autoChangeFSMode.modes) {
        if (mode.bChecked && dMediaFPS >= mode.dFrameRateStart && dMediaFPS <= mode.dFrameRateStop) {
            SetDispMode(s.strFullScreenMonitor, mode.dm, mode.msAudioDelay);
            return;
        }
    }
    SetDispMode(s.strFullScreenMonitor, s.autoChangeFSMode.modes[0].dm, s.fAudioTimeShift ? s.iAudioTimeShift : 0); // Restore default time shift
}

void CMainFrame::MoveVideoWindow(bool fShowStats/* = false*/, bool bSetStoppedVideoRect/* = false*/)
{
    m_dLastVideoScaleFactor = 0;
    m_lastVideoSize.SetSize(0, 0);

    if (!m_bDelaySetOutputRect && GetLoadState() == MLS::LOADED && !m_fAudioOnly && IsWindowVisible()) {
        CRect windowRect(0, 0, 0, 0);
        CRect videoRect(0, 0, 0, 0);

        if (IsD3DFullScreenMode()) {
            m_pFullscreenWnd->GetClientRect(windowRect);
        } else {
            m_wndView.GetClientRect(windowRect);
        }

        const bool bToolbarsOnVideo = m_controls.ToolbarsCoverVideo();
        const bool bPanelsOnVideo = m_controls.PanelsCoverVideo();
        if (bPanelsOnVideo) {
            unsigned uTop, uLeft, uRight, uBottom;
            m_controls.GetVisibleDockZones(uTop, uLeft, uRight, uBottom);
            if (!bToolbarsOnVideo) {
                uBottom -= m_controls.GetVisibleToolbarsHeight();
            }
            windowRect.InflateRect(uLeft, uTop, uRight, uBottom);
        } else if (bToolbarsOnVideo) {
            windowRect.bottom += m_controls.GetVisibleToolbarsHeight();
        }

        int nCompensateForMenubar = m_bShowingFloatingMenubar && !IsD3DFullScreenMode() ? GetSystemMetrics(SM_CYMENU) : 0;
        windowRect.bottom += nCompensateForMenubar;

        OAFilterState fs = GetMediaState();
        if (fs != State_Stopped || bSetStoppedVideoRect || m_fShockwaveGraph) {
            const CSize szVideo = GetVideoSize();

            m_dLastVideoScaleFactor = std::min((double)windowRect.Size().cx / szVideo.cx,
                                               (double)windowRect.Size().cy / szVideo.cy);
            m_lastVideoSize = szVideo;

            const double dVideoAR = double(szVideo.cx) / szVideo.cy;

            // because we don't have a way to get .swf size reliably,
            // other modes don't make sense
            const dvstype iDefaultVideoSize = m_fShockwaveGraph ? DVS_STRETCH :
                                              static_cast<dvstype>(AfxGetAppSettings().iDefaultVideoSize);

            const double dWRWidth  = windowRect.Width();
            const double dWRHeight = windowRect.Height();

            double dVRWidth = dWRHeight * dVideoAR;
            double dVRHeight;

            double madVRZoomFactor = 1.0;

            switch (iDefaultVideoSize) {
                case DVS_HALF:
                    dVRWidth = szVideo.cx * 0.5;
                    dVRHeight = szVideo.cy * 0.5;
                    break;
                case DVS_NORMAL:
                    dVRWidth = szVideo.cx;
                    dVRHeight = szVideo.cy;
                    break;
                case DVS_DOUBLE:
                    dVRWidth = szVideo.cx * 2.0;
                    dVRHeight = szVideo.cy * 2.0;
                    break;
                case DVS_STRETCH:
                    dVRWidth = dWRWidth;
                    dVRHeight = dWRHeight;
                    break;
                default:
                    ASSERT(FALSE);
                // Fallback to "Touch Window From Inside" if settings were corrupted.
                case DVS_FROMINSIDE:
                case DVS_FROMOUTSIDE:
                    if ((dWRWidth < dVRWidth) != (iDefaultVideoSize == DVS_FROMOUTSIDE)) {
                        dVRWidth = dWRWidth;
                        dVRHeight = dVRWidth / dVideoAR;
                    } else {
                        dVRHeight = dWRHeight;
                    }
                    break;
                case DVS_ZOOM1:
                case DVS_ZOOM2: {
                    double scale = iDefaultVideoSize == DVS_ZOOM1 ? 1.0 / 3.0 : 2.0 / 3.0;
                    double minw = std::min(dWRWidth, dVRWidth);
                    double zoomValue = (std::max(dWRWidth, dVRWidth) - minw) * scale;
                    madVRZoomFactor = (minw + zoomValue) / minw;
                    dVRWidth = minw + zoomValue;
                    dVRHeight = dVRWidth / dVideoAR;
                    break;
                }
            }

            // Scale video frame
            double dScaledVRWidth  = m_ZoomX * dVRWidth;
            double dScaledVRHeight = m_ZoomY * dVRHeight;

            auto vertAlign = AfxGetAppSettings().iVerticalAlignVideo;
            double vertAlignOffset = 0;
            if (vertAlign == CAppSettings::verticalAlignVideoType::ALIGN_TOP) {
                vertAlignOffset = -(dWRHeight - dScaledVRHeight) / 2;
            } else if (vertAlign == CAppSettings::verticalAlignVideoType::ALIGN_BOTTOM) {
                vertAlignOffset = (dWRHeight - dScaledVRHeight) / 2;
            }

            // Position video frame
            // left and top parts are allowed to be negative
            videoRect.left   = lround(m_PosX * (dWRWidth * 3.0 - dScaledVRWidth) - dWRWidth);
            videoRect.top    = lround(m_PosY * (dWRHeight * 3.0 - dScaledVRHeight) - dWRHeight + vertAlignOffset);
            // right and bottom parts are always at picture center or beyond, so never negative
            videoRect.right  = lround(videoRect.left + dScaledVRWidth);
            videoRect.bottom = lround(videoRect.top  + dScaledVRHeight);

            ASSERT(videoRect.Width()  == lround(dScaledVRWidth));
            ASSERT(videoRect.Height() == lround(dScaledVRHeight));

            if (m_pMVRC) {
                static constexpr const LPCWSTR madVRModesMap[] = {
                    L"50%",
                    L"100%",
                    L"200%",
                    L"stretch",
                    L"touchInside",
                    L"touchOutside",
                    L"touchInside",
                    L"touchInside"
                };

                m_pMVRC->SendCommandString("setZoomMode", const_cast<LPWSTR>(madVRModesMap[iDefaultVideoSize]));
                m_pMVRC->SendCommandDouble("setZoomFactorX", madVRZoomFactor * m_ZoomX);
                m_pMVRC->SendCommandDouble("setZoomFactorY", madVRZoomFactor * m_ZoomY);
                m_pMVRC->SendCommandDouble("setZoomOffsetX", 2 * m_PosX - 1.0);
                m_pMVRC->SendCommandDouble("setZoomOffsetY", 2 * m_PosY - 1.0);
            }

            if (fShowStats) {
                CString info;
                info.Format(_T("Pos %.3f %.3f, Zoom %.3f %.3f, AR %.3f"), m_PosX, m_PosY, m_ZoomX, m_ZoomY, double(videoRect.Width()) / videoRect.Height());
                SendStatusMessage(info, 3000);
            }
        } else if (m_pMVRC) {
            m_pMVRC->SendCommandString("setZoomMode", const_cast<LPWSTR>(L"autoDetect"));
        }

        windowRect.top -= nCompensateForMenubar;
        windowRect.bottom -= nCompensateForMenubar;

        if (m_pCAP) {
            m_pCAP->SetPosition(windowRect, videoRect);
            UpdateSubAspectRatioCompensation();
        } else  {
            if (m_pBV) {
                m_pBV->SetDefaultSourcePosition();
                m_pBV->SetDestinationPosition(videoRect.left, videoRect.top, videoRect.Width(), videoRect.Height());
            }
            if (m_pVW) {
                m_pVW->SetWindowPosition(windowRect.left, windowRect.top, windowRect.Width(), windowRect.Height());
            }

            if (m_pMFVDC) {
                m_pMFVDC->SetVideoPosition(nullptr, &windowRect);
            }
        }

        m_wndView.SetVideoRect(&windowRect);
        m_OSD.SetSize(windowRect, videoRect);
    } else {
        m_wndView.SetVideoRect();
    }
}

void CMainFrame::SetPreviewVideoPosition() {
    if (m_bUseSeekPreview) {
        CPoint point;
        GetCursorPos(&point);
        m_wndSeekBar.ScreenToClient(&point);
        m_wndSeekBar.UpdateToolTipPosition(point);

        CRect wr;
        m_wndPreView.GetVideoRect(&wr);

        const CSize ws(wr.Size());
        int w = ws.cx;
        int h = ws.cy;

        const CSize arxy(GetVideoSize());
        {
            const int dh = ws.cy;
            const int dw = MulDiv(dh, arxy.cx, arxy.cy);

            int minw = dw;
            int maxw = dw;
            if (ws.cx < dw) {
                minw = ws.cx;
            } else if (ws.cx > dw) {
                maxw = ws.cx;
            }

            const float scale = 1 / 3.0f;
            w = int(minw + (maxw - minw) * scale);
            h = MulDiv(w, arxy.cy, arxy.cx);
        }

        const CPoint pos(int(m_PosX * (wr.Width() * 3 - w) - wr.Width()), int(m_PosY * (wr.Height() * 3 - h) - wr.Height()));
        const CRect vr(pos, CSize(w, h));

        if (m_pMFVDC_preview) {
            m_pMFVDC_preview->SetVideoPosition(nullptr, wr);
        }
        if (m_pVMR9C_preview) {
            m_pVMR9C_preview->SetVideoPosition(nullptr, wr);
            m_pVMR9C_preview->SetAspectRatioMode(VMR9ARMode_LetterBox);
        }

        m_pBV_preview->SetDefaultSourcePosition();
        m_pBV_preview->SetDestinationPosition(vr.left, vr.top, vr.Width(), vr.Height());
        m_pVW_preview->SetWindowPosition(wr.left, wr.top, wr.Width(), wr.Height());
    }
}

void CMainFrame::HideVideoWindow(bool fHide)
{
    CRect wr;
    if (IsD3DFullScreenMode()) {
        m_pFullscreenWnd->GetClientRect(&wr);
    } else if (!m_fFullScreen) {
        m_wndView.GetClientRect(&wr);
    } else {
        GetWindowRect(&wr);

        // this code is needed to work in fullscreen on secondary monitor
        CRect r;
        m_wndView.GetWindowRect(&r);
        wr -= r.TopLeft();
    }

    if (m_pCAP) {
        if (fHide) {
            CRect vr = CRect(0, 0, 0, 0);
            m_pCAP->SetPosition(vr, vr);    // hide
        } else {
            m_pCAP->SetPosition(wr, wr);    // show
        }
    }
    m_bLockedZoomVideoWindow = fHide;
}

CSize CMainFrame::GetZoomWindowSize(double dScale)
{
    const auto& s = AfxGetAppSettings();
    CSize ret;

    if (dScale >= 0.0 && GetLoadState() == MLS::LOADED) {
        MINMAXINFO mmi;
        OnGetMinMaxInfo(&mmi);
        CSize videoSize;

        if (m_fAudioOnly) {
            videoSize = m_wndView.GetLogoSize();

            if (videoSize.cx > videoSize.cy) {
                if (videoSize.cx > s.nCoverArtSizeLimit) {
                    videoSize.cy = MulDiv(videoSize.cy, s.nCoverArtSizeLimit, videoSize.cx);
                    videoSize.cx = s.nCoverArtSizeLimit;
                }
            } else {
                if (videoSize.cy > s.nCoverArtSizeLimit) {
                    videoSize.cx = MulDiv(videoSize.cx, s.nCoverArtSizeLimit, videoSize.cy);
                    videoSize.cy = s.nCoverArtSizeLimit;
                }
            }

            if (videoSize.cx <= 1 || videoSize.cy <= 1) {
                // Do not adjust window width if blank logo is used (1x1px) or cover-art size is limited
                // to avoid shrinking window width too much and give ability to revert pre 94dc87c behavior
                videoSize.SetSize(0, 0);
                CRect windowRect;
                GetWindowRect(windowRect);
                mmi.ptMinTrackSize.x = std::max<long>(windowRect.Width(), mmi.ptMinTrackSize.x);
            }
        } else {
            videoSize = GetVideoSize();
        }

        CSize videoTargetSize(int(videoSize.cx * dScale + 0.5), int(videoSize.cy * dScale + 0.5));

        CSize controlsSize;
        const bool bToolbarsOnVideo = m_controls.ToolbarsCoverVideo();
        const bool bPanelsOnVideo = m_controls.PanelsCoverVideo();
        if (!bPanelsOnVideo) {
            unsigned uTop, uLeft, uRight, uBottom;
            m_controls.GetDockZones(uTop, uLeft, uRight, uBottom);
            if (bToolbarsOnVideo) {
                uBottom -= m_controls.GetToolbarsHeight();
            }
            controlsSize.cx = uLeft + uRight;
            controlsSize.cy = uTop + uBottom;
        } else if (!bToolbarsOnVideo) {
            controlsSize.cy = m_controls.GetToolbarsHeight();
        }

        CRect decorationsRect;
        VERIFY(AdjustWindowRectEx(decorationsRect, GetWindowStyle(m_hWnd), s.eCaptionMenuMode == MODE_SHOWCAPTIONMENU,
                                  GetWindowExStyle(m_hWnd)));

        CRect workRect;
        CMonitors::GetNearestMonitor(this).GetWorkAreaRect(workRect);

        if (workRect.Width() && workRect.Height()) {
            // account for invisible borders on Windows 10 by allowing
            // the window to go out of screen a bit
            if (IsWindows10OrGreater()) {
                workRect.InflateRect(GetInvisibleBorderSize());
            }

            // don't go larger than the current monitor working area and prevent black bars in this case
            CSize videoSpaceSize = workRect.Size() - controlsSize - decorationsRect.Size();

            // Do not adjust window size for video frame aspect ratio when video size is independent from window size
            const bool bAdjustWindowAR = !(s.iDefaultVideoSize == DVS_HALF || s.iDefaultVideoSize == DVS_NORMAL || s.iDefaultVideoSize == DVS_DOUBLE);
            const double videoAR = videoSize.cx / (double)videoSize.cy;

            if (videoTargetSize.cx > videoSpaceSize.cx) {
                videoTargetSize.cx = videoSpaceSize.cx;
                if (bAdjustWindowAR) {
                    videoTargetSize.cy = std::lround(videoSpaceSize.cx / videoAR);
                }
            }

            if (videoTargetSize.cy > videoSpaceSize.cy) {
                videoTargetSize.cy = videoSpaceSize.cy;
                if (bAdjustWindowAR) {
                    videoTargetSize.cx = std::lround(videoSpaceSize.cy * videoAR);
                }
            }
        } else {
            ASSERT(FALSE);
        }

        ret = videoTargetSize + controlsSize + decorationsRect.Size();
        ret.cx = std::max(ret.cx, mmi.ptMinTrackSize.x);
        ret.cy = std::max(ret.cy, mmi.ptMinTrackSize.y);
    } else {
        ASSERT(FALSE);
    }

    return ret;
}

CRect CMainFrame::GetZoomWindowRect(const CSize& size)
{
    const auto& s = AfxGetAppSettings();
    CRect ret;
    GetWindowRect(ret);

    MONITORINFO mi = { sizeof(mi) };
    if (GetMonitorInfo(MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST), &mi)) {
        CRect rcWork = mi.rcWork;
        // account for invisible borders on Windows 10 by allowing
        // the window to go out of screen a bit
        if (IsWindows10OrGreater()) {
            rcWork.InflateRect(GetInvisibleBorderSize());
        }

        CSize windowSize(size);

        // don't go larger than the current monitor working area
        windowSize.cx = std::min<long>(windowSize.cx, rcWork.Width());
        windowSize.cy = std::min<long>(windowSize.cy, rcWork.Height());

        // retain snapping or try not to be move the center of the window
        // if we don't remember its position
        if (m_bWasSnapped && ret.left == rcWork.left) {
            // do nothing
        } else if (m_bWasSnapped && ret.right == rcWork.right) {
            ret.left = ret.right - windowSize.cx;
        } else if (!s.fRememberWindowPos) {
            ret.left += (ret.right - ret.left) / 2 - windowSize.cx / 2;
        }
        if (m_bWasSnapped && ret.top == rcWork.top) {
            // do nothing
        } else if (m_bWasSnapped && ret.bottom == rcWork.bottom) {
            ret.top = ret.bottom - windowSize.cy;
        } else if (!s.fRememberWindowPos) {
            ret.top += (ret.bottom - ret.top) / 2 - windowSize.cy / 2;
        }

        ret.right = ret.left + windowSize.cx;
        ret.bottom = ret.top + windowSize.cy;

        // don't go beyond the current monitor working area
        if (ret.right > rcWork.right) {
            ret.OffsetRect(rcWork.right - ret.right, 0);
        }
        if (ret.left < rcWork.left) {
            ret.OffsetRect(rcWork.left - ret.left, 0);
        }
        if (ret.bottom > rcWork.bottom) {
            ret.OffsetRect(0, rcWork.bottom - ret.bottom);
        }
        if (ret.top < rcWork.top) {
            ret.OffsetRect(0, rcWork.top - ret.top);
        }
    } else {
        ASSERT(FALSE);
    }

    return ret;
}

void CMainFrame::ZoomVideoWindow(double dScale/* = ZOOM_DEFAULT_LEVEL*/)
{
    const auto& s = AfxGetAppSettings();

    if ((GetLoadState() != MLS::LOADED) ||
            (m_nLockedZoomVideoWindow > 0) || m_bLockedZoomVideoWindow) {
        if (m_nLockedZoomVideoWindow > 0) {
            m_nLockedZoomVideoWindow--;
        }
        return;
    }

    // Leave fullscreen when changing the zoom level
    if (m_fFullScreen || IsD3DFullScreenMode()) {
        OnViewFullscreen();
    }

    if (!s.HasFixedWindowSize()) {
        ShowWindow(SW_SHOWNOACTIVATE);
        if (dScale == (double)ZOOM_DEFAULT_LEVEL) {
            if (s.fRememberWindowSize) {
                return;    // ignore default auto-zoom setting
            }
            dScale =
                s.iZoomLevel == -1 ? 0.25 :
                s.iZoomLevel == 0 ? 0.5 :
                s.iZoomLevel == 1 ? 1.0 :
                s.iZoomLevel == 2 ? 2.0 :
                s.iZoomLevel == 3 ? GetZoomAutoFitScale(false) :
                s.iZoomLevel == 4 ? GetZoomAutoFitScale(true) : 1.0;
        } else if (dScale == (double)ZOOM_AUTOFIT) {
            dScale = GetZoomAutoFitScale(false);
        } else if (dScale == (double)ZOOM_AUTOFIT_LARGER) {
            dScale = GetZoomAutoFitScale(true);
        } else if (dScale <= 0.0) {
            ASSERT(FALSE);
            return;
        }
        MoveWindow(GetZoomWindowRect(GetZoomWindowSize(dScale)));
    }
}

double CMainFrame::GetZoomAutoFitScale(bool bLargerOnly)
{
    if (GetLoadState() != MLS::LOADED || m_fAudioOnly) {
        return 1.0;
    }

    const CAppSettings& s = AfxGetAppSettings();

    CSize arxy = GetVideoSize();

    // get the work area
    MONITORINFO mi;
    mi.cbSize = sizeof(MONITORINFO);
    HMONITOR hMonitor = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
    GetMonitorInfo(hMonitor, &mi);
    RECT& wa = mi.rcWork;

    DWORD style = GetStyle();
    CSize decorationsSize(0, 0);

    if (style & WS_CAPTION) {
        // caption
        decorationsSize.cy += GetSystemMetrics(SM_CYCAPTION);
        // menu
        if (s.eCaptionMenuMode == MODE_SHOWCAPTIONMENU) {
            decorationsSize.cy += GetSystemMetrics(SM_CYMENU);
        }
    }

    if (style & WS_THICKFRAME) {
        // vertical borders
        decorationsSize.cx += 2 * ::GetSystemMetrics(SM_CXSIZEFRAME);
        // horizontal borders
        decorationsSize.cy += 2 * ::GetSystemMetrics(SM_CYSIZEFRAME);

        // account for invisible borders on Windows 10
        if (IsWindows10OrGreater()) {
            RECT invisibleBorders = GetInvisibleBorderSize();

            decorationsSize.cx -= (invisibleBorders.left + invisibleBorders.right);
            decorationsSize.cy -= (invisibleBorders.top + invisibleBorders.bottom);
        }

        if (!(style & WS_CAPTION)) {
            decorationsSize.cx -= 2;
            decorationsSize.cy -= 2;
        }
    }

    const bool bToolbarsOnVideo = m_controls.ToolbarsCoverVideo();
    const bool bPanelsOnVideo = m_controls.PanelsCoverVideo();
    if (!bPanelsOnVideo) {
        unsigned uTop, uLeft, uRight, uBottom;
        m_controls.GetDockZones(uTop, uLeft, uRight, uBottom);
        if (bToolbarsOnVideo) {
            uBottom -= m_controls.GetVisibleToolbarsHeight();
        }
        decorationsSize.cx += uLeft + uRight;
        decorationsSize.cy += uTop + uBottom;
    } else if (!bToolbarsOnVideo) {
        decorationsSize.cy -= m_controls.GetVisibleToolbarsHeight();
    }

    LONG width = wa.right - wa.left;
    LONG height = wa.bottom - wa.top;

    double sx = ((double)width  * s.nAutoFitFactor / 100 - decorationsSize.cx) / arxy.cx;
    double sy = ((double)height * s.nAutoFitFactor / 100 - decorationsSize.cy) / arxy.cy;
    sx = std::min(sx, sy);
    // Take movie aspect ratio into consideration
    // The scaling is computed so that the height is an integer value
    sy = floor(arxy.cy * floor(arxy.cx * sx + 0.5) / arxy.cx + 0.5) / arxy.cy;

    if (sy < 0.0) {
        ASSERT(FALSE);
        sy = 0.0;
    }

    if (bLargerOnly && sy >= 1.0) {
        return 1.0;
    }

    return sy;
}

void CMainFrame::RepaintVideo()
{
    if (!m_bDelaySetOutputRect && (m_pCAP || m_pMFVDC) && GetMediaState() != State_Running) {
        if (m_pCAP) {
            m_pCAP->Paint(false);
        } else if (m_pMFVDC) {
            m_pMFVDC->RepaintVideo();
        }
    }
}

ShaderC* CMainFrame::GetShader(CString path, bool bD3D11)
{
	ShaderC* pShader = nullptr;

    CString shadersDir = ShaderList::GetShadersDir();
    CString shadersDir11 = ShaderList::GetShadersDir11();
    CString tPath = path;
    tPath.Replace(shadersDir, shadersDir11);

    //if the shader exists in the Shaders11 folder, use that one
    if (bD3D11 && ::PathFileExistsW(tPath)) {
        path = tPath;
    }

	for (auto& shader : m_ShaderCache) {
		if (shader.Match(path, bD3D11)) {
			pShader = &shader;
			break;
		}
	}

	if (!pShader) {
		if (::PathFileExistsW(path)) {
			CStdioFile file;
			if (file.Open(path, CFile::modeRead | CFile::shareDenyWrite | CFile::typeText)) {
				ShaderC shader;
				shader.label = path;

				CString str;
				file.ReadString(str); // read first string
				if (str.Left(25) == L"// $MinimumShaderProfile:") {
					shader.profile = str.Mid(25).Trim(); // shader version property
				} else {
					file.SeekToBegin();
				}

                if (bD3D11) {
                    shader.profile = L"ps_4_0";
                } else if (shader.profile == L"ps_3_sw") {
					shader.profile = L"ps_3_0";
				} else if (shader.profile != L"ps_2_0"
						&& shader.profile != L"ps_2_a"
						&& shader.profile != L"ps_2_b"
						&& shader.profile != L"ps_3_0") {
					shader.profile = L"ps_2_0";
				}

				while (file.ReadString(str)) {
					shader.srcdata += str + L"\n";
				}

				shader.length = file.GetLength();

				FILETIME ftCreate, ftAccess, ftWrite;
				if (GetFileTime(file.m_hFile, &ftCreate, &ftAccess, &ftWrite)) {
					shader.ftwrite = ftWrite;
				}

				file.Close();

				m_ShaderCache.push_back(shader);
				pShader = &m_ShaderCache.back();
			}
		}
	}

	return pShader;
}

bool CMainFrame::SaveShaderFile(ShaderC* shader)
{
	CString path;
	if (AfxGetMyApp()->GetAppSavePath(path)) {
		path.AppendFormat(L"Shaders\\%s.hlsl", static_cast<LPCWSTR>(shader->label));

		CStdioFile file;
		if (file.Open(path, CFile::modeWrite  | CFile::shareExclusive | CFile::typeText)) {
			file.SetLength(0);

			CString str;
			str.Format(L"// $MinimumShaderProfile: %s\n", static_cast<LPCWSTR>(shader->profile));
			file.WriteString(static_cast<LPCWSTR>(str));

			file.WriteString(static_cast<LPCWSTR>(shader->srcdata));
			file.Close();

			// delete out-of-date data from the cache
			for (auto it = m_ShaderCache.begin(), end = m_ShaderCache.end(); it != end; ++it) {
				if (it->Match(shader->label, false)) {
					m_ShaderCache.erase(it);
					break;
				}
			}

			return true;
		}
	}
	return false;
}

bool CMainFrame::DeleteShaderFile(LPCWSTR label)
{
	CString path;
	if (AfxGetMyApp()->GetAppSavePath(path)) {
		path.AppendFormat(L"Shaders\\%s.hlsl", label);

		if (!::PathFileExistsW(path) || ::DeleteFileW(path)) {
			// if the file is missing or deleted successfully, then remove it from the cache
			for (auto it = m_ShaderCache.begin(), end = m_ShaderCache.end(); it != end; ++it) {
				if (it->Match(label, false)) {
					m_ShaderCache.erase(it);
					return true;
				}
			}
		}
	}

	return false;
}

void CMainFrame::TidyShaderCache()
{
	CString appsavepath;
	if (!AfxGetMyApp()->GetAppSavePath(appsavepath)) {
		return;
	}

	for (auto it = m_ShaderCache.cbegin(); it != m_ShaderCache.cend(); ) {
		CString path(appsavepath);
		path += L"Shaders\\";
		path += (*it).label + L".hlsl";

		CFile file;
		if (file.Open(path, CFile::modeRead | CFile::modeCreate | CFile::shareDenyNone)) {
			ULONGLONG length = file.GetLength();
			FILETIME ftCreate = {}, ftAccess = {}, ftWrite = {};
			GetFileTime(file.m_hFile, &ftCreate, &ftAccess, &ftWrite);

			file.Close();

			if ((*it).length == length && CompareFileTime(&(*it).ftwrite, &ftWrite) == 0) {
				it++;
				continue; // actual shader
			}
		}

		m_ShaderCache.erase(it++); // outdated shader
	}
}

void CMainFrame::SetShaders(bool bSetPreResize/* = true*/, bool bSetPostResize/* = true*/)
{
    if (GetLoadState() != MLS::LOADED) {
        return;
    }

    const auto& s = AfxGetAppSettings();
    bool preFailed = false, postFailed = false;

    if (m_pCAP3) { //interfaces for madVR and MPC-VR
        TidyShaderCache();
        const int PShaderMode = m_pCAP3->GetPixelShaderMode();
        if (PShaderMode != 9 && PShaderMode != 11) {
            return;
        }

        CStringList processedShaders;
        m_pCAP3->ClearPixelShaders(TARGET_FRAME);
        m_pCAP3->ClearPixelShaders(TARGET_SCREEN);
        if (bSetPreResize) {
            int preTarget;
            if (s.iDSVideoRendererType == VIDRNDT_DS_MPCVR) { //for now MPC-VR does not support pre-size shaders
                preTarget = TARGET_SCREEN;
            } else {
                preTarget = TARGET_FRAME;
            }
            for (const auto& shader : s.m_Shaders.GetCurrentPreset().GetPreResize().ExpandMultiPassShaderList()) {
                ShaderC* pShader = GetShader(shader.filePath, PShaderMode == 11);
                if (pShader) {
                    CStringW label = pShader->label;
                    if (processedShaders.Find(label)) {
                        continue;
                    }
                    CStringA profile = pShader->profile;
                    CStringA srcdata = pShader->srcdata;
                    if (FAILED(m_pCAP3->AddPixelShader(preTarget, label, profile, srcdata))) {
                        preFailed = true;
                        m_pCAP3->ClearPixelShaders(preTarget);
                        break;
                    }
                    processedShaders.AddTail(label);
                }
            }
        }
        if (bSetPostResize) {
            for (const auto& shader : s.m_Shaders.GetCurrentPreset().GetPostResize().ExpandMultiPassShaderList()) {
                ShaderC* pShader = GetShader(shader.filePath, PShaderMode == 11);
                if (pShader) {
                    CStringW label = pShader->label;
                    if (processedShaders.Find(label)) {
                        continue;
                    }
                    CStringA profile = pShader->profile;
                    CStringA srcdata = pShader->srcdata;
                    if (FAILED(m_pCAP3->AddPixelShader(TARGET_SCREEN, label, profile, srcdata))) {
                        postFailed = true;
                        m_pCAP3->ClearPixelShaders(TARGET_SCREEN);
                        break;
                    }
                    processedShaders.AddTail(label);
                }
            }
        }
    } else if (m_pCAP2) {
        // When pTarget parameter of ISubPicAllocatorPresenter2::SetPixelShader2() is nullptr,
        // internal video renderers select maximum available profile and madVR (the only external renderer that
        // supports shader part of ISubPicAllocatorPresenter2 interface) seems to ignore it altogether.
        m_pCAP2->SetPixelShader2(nullptr, nullptr, false);
        if (bSetPreResize) {
            for (const auto& shader : s.m_Shaders.GetCurrentPreset().GetPreResize().ExpandMultiPassShaderList()) {
                if (FAILED(m_pCAP2->SetPixelShader2(shader.GetCode(), nullptr, false))) {
                    preFailed = true;
                    m_pCAP2->SetPixelShader2(nullptr, nullptr, false);
                    break;
                }
            }
        }
        m_pCAP2->SetPixelShader2(nullptr, nullptr, true);
        if (bSetPostResize) {
            for (const auto& shader : s.m_Shaders.GetCurrentPreset().GetPostResize().ExpandMultiPassShaderList()) {
                if (FAILED(m_pCAP2->SetPixelShader2(shader.GetCode(), nullptr, true))) {
                    postFailed = true;
                    m_pCAP2->SetPixelShader2(nullptr, nullptr, true);
                    break;
                }
            }
        }
    } else if (m_pCAP) {
        // shouldn't happen, all known renderers that support ISubPicAllocatorPresenter interface
        // support ISubPicAllocatorPresenter2 as well, and it takes priority
        ASSERT(FALSE);
        m_pCAP->SetPixelShader(nullptr, nullptr);
        if (bSetPreResize) {
            for (const auto& shader : s.m_Shaders.GetCurrentPreset().GetPreResize().ExpandMultiPassShaderList()) {
                if (FAILED(m_pCAP->SetPixelShader(shader.GetCode(), nullptr))) {
                    preFailed = true;
                    m_pCAP->SetPixelShader(nullptr, nullptr);
                    break;
                }
            }
        }
        postFailed = !s.m_Shaders.GetCurrentPreset().GetPostResize().empty();
    }

    CString errMsg;
    if (preFailed && postFailed) {
        VERIFY(errMsg.LoadString(IDS_MAINFRM_BOTH_SHADERS_FAILED));
    } else if (preFailed) {
        VERIFY(errMsg.LoadString(IDS_MAINFRM_PRE_SHADERS_FAILED));
    } else if (postFailed) {
        VERIFY(errMsg.LoadString(IDS_MAINFRM_POST_SHADERS_FAILED));
    } else {
        return;
    }
    SendStatusMessage(errMsg, 3000);
}

void CMainFrame::SetBalance(int balance)
{
    int sign = balance > 0 ? -1 : 1; // -1: invert sign for more right channel
    int balance_dB;
    if (balance > -100 && balance < 100) {
        balance_dB = sign * (int)(100 * 20 * log10(1 - abs(balance) / 100.0f));
    } else {
        balance_dB = sign * (-10000);  // -10000: only left, 10000: only right
    }

    if (GetLoadState() == MLS::LOADED) {
        CString strBalance, strBalanceOSD;

        m_pBA->put_Balance(balance_dB);

        if (balance == 0) {
            strBalance.LoadString(IDS_BALANCE);
        } else if (balance < 0) {
            strBalance.Format(IDS_BALANCE_L, -balance);
        } else { //if (m_nBalance > 0)
            strBalance.Format(IDS_BALANCE_R, balance);
        }

        strBalanceOSD.Format(IDS_BALANCE_OSD, strBalance.GetString());
        m_OSD.DisplayMessage(OSD_TOPLEFT, strBalanceOSD);
    }
}

//
// Open/Close
//

bool CMainFrame::IsRealEngineCompatible(CString strFilename) const
{
    // Real Media engine didn't support Unicode filename (nor filenames with # characters)
    for (int i = 0; i < strFilename.GetLength(); i++) {
        WCHAR Char = strFilename[i];
        if (Char < 32 || Char > 126 || Char == 35) {
            return false;
        }
    }
    return true;
}

// Called from GraphThread
void CMainFrame::OpenCreateGraphObject(OpenMediaData* pOMD)
{
    ASSERT(m_pGB == nullptr);

    m_fCustomGraph = false;
    m_fShockwaveGraph = false;

    const CAppSettings& s = AfxGetAppSettings();

    m_pGB_preview = nullptr;
    m_bUseSeekPreview = s.fSeekPreview && ::IsWindow(m_wndPreView.m_hWnd);
    if (OpenFileData* pFileData = dynamic_cast<OpenFileData*>(pOMD)) {
        CString fn = pFileData->fns.GetHead();
        if (!fn.IsEmpty() && ((fn.Find(L"://") >= 0) || IsAudioFilename(fn))) {
            // disable seek preview for streaming data and audio files
            m_bUseSeekPreview = false;
        }
    }

    if (auto pOpenFileData = dynamic_cast<OpenFileData*>(pOMD)) {
        engine_t engine = s.m_Formats.GetEngine(pOpenFileData->fns.GetHead());

        HRESULT hr = E_FAIL;
        CComPtr<IUnknown> pUnk;

        if (engine == ShockWave) {
            pUnk = (IUnknown*)(INonDelegatingUnknown*)DEBUG_NEW DSObjects::CShockwaveGraph(m_pVideoWnd->m_hWnd, hr);
            if (!pUnk) {
                throw (UINT)IDS_AG_OUT_OF_MEMORY;
            }

            if (SUCCEEDED(hr)) {
                m_pGB = CComQIPtr<IGraphBuilder>(pUnk);
            }
            if (FAILED(hr) || !m_pGB) {
                throw (UINT)IDS_MAINFRM_77;
            }
            m_fShockwaveGraph = true;
        }

        m_fCustomGraph = m_fShockwaveGraph;

        if (!m_fCustomGraph) {
            m_pGB = DEBUG_NEW CFGManagerPlayer(_T("CFGManagerPlayer"), nullptr, m_pVideoWnd->m_hWnd);

            if (m_pGB && m_bUseSeekPreview) {
                // build graph for preview
                m_pGB_preview = DEBUG_NEW CFGManagerPlayer(L"CFGManagerPlayer", nullptr, m_wndPreView.GetVideoHWND(), true);
            }
        }
    } else if (auto pOpenDVDData = dynamic_cast<OpenDVDData*>(pOMD)) {
        m_pGB = DEBUG_NEW CFGManagerDVD(_T("CFGManagerDVD"), nullptr, m_pVideoWnd->m_hWnd);

        if (m_bUseSeekPreview) {
            CString drive = pOpenDVDData->path.Left(2);
            UINT type = GetDriveType(drive);
            if (type != DRIVE_CDROM || IsDriveVirtual(drive)) { //no preview seeking for spinning disks
                m_pGB_preview = DEBUG_NEW CFGManagerDVD(L"CFGManagerDVD", nullptr, m_wndPreView.GetVideoHWND(), true);
            }
        }
    } else if (auto pOpenDeviceData = dynamic_cast<OpenDeviceData*>(pOMD)) {
        if (s.iDefaultCaptureDevice == 1) {
            m_pGB = DEBUG_NEW CFGManagerBDA(_T("CFGManagerBDA"), nullptr, m_pVideoWnd->m_hWnd);
        } else {
            m_pGB = DEBUG_NEW CFGManagerCapture(_T("CFGManagerCapture"), nullptr, m_pVideoWnd->m_hWnd);
        }
    }

    if (!m_pGB) {
        throw (UINT)IDS_MAINFRM_80;
    }

    if (!m_pGB_preview) {
        m_bUseSeekPreview = false;
    }

    m_pGB->AddToROT();

    m_pMC = m_pGB;
    m_pME = m_pGB;
    m_pMS = m_pGB; // general
    m_pVW = m_pGB;
    m_pBV = m_pGB; // video
    m_pBA = m_pGB; // audio
    m_pFS = m_pGB;

    if (m_bUseSeekPreview) {
        m_pGB_preview->AddToROT();

        m_pMC_preview = m_pGB_preview;
        m_pME_preview = m_pGB_preview;
        m_pMS_preview = m_pGB_preview; // general

        m_pVW_preview = m_pGB_preview;
        m_pBV_preview = m_pGB_preview;

        m_pFS_preview = m_pGB_preview;
    }

    if (!(m_pMC && m_pME && m_pMS)
            || !(m_pVW && m_pBV)
            || !(m_pBA)) {
        throw (UINT)IDS_GRAPH_INTERFACES_ERROR;
    }

    if (FAILED(m_pME->SetNotifyWindow((OAHWND)m_hWnd, WM_GRAPHNOTIFY, 0))) {
        throw (UINT)IDS_GRAPH_TARGET_WND_ERROR;
    }

    m_pProv = (IUnknown*)DEBUG_NEW CKeyProvider();

    if (CComQIPtr<IObjectWithSite> pObjectWithSite = m_pGB) {
        pObjectWithSite->SetSite(m_pProv);
    }

    m_pCB = DEBUG_NEW CDSMChapterBag(nullptr, nullptr);
}

CWnd* CMainFrame::GetModalParent()
{
    const CAppSettings& s = AfxGetAppSettings();
    CWnd* pParentWnd = this;
    if (IsD3DFullScreenMode() && s.m_RenderersSettings.m_AdvRendSets.bVMR9FullscreenGUISupport) {
        pParentWnd = m_pFullscreenWnd;
    }
    return pParentWnd;
}

void CMainFrame::ShowMediaTypesDialog() {
    CAutoLock lck(&lockModalDialog);
    CComQIPtr<IGraphBuilderDeadEnd> pGBDE = m_pGB;
    if (pGBDE && pGBDE->GetCount()) {
        mediaTypesErrorDlg = DEBUG_NEW CMediaTypesDlg(pGBDE, GetModalParent());
        mediaTypesErrorDlg->DoModal();
        delete mediaTypesErrorDlg;
        mediaTypesErrorDlg = nullptr;
    }
}

HRESULT CMainFrame::PreviewWindowHide() {
    HRESULT hr = S_OK;

    if (!m_bUseSeekPreview) {
        return E_FAIL;
    }

    if (m_wndPreView.IsWindowVisible()) {
        // Disable animation
        ANIMATIONINFO AnimationInfo;
        AnimationInfo.cbSize = sizeof(ANIMATIONINFO);
        ::SystemParametersInfo(SPI_GETANIMATION, sizeof(ANIMATIONINFO), &AnimationInfo, 0);
        int WindowAnimationType = AnimationInfo.iMinAnimate;
        AnimationInfo.iMinAnimate = 0;
        ::SystemParametersInfo(SPI_SETANIMATION, sizeof(ANIMATIONINFO), &AnimationInfo, 0);

        m_wndPreView.ShowWindow(SW_HIDE);
        m_wndPreView.SetWindowPos(&wndNoTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        if (WindowExpectedOnTop()) {
            SetWindowPos(&wndTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }

        // Enable animation
        AnimationInfo.iMinAnimate = WindowAnimationType;
        ::SystemParametersInfo(SPI_SETANIMATION, sizeof(ANIMATIONINFO), &AnimationInfo, 0);

        if (m_pGB_preview) {
            m_pMC_preview->Pause();
        }
    }

    return hr;
}

HRESULT CMainFrame::PreviewWindowShow(REFERENCE_TIME rtCur2) {
    if (!CanPreviewUse()) {
        return E_FAIL;
    }

    HRESULT hr = S_OK;
    if (GetPlaybackMode() == PM_DVD && m_pDVDC_preview) {
        DVD_PLAYBACK_LOCATION2 Loc, Loc2;
        double fps = 0;

        hr = m_pDVDI->GetCurrentLocation(&Loc);
        if (FAILED(hr)) {
            return hr;
        }

        hr = m_pDVDI_preview->GetCurrentLocation(&Loc2);

        fps = Loc.TimeCodeFlags == DVD_TC_FLAG_25fps ? 25.0
            : Loc.TimeCodeFlags == DVD_TC_FLAG_30fps ? 30.0
            : Loc.TimeCodeFlags == DVD_TC_FLAG_DropFrame ? 29.97
            : 25.0;

        DVD_HMSF_TIMECODE dvdTo = RT2HMSF(rtCur2, fps);

        if (FAILED(hr) || (Loc.TitleNum != Loc2.TitleNum)) {
            hr = m_pDVDC_preview->PlayTitle(Loc.TitleNum, DVD_CMD_FLAG_Flush, nullptr);
            if (FAILED(hr)) {
                return hr;
            }
            m_pDVDC_preview->Resume(DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, nullptr);
            if (SUCCEEDED(hr)) {
                hr = m_pDVDC_preview->PlayAtTime(&dvdTo, DVD_CMD_FLAG_Flush, nullptr);
                if (FAILED(hr)) {
                    return hr;
                }
            } else {
                hr = m_pDVDC_preview->PlayChapterInTitle(Loc.TitleNum, 1, DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, nullptr);
                hr = m_pDVDC_preview->PlayAtTime(&dvdTo, DVD_CMD_FLAG_Flush, nullptr);
                if (FAILED(hr)) {
                    hr = m_pDVDC_preview->PlayAtTimeInTitle(Loc.TitleNum, &dvdTo, DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, nullptr);
                    if (FAILED(hr)) {
                        return hr;
                    }
                }
            }
        } else {
            hr = m_pDVDC_preview->PlayAtTime(&dvdTo, DVD_CMD_FLAG_Flush, nullptr);
            if (FAILED(hr)) {
                return hr;
            }
        }

        m_pDVDI_preview->GetCurrentLocation(&Loc2);

        m_pMC_preview->Run();
        Sleep(10);
        m_pMC_preview->Pause();
    } else if (GetPlaybackMode() == PM_FILE && m_pMS_preview) {
        hr = m_pMS_preview->SetPositions(&rtCur2, AM_SEEKING_AbsolutePositioning, nullptr, AM_SEEKING_NoPositioning);
    } else {
        return E_FAIL;
    }

    if (FAILED(hr)) {
        return hr;
    }

    /*
    if (GetPlaybackMode() == PM_FILE) {
        hr = pFS2 ? pFS2->Step(2, nullptr) : E_FAIL;
        if (SUCCEEDED(hr)) {
            Sleep(10);
        }
    }
    */

    if (!m_wndPreView.IsWindowVisible()) {
        m_wndPreView.SetRelativeSize(AfxGetAppSettings().iSeekPreviewSize);
        m_wndPreView.ShowWindow(SW_SHOWNOACTIVATE);
        m_wndPreView.SetWindowSize();
    }

    return hr;
}

// Called from GraphThread
void CMainFrame::OpenFile(OpenFileData* pOFD)
{
    if (pOFD->fns.IsEmpty()) {
        throw (UINT)IDS_MAINFRM_81;
    }

    CAppSettings& s = AfxGetAppSettings();

    bool bMainFile = true;

    POSITION pos = pOFD->fns.GetHeadPosition();
    while (pos) {
        CString fn = pOFD->fns.GetNext(pos);

        fn.Trim();
        if (fn.IsEmpty() && !bMainFile) {
            break;
        }
        lastOpenFile = fn; //this is only used for skipping to other files, so it may not have been "open"
        CString ext = fn.Mid(fn.ReverseFind('.'));
        if (ext == ".mpls") {
            CString fnn = PathUtils::StripPathOrUrl(fn);
            CString tempath(fn);
            tempath.Replace(fnn, _T(""));
            tempath.Replace(_T("BDMV\\PLAYLIST\\"), _T(""));
            CHdmvClipInfo clipinfo;
            m_bHasBDMeta = clipinfo.ReadMeta(tempath, m_BDMeta);
        }
        HRESULT hr = m_pGB->RenderFile(CStringW(fn), nullptr);

        if (FAILED(hr)) {
            if (bMainFile) {
                pOFD->title = fn; //we can use this later for skipping to the next file

                if (s.fReportFailedPins) {
                    ShowMediaTypesDialog();
                }

                UINT err;

                switch (hr) {
                    case E_ABORT:
                    case RFS_E_ABORT:
                        err = IDS_MAINFRM_82;
                        break;
                    case E_FAIL:
                    case E_POINTER:
                    default:
                        err = IDS_MAINFRM_83;
                        break;
                    case E_INVALIDARG:
                        err = IDS_MAINFRM_84;
                        break;
                    case E_OUTOFMEMORY:
                        err = IDS_AG_OUT_OF_MEMORY;
                        break;
                    case VFW_E_CANNOT_CONNECT:
                        err = IDS_MAINFRM_86;
                        break;
                    case VFW_E_CANNOT_LOAD_SOURCE_FILTER:
                        err = IDS_MAINFRM_87;
                        break;
                    case VFW_E_CANNOT_RENDER:
                        err = IDS_MAINFRM_88;
                        break;
                    case VFW_E_INVALID_FILE_FORMAT:
                        err = IDS_MAINFRM_89;
                        break;
                    case VFW_E_NOT_FOUND:
                        err = IDS_MAINFRM_90;
                        break;
                    case VFW_E_UNKNOWN_FILE_TYPE:
                        err = IDS_MAINFRM_91;
                        break;
                    case VFW_E_UNSUPPORTED_STREAM:
                        err = IDS_MAINFRM_92;
                        break;
                    case RFS_E_NO_FILES:
                        err = IDS_RFS_NO_FILES;
                        break;
                    case RFS_E_COMPRESSED:
                        err = IDS_RFS_COMPRESSED;
                        break;
                    case RFS_E_ENCRYPTED:
                        err = IDS_RFS_ENCRYPTED;
                        break;
                    case RFS_E_MISSING_VOLS:
                        err = IDS_RFS_MISSING_VOLS;
                        break;
                }

                throw err;
            }
        }

        if (bMainFile) {
            bool bIsVideo = false;
            bool isRFS = false;
            CStringW entryRFS;

            // Check for supported interfaces
            BeginEnumFilters(m_pGB, pEF, pBF);
            if (!m_pFSF) {
                m_pFSF = pBF;
                if (m_pFSF && m_bUseSeekPreview) {
                    CLSID clsid2;
                    if (S_OK == pBF->GetClassID(&clsid2) && clsid2 == __uuidof(CRARFileSource)) {
                        WCHAR* pFN = nullptr;
                        AM_MEDIA_TYPE mt;
                        if (SUCCEEDED(m_pFSF->GetCurFile(&pFN, &mt)) && pFN && *pFN) {
                            isRFS = true;
                            entryRFS = pFN;
                            CoTaskMemFree(pFN);
                        }
                    }
                }
            }
            if (!m_pKFI) {
                m_pKFI = pBF;
            }
            if (!m_pAMOP) {
                m_pAMOP = pBF;
            }
            if (!m_pAMMC) {
                m_pAMMC = pBF;
            }
            if (m_bUseSeekPreview && !bIsVideo && IsVideoRenderer(pBF)) {
                bIsVideo = true;
            }
            EndEnumFilters;
            if (!m_pFSF) {
                m_pFSF = m_pGB;
            }

            if (m_bUseSeekPreview && bIsVideo) {
                HRESULT previewHR;
                if (isRFS) {
                    CComPtr<CFGManager> fgm = static_cast<CFGManager*>(m_pGB_preview.p);
                    previewHR = fgm->RenderRFSFileEntry(fn, nullptr, entryRFS);
                } else {
                    previewHR = m_pGB_preview->RenderFile(fn, nullptr);
                }

                if (FAILED(previewHR)) {
                    if (m_pGB_preview) {
                        m_pMFVP_preview = nullptr;
                        m_pMFVDC_preview = nullptr;
                        m_pVMR9C_preview = nullptr;

                        m_pMC_preview.Release();
                        m_pME_preview.Release();
                        m_pMS_preview.Release();
                        m_pVW_preview.Release();
                        m_pBV_preview.Release();
                        m_pFS_preview.Release();

                        if (m_pDVDC_preview) {
                            m_pDVDC_preview.Release();
                            m_pDVDI_preview.Release();
                        }

                        m_pGB_preview->RemoveFromROT();
                        m_pGB_preview.Release();
                        m_pGB_preview = nullptr;
                    }
                    m_bUseSeekPreview = false;
                }
            }
        }

        // We don't keep track of piped inputs since that hardly makes any sense
        if (s.fKeepHistory && fn.Find(_T("pipe:")) != 0 && pOFD->bAddToRecent) {
            if (bMainFile) {
                auto* pMRU = &s.MRU;
                pMRU->ReadList();
                RecentFileEntry r;
                r.fns.AddTail(fn);
                CPlaylistItem* m_pli = m_wndPlaylistBar.GetCur();
                if (m_pli && !m_pli->m_label.IsEmpty()) {
                    if (m_pli->m_bYoutubeDL || !IsNameSimilar(m_pli->m_label, PathUtils::StripPathOrUrl(fn))) {
                        if (!m_pli->m_bYoutubeDL || fn == m_pli->m_ydlSourceURL) r.title = m_pli->m_label;
                        else {
                            CString videoName(m_pli->m_label);
                            int m = LastIndexOfCString(videoName, _T(" ("));
                            if (m > 0) videoName = m_pli->m_label.Left(m);
                            r.title = videoName;
                        }
                    }
                }
                else {
                    CString title;
                    if (m_pAMMC) {
                        CComBSTR bstr;
                        if (SUCCEEDED(m_pAMMC->get_Title(&bstr)) && bstr.Length()) {
                            title = bstr.m_str;
                            title.Trim();
                            break;
                        }
                    }
                    if (!title.IsEmpty() && !IsNameSimilar(title, PathUtils::StripPathOrUrl(fn))) r.title = title;
                }
                if (m_pli) {
                    if (!m_pli->m_bYoutubeDL && m_pli->m_fns.GetCount() > 1) {
                        r.fns.RemoveAll();
                        r.fns.AddHeadList(&m_pli->m_fns);
                    } else if (m_pli->m_bYoutubeDL) {
                        r.fns.RemoveAll();
                        r.fns.AddTail(m_pli->m_ydlSourceURL);
                    }
                    if (m_pli->m_cue) {
                        r.cue = m_pli->m_cue_filename;
                    }
                    if (m_pli->m_subs.GetCount() > 0) {
                        r.subs.AddHeadList(&m_pli->m_subs);
                    }
                }
                m_current_rfe = r;
                pMRU->Add(r);
                pMRU->WriteList();
                SHAddToRecentDocs(SHARD_PATH, fn);
            }
            else {
                CRecentFileList* pMRU = &s.MRUDub;
                pMRU->ReadList();
                pMRU->Add(fn);
                pMRU->WriteList();
                SHAddToRecentDocs(SHARD_PATH, fn);
            }
        }

        if (bMainFile) {
            pOFD->title = fn;
        }

        bMainFile = false;

        if (m_fCustomGraph) {
            break;
        }
    }

    if (s.fReportFailedPins) {
        ShowMediaTypesDialog();
    }

    SetupChapters();

    SetPlaybackMode(PM_FILE);
}

void CMainFrame::SetupChapters()
{
    // Release the old chapter bag and create a new one.
    // Due to smart pointers the old chapter bag won't
    // be deleted until all classes release it.
    m_pCB.Release();
    m_pCB = DEBUG_NEW CDSMChapterBag(nullptr, nullptr);

    CInterfaceList<IBaseFilter> pBFs;
    BeginEnumFilters(m_pGB, pEF, pBF);
    pBFs.AddTail(pBF);
    EndEnumFilters;

    POSITION pos;

    pos = pBFs.GetHeadPosition();
    while (pos && !m_pCB->ChapGetCount()) {
        IBaseFilter* pBF = pBFs.GetNext(pos);

        CComQIPtr<IDSMChapterBag> pCB = pBF;
        if (!pCB) {
            continue;
        }

        for (DWORD i = 0, cnt = pCB->ChapGetCount(); i < cnt; i++) {
            REFERENCE_TIME rt;
            CComBSTR name;
            if (SUCCEEDED(pCB->ChapGet(i, &rt, &name))) {
                m_pCB->ChapAppend(rt, name);
            }
        }
    }

    pos = pBFs.GetHeadPosition();
    while (pos && !m_pCB->ChapGetCount()) {
        IBaseFilter* pBF = pBFs.GetNext(pos);

        CComQIPtr<IChapterInfo> pCI = pBF;
        if (!pCI) {
            continue;
        }

        CHAR iso6391[3];
        ::GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, iso6391, 3);
        CStringA iso6392 = ISOLang::ISO6391To6392(iso6391);
        if (iso6392.GetLength() < 3) {
            iso6392 = "eng";
        }

        UINT cnt = pCI->GetChapterCount(CHAPTER_ROOT_ID);
        for (UINT i = 1; i <= cnt; i++) {
            UINT cid = pCI->GetChapterId(CHAPTER_ROOT_ID, i);

            ChapterElement ce;
            if (pCI->GetChapterInfo(cid, &ce)) {
                char pl[3] = {iso6392[0], iso6392[1], iso6392[2]};
                char cc[] = "  ";
                CComBSTR name;
                name.Attach(pCI->GetChapterStringInfo(cid, pl, cc));
                m_pCB->ChapAppend(ce.rtStart, name);
            }
        }
    }

    pos = pBFs.GetHeadPosition();
    while (pos && !m_pCB->ChapGetCount()) {
        IBaseFilter* pBF = pBFs.GetNext(pos);

        CComQIPtr<IAMExtendedSeeking, &IID_IAMExtendedSeeking> pES = pBF;
        if (!pES) {
            continue;
        }

        long MarkerCount = 0;
        if (SUCCEEDED(pES->get_MarkerCount(&MarkerCount))) {
            for (long i = 1; i <= MarkerCount; i++) {
                double MarkerTime = 0;
                if (SUCCEEDED(pES->GetMarkerTime(i, &MarkerTime))) {
                    CStringW name;
                    name.Format(IDS_AG_CHAPTER, i);

                    CComBSTR bstr;
                    if (S_OK == pES->GetMarkerName(i, &bstr)) {
                        name = bstr;
                    }

                    m_pCB->ChapAppend(REFERENCE_TIME(MarkerTime * 10000000), name);
                }
            }
        }
    }

    pos = pBFs.GetHeadPosition();
    while (pos && !m_pCB->ChapGetCount()) {
        IBaseFilter* pBF = pBFs.GetNext(pos);

        if (GetCLSID(pBF) != CLSID_OggSplitter) {
            continue;
        }

        BeginEnumPins(pBF, pEP, pPin) {
            if (m_pCB->ChapGetCount()) {
                break;
            }

            if (CComQIPtr<IPropertyBag> pPB = pPin) {
                for (int i = 1; ; i++) {
                    CStringW str;
                    CComVariant var;

                    var.Clear();
                    str.Format(L"CHAPTER%02d", i);
                    if (S_OK != pPB->Read(str, &var, nullptr)) {
                        break;
                    }

                    int h, m, s, ms;
                    WCHAR wc;
                    if (7 != swscanf_s(CStringW(var), L"%d%c%d%c%d%c%d",
                                       &h, &wc, 1, &m, &wc, 1, &s, &wc, 1, &ms)) {
                        break;
                    }

                    CStringW name;
                    name.Format(IDS_AG_CHAPTER, i);
                    var.Clear();
                    str += L"NAME";
                    if (S_OK == pPB->Read(str, &var, nullptr)) {
                        name = var;
                    }

                    m_pCB->ChapAppend(10000i64 * (((h * 60 + m) * 60 + s) * 1000 + ms), name);
                }
            }
        }
        EndEnumPins;
    }

    CPlaylistItem* pli = m_wndPlaylistBar.GetCur();
    if (pli && pli->m_cue) {
        SetupCueChapters(pli->m_cue_filename);
    }

    UpdateSeekbarChapterBag();
}

void CMainFrame::SetupCueChapters(CString fn) {
    CString str;
    int cue_index(-1);
    CPlaylistItem* pli = m_wndPlaylistBar.GetCur();

    CWebTextFile f(CTextFile::UTF8);
    f.SetFallbackEncoding(CTextFile::ANSI);
    if (!f.Open(fn) || !f.ReadString(str)) {
        return;
    }
    f.Seek(0, CFile::SeekPosition::begin);

    CString base;
    bool isurl = PathUtils::IsURL(fn);
    if (isurl) {
        int p = fn.Find(_T('?'));
        if (p > 0) {
            fn = fn.Left(p);
        }
        p = fn.ReverseFind(_T('/'));
        if (p > 0) {
            base = fn.Left(p + 1);
        }
    }
    else {
        CPath basefilepath(fn);
        basefilepath.RemoveFileSpec();
        basefilepath.AddBackslash();
        base = basefilepath.m_strPath;
    }

    CString title;
    CString performer;
    CAtlList<CueTrackMeta> trackl;
    CueTrackMeta track;
    int trackID(0);

    while (f.ReadString(str)) {
        str.Trim();
        if (cue_index == -1 && str.Left(5) == _T("TITLE")) {
            title = str.Mid(6).Trim(_T("\""));
        }
        else if (cue_index == -1 && str.Left(9) == _T("PERFORMER")) {
            performer = str.Mid(10).Trim(_T("\""));
        }
        else if (str.Left(4) == _T("FILE")) {
            if (str.Right(4) == _T("WAVE") || str.Right(3) == _T("MP3") || str.Right(4) == _T("AIFF")) { // We just support audio file.
                cue_index++;
            }
        }
        else if (cue_index >= 0) {
            if (str.Left(5) == _T("TRACK") && str.Right(5) == _T("AUDIO")) {
                CT2CA tmp(str.Mid(6, str.GetLength() - 12));
                const char* tmp2(tmp);
                sscanf_s(tmp2, "%d", &trackID);
                if (track.trackID != 0) {
                    trackl.AddTail(track);
                    track = CueTrackMeta();
                }
                track.trackID = trackID;
            }
            else if (str.Left(5) == _T("TITLE")) {
                track.title = str.Mid(6).Trim(_T("\""));
            }
            else if (str.Left(9) == _T("PERFORMER")) {
                track.performer = str.Mid(10).Trim(_T("\""));
            }
            else if (str.Left(5) == _T("INDEX")) {
                CT2CA tmp(str.Mid(6));
                const char* tmp2(tmp);
                int i1(0), m(0), s(0), ms(0);
                sscanf_s(tmp2, "%d %d:%d:%d", &i1, &m, &s, &ms);
                if (i1 != 0) track.time = 10000i64 * ((m * 60 + s) * 1000 + ms);
            }
        }
    }

    if (track.trackID != 0) {
        trackl.AddTail(track);
    }

    if ((cue_index == 0 && trackl.GetCount() == 1) || cue_index > 1) pli->m_cue = false; // avoid unnecessary parsing of cue again later

    if (trackl.GetCount() >= 1) {
        POSITION p = trackl.GetHeadPosition();
        bool b(true);
        do {
            if (p == trackl.GetTailPosition()) b = false;
            CueTrackMeta c(trackl.GetNext(p));
            if (cue_index == 0 || (cue_index > 0 && c.trackID == (pli->m_cue_index + 1))) {
                CString label;
                if (c.trackID != 0 && !c.title.IsEmpty()) {
                    label = c.title;
                    if (!c.performer.IsEmpty()) {
                        label += (_T(" - ") + c.performer);
                    }
                    else if (!performer.IsEmpty()) {
                        label += (_T(" - ") + performer);
                    }
                }
                REFERENCE_TIME time(c.time);
                if (cue_index > 0) time = 0; // We don't support gap.
                m_pCB->ChapAppend(time, label);
            }
        } while (b);
    }
}

void CMainFrame::SetupDVDChapters()
{
    // Release the old chapter bag and create a new one.
    // Due to smart pointers the old chapter bag won't
    // be deleted until all classes release it.
    m_pCB.Release();
    m_pCB = DEBUG_NEW CDSMChapterBag(nullptr, nullptr);

    WCHAR buff[MAX_PATH];
    ULONG len, ulNumOfChapters;
    DVD_PLAYBACK_LOCATION2 loc;

    if (m_pDVDI && SUCCEEDED(m_pDVDI->GetDVDDirectory(buff, _countof(buff), &len))
            && SUCCEEDED(m_pDVDI->GetCurrentLocation(&loc))
            && SUCCEEDED(m_pDVDI->GetNumberOfChapters(loc.TitleNum, &ulNumOfChapters))) {
        CString path;
        path.Format(L"%s\\video_ts.IFO", buff);
        ULONG VTSN, TTN;

        if (CVobFile::GetTitleInfo(path, loc.TitleNum, VTSN, TTN)) {
            path.Format(L"%s\\VTS_%02lu_0.IFO", buff, VTSN);
            CAtlList<CString> files;

            CVobFile vob;
            if (vob.Open(path, files, TTN, false)) {
                int iChaptersCount = vob.GetChaptersCount();
                if (ulNumOfChapters == (ULONG)iChaptersCount) {
                    for (int i = 0; i < iChaptersCount; i++) {
                        REFERENCE_TIME rt = vob.GetChapterOffset(i);

                        CStringW str;
                        str.Format(IDS_AG_CHAPTER, i + 1);

                        m_pCB->ChapAppend(rt, str);
                    }
                } else {
                    // Parser failed!
                    ASSERT(FALSE);
                }
                vob.Close();
            }
        }
    }

    m_pCB->ChapSort();

    UpdateSeekbarChapterBag();
}

// Called from GraphThread
void CMainFrame::OpenDVD(OpenDVDData* pODD)
{
    HRESULT hr = m_pGB->RenderFile(CStringW(pODD->path), nullptr);

    CAppSettings& s = AfxGetAppSettings();

    if (s.fReportFailedPins) {
        ShowMediaTypesDialog();
    }

    if (SUCCEEDED(hr) && m_bUseSeekPreview) {
        if (FAILED(hr = m_pGB_preview->RenderFile(pODD->path, nullptr))) {
            m_bUseSeekPreview = false;
        }
    }

    BeginEnumFilters(m_pGB, pEF, pBF) {
        if ((m_pDVDC = pBF) && (m_pDVDI = pBF)) {
            break;
        }
    }
    EndEnumFilters;

    if (m_bUseSeekPreview) {
        BeginEnumFilters(m_pGB_preview, pEF, pBF) {
            if ((m_pDVDC_preview = pBF) && (m_pDVDI_preview = pBF)) {
                break;
            }
        }
        EndEnumFilters;
    }

    if (hr == E_INVALIDARG) {
        throw (UINT)IDS_MAINFRM_93;
    } else if (hr == VFW_E_CANNOT_RENDER) {
        throw (UINT)IDS_DVD_NAV_ALL_PINS_ERROR;
    } else if (hr == VFW_S_PARTIAL_RENDER) {
        throw (UINT)IDS_DVD_NAV_SOME_PINS_ERROR;
    } else if (hr == E_NOINTERFACE || !m_pDVDC || !m_pDVDI) {
        throw (UINT)IDS_DVD_INTERFACES_ERROR;
    } else if (hr == VFW_E_CANNOT_LOAD_SOURCE_FILTER) {
        throw (UINT)IDS_MAINFRM_94;
    } else if (FAILED(hr)) {
        throw (UINT)IDS_AG_FAILED;
    }

    WCHAR buff[MAX_PATH];
    ULONG len = 0;
    if (SUCCEEDED(hr = m_pDVDI->GetDVDDirectory(buff, _countof(buff), &len))) {
        pODD->title = CString(CStringW(buff)).Trim(_T("\\"));
    }

    if (s.fKeepHistory) {
        auto* pMRU = &s.MRU;
        pMRU->ReadList();
        pMRU->Add(pODD->title);
        pMRU->WriteList();
        SHAddToRecentDocs(SHARD_PATH, pODD->title);
    }

    // TODO: resetdvd
    m_pDVDC->SetOption(DVD_ResetOnStop, FALSE);
    m_pDVDC->SetOption(DVD_HMSF_TimeCodeEvents, TRUE);

    if (m_bUseSeekPreview && m_pDVDC_preview) {
        m_pDVDC_preview->SetOption(DVD_ResetOnStop, FALSE);
        m_pDVDC_preview->SetOption(DVD_HMSF_TimeCodeEvents, TRUE);
    }

    if (s.idMenuLang) {
        m_pDVDC->SelectDefaultMenuLanguage(s.idMenuLang);
    }
    if (s.idAudioLang) {
        m_pDVDC->SelectDefaultAudioLanguage(s.idAudioLang, DVD_AUD_EXT_NotSpecified);
    }
    if (s.idSubtitlesLang) {
        m_pDVDC->SelectDefaultSubpictureLanguage(s.idSubtitlesLang, DVD_SP_EXT_NotSpecified);
    }

    m_iDVDDomain = DVD_DOMAIN_Stop;

    SetPlaybackMode(PM_DVD);
}

// Called from GraphThread
HRESULT CMainFrame::OpenBDAGraph()
{
    HRESULT hr = m_pGB->RenderFile(L"", L"");
    if (SUCCEEDED(hr)) {
        SetPlaybackMode(PM_DIGITAL_CAPTURE);
        m_pDVBState = std::make_unique<DVBState>();
    }
    return hr;
}

// Called from GraphThread
void CMainFrame::OpenCapture(OpenDeviceData* pODD)
{
    m_wndCaptureBar.InitControls();

    CStringW vidfrname, audfrname;
    CComPtr<IBaseFilter> pVidCapTmp, pAudCapTmp;

    m_VidDispName = pODD->DisplayName[0];

    if (!m_VidDispName.IsEmpty()) {
        if (!CreateFilter(m_VidDispName, &pVidCapTmp, vidfrname)) {
            throw (UINT)IDS_MAINFRM_96;
        }
    }

    m_AudDispName = pODD->DisplayName[1];

    if (!m_AudDispName.IsEmpty()) {
        if (!CreateFilter(m_AudDispName, &pAudCapTmp, audfrname)) {
            throw (UINT)IDS_MAINFRM_96;
        }
    }

    if (!pVidCapTmp && !pAudCapTmp) {
        throw (UINT)IDS_MAINFRM_98;
    }

    m_pCGB = nullptr;
    m_pVidCap = nullptr;
    m_pAudCap = nullptr;

    if (FAILED(m_pCGB.CoCreateInstance(CLSID_CaptureGraphBuilder2))) {
        throw (UINT)IDS_MAINFRM_99;
    }

    HRESULT hr;

    m_pCGB->SetFiltergraph(m_pGB);

    if (pVidCapTmp) {
        if (FAILED(hr = m_pGB->AddFilter(pVidCapTmp, vidfrname))) {
            throw (UINT)IDS_CAPTURE_ERROR_VID_FILTER;
        }

        m_pVidCap = pVidCapTmp;

        if (!pAudCapTmp) {
            if (FAILED(m_pCGB->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Interleaved, m_pVidCap, IID_PPV_ARGS(&m_pAMVSCCap)))
                    && FAILED(m_pCGB->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, m_pVidCap, IID_PPV_ARGS(&m_pAMVSCCap)))) {
                TRACE(_T("Warning: No IAMStreamConfig interface for vidcap capture"));
            }

            if (FAILED(m_pCGB->FindInterface(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Interleaved, m_pVidCap, IID_PPV_ARGS(&m_pAMVSCPrev)))
                    && FAILED(m_pCGB->FindInterface(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, m_pVidCap, IID_PPV_ARGS(&m_pAMVSCPrev)))) {
                TRACE(_T("Warning: No IAMStreamConfig interface for vidcap capture"));
            }

            if (FAILED(m_pCGB->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Audio, m_pVidCap, IID_PPV_ARGS(&m_pAMASC)))
                    && FAILED(m_pCGB->FindInterface(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Audio, m_pVidCap, IID_PPV_ARGS(&m_pAMASC)))) {
                TRACE(_T("Warning: No IAMStreamConfig interface for vidcap"));
            } else {
                m_pAudCap = m_pVidCap;
            }
        } else {
            if (FAILED(m_pCGB->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, m_pVidCap, IID_PPV_ARGS(&m_pAMVSCCap)))) {
                TRACE(_T("Warning: No IAMStreamConfig interface for vidcap capture"));
            }

            if (FAILED(m_pCGB->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, m_pVidCap, IID_PPV_ARGS(&m_pAMVSCPrev)))) {
                TRACE(_T("Warning: No IAMStreamConfig interface for vidcap capture"));
            }
        }

        if (FAILED(m_pCGB->FindInterface(&LOOK_UPSTREAM_ONLY, nullptr, m_pVidCap, IID_PPV_ARGS(&m_pAMXBar)))) {
            TRACE(_T("Warning: No IAMCrossbar interface was found\n"));
        }

        if (FAILED(m_pCGB->FindInterface(&LOOK_UPSTREAM_ONLY, nullptr, m_pVidCap, IID_PPV_ARGS(&m_pAMTuner)))) {
            TRACE(_T("Warning: No IAMTVTuner interface was found\n"));
        }
        // TODO: init m_pAMXBar

        if (m_pAMTuner) { // load saved channel
            m_pAMTuner->put_CountryCode(AfxGetApp()->GetProfileInt(_T("Capture"), _T("Country"), 1));

            int vchannel = pODD->vchannel;
            if (vchannel < 0) {
                vchannel = AfxGetApp()->GetProfileInt(_T("Capture\\") + CString(m_VidDispName), _T("Channel"), -1);
            }
            if (vchannel >= 0) {
                OAFilterState fs = State_Stopped;
                m_pMC->GetState(0, &fs);
                if (fs == State_Running) {
                    m_pMC->Pause();
                }
                m_pAMTuner->put_Channel(vchannel, AMTUNER_SUBCHAN_DEFAULT, AMTUNER_SUBCHAN_DEFAULT);
                if (fs == State_Running) {
                    m_pMC->Run();
                }
            }
        }
    }

    if (pAudCapTmp) {
        if (FAILED(hr = m_pGB->AddFilter(pAudCapTmp, CStringW(audfrname)))) {
            throw (UINT)IDS_CAPTURE_ERROR_AUD_FILTER;
        }

        m_pAudCap = pAudCapTmp;

        if (FAILED(m_pCGB->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Audio, m_pAudCap, IID_PPV_ARGS(&m_pAMASC)))
                && FAILED(m_pCGB->FindInterface(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Audio, m_pAudCap, IID_PPV_ARGS(&m_pAMASC)))) {
            TRACE(_T("Warning: No IAMStreamConfig interface for vidcap"));
        }
        /*
        CInterfaceArray<IAMAudioInputMixer> pAMAIM;

        BeginEnumPins(m_pAudCap, pEP, pPin)
        {
            PIN_DIRECTION dir;
            if (FAILED(pPin->QueryDirection(&dir)) || dir != PINDIR_INPUT)
                continue;

            if (CComQIPtr<IAMAudioInputMixer> pAIM = pPin)
                pAMAIM.Add(pAIM);
        }
        EndEnumPins;

        if (m_pAMASC)
        {
            m_wndCaptureBar.m_capdlg.SetupAudioControls(auddispname, m_pAMASC, pAMAIM);
        }
        */
    }

    if (!(m_pVidCap || m_pAudCap)) {
        throw (UINT)IDS_MAINFRM_108;
    }

    pODD->title.LoadString(IDS_CAPTURE_LIVE);

    SetPlaybackMode(PM_ANALOG_CAPTURE);
}

// Called from GraphThread
void CMainFrame::OpenCustomizeGraph()
{
    if (GetPlaybackMode() != PM_FILE && GetPlaybackMode() != PM_DVD) {
        return;
    }

    CleanGraph();

    if (GetPlaybackMode() == PM_FILE) {
        if (m_pCAP && AfxGetAppSettings().IsISRAutoLoadEnabled()) {
            AddTextPassThruFilter();
        }
    }

    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& r = s.m_RenderersSettings;
    if (r.m_AdvRendSets.bSynchronizeVideo && s.iDSVideoRendererType == VIDRNDT_DS_SYNC) {
        HRESULT hr = S_OK;
        m_pRefClock = DEBUG_NEW CSyncClockFilter(nullptr, &hr);

        if (SUCCEEDED(hr) && SUCCEEDED(m_pGB->AddFilter(m_pRefClock, L"SyncClock Filter"))) {
            CComQIPtr<IReferenceClock> refClock = m_pRefClock;
            CComQIPtr<IMediaFilter> mediaFilter = m_pGB;

            if (refClock && mediaFilter) {
                VERIFY(SUCCEEDED(mediaFilter->SetSyncSource(refClock)));
                mediaFilter = nullptr;
                refClock = nullptr;

                VERIFY(SUCCEEDED(m_pRefClock->QueryInterface(IID_PPV_ARGS(&m_pSyncClock))));
                CComQIPtr<ISyncClockAdviser> pAdviser = m_pCAP;
                if (pAdviser) {
                    VERIFY(SUCCEEDED(pAdviser->AdviseSyncClock(m_pSyncClock)));
                }
            }
        }
    }

    if (GetPlaybackMode() == PM_DVD) {
        BeginEnumFilters(m_pGB, pEF, pBF) {
            if (CComQIPtr<IDirectVobSub2> pDVS2 = pBF) {
                //pDVS2->AdviseSubClock(m_pSubClock = DEBUG_NEW CSubClock);
                //break;

                // TODO: test multiple dvobsub instances with one clock
                if (!m_pSubClock) {
                    m_pSubClock = DEBUG_NEW CSubClock;
                }
                pDVS2->AdviseSubClock(m_pSubClock);
            }
        }
        EndEnumFilters;
    }

    BeginEnumFilters(m_pGB, pEF, pBF) {
        if (GetCLSID(pBF) == CLSID_OggSplitter) {
            if (CComQIPtr<IAMStreamSelect> pSS = pBF) {
                LCID idAudio = s.idAudioLang;
                if (!idAudio) {
                    idAudio = GetUserDefaultLCID();
                }
                LCID idSub = s.idSubtitlesLang;
                if (!idSub) {
                    idSub = GetUserDefaultLCID();
                }

                DWORD cnt = 0;
                if (SUCCEEDED(pSS->Count(&cnt))) {
                    for (DWORD i = 0; i < cnt; i++) {
                        AM_MEDIA_TYPE* pmt = nullptr;
                        DWORD dwFlags = 0;
                        LCID lcid = 0;
                        DWORD dwGroup = 0;
                        WCHAR* pszName = nullptr;
                        if (SUCCEEDED(pSS->Info((long)i, &pmt, &dwFlags, &lcid, &dwGroup, &pszName, nullptr, nullptr))) {
                            CStringW name(pszName), sound(StrRes(IDS_AG_SOUND)), subtitle(L"Subtitle");

                            if (idAudio != (LCID) - 1 && (idAudio & 0x3ff) == (lcid & 0x3ff) // sublang seems to be zeroed out in ogm...
                                    && name.GetLength() > sound.GetLength()
                                    && !name.Left(sound.GetLength()).CompareNoCase(sound)) {
                                if (SUCCEEDED(pSS->Enable(i, AMSTREAMSELECTENABLE_ENABLE))) {
                                    idAudio = (LCID) - 1;
                                }
                            }

                            if (idSub != (LCID) - 1 && (idSub & 0x3ff) == (lcid & 0x3ff) // sublang seems to be zeroed out in ogm...
                                    && name.GetLength() > subtitle.GetLength()
                                    && !name.Left(subtitle.GetLength()).CompareNoCase(subtitle)
                                    && name.Mid(subtitle.GetLength()).Trim().CompareNoCase(L"off")) {
                                if (SUCCEEDED(pSS->Enable(i, AMSTREAMSELECTENABLE_ENABLE))) {
                                    idSub = (LCID) - 1;
                                }
                            }

                            if (pmt) {
                                DeleteMediaType(pmt);
                            }
                            if (pszName) {
                                CoTaskMemFree(pszName);
                            }
                        }
                    }
                }
            }
        }
    }
    EndEnumFilters;

    CleanGraph();
}

// Called from GraphThread
void CMainFrame::OpenSetupVideo()
{
    m_fAudioOnly = true;

    if (m_pMFVDC) { // EVR
        m_fAudioOnly = false;
    } else if (m_pCAP) {
        CSize vs = m_pCAP->GetVideoSize(false);
        m_fAudioOnly = (vs.cx <= 0 || vs.cy <= 0);
    } else {
        {
            long w = 0, h = 0;

            if (CComQIPtr<IBasicVideo> pBV = m_pGB) {
                pBV->GetVideoSize(&w, &h);
            }

            if (w > 0 && h > 0) {
                m_fAudioOnly = false;
            }
        }

        if (m_fAudioOnly) {
            BeginEnumFilters(m_pGB, pEF, pBF) {
                long w = 0, h = 0;

                if (CComQIPtr<IVideoWindow> pVW = pBF) {
                    long lVisible;
                    if (FAILED(pVW->get_Visible(&lVisible))) {
                        continue;
                    }

                    pVW->get_Width(&w);
                    pVW->get_Height(&h);
                }

                if (w > 0 && h > 0) {
                    m_fAudioOnly = false;
                    break;
                }
            }
            EndEnumFilters;
        }
    }

    if (m_fShockwaveGraph) {
        m_fAudioOnly = false;
    }

    m_pVW->put_Owner((OAHWND)m_pVideoWnd->m_hWnd);
    m_pVW->put_WindowStyle(WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
    m_pVW->put_MessageDrain((OAHWND)m_pVideoWnd->m_hWnd);

    for (CWnd* pWnd = m_pVideoWnd->GetWindow(GW_CHILD); pWnd; pWnd = pWnd->GetNextWindow()) {
        // 1. lets WM_SETCURSOR through (not needed as of now)
        // 2. allows CMouse::CursorOnWindow() to work with m_pVideoWnd
        pWnd->EnableWindow(FALSE);
    }

    if (m_bUseSeekPreview) {
        m_pVW_preview->put_Owner((OAHWND)m_wndPreView.GetVideoHWND());
        m_pVW_preview->put_WindowStyle(WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
    }

    if (!m_fAudioOnly) {
        UpdateDXVAStatus();
    } else if (IsD3DFullScreenMode()) {
        m_pFullscreenWnd->DestroyWindow();
    }
}

// Called from GraphThread
void CMainFrame::OpenSetupAudio()
{
    m_pBA->put_Volume(m_wndToolBar.Volume);

    // FIXME
    int balance = AfxGetAppSettings().nBalance;

    int sign = balance > 0 ? -1 : 1; // -1: invert sign for more right channel
    if (balance > -100 && balance < 100) {
        balance = sign * (int)(100 * 20 * log10(1 - abs(balance) / 100.0f));
    } else {
        balance = sign * (-10000);  // -10000: only left, 10000: only right
    }

    m_pBA->put_Balance(balance);
}

void CMainFrame::OpenSetupCaptureBar()
{
    if (GetPlaybackMode() == PM_ANALOG_CAPTURE) {
        if (m_pVidCap && m_pAMVSCCap) {
            CComQIPtr<IAMVfwCaptureDialogs> pVfwCD = m_pVidCap;

            if (!m_pAMXBar && pVfwCD) {
                m_wndCaptureBar.m_capdlg.SetupVideoControls(m_VidDispName, m_pAMVSCCap, pVfwCD);
            } else {
                m_wndCaptureBar.m_capdlg.SetupVideoControls(m_VidDispName, m_pAMVSCCap, m_pAMXBar, m_pAMTuner);
            }
        }

        if (m_pAudCap && m_pAMASC) {
            CInterfaceArray<IAMAudioInputMixer> pAMAIM;

            BeginEnumPins(m_pAudCap, pEP, pPin) {
                if (CComQIPtr<IAMAudioInputMixer> pAIM = pPin) {
                    pAMAIM.Add(pAIM);
                }
            }
            EndEnumPins;

            m_wndCaptureBar.m_capdlg.SetupAudioControls(m_AudDispName, m_pAMASC, pAMAIM);
        }

        BuildGraphVideoAudio(
            m_wndCaptureBar.m_capdlg.m_fVidPreview, false,
            m_wndCaptureBar.m_capdlg.m_fAudPreview, false);
    }
}

void CMainFrame::OpenSetupInfoBar(bool bClear /*= true*/)
{
    bool bRecalcLayout = false;

    if (bClear) {
        m_wndInfoBar.RemoveAllLines();
    }

    if (GetPlaybackMode() == PM_FILE) {
        CComBSTR bstr;
        CString title, author, copyright, rating, description;
        if (m_pAMMC) {
            if (SUCCEEDED(m_pAMMC->get_Title(&bstr)) && bstr.Length()) {
                title = bstr.m_str;
                title.Trim();
            }
            bstr.Empty();
            if (SUCCEEDED(m_pAMMC->get_AuthorName(&bstr)) && bstr.Length()) {
                author = bstr.m_str;
            }
            bstr.Empty();
            if (SUCCEEDED(m_pAMMC->get_Copyright(&bstr)) && bstr.Length()) {
                copyright = bstr.m_str;
            }
            bstr.Empty();
            if (SUCCEEDED(m_pAMMC->get_Rating(&bstr)) && bstr.Length()) {
                rating = bstr.m_str;
            }
            bstr.Empty();
            if (SUCCEEDED(m_pAMMC->get_Description(&bstr)) && bstr.Length()) {
                description = bstr.m_str;
            }
            bstr.Empty();
        }

        bRecalcLayout |= m_wndInfoBar.SetLine(StrRes(IDS_INFOBAR_TITLE), title);
        CString fn(GetFileName());
        CString ext(fn.Mid(fn.ReverseFind('.')));
        if (ext == ".mpls" && m_bHasBDMeta) {
            CHdmvClipInfo::BDMVMeta meta(GetBDMVMeta());
            CString disctitle(meta.title);
            if (!meta.langcode.IsEmpty()) disctitle += (_T(" (") + meta.langcode + _T(")"));
            bRecalcLayout |= m_wndInfoBar.SetLine(StrRes(IDS_INFOBAR_DISCTITLE), disctitle);
        }
        UpdateChapterInInfoBar();
        bRecalcLayout |= m_wndInfoBar.SetLine(StrRes(IDS_INFOBAR_AUTHOR), author);
        bRecalcLayout |= m_wndInfoBar.SetLine(StrRes(IDS_INFOBAR_COPYRIGHT), copyright);
        bRecalcLayout |= m_wndInfoBar.SetLine(StrRes(IDS_INFOBAR_RATING), rating);
        bRecalcLayout |= m_wndInfoBar.SetLine(StrRes(IDS_INFOBAR_DESCRIPTION), description);
    } else if (GetPlaybackMode() == PM_DVD) {
        CString info(_T('-'));
        bRecalcLayout |= m_wndInfoBar.SetLine(StrRes(IDS_INFOBAR_DOMAIN), info);
        bRecalcLayout |= m_wndInfoBar.SetLine(StrRes(IDS_INFOBAR_LOCATION), info);
        bRecalcLayout |= m_wndInfoBar.SetLine(StrRes(IDS_INFOBAR_VIDEO), info);
        bRecalcLayout |= m_wndInfoBar.SetLine(StrRes(IDS_INFOBAR_AUDIO), info);
        bRecalcLayout |= m_wndInfoBar.SetLine(StrRes(IDS_INFOBAR_SUBTITLES), info);
    }

    if (bRecalcLayout) {
        RecalcLayout();
    }
}

void CMainFrame::UpdateChapterInInfoBar()
{
    CString chapter;
    if (m_pCB) {
        DWORD dwChapCount = m_pCB->ChapGetCount();
        if (dwChapCount) {
            REFERENCE_TIME rtNow;
            m_pMS->GetCurrentPosition(&rtNow);

            CComBSTR bstr;
            long currentChap = m_pCB->ChapLookup(&rtNow, &bstr);
            if (bstr.Length()) {
                chapter.Format(_T("%s (%ld/%lu)"), bstr.m_str, std::max(0l, currentChap + 1l), dwChapCount);
            } else {
                chapter.Format(_T("%ld/%lu"), currentChap + 1, dwChapCount);
            }
        }
    }
    if (m_wndInfoBar.SetLine(StrRes(IDS_INFOBAR_CHAPTER), chapter)) {
        RecalcLayout();
    }
}

void CMainFrame::OpenSetupStatsBar()
{
    m_wndStatsBar.RemoveAllLines();

    if (GetLoadState() == MLS::LOADED) {
        CString info(_T('-'));
        bool bFoundIBitRateInfo = false;

        BeginEnumFilters(m_pGB, pEF, pBF) {
            if (!m_pQP) {
                m_pQP = pBF;
            }
            if (!m_pBI) {
                m_pBI = pBF;
            }
            if (!bFoundIBitRateInfo) {
                BeginEnumPins(pBF, pEP, pPin) {
                    if (CComQIPtr<IBitRateInfo> pBRI = pPin) {
                        bFoundIBitRateInfo = true;
                        break;
                    }
                }
                EndEnumPins;
            }
            if (m_pQP && m_pBI && bFoundIBitRateInfo) {
                break;
            }
        }
        EndEnumFilters;

        if (m_pQP) {
            m_wndStatsBar.SetLine(StrRes(IDS_AG_FRAMERATE), info);
            m_wndStatsBar.SetLine(StrRes(IDS_STATSBAR_SYNC_OFFSET), info);
            m_wndStatsBar.SetLine(StrRes(IDS_AG_FRAMES), info);
            m_wndStatsBar.SetLine(StrRes(IDS_STATSBAR_JITTER), info);
        } else {
            m_wndStatsBar.SetLine(StrRes(IDS_STATSBAR_PLAYBACK_RATE), info);
        }
        if (GetPlaybackMode() == PM_DIGITAL_CAPTURE) {
            m_wndStatsBar.SetLine(StrRes(IDS_STATSBAR_SIGNAL), info);
        }
        if (m_pBI) {
            m_wndStatsBar.SetLine(StrRes(IDS_AG_BUFFERS), info);
        }
        if (bFoundIBitRateInfo) {
            m_wndStatsBar.SetLine(StrRes(IDS_STATSBAR_BITRATE), info);
        }
    }
}

void CMainFrame::OpenSetupStatusBar()
{
    m_wndStatusBar.ShowTimer(true);

    if (!m_fCustomGraph) {
        UINT id = IDB_AUDIOTYPE_NOAUDIO;

        BeginEnumFilters(m_pGB, pEF, pBF) {
            CComQIPtr<IBasicAudio> pBA = pBF;
            if (!pBA) {
                continue;
            }

            BeginEnumPins(pBF, pEP, pPin) {
                AM_MEDIA_TYPE mt;
                if (S_OK == m_pGB->IsPinDirection(pPin, PINDIR_INPUT)
                        && S_OK == m_pGB->IsPinConnected(pPin)
                        && SUCCEEDED(pPin->ConnectionMediaType(&mt))) {
                    if (mt.majortype == MEDIATYPE_Audio && mt.formattype == FORMAT_WaveFormatEx) {
                        switch (((WAVEFORMATEX*)mt.pbFormat)->nChannels) {
                            case 1:
                                id = IDB_AUDIOTYPE_MONO;
                                break;
                            case 2:
                            default:
                                id = IDB_AUDIOTYPE_STEREO;
                                break;
                        }
                        break;
                    } else if (mt.majortype == MEDIATYPE_Midi) {
                        id = 0;
                        break;
                    }
                }
            }
            EndEnumPins;

            if (id != IDB_AUDIOTYPE_NOAUDIO) {
                break;
            }
        }
        EndEnumFilters;

        m_wndStatusBar.SetStatusBitmap(id);
    }
}

// Called from GraphThread
void CMainFrame::OpenSetupWindowTitle(bool reset /*= false*/)
{
    CString title(StrRes(IDR_MAINFRAME));
#ifdef MPCHC_LITE
    title += _T(" Lite");
#endif

    const CAppSettings& s = AfxGetAppSettings();

    int i = s.iTitleBarTextStyle;

    if (!reset && (i == 0 || i == 1)) {
        // There is no path in capture mode
        if (IsPlaybackCaptureMode()) {
            title = GetCaptureTitle();
        } else if (i == 1) { // Show filename or title
            if (GetPlaybackMode() == PM_FILE) {
                bool has_title = false;

                if (s.fTitleBarTextTitle) {
                    if (m_pAMMC) {
                        CComBSTR bstr;
                        if (SUCCEEDED(m_pAMMC->get_Title(&bstr)) && bstr.Length()) {
                            title = bstr.m_str;
                            title.Trim();
                            has_title = !title.IsEmpty();
                        }
                    }
                }

                if (!has_title) {
                    CString ext = GetFileName().Mid(GetFileName().ReverseFind('.'));
                    if (ext == ".mpls" && m_bHasBDMeta) {
                        title = GetBDMVMeta().title;
                        has_title = true;
                    } else if (ext != ".mpls") {
                        m_bHasBDMeta = false;
                        m_BDMeta.RemoveAll();
                    }
                }

                if (!has_title) {
                    CPlaylistItem* pli = m_wndPlaylistBar.GetCur();               
                    if (pli && !pli->m_fns.IsEmpty() && pli->m_label && !pli->m_label.IsEmpty()) {
                        if (s.fTitleBarTextTitle || pli->m_bYoutubeDL) {
                            title = pli->m_label;
                            has_title = true;
                        }
                    }
                }

                CString fn = GetFileName();

                if (has_title & !IsNameSimilar(title, fn)) m_current_rfe.title = title;

                if (!has_title) {
                    title = fn;
                    has_title = true;
                }
            } else if (GetPlaybackMode() == PM_DVD) {
                title = _T("DVD");
                CString path;
                ULONG len = 0;
                if (m_pDVDI && SUCCEEDED(m_pDVDI->GetDVDDirectory(path.GetBufferSetLength(MAX_PATH), MAX_PATH, &len)) && len) {
                    path.ReleaseBuffer();
                    if (path.Find(_T("\\VIDEO_TS")) == 2) {
                        title.AppendFormat(_T(" - %s"), GetDriveLabel(CPath(path)).GetString());
                    }
                }
            }
        } else { // Show full path
            if (GetPlaybackMode() == PM_FILE) {
                title = m_wndPlaylistBar.GetCurFileName();
            } else if (GetPlaybackMode() == PM_DVD) {
                title = _T("DVD");
                ULONG len = 0;
                if (m_pDVDI) {
                    VERIFY(SUCCEEDED(m_pDVDI->GetDVDDirectory(title.GetBufferSetLength(MAX_PATH), MAX_PATH, &len)));
                    title.ReleaseBuffer();
                }
            }
        }
    }

    SetWindowText(title);
    m_Lcd.SetMediaTitle(title);
}

// Called from GraphThread
int CMainFrame::SetupAudioStreams()
{
    bool bIsSplitter = false;
    CComQIPtr<IAMStreamSelect> pSS = FindFilter(__uuidof(CAudioSwitcherFilter), m_pGB);

    if (!pSS && m_pFSF) { // Try to find the main splitter
        pSS = m_pFSF;
        if (!pSS) { // If the source filter isn't a splitter
            CComQIPtr<IBaseFilter> pBF = m_pFSF;
            if (pBF) { // try to get the main splitter
                PIN_DIRECTION pinDir;
                BeginEnumPins(pBF, pEP, pPin) {
                    CComPtr<IPin> pPinConnectedTo;
                    if (SUCCEEDED(pPin->QueryDirection(&pinDir)) && pinDir == PINDIR_OUTPUT
                            && SUCCEEDED(pPin->ConnectedTo(&pPinConnectedTo)) && pPinConnectedTo) {
                        pSS = GetFilterFromPin(pPinConnectedTo);
                        break;
                    }
                }
                EndEnumPins
            }
        }
        bIsSplitter = true;
    }

    DWORD cStreams = 0;
    if (pSS && SUCCEEDED(pSS->Count(&cStreams)) && cStreams > 1) {
        const CAppSettings& s = AfxGetAppSettings();

        CAtlArray<CString> langs;
        int tPos = 0;
        CString lang = s.strAudiosLanguageOrder.Tokenize(_T(",; "), tPos);
        while (tPos != -1) {
            lang.MakeLower();
            langs.Add(lang);
            // Try to match the full language if possible
            lang = ISOLang::ISO639XToLanguage(CStringA(lang));
            if (!lang.IsEmpty()) {
                langs.Add(lang.MakeLower());
            }
            lang = s.strAudiosLanguageOrder.Tokenize(_T(",; "), tPos);
        }

        int selected = -1, id = 0;
        int maxrating = -1;
        for (DWORD i = 0; i < cStreams; i++) {
            DWORD dwFlags, dwGroup;
            WCHAR* pName = nullptr;
            CComPtr<IUnknown> pObject;
            if (FAILED(pSS->Info(i, nullptr, &dwFlags, nullptr, &dwGroup, &pName, &pObject, nullptr))) {
                continue;
            }
            CString name(pName);
            CoTaskMemFree(pName);

            // Skip no-audio track if we are dealing directly with a splitter
            if (bIsSplitter && dwGroup != 1) {
                continue;
            }

            int rating = 0;
            // If the track is controlled by a splitter (directly or not) and isn't selected at splitter level
            if ((!bIsSplitter && dwGroup == 1)
                    || (bIsSplitter && !(dwFlags & (AMSTREAMSELECTINFO_ENABLED | AMSTREAMSELECTINFO_EXCLUSIVE)))) {
                bool bSkipTrack;

                // If the splitter is the internal LAV Splitter and no language preferences
                // have been set at splitter level, we can override its choice safely
                CComQIPtr<IBaseFilter> pBF = bIsSplitter ? pSS : reinterpret_cast<IBaseFilter*>(pObject.p);
                if (pBF && CFGFilterLAV::IsInternalInstance(pBF)) {
                    bSkipTrack = false;
                    if (CComQIPtr<ILAVFSettings> pLAVFSettings = pBF) {
                        LPWSTR langPrefs = nullptr;
                        if (SUCCEEDED(pLAVFSettings->GetPreferredLanguages(&langPrefs)) && langPrefs && wcslen(langPrefs)) {
                            bSkipTrack = true;
                        }
                        CoTaskMemFree(langPrefs);
                    }
                } else {
                    bSkipTrack = !s.bAllowOverridingExternalSplitterChoice;
                }

                if (bSkipTrack) {
                    id++;
                    continue;
                }
            } else if ((!bIsSplitter && dwGroup == 2)
                       || (bIsSplitter && (dwFlags & (AMSTREAMSELECTINFO_ENABLED | AMSTREAMSELECTINFO_EXCLUSIVE)))) {
                // If the track is controlled by a splitter (directly or not) and is selected at splitter level, we give it
                // a slightly higher priority so that we won't override selected track in case all have the same rating.
                rating += 1;
            }

            name.Trim();
            name.MakeLower();

            for (size_t j = 0; j < langs.GetCount(); j++) {
                int num = _tstoi(langs[j]) - 1;
                if (num >= 0) { // this is track number
                    if (id != num) {
                        continue;  // not matched
                    }
                } else { // this is lang string
                    int len = langs[j].GetLength();
                    if (name.Left(len) != langs[j] && name.Find(_T("[") + langs[j]) < 0) {
                        continue; // not matched
                    }
                }
                rating += 16 * int(langs.GetCount() - j);
                break;
            }
            if (name.Find(_T("[default,forced]")) != -1) { // for LAV Splitter
                rating += 4 + 2;
            }
            if (name.Find(_T("[forced]")) != -1) {
                rating += 4;
            }
            if (name.Find(_T("[default]")) != -1) {
                rating += 2;
            }

            if (rating > maxrating) {
                maxrating = rating;
                selected = id;
            }

            id++;
        }
        return selected + !bIsSplitter;
    }

    return -1;
}

// Called from GraphThread
int CMainFrame::SetupSubtitleStreams()
{
    const CAppSettings& s = AfxGetAppSettings();
    int selected = -1;

    if (!m_pSubStreams.IsEmpty()) {
        bool externalPriority = false;
        std::list<ISOLangT<CString>> langs;
        int tPos = 0;
        CString lang = s.strSubtitlesLanguageOrder.Tokenize(_T(",; "), tPos);
        while (tPos != -1) {
            lang.MakeLower();
            ISOLangT<CString> l = ISOLang::ISO639XToISOLang(CStringA(lang));
            if (l.name.IsEmpty()) {
                l.name = lang;
            } else {
                l.name.MakeLower();
            }
            langs.emplace_back(l);

            lang = s.strSubtitlesLanguageOrder.Tokenize(_T(",; "), tPos);
        }

        int i = 0;
        int maxrating = -1;
        POSITION pos = m_pSubStreams.GetHeadPosition();
        while (pos) {
            if (m_posFirstExtSub == pos) {
                externalPriority = s.fPrioritizeExternalSubtitles;
            }
            SubtitleInput& subInput = m_pSubStreams.GetNext(pos);
            CComPtr<ISubStream> pSubStream = subInput.pSubStream;
            CComQIPtr<IAMStreamSelect> pSSF = subInput.pSourceFilter;

            bool bAllowOverridingSplitterChoice;
            // If the internal LAV Splitter has its own language preferences set, we choose not to override its choice
            if (pSSF && CFGFilterLAV::IsInternalInstance(subInput.pSourceFilter)) {
                bAllowOverridingSplitterChoice = true;
                if (CComQIPtr<ILAVFSettings> pLAVFSettings = subInput.pSourceFilter) {
                    CComHeapPtr<WCHAR> pLangPrefs;
                    LAVSubtitleMode subtitleMode = pLAVFSettings->GetSubtitleMode();
                    if ((((subtitleMode == LAVSubtitleMode_Default && SUCCEEDED(pLAVFSettings->GetPreferredSubtitleLanguages(&pLangPrefs)))
                            || (subtitleMode == LAVSubtitleMode_Advanced && SUCCEEDED(pLAVFSettings->GetAdvancedSubtitleConfig(&pLangPrefs))))
                            && pLangPrefs && wcslen(pLangPrefs))
                            || subtitleMode == LAVSubtitleMode_ForcedOnly || subtitleMode == LAVSubtitleMode_NoSubs) {
                        bAllowOverridingSplitterChoice = false;
                    }
                }
            } else {
                bAllowOverridingSplitterChoice = s.bAllowOverridingExternalSplitterChoice;
            }

            int count = 0;
            if (pSSF) {
                DWORD cStreams;
                if (SUCCEEDED(pSSF->Count(&cStreams))) {
                    count = (int)cStreams;
                }
            } else {
                count = pSubStream->GetStreamCount();
            }

            for (int j = 0; j < count; j++) {
                CComHeapPtr<WCHAR> pName;
                LCID lcid = 0;
                int rating = 0;
                if (pSSF) {
                    DWORD dwFlags, dwGroup = 2;
                    pSSF->Info(j, nullptr, &dwFlags, &lcid, &dwGroup, &pName, nullptr, nullptr);
                    if (dwGroup != 2) { // If the track isn't a subtitle track, we skip it
                        continue;
                    } else if (dwFlags & (AMSTREAMSELECTINFO_ENABLED | AMSTREAMSELECTINFO_EXCLUSIVE)) {
                        // Give slightly higher priority to the track selected by splitter so that
                        // we won't override selected track in case all have the same rating.
                        rating += 1;
                    } else if (!bAllowOverridingSplitterChoice) {
                        // If we aren't allowed to modify the splitter choice and the current
                        // track isn't already selected at splitter level we need to skip it.
                        i++;
                        continue;
                    }
                } else {
                    pSubStream->GetStreamInfo(j, &pName, &lcid);
                }
                CString name(pName);
                name.Trim();
                name.MakeLower();

                size_t k = 0;
                for (const auto& l : langs) {
                    int num = _tstoi(l.name) - 1;
                    if (num >= 0) { // this is track number
                        if (i != num) {
                            k++;
                            continue;  // not matched
                        }
                    } else { // this is lang string
                        if (lcid == 0 || lcid == LCID(-1) || lcid != l.lcid) {
                            // no LCID match, analyze track name for language match
                            auto findCode = [](const CString & name, const CString & code) {
                                int nPos = code.IsEmpty() ? -1 : name.Find(code);
                                return nPos == 0 && name.GetLength() == code.GetLength()
                                       || nPos > 0 && name[nPos - 1] == _T('\t')
                                       || nPos > 0 && name[nPos - 1] == _T('[') && name.GetLength() >= (nPos + code.GetLength() + 1) && name[nPos + code.GetLength()] == _T(']')
                                       || nPos > 0 && name[nPos - 1] == _T('.') && name.GetLength() >= (nPos + code.GetLength() + 1) && name[nPos + code.GetLength()] == _T('.');
                            };

                            // match anything that starts with the language name or that seems to use a code that matches
                            if (name.Find(l.name) != 0 && !findCode(name, l.name) && !findCode(name, l.iso6392) && !findCode(name, l.iso6391)) {
                                k++;
                                continue; // not matched
                            }
                        }
                        // LCID match
                    }
                    rating += 16 * int(langs.size() - k);
                    break;
                }
                if (externalPriority) { // External tracks are given a higher priority than language matches
                    rating += 16 * int(langs.size() + 1);
                }
                if (s.bPreferDefaultForcedSubtitles) {
                    if (name.Find(_T("[default,forced]")) != -1) { // for LAV Splitter
                        rating += 4 + 2;
                    }
                    if (name.Find(_T("[forced]")) != -1) {
                        rating += 4;
                    }
                    if (name.Find(_T("[default]")) != -1) {
                        rating += 2;
                    }
                }

                if (rating > maxrating) {
                    maxrating = rating;
                    selected = i;
                }
                i++;
            }
        }
    }

    if (s.IsISRAutoLoadEnabled() && !m_fAudioOnly) {
        if (s.bAutoDownloadSubtitles && m_pSubStreams.IsEmpty()) {
            m_pSubtitlesProviders->Search(TRUE);
        } else if (m_wndSubtitlesDownloadDialog.IsWindowVisible()) {
            m_pSubtitlesProviders->Search(FALSE);
        }
    }

    return selected;
}

bool CMainFrame::OpenMediaPrivate(CAutoPtr<OpenMediaData> pOMD)
{
    ASSERT(GetLoadState() == MLS::LOADING);
    auto& s = AfxGetAppSettings();

    m_fValidDVDOpen = false;
    m_iDefRotation = 0;

    OpenFileData* pFileData = dynamic_cast<OpenFileData*>(pOMD.m_p);
    OpenDVDData* pDVDData = dynamic_cast<OpenDVDData*>(pOMD.m_p);
    OpenDeviceData* pDeviceData = dynamic_cast<OpenDeviceData*>(pOMD.m_p);
    ASSERT(pFileData || pDVDData || pDeviceData);

    // Clear DXVA state ...
    ClearDXVAState();

#ifdef _DEBUG
    // Debug trace code - Begin
    // Check for bad / buggy auto loading file code
    if (pFileData) {
        POSITION pos = pFileData->fns.GetHeadPosition();
        UINT index = 0;
        while (pos != nullptr) {
            CString path = pFileData->fns.GetNext(pos);
            TRACE(_T("--> CMainFrame::OpenMediaPrivate - pFileData->fns[%u]:\n"), index);
            TRACE(_T("\t%ws\n"), path.GetString()); // %ws - wide character string always
            index++;
        }
    }
    // Debug trace code - End
#endif

    CString err;
    try {
        auto checkAborted = [&]() {
            if (m_fOpeningAborted) {
                throw (UINT)IDS_AG_ABORTED;
            }
        };
        checkAborted();

        OpenCreateGraphObject(pOMD);
        checkAborted();

        if (pFileData) {
            OpenFile(pFileData);
        } else if (pDVDData) {
            OpenDVD(pDVDData);
        } else if (pDeviceData) {
            if (s.iDefaultCaptureDevice == 1) {
                HRESULT hr = OpenBDAGraph();
                if (FAILED(hr)) {
                    throw (UINT)IDS_CAPTURE_ERROR_DEVICE;
                }
            } else {
                OpenCapture(pDeviceData);
            }
        } else {
            throw (UINT)IDS_INVALID_PARAMS_ERROR;
        }

        m_pCAP3  = nullptr;
        m_pCAP2  = nullptr;
        m_pCAP   = nullptr;
        m_pVMRWC = nullptr;
        m_pVMRMC = nullptr;
        m_pMFVDC = nullptr;
        m_pMFVP  = nullptr;

        CComPtr<IVMRMixerBitmap9>    pVMB   = nullptr;
        CComPtr<IMFVideoMixerBitmap> pMFVMB = nullptr;
        CComPtr<IMadVRTextOsd>       pMVTO  = nullptr;

        m_pGB->FindInterface(IID_PPV_ARGS(&m_pCAP), TRUE);
        m_pGB->FindInterface(IID_PPV_ARGS(&m_pCAP2), TRUE);
        m_pGB->FindInterface(IID_PPV_ARGS(&m_pCAP3), TRUE);
        m_pGB->FindInterface(IID_PPV_ARGS(&m_pVMRWC), FALSE); // might have IVMRMixerBitmap9, but not IVMRWindowlessControl9
        m_pGB->FindInterface(IID_PPV_ARGS(&m_pVMRMC), TRUE);
        m_pGB->FindInterface(IID_PPV_ARGS(&pVMB), TRUE);
        m_pGB->FindInterface(IID_PPV_ARGS(&pMFVMB), TRUE);

        m_pMVRC = m_pCAP;
        m_pMVRI = m_pCAP;
        m_pMVRS = m_pCAP;
        m_pMVRSR = m_pCAP;
        m_pMVRFG = m_pCAP;
        pMVTO = m_pCAP;

        if (s.fShowOSD || s.fShowDebugInfo) { // Force OSD on when the debug switch is used
            if (pVMB) {
                m_OSD.Start(m_pVideoWnd, pVMB, IsD3DFullScreenMode());
            } else if (pMFVMB) {
                m_OSD.Start(m_pVideoWnd, pMFVMB, IsD3DFullScreenMode());
            } else if (pMVTO) {
                m_OSD.Start(m_pVideoWnd, pMVTO);
            }
        }
        checkAborted();

        SetupVMR9ColorControl();
        checkAborted();

        m_pGB->FindInterface(IID_PPV_ARGS(&m_pMFVDC), TRUE);
        m_pGB->FindInterface(IID_PPV_ARGS(&m_pMFVP), TRUE);
        if (m_pMFVDC) {
            m_pMFVDC->SetVideoWindow(m_pVideoWnd->m_hWnd);
        }

        // COMMENTED OUT: does not work at this location, need to choose the correct mode (IMFVideoProcessor::SetVideoProcessorMode)
        //SetupEVRColorControl();

        if (m_bUseSeekPreview) {
            m_pGB_preview->FindInterface(IID_PPV_ARGS(&m_pMFVDC_preview), TRUE);
            m_pGB_preview->FindInterface(IID_PPV_ARGS(&m_pMFVP_preview), TRUE);
            m_pGB_preview->FindInterface(IID_PPV_ARGS(&m_pVMR9C_preview), TRUE);

            if (m_pMFVDC_preview) {
                RECT Rect;
                m_wndPreView.GetClientRect(&Rect);
                m_pMFVDC_preview->SetVideoWindow(m_wndPreView.GetVideoHWND());
                m_pMFVDC_preview->SetVideoPosition(nullptr, &Rect);
            }
        }

        BeginEnumFilters(m_pGB, pEF, pBF) {
            if (m_pLN21 = pBF) {
                m_pLN21->SetServiceState(s.fClosedCaptions ? AM_L21_CCSTATE_On : AM_L21_CCSTATE_Off);
                break;
            }
        }
        EndEnumFilters;
        checkAborted();

        OpenCustomizeGraph();
        checkAborted();

        OpenSetupVideo();
        checkAborted();

        OpenSetupAudio();
        checkAborted();

        if (GetPlaybackMode() == PM_FILE && pFileData) {
            const CString& fn = pFileData->fns.GetHead();
            // Don't try to save file position if source isn't seekable
            REFERENCE_TIME rtPos = 0;
            REFERENCE_TIME rtDur = 0;
            m_pMS->GetDuration(&rtDur);

            m_bRememberFilePos = s.fKeepHistory && s.fRememberFilePos && rtDur > (s.iRememberPosForLongerThan * 10000000i64 * 60i64) && (s.bRememberPosForAudioFiles || !m_fAudioOnly);

            // Set start time but seek only after all files are loaded
            if (pFileData->rtStart > 0) { // Check if an explicit start time was given
                rtPos = pFileData->rtStart;
            }
            if (m_bRememberFilePos) { // Check if we want to remember the position
                // Always update the file positions list so that the position
                // is correctly saved but only restore the remembered position
                // if no explicit start time was already set.
                if (!s.filePositions.AddEntry(fn) && !rtPos) {
                    rtPos = s.filePositions.GetLatestEntry()->llPosition;
                }
            }

            if (rtPos) {
                m_pMS->SetPositions(&rtPos, AM_SEEKING_AbsolutePositioning, nullptr, AM_SEEKING_NoPositioning);
            }

            if (m_pFSF && (m_pCAP2 || m_pCAP3)) {
                CComQIPtr<IBaseFilter> pBF = m_pFSF;
                if (GetCLSID(pBF) == GUID_LAVSplitter || GetCLSID(pBF) == GUID_LAVSplitterSource) {
                    if (CComQIPtr<IPropertyBag> pPB = pBF) {
                        CComVariant var;
                        if (SUCCEEDED(pPB->Read(_T("rotation"), &var, nullptr)) && var.vt == VT_BSTR) {
                            int rotatevalue = _wtoi(var.bstrVal);
                            if (rotatevalue == 90 || rotatevalue == 180 || rotatevalue == 270) {
                                m_iDefRotation = rotatevalue;
                                if (m_pCAP3) {
                                    m_pCAP3->SetRotation(rotatevalue);
                                } else {
                                    m_pCAP2->SetDefaultVideoAngle(Vector(0, 0, Vector::DegToRad(360 - rotatevalue)));
                                }
                            }
                        }
                        var.Clear();
                    }
                }
            }
        }
        checkAborted();

        if (m_pCAP && s.IsISRAutoLoadEnabled() && !m_fAudioOnly) {
            if (s.fDisableInternalSubtitles) {
                m_pSubStreams.RemoveAll(); // Needs to be replaced with code that checks for forced subtitles.
            }
            m_posFirstExtSub = nullptr;
            if (!pOMD->subs.IsEmpty()) {
                POSITION pos = pOMD->subs.GetHeadPosition();
                while (pos) {
                    LoadSubtitle(pOMD->subs.GetNext(pos), nullptr, true);
                }
            }

            auto* pli = m_wndPlaylistBar.GetCur();
            if (pli != nullptr && pli->m_bYoutubeDL && !s.sYDLSubsPreference.IsEmpty()) {
                POSITION pos2 = pli->m_ydl_subs.GetHeadPosition();
                while (pos2) {
                    CYoutubeDLInstance::YDLSubInfo ydlsub = pli->m_ydl_subs.GetNext(pos2);
                    if (!ydlsub.isAutomaticCaptions || s.bUseAutomaticCaptions) {
                        LoadSubtitle(ydlsub);
                    }
                }
            }
        }
        checkAborted();

        OpenSetupWindowTitle();
        checkAborted();

        int audstm = SetupAudioStreams();
        if (audstm >= 0) {
            OnPlayAudio(ID_AUDIO_SUBITEM_START + audstm);
        }
        checkAborted();

        int substm = SetupSubtitleStreams();
        if (substm >= 0) {
            SetSubtitle(substm);
        }
        checkAborted();

        // apply /dubdelay command-line switch
        // TODO: that command-line switch probably needs revision
        if (s.rtShift != 0) {
            SetAudioDelay(s.rtShift);
            s.rtShift = 0;
        }
    } catch (LPCTSTR msg) {
        err = msg;
    } catch (CString& msg) {
        err = msg;
    } catch (UINT msg) {
        err.LoadString(msg);
    }

    if (m_bUseSeekPreview && m_pMC_preview) {
        m_pMC_preview->Pause();
    }

    m_closingmsg = err;

    auto getMessageArgs = [&]() {
        WPARAM wp = pFileData ? PM_FILE : pDVDData ? PM_DVD : pDeviceData ? (s.iDefaultCaptureDevice == 1 ? PM_DIGITAL_CAPTURE : PM_ANALOG_CAPTURE) : PM_NONE;
        ASSERT(wp != PM_NONE);
        LPARAM lp = (LPARAM)pOMD.Detach();
        ASSERT(lp);
        return std::make_pair(wp, lp);
    };
    if (err.IsEmpty()) {
        auto args = getMessageArgs();
        if (!m_bOpenedThroughThread) {
            ASSERT(GetCurrentThreadId() == AfxGetApp()->m_nThreadID);
            OnFilePostOpenmedia(args.first, args.second);
        } else {
            PostMessage(WM_POSTOPEN, args.first, args.second);
        }
    } else if (!m_fOpeningAborted) {
        auto args = getMessageArgs();
        if (!m_bOpenedThroughThread) {
            ASSERT(GetCurrentThreadId() == AfxGetApp()->m_nThreadID);
            OnOpenMediaFailed(args.first, args.second);
        } else {
            PostMessage(WM_OPENFAILED, args.first, args.second);
        }
    }

    return err.IsEmpty();
}

void CMainFrame::CloseMediaPrivate()
{
    ASSERT(GetLoadState() == MLS::CLOSING);

    if (m_pMC) {
        m_pMC->Stop(); // needed for StreamBufferSource, because m_iMediaLoadState is always MLS::CLOSED // TODO: fix the opening for such media
    }
    m_fLiveWM = false;
    m_fEndOfStream = false;
    m_bBuffering = false;
    m_rtDurationOverride = -1;
    m_bUsingDXVA = false;
    if (m_pDVBState) {
        m_pDVBState->Join();
        m_pDVBState = nullptr;
    }
    m_pCB.Release();

    {
        CAutoLock cAutoLock(&m_csSubLock);
        m_pCurrentSubInput = SubtitleInput(nullptr);
        m_pSubStreams.RemoveAll();
        m_ExternalSubstreams.clear();
    }
    m_pSubClock.Release();

    if (m_pVW && !m_pMVRS) {
        m_pVW->put_Owner(NULL);
    }

    // IMPORTANT: IVMRSurfaceAllocatorNotify/IVMRSurfaceAllocatorNotify9 has to be released before the VMR/VMR9, otherwise it will crash in Release()
    m_OSD.Stop();
    m_pMVRFG.Release();
    m_pMVRSR.Release();
    m_pMVRS.Release();
    m_pMVRC.Release();
    m_pMVRI.Release();
    m_pCAP3.Release();
    m_pCAP2.Release();
    m_pCAP.Release();
    m_pVMRWC.Release();
    m_pVMRMC.Release();
    m_pMFVP.Release();
    m_pMFVDC.Release();
    m_pLN21.Release();
    m_pSyncClock.Release();

    m_pAMXBar.Release();
    m_pAMDF.Release();
    m_pAMVCCap.Release();
    m_pAMVCPrev.Release();
    m_pAMVSCCap.Release();
    m_pAMVSCPrev.Release();
    m_pAMASC.Release();
    m_pVidCap.Release();
    m_pAudCap.Release();
    m_pAMTuner.Release();
    m_pCGB.Release();

    m_pDVDC.Release();
    m_pDVDI.Release();
    m_pAMOP.Release();
    m_pBI.Release();
    m_pQP.Release();
    m_pFS.Release();
    m_pMS.Release();
    m_pBA.Release();
    m_pBV.Release();
    m_pVW.Release();
    m_pME.Release();
    m_pMC.Release();
    m_pFSF.Release();
    m_pKFI.Release();
    m_pAMMC.Release();

    if (m_pGB) {
        m_pGB->RemoveFromROT();
        m_pGB.Release();
    }

    if (m_pGB_preview) {
        PreviewWindowHide();
        m_pMFVP_preview.Release();
        m_pMFVDC_preview.Release();
        m_pVMR9C_preview.Release();

        m_pFS_preview.Release();
        m_pMS_preview.Release();
        m_pBV_preview.Release();
        m_pVW_preview.Release();
        m_pME_preview.Release();
        m_pMC_preview.Release();

        if (m_pDVDC_preview) {
            m_pDVDC_preview.Release();
            m_pDVDI_preview.Release();
        }

        m_pGB_preview->RemoveFromROT();
        m_pGB_preview.Release();
    }

    m_pProv.Release();

    m_fCustomGraph = m_fShockwaveGraph = false;

    m_VidDispName.Empty();
    m_AudDispName.Empty();
	
	m_FontInstaller.UninstallFonts();
}

bool CMainFrame::SearchInDir(bool bDirForward, bool bLoop /*= false*/)
{
    ASSERT(GetPlaybackMode() == PM_FILE || CanSkipFromClosedFile());

    CString title;

    auto pFileData = dynamic_cast<OpenFileData*>(m_lastOMD.m_p);
    if (!pFileData) {
        if (CanSkipFromClosedFile()) {
            if (m_wndPlaylistBar.GetCount() == 1) {
                title = m_wndPlaylistBar.m_pl.GetHead().m_fns.GetHead();
            } else {
                title = lastOpenFile;
            }
        } else {
            ASSERT(FALSE);
            return false;
        }
    } else {
        title = pFileData->title;
    }

    std::set<CString, CStringUtils::LogicalLess> files;
    const CMediaFormats& mf = AfxGetAppSettings().m_Formats;
    CString mask = title.Left(title.ReverseFind(_T('\\')) + 1) + _T("*.*");
    CFileFind finder;
    BOOL bHasNext = finder.FindFile(mask);

    while (bHasNext) {
        bHasNext = finder.FindNextFile();

        if (!finder.IsDirectory()) {
            CString path = finder.GetFilePath();
            CString ext = path.Mid(path.ReverseFind('.'));
            if (mf.FindExt(ext)) {
                files.insert(path);
            }
        }
    }

    finder.Close();

    if (files.size() <= 1) {
        return false;
    }

    // We make sure that the currently opened file is added to the list
    // even if it's of an unknown format.
    auto current = files.insert(title).first;

    if (bDirForward) {
        current++;
        if (current == files.end()) {
            if (bLoop) {
                current = files.begin();
            } else {
                return false;
            }
        }
    } else {
        if (current == files.begin()) {
            if (bLoop) {
                current = files.end();
            } else {
                return false;
            }
        }
        current--;
    }

    CAtlList<CString> sl;
    sl.AddHead(*current);
    m_wndPlaylistBar.Open(sl, false);
    OpenCurPlaylistItem();

    return true;
}

void CMainFrame::DoTunerScan(TunerScanData* pTSD)
{
    if (GetPlaybackMode() == PM_DIGITAL_CAPTURE) {
        CComQIPtr<IBDATuner> pTun = m_pGB;
        if (pTun) {
            BOOLEAN bPresent;
            BOOLEAN bLocked;
            LONG lDbStrength = 0;
            LONG lPercentQuality = 0;
            int nOffset = pTSD->Offset ? 3 : 1;
            LONG lOffsets[3] = {0, pTSD->Offset, -pTSD->Offset};
            m_bStopTunerScan = false;
            pTun->Scan(0, 0, NULL);  // Clear maps

            for (ULONG ulFrequency = pTSD->FrequencyStart; ulFrequency <= pTSD->FrequencyStop; ulFrequency += pTSD->Bandwidth) {
                bool bSucceeded = false;
                for (int nOffsetPos = 0; nOffsetPos < nOffset && !bSucceeded; nOffsetPos++) {
                    if (SUCCEEDED(pTun->SetFrequency(ulFrequency + lOffsets[nOffsetPos], pTSD->Bandwidth))) {
                        Sleep(200); // Let the tuner some time to detect the signal
                        if (SUCCEEDED(pTun->GetStats(bPresent, bLocked, lDbStrength, lPercentQuality)) && bPresent) {
                            ::SendMessage(pTSD->Hwnd, WM_TUNER_STATS, lDbStrength, lPercentQuality);
                            pTun->Scan(ulFrequency + lOffsets[nOffsetPos], pTSD->Bandwidth, pTSD->Hwnd);
                            bSucceeded = true;
                        }
                    }
                }

                int nProgress = MulDiv(ulFrequency - pTSD->FrequencyStart, 100, pTSD->FrequencyStop - pTSD->FrequencyStart);
                ::SendMessage(pTSD->Hwnd, WM_TUNER_SCAN_PROGRESS, nProgress, 0);
                ::SendMessage(pTSD->Hwnd, WM_TUNER_STATS, lDbStrength, lPercentQuality);

                if (m_bStopTunerScan) {
                    break;
                }
            }

            ::SendMessage(pTSD->Hwnd, WM_TUNER_SCAN_END, 0, 0);
        }
    }
}

// Skype

void CMainFrame::SendNowPlayingToSkype()
{
    if (!m_pSkypeMoodMsgHandler) {
        return;
    }

    CString msg;

    if (GetLoadState() == MLS::LOADED) {
        CString title, author;

        m_wndInfoBar.GetLine(StrRes(IDS_INFOBAR_TITLE), title);
        m_wndInfoBar.GetLine(StrRes(IDS_INFOBAR_AUTHOR), author);

        if (title.IsEmpty()) {
            CPlaylistItem pli;
            m_wndPlaylistBar.GetCur(pli);

            if (!pli.m_fns.IsEmpty()) {
                CString label = !pli.m_label.IsEmpty() ? pli.m_label : pli.m_fns.GetHead();

                if (GetPlaybackMode() == PM_FILE) {
                    CString fn = label;
                    if (!pli.m_bYoutubeDL && PathUtils::IsURL(fn)) {
                        int i = fn.Find('?');
                        if (i >= 0) {
                            fn = fn.Left(i);
                        }
                    }
                    CPath path(fn);
                    path.StripPath();
                    path.MakePretty();
                    path.RemoveExtension();
                    title = (LPCTSTR)path;
                    author.Empty();
                } else if (IsPlaybackCaptureMode()) {
                    title = GetCaptureTitle();
                    author.Empty();
                } else if (GetPlaybackMode() == PM_DVD) {
                    title = _T("DVD");
                    author.Empty();
                }
            }
        }

        if (!author.IsEmpty()) {
            msg.Format(_T("%s - %s"), author.GetString(), title.GetString());
        } else {
            msg = title;
        }
    }

    m_pSkypeMoodMsgHandler->SendMoodMessage(msg);
}

// dynamic menus

void CMainFrame::CreateDynamicMenus()
{
    VERIFY(m_openCDsMenu.CreatePopupMenu());
    VERIFY(m_filtersMenu.CreatePopupMenu());
    VERIFY(m_subtitlesMenu.CreatePopupMenu());
    VERIFY(m_audiosMenu.CreatePopupMenu());
    VERIFY(m_videoStreamsMenu.CreatePopupMenu());
    VERIFY(m_chaptersMenu.CreatePopupMenu());
    VERIFY(m_titlesMenu.CreatePopupMenu());
    VERIFY(m_playlistMenu.CreatePopupMenu());
    VERIFY(m_BDPlaylistMenu.CreatePopupMenu());
    VERIFY(m_channelsMenu.CreatePopupMenu());
    VERIFY(m_favoritesMenu.CreatePopupMenu());
    VERIFY(m_shadersMenu.CreatePopupMenu());
    VERIFY(m_recentFilesMenu.CreatePopupMenu());
}

void CMainFrame::DestroyDynamicMenus()
{
    VERIFY(m_openCDsMenu.DestroyMenu());
    VERIFY(m_filtersMenu.DestroyMenu());
    VERIFY(m_subtitlesMenu.DestroyMenu());
    VERIFY(m_audiosMenu.DestroyMenu());
    VERIFY(m_videoStreamsMenu.DestroyMenu());
    VERIFY(m_chaptersMenu.DestroyMenu());
    VERIFY(m_titlesMenu.DestroyMenu());
    VERIFY(m_playlistMenu.DestroyMenu());
    VERIFY(m_BDPlaylistMenu.DestroyMenu());
    VERIFY(m_channelsMenu.DestroyMenu());
    VERIFY(m_favoritesMenu.DestroyMenu());
    VERIFY(m_shadersMenu.DestroyMenu());
    VERIFY(m_recentFilesMenu.DestroyMenu());
    m_nJumpToSubMenusCount = 0;
}

void CMainFrame::SetupOpenCDSubMenu()
{
    CMenu& subMenu = m_openCDsMenu;
    // Empty the menu
    while (subMenu.RemoveMenu(0, MF_BYPOSITION));

    if (GetLoadState() == MLS::LOADING || AfxGetAppSettings().fHideCDROMsSubMenu) {
        return;
    }

    UINT id = ID_FILE_OPEN_OPTICAL_DISK_START;
    for (TCHAR drive = _T('A'); drive <= _T('Z'); drive++) {
        CAtlList<CString> files;
        OpticalDiskType_t opticalDiskType = GetOpticalDiskType(drive, files);

        if (opticalDiskType != OpticalDisk_NotFound && opticalDiskType != OpticalDisk_Unknown) {
            CString label = GetDriveLabel(drive);
            if (label.IsEmpty()) {
                switch (opticalDiskType) {
                    case OpticalDisk_Audio:
                        label = _T("Audio CD");
                        break;
                    case OpticalDisk_VideoCD:
                        label = _T("(S)VCD");
                        break;
                    case OpticalDisk_DVDVideo:
                        label = _T("DVD Video");
                        break;
                    case OpticalDisk_BD:
                        label = _T("Blu-ray Disc");
                        break;
                    default:
                        ASSERT(FALSE);
                        break;
                }
            }

            CString str;
            str.Format(_T("%s (%c:)"), label.GetString(), drive);

            VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, id++, str));
        }
    }
}

void CMainFrame::SetupFiltersSubMenu()
{
    CMPCThemeMenu& subMenu = m_filtersMenu;
    // Empty the menu
    while (subMenu.RemoveMenu(0, MF_BYPOSITION));

    m_pparray.RemoveAll();
    m_ssarray.RemoveAll();

    if (GetLoadState() == MLS::LOADED) {
        UINT idf = 1; //used as an id, so make non-zero to start
        UINT ids = ID_FILTERS_SUBITEM_START;
        UINT idl = ID_FILTERSTREAMS_SUBITEM_START;

        BeginEnumFilters(m_pGB, pEF, pBF) {
            CString filterName(GetFilterName(pBF));
            if (filterName.GetLength() >= 43) {
                filterName = filterName.Left(40) + _T("...");
            }

            CLSID clsid = GetCLSID(pBF);
            if (clsid == CLSID_AVIDec) {
                CComPtr<IPin> pPin = GetFirstPin(pBF);
                AM_MEDIA_TYPE mt;
                if (pPin && SUCCEEDED(pPin->ConnectionMediaType(&mt))) {
                    DWORD c = ((VIDEOINFOHEADER*)mt.pbFormat)->bmiHeader.biCompression;
                    switch (c) {
                        case BI_RGB:
                            filterName += _T(" (RGB)");
                            break;
                        case BI_RLE4:
                            filterName += _T(" (RLE4)");
                            break;
                        case BI_RLE8:
                            filterName += _T(" (RLE8)");
                            break;
                        case BI_BITFIELDS:
                            filterName += _T(" (BITF)");
                            break;
                        default:
                            filterName.AppendFormat(_T(" (%c%c%c%c)"),
                                                    (TCHAR)((c >> 0) & 0xff),
                                                    (TCHAR)((c >> 8) & 0xff),
                                                    (TCHAR)((c >> 16) & 0xff),
                                                    (TCHAR)((c >> 24) & 0xff));
                            break;
                    }
                }
            } else if (clsid == CLSID_ACMWrapper) {
                CComPtr<IPin> pPin = GetFirstPin(pBF);
                AM_MEDIA_TYPE mt;
                if (pPin && SUCCEEDED(pPin->ConnectionMediaType(&mt))) {
                    WORD c = ((WAVEFORMATEX*)mt.pbFormat)->wFormatTag;
                    filterName.AppendFormat(_T(" (0x%04x)"), (int)c);
                }
            } else if (clsid == __uuidof(CTextPassThruFilter)
                       || clsid == __uuidof(CNullTextRenderer)
                       || clsid == GUIDFromCString(_T("{48025243-2D39-11CE-875D-00608CB78066}"))) { // ISCR
                // hide these
                continue;
            }

            CMenu internalSubMenu;
            VERIFY(internalSubMenu.CreatePopupMenu());

            int nPPages = 0;

            CComQIPtr<ISpecifyPropertyPages> pSPP = pBF;

            m_pparray.Add(pBF);
            VERIFY(internalSubMenu.AppendMenu(MF_STRING | MF_ENABLED, ids, ResStr(IDS_MAINFRM_116)));

            nPPages++;

            BeginEnumPins(pBF, pEP, pPin) {
                CString pinName = GetPinName(pPin);
                pinName.Replace(_T("&"), _T("&&"));

                if (pSPP = pPin) {
                    CAUUID caGUID;
                    caGUID.pElems = nullptr;
                    if (SUCCEEDED(pSPP->GetPages(&caGUID)) && caGUID.cElems > 0) {
                        m_pparray.Add(pPin);
                        VERIFY(internalSubMenu.AppendMenu(MF_STRING | MF_ENABLED, ids + nPPages, pinName + ResStr(IDS_MAINFRM_117)));

                        if (caGUID.pElems) {
                            CoTaskMemFree(caGUID.pElems);
                        }

                        nPPages++;
                    }
                }
            }
            EndEnumPins;

            CComQIPtr<IAMStreamSelect> pSS = pBF;
            DWORD nStreams = 0;
            if (pSS && SUCCEEDED(pSS->Count(&nStreams))) {
                DWORD flags = DWORD_MAX;
                DWORD group = DWORD_MAX;
                DWORD prevgroup = DWORD_MAX;
                LCID lcid = 0;
                WCHAR* wname = nullptr;
                UINT uMenuFlags;

                if (nStreams > 0 && nPPages > 0) {
                    VERIFY(internalSubMenu.AppendMenu(MF_SEPARATOR | MF_ENABLED));
                }

                UINT idlstart = idl;
                UINT selectedInGroup = 0;

                for (DWORD i = 0; i < nStreams; i++) {
                    m_ssarray.Add(pSS);

                    flags = group = 0;
                    wname = nullptr;
                    if (FAILED(pSS->Info(i, nullptr, &flags, &lcid, &group, &wname, nullptr, nullptr))) {
                        continue;
                    }

                    if (group != prevgroup && idl > idlstart) {
                        if (selectedInGroup) {
                            VERIFY(internalSubMenu.CheckMenuRadioItem(idlstart, idl - 1, selectedInGroup, MF_BYCOMMAND));
                            selectedInGroup = 0;
                        }
                        VERIFY(internalSubMenu.AppendMenu(MF_SEPARATOR | MF_ENABLED));
                        idlstart = idl;
                    }
                    prevgroup = group;

                    uMenuFlags = MF_STRING | MF_ENABLED;
                    if (flags & AMSTREAMSELECTINFO_EXCLUSIVE) {
                        selectedInGroup = idl;
                    } else if (flags & AMSTREAMSELECTINFO_ENABLED) {
                        uMenuFlags |= MF_CHECKED;
                    }

                    CString streamName;
                    if (!wname) {
                        streamName.LoadString(IDS_AG_UNKNOWN_STREAM);
                        streamName.AppendFormat(_T(" %lu"), i + 1);
                    } else {
                        streamName = wname;
                        streamName.Replace(_T("&"), _T("&&"));
                        CoTaskMemFree(wname);
                    }

                    VERIFY(internalSubMenu.AppendMenu(uMenuFlags, idl++, streamName));
                }
                if (selectedInGroup) {
                    VERIFY(internalSubMenu.CheckMenuRadioItem(idlstart, idl - 1, selectedInGroup, MF_BYCOMMAND));
                }

                if (nStreams == 0) {
                    pSS.Release();
                }
            }

            if (nPPages == 1 && !pSS) {
                VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, ids, filterName));
            } else {
                if (nPPages > 0 || pSS) {
                    UINT nFlags = MF_STRING | MF_POPUP | ((pSPP || pSS) ? MF_ENABLED : MF_GRAYED);
                    VERIFY(subMenu.AppendMenu(nFlags, (UINT_PTR)internalSubMenu.Detach(), filterName));
                } else {
                    VERIFY(subMenu.AppendMenu(MF_STRING | MF_GRAYED, idf, filterName));
                }
            }

            ids += nPPages;
            idf++;
        }
        EndEnumFilters;

        if (subMenu.GetMenuItemCount() > 0) {
            VERIFY(subMenu.InsertMenu(0, MF_STRING | MF_ENABLED | MF_BYPOSITION, ID_FILTERS_COPY_TO_CLIPBOARD, ResStr(IDS_FILTERS_COPY_TO_CLIPBOARD)));
            VERIFY(subMenu.InsertMenu(1, MF_SEPARATOR | MF_ENABLED | MF_BYPOSITION));
        }
        subMenu.fulfillThemeReqs();
    }
}

void CMainFrame::SetupAudioSubMenu()
{
    currentAudioLang = _T("");
    CMenu& subMenu = m_audiosMenu;
    // Empty the menu
    while (subMenu.RemoveMenu(0, MF_BYPOSITION));

    if (GetLoadState() != MLS::LOADED) {
        return;
    }

    UINT id = ID_AUDIO_SUBITEM_START;

    CComQIPtr<IAMStreamSelect> pSS = FindFilter(__uuidof(CAudioSwitcherFilter), m_pGB);

    DWORD cStreams = 0;

    if (GetPlaybackMode() == PM_DVD) {
        ULONG ulStreamsAvailable, ulCurrentStream;
        if (FAILED(m_pDVDI->GetCurrentAudio(&ulStreamsAvailable, &ulCurrentStream))) {
            return;
        }

        LCID DefLanguage;
        DVD_AUDIO_LANG_EXT ext;
        if (FAILED(m_pDVDI->GetDefaultAudioLanguage(&DefLanguage, &ext))) {
            return;
        }

        for (ULONG i = 0; i < ulStreamsAvailable; i++) {
            LCID Language;
            if (FAILED(m_pDVDI->GetAudioLanguage(i, &Language))) {
                continue;
            }

            UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;
            if (Language == DefLanguage) {
                flags |= MF_DEFAULT;
            }
            if (i == ulCurrentStream) {
                flags |= MF_CHECKED;
                if (Language) {
                    GetLocaleString(Language, LOCALE_SISO639LANGNAME2, currentAudioLang);
                }
            }

            CString str;
            if (Language) {
                GetLocaleString(Language, LOCALE_SENGLANGUAGE, str);
            } else {
                str.Format(IDS_AG_UNKNOWN, i + 1);
            }

            DVD_AudioAttributes ATR;
            if (SUCCEEDED(m_pDVDI->GetAudioAttributes(i, &ATR))) {
                switch (ATR.LanguageExtension) {
                    case DVD_AUD_EXT_NotSpecified:
                    default:
                        break;
                    case DVD_AUD_EXT_Captions:
                        str += _T(" (Captions)");
                        break;
                    case DVD_AUD_EXT_VisuallyImpaired:
                        str += _T(" (Visually Impaired)");
                        break;
                    case DVD_AUD_EXT_DirectorComments1:
                        str.AppendFormat(IDS_MAINFRM_121);
                        break;
                    case DVD_AUD_EXT_DirectorComments2:
                        str.AppendFormat(IDS_MAINFRM_122);
                        break;
                }

                CString format = GetDVDAudioFormatName(ATR);

                if (!format.IsEmpty()) {
                    str.Format(IDS_MAINFRM_11,
                               CString(str).GetString(),
                               format.GetString(),
                               ATR.dwFrequency,
                               ATR.bQuantization,
                               ATR.bNumberOfChannels,
                               ResStr(ATR.bNumberOfChannels > 1 ? IDS_MAINFRM_13 : IDS_MAINFRM_12).GetString()
                              );
                }
            }

            str.Replace(_T("&"), _T("&&"));

            VERIFY(subMenu.AppendMenu(flags, id++, str));
        }
    }
    // If available use the audio switcher for everything but DVDs
    else if (pSS && SUCCEEDED(pSS->Count(&cStreams)) && cStreams > 0) {
        VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, id++, ResStr(IDS_SUBTITLES_OPTIONS)));
        VERIFY(subMenu.AppendMenu(MF_SEPARATOR | MF_ENABLED));

        long iSel = 0;

        for (long i = 0; i < (long)cStreams; i++) {
            DWORD dwFlags;
            WCHAR* pName = nullptr;
            LCID lcid = 0;
            if (FAILED(pSS->Info(i, nullptr, &dwFlags, &lcid, nullptr, &pName, nullptr, nullptr))) {
                break;
            }
            if (dwFlags) {
                iSel = i;
                if (lcid) {
                    GetLocaleString(lcid, LOCALE_SISO639LANGNAME2, currentAudioLang);
                }
            }

            CString name(pName);
            name.Replace(_T("&"), _T("&&"));

            VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, id++, name));

            CoTaskMemFree(pName);
        }
        VERIFY(subMenu.CheckMenuRadioItem(2, 2 + cStreams - 1, 2 + iSel, MF_BYPOSITION));
    } else if (GetPlaybackMode() == PM_FILE || GetPlaybackMode() == PM_DIGITAL_CAPTURE) {
        SetupNavStreamSelectSubMenu(subMenu, id, 1);
    }
}

void CMainFrame::SetupSubtitlesSubMenu()
{
    CMenu& subMenu = m_subtitlesMenu;
    // Empty the menu
    while (subMenu.RemoveMenu(0, MF_BYPOSITION));

    if (GetLoadState() != MLS::LOADED || m_fAudioOnly) {
        return;
    }

    UINT id = ID_SUBTITLES_SUBITEM_START;

    // DVD subtitles in DVD mode are never handled by the internal subtitles renderer
    // but it is still possible to load external subtitles so we keep that if block
    // separated from the rest
    if (GetPlaybackMode() == PM_DVD) {
        ULONG ulStreamsAvailable, ulCurrentStream;
        BOOL bIsDisabled;
        if (SUCCEEDED(m_pDVDI->GetCurrentSubpicture(&ulStreamsAvailable, &ulCurrentStream, &bIsDisabled))
                && ulStreamsAvailable > 0) {
            LCID DefLanguage;
            DVD_SUBPICTURE_LANG_EXT ext;
            if (FAILED(m_pDVDI->GetDefaultSubpictureLanguage(&DefLanguage, &ext))) {
                return;
            }

            VERIFY(subMenu.AppendMenu(MF_STRING | (bIsDisabled ? 0 : MF_CHECKED), id++, ResStr(IDS_DVD_SUBTITLES_ENABLE)));
            VERIFY(subMenu.AppendMenu(MF_SEPARATOR | MF_ENABLED));

            for (ULONG i = 0; i < ulStreamsAvailable; i++) {
                LCID Language;
                if (FAILED(m_pDVDI->GetSubpictureLanguage(i, &Language))) {
                    continue;
                }

                UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;
                if (Language == DefLanguage) {
                    flags |= MF_DEFAULT;
                }
                if (i == ulCurrentStream) {
                    flags |= MF_CHECKED;
                }

                CString str;
                if (Language) {
                    GetLocaleString(Language, LOCALE_SENGLANGUAGE, str);
                } else {
                    str.Format(IDS_AG_UNKNOWN, i + 1);
                }

                DVD_SubpictureAttributes ATR;
                if (SUCCEEDED(m_pDVDI->GetSubpictureAttributes(i, &ATR))) {
                    switch (ATR.LanguageExtension) {
                        case DVD_SP_EXT_NotSpecified:
                        default:
                            break;
                        case DVD_SP_EXT_Caption_Normal:
                            str += _T("");
                            break;
                        case DVD_SP_EXT_Caption_Big:
                            str += _T(" (Big)");
                            break;
                        case DVD_SP_EXT_Caption_Children:
                            str += _T(" (Children)");
                            break;
                        case DVD_SP_EXT_CC_Normal:
                            str += _T(" (CC)");
                            break;
                        case DVD_SP_EXT_CC_Big:
                            str += _T(" (CC Big)");
                            break;
                        case DVD_SP_EXT_CC_Children:
                            str += _T(" (CC Children)");
                            break;
                        case DVD_SP_EXT_Forced:
                            str += _T(" (Forced)");
                            break;
                        case DVD_SP_EXT_DirectorComments_Normal:
                            str += _T(" (Director Comments)");
                            break;
                        case DVD_SP_EXT_DirectorComments_Big:
                            str += _T(" (Director Comments, Big)");
                            break;
                        case DVD_SP_EXT_DirectorComments_Children:
                            str += _T(" (Director Comments, Children)");
                            break;
                    }
                }

                str.Replace(_T("&"), _T("&&"));

                VERIFY(subMenu.AppendMenu(flags, id++, str));
            }
        }
    }

    POSITION pos = m_pSubStreams.GetHeadPosition();

    if (GetPlaybackMode() == PM_DIGITAL_CAPTURE) {
        SetupNavStreamSelectSubMenu(subMenu, id, 2);
    } else if (pos) { // Internal subtitles renderer
        int nItemsBeforeStart = id - ID_SUBTITLES_SUBITEM_START;
        if (nItemsBeforeStart > 0) {
            VERIFY(subMenu.AppendMenu(MF_SEPARATOR));
            nItemsBeforeStart += 2; // Separators
        }

        // Build the static menu's items
        bool bTextSubtitles = false;
        VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, id++, ResStr(IDS_SUBTITLES_OPTIONS)));
        VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, id++, ResStr(IDS_SUBTITLES_STYLES)));
        VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, id++, ResStr(IDS_SUBTITLES_RELOAD)));
        VERIFY(subMenu.AppendMenu(MF_SEPARATOR));

        VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, id++, ResStr(IDS_SUBTITLES_ENABLE)));
        VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, id++, ResStr(IDS_SUBTITLES_DEFAULT_STYLE)));
        VERIFY(subMenu.AppendMenu(MF_SEPARATOR));

        // Build the dynamic menu's items
        int i = 0, iSelected = -1;
        while (pos) {
            SubtitleInput& subInput = m_pSubStreams.GetNext(pos);

            if (CComQIPtr<IAMStreamSelect> pSSF = subInput.pSourceFilter) {
                DWORD cStreams;
                if (FAILED(pSSF->Count(&cStreams))) {
                    continue;
                }

                for (int j = 0, cnt = (int)cStreams; j < cnt; j++) {
                    DWORD dwFlags, dwGroup;
                    LCID lcid = 0;
                    WCHAR* pszName = nullptr;

                    if (FAILED(pSSF->Info(j, nullptr, &dwFlags, &lcid, &dwGroup, &pszName, nullptr, nullptr))
                            || !pszName) {
                        continue;
                    }

                    CString name(pszName);
                    CString lcname = CString(name).MakeLower();
                    CoTaskMemFree(pszName);

                    if (dwGroup != 2) {
                        continue;
                    }

                    if (subInput.pSubStream == m_pCurrentSubInput.pSubStream
                            && dwFlags & (AMSTREAMSELECTINFO_ENABLED | AMSTREAMSELECTINFO_EXCLUSIVE)) {
                        iSelected = i;
                    }

                    CString str;

                    if (lcname.Find(_T(" off")) >= 0) {
                        str.LoadString(IDS_AG_DISABLED);
                    } else {
                        if (lcid != 0) {
                            GetLocaleString(lcid, LOCALE_SENGLANGUAGE, str);
                        }

                        CString lcstr = CString(str).MakeLower();

                        if (str.IsEmpty() || lcname.Find(lcstr) >= 0) {
                            str = name;
                        } else if (!name.IsEmpty()) {
                            str = name + _T(" (") + str + _T(")");
                        }
                    }

                    str.Replace(_T("&"), _T("&&"));
                    VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, id++, str));
                    i++;
                }
            } else {
                CComPtr<ISubStream> pSubStream = subInput.pSubStream;
                if (!pSubStream) {
                    continue;
                }

                if (subInput.pSubStream == m_pCurrentSubInput.pSubStream) {
                    iSelected = i + pSubStream->GetStream();
                }

                for (int j = 0, cnt = pSubStream->GetStreamCount(); j < cnt; j++) {
                    CComHeapPtr<WCHAR> pName;
                    if (SUCCEEDED(pSubStream->GetStreamInfo(j, &pName, nullptr))) {
                        CString name(pName);
                        name.Replace(_T("&"), _T("&&"));

                        VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, id++, name));
                    } else {
                        VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, id++, ResStr(IDS_AG_UNKNOWN_STREAM)));
                    }
                    i++;
                }
            }

            if (subInput.pSubStream == m_pCurrentSubInput.pSubStream) {
                CLSID clsid;
                if (SUCCEEDED(subInput.pSubStream->GetClassID(&clsid))
                        && clsid == __uuidof(CRenderedTextSubtitle)) {
                    bTextSubtitles = true;
                }
            }

            // TODO: find a better way to group these entries
            /*if (pos && m_pSubStreams.GetAt(pos).subStream) {
                CLSID cur, next;
                pSubStream->GetClassID(&cur);
                m_pSubStreams.GetAt(pos).subStream->GetClassID(&next);

                if (cur != next) {
                    VERIFY(subMenu.AppendMenu(MF_SEPARATOR));
                }
            }*/
        }

        // Set the menu's items' state
        const CAppSettings& s = AfxGetAppSettings();
        // Style
        if (!bTextSubtitles) {
            subMenu.EnableMenuItem(nItemsBeforeStart + 1, MF_BYPOSITION | MF_GRAYED);
        }
        // Enabled
        if (s.fEnableSubtitles) {
            subMenu.CheckMenuItem(nItemsBeforeStart + 4, MF_BYPOSITION | MF_CHECKED);
        }
        // Default style
        // TODO: foxX - default subtitles style toggle here; still wip
        if (!bTextSubtitles) {
            subMenu.EnableMenuItem(nItemsBeforeStart + 5, MF_BYPOSITION | MF_GRAYED);
        }
        if (s.fUseDefaultSubtitlesStyle) {
            subMenu.CheckMenuItem(nItemsBeforeStart + 5, MF_BYPOSITION | MF_CHECKED);
        }
        VERIFY(subMenu.CheckMenuRadioItem(nItemsBeforeStart + 7, nItemsBeforeStart + 7 + i - 1, nItemsBeforeStart + 7 + iSelected, MF_BYPOSITION));
    } else if (GetPlaybackMode() == PM_FILE) {
        SetupNavStreamSelectSubMenu(subMenu, id, 2);
    }
}

void CMainFrame::SetupVideoStreamsSubMenu()
{
    CMenu& subMenu = m_videoStreamsMenu;
    // Empty the menu
    while (subMenu.RemoveMenu(0, MF_BYPOSITION));

    if (GetLoadState() != MLS::LOADED) {
        return;
    }

    UINT id = ID_VIDEO_STREAMS_SUBITEM_START;

    if (GetPlaybackMode() == PM_FILE) {
        SetupNavStreamSelectSubMenu(subMenu, id, 0);
    } else if (GetPlaybackMode() == PM_DVD) {
        ULONG ulStreamsAvailable, ulCurrentStream;
        if (FAILED(m_pDVDI->GetCurrentAngle(&ulStreamsAvailable, &ulCurrentStream))) {
            return;
        }

        if (ulStreamsAvailable < 2) {
            return;    // one choice is not a choice...
        }

        for (ULONG i = 1; i <= ulStreamsAvailable; i++) {
            UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;
            if (i == ulCurrentStream) {
                flags |= MF_CHECKED;
            }

            CString str;
            str.Format(IDS_AG_ANGLE, i);

            VERIFY(subMenu.AppendMenu(flags, id++, str));
        }
    }
}

void CMainFrame::SetupJumpToSubMenus(CMenu* parentMenu /*= nullptr*/, int iInsertPos /*= -1*/)
{
    const CAppSettings& s = AfxGetAppSettings();
    auto emptyMenu = [&](CMPCThemeMenu & menu) {
        while (menu.RemoveMenu(0, MF_BYPOSITION));
    };

    // Empty the submenus
    emptyMenu(m_chaptersMenu);
    emptyMenu(m_titlesMenu);
    emptyMenu(m_playlistMenu);
    emptyMenu(m_BDPlaylistMenu);
    emptyMenu(m_channelsMenu);
    // Remove the submenus from the "Navigate" menu
    if (parentMenu && iInsertPos >= 0) {
        for (; m_nJumpToSubMenusCount > 0; m_nJumpToSubMenusCount--) {
            VERIFY(parentMenu->RemoveMenu(iInsertPos, MF_BYPOSITION));
        }
    }

    if (GetLoadState() != MLS::LOADED) {
        return;
    }

    UINT id = ID_NAVIGATE_JUMPTO_SUBITEM_START, idStart, idSelected;

    auto menuStartRadioSection = [&]() {
        idStart = id;
        idSelected = UINT_ERROR;
    };
    auto menuEndRadioSection = [&](CMenu & menu) {
        if (idSelected != UINT_ERROR) {
            VERIFY(menu.CheckMenuRadioItem(idStart, id - 1, idSelected,
                                           idStart >= ID_NAVIGATE_JUMPTO_SUBITEM_START ? MF_BYCOMMAND : MF_BYPOSITION));
        }
    };
    auto addSubMenuIfPossible = [&](CString subMenuName, CMenu & subMenu) {
        if (parentMenu && iInsertPos >= 0) {
            if (parentMenu->InsertMenu(iInsertPos + m_nJumpToSubMenusCount, MF_POPUP | MF_BYPOSITION,
                                       (UINT_PTR)(HMENU)subMenu, subMenuName)) {
                CMPCThemeMenu::fulfillThemeReqsItem(parentMenu, iInsertPos + m_nJumpToSubMenusCount);
                m_nJumpToSubMenusCount++;
            } else {
                ASSERT(FALSE);
            }
        }
    };

    if (GetPlaybackMode() == PM_FILE) {
        if (m_MPLSPlaylist.GetCount() > 1) {
            menuStartRadioSection();
            POSITION pos = m_MPLSPlaylist.GetHeadPosition();
            while (pos) {
                UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;
                CHdmvClipInfo::PlaylistItem Item = m_MPLSPlaylist.GetNext(pos);
                CString time = _T("[") + ReftimeToString2(Item.Duration()) + _T("]");
                CString name = PathUtils::StripPathOrUrl(Item.m_strFileName);

                if (name == m_wndPlaylistBar.m_pl.GetHead().GetLabel()) {
                    idSelected = id;
                }

                name.Replace(_T("&"), _T("&&"));
                VERIFY(m_BDPlaylistMenu.AppendMenu(flags, id++, name + '\t' + time));
            }
            menuEndRadioSection(m_BDPlaylistMenu);
            addSubMenuIfPossible(StrRes(IDS_NAVIGATE_BD_PLAYLISTS), m_BDPlaylistMenu);
        }

        SetupChapters();
        if (m_pCB->ChapGetCount() > 1) {
            REFERENCE_TIME rt = GetPos();
            DWORD j = m_pCB->ChapLookup(&rt, nullptr);
            menuStartRadioSection();
            for (DWORD i = 0; i < m_pCB->ChapGetCount(); i++, id++) {
                rt = 0;
                CComBSTR bstr;
                if (FAILED(m_pCB->ChapGet(i, &rt, &bstr))) {
                    continue;
                }

                CString time = _T("[") + ReftimeToString2(rt) + _T("]");

                CString name = CString(bstr);
                name.Replace(_T("&"), _T("&&"));
                name.Replace(_T("\t"), _T(" "));

                UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;
                if (i == j) {
                    idSelected = id;
                }

                VERIFY(m_chaptersMenu.AppendMenu(flags, id, name + '\t' + time));
            }
            menuEndRadioSection(m_chaptersMenu);
            addSubMenuIfPossible(StrRes(IDS_NAVIGATE_CHAPTERS), m_chaptersMenu);
        }

        if (m_wndPlaylistBar.GetCount() > 1) {
            menuStartRadioSection();
            POSITION pos = m_wndPlaylistBar.m_pl.GetHeadPosition();
            while (pos && id < ID_NAVIGATE_JUMPTO_SUBITEM_START + 128) {
                UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;
                if (pos == m_wndPlaylistBar.m_pl.GetPos()) {
                    idSelected = id;
                }
                CPlaylistItem& pli = m_wndPlaylistBar.m_pl.GetNext(pos);
                CString name = pli.GetLabel();
                name.Replace(_T("&"), _T("&&"));
                VERIFY(m_playlistMenu.AppendMenu(flags, id++, name));
            }
            menuEndRadioSection(m_playlistMenu);
            addSubMenuIfPossible(StrRes(IDS_NAVIGATE_PLAYLIST), m_playlistMenu);
        }
    } else if (GetPlaybackMode() == PM_DVD) {
        ULONG ulNumOfVolumes, ulVolume, ulNumOfTitles, ulNumOfChapters, ulUOPs;
        DVD_DISC_SIDE Side;
        DVD_PLAYBACK_LOCATION2 Location;

        if (SUCCEEDED(m_pDVDI->GetCurrentLocation(&Location))
                && SUCCEEDED(m_pDVDI->GetCurrentUOPS(&ulUOPs))
                && SUCCEEDED(m_pDVDI->GetNumberOfChapters(Location.TitleNum, &ulNumOfChapters))
                && SUCCEEDED(m_pDVDI->GetDVDVolumeInfo(&ulNumOfVolumes, &ulVolume, &Side, &ulNumOfTitles))) {
            menuStartRadioSection();
            for (ULONG i = 1; i <= ulNumOfTitles; i++) {
                UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;
                if (i == Location.TitleNum) {
                    idSelected = id;
                }
                if (ulUOPs & UOP_FLAG_Play_Title) {
                    flags |= MF_GRAYED;
                }

                CString str;
                str.Format(IDS_AG_TITLE, i);

                VERIFY(m_titlesMenu.AppendMenu(flags, id++, str));
            }
            menuEndRadioSection(m_titlesMenu);
            addSubMenuIfPossible(StrRes(IDS_NAVIGATE_TITLES), m_titlesMenu);

            menuStartRadioSection();
            for (ULONG i = 1; i <= ulNumOfChapters; i++) {
                UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;
                if (i == Location.ChapterNum) {
                    idSelected = id;
                }
                if (ulUOPs & UOP_FLAG_Play_Chapter) {
                    flags |= MF_GRAYED;
                }

                CString str;
                str.Format(IDS_AG_CHAPTER, i);

                VERIFY(m_chaptersMenu.AppendMenu(flags, id++, str));
            }
            menuEndRadioSection(m_chaptersMenu);
            addSubMenuIfPossible(StrRes(IDS_NAVIGATE_CHAPTERS), m_chaptersMenu);
        }
    } else if (GetPlaybackMode() == PM_DIGITAL_CAPTURE) {
        menuStartRadioSection();
        for (const auto& channel : s.m_DVBChannels) {
            UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;

            if (channel.GetPrefNumber() == s.nDVBLastChannel) {
                idSelected = id;
            }
            VERIFY(m_channelsMenu.AppendMenu(flags, ID_NAVIGATE_JUMPTO_SUBITEM_START + channel.GetPrefNumber(), channel.GetName()));
            id++;
        }
        menuEndRadioSection(m_channelsMenu);
        addSubMenuIfPossible(StrRes(IDS_NAVIGATE_CHANNELS), m_channelsMenu);
    }
}

void CMainFrame::SetupNavStreamSelectSubMenu(CMenu& subMenu, UINT id, DWORD dwSelGroup)
{
    bool bAddSeparator = false;

    auto addStreamSelectFilter = [&](CComPtr<IAMStreamSelect> pSS) {
        DWORD cStreams;
        if (!pSS || FAILED(pSS->Count(&cStreams))) {
            return;
        }

        bool bAdded = false;
        for (DWORD i = 0; i < cStreams; i++) {
            DWORD dwFlags, dwGroup;
            LCID lcid = 0;
            CComHeapPtr<WCHAR> pszName;

            if (FAILED(pSS->Info(i, nullptr, &dwFlags, &lcid, &dwGroup, &pszName, nullptr, nullptr))
                    || !pszName) {
                continue;
            }

            if (dwGroup != dwSelGroup) {
                continue;
            }

            CString str;
            CString lcname(pszName);
            lcname.MakeLower();

            if (lcname.Find(_T(" off")) >= 0) {
                str.LoadString(IDS_AG_DISABLED);
            } else {
                if (lcid && lcid != LCID(-1)) {
                    GetLocaleString(lcid, LOCALE_SENGLANGUAGE, str);
                }

                CString lcstr(str);
                lcstr.MakeLower();

                if (str.IsEmpty() || lcname.Find(lcstr) >= 0) {
                    str = pszName;
                } else if (wcslen(pszName)) {
                    str = CString(pszName) + _T(" (") + str + _T(")");
                }
            }

            UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;
            if (dwFlags) {
                flags |= MF_CHECKED;
            }

            if (bAddSeparator) {
                VERIFY(subMenu.AppendMenu(MF_SEPARATOR));
                bAddSeparator = false;
            }
            bAdded = true;

            str.Replace(_T("&"), _T("&&"));
            VERIFY(subMenu.AppendMenu(flags, id++, str));
        }

        if (bAdded) {
            bAddSeparator = true;
        }
    };

    CComQIPtr<IAMStreamSelect> pSS;

    BeginEnumFilters(m_pGB, pEF, pBF) {
        CLSID filterid = GetCLSID(pBF);
        if (filterid == __uuidof(CAudioSwitcherFilter) || filterid == CLSID_MPCBEAudioRenderer) {
            continue;
        }

        if (pSS = pBF) {
            addStreamSelectFilter(pSS);
        }
    }
    EndEnumFilters

    if (pSS = m_pGB) {
        addStreamSelectFilter(pSS);
    }
}

void CMainFrame::OnNavStreamSelectSubMenu(UINT id, DWORD dwSelGroup)
{
    auto processStreamSelectFilter = [&](CComPtr<IAMStreamSelect> pSS) {
        bool bSelected = false;

        DWORD cStreams;
        if (SUCCEEDED(pSS->Count(&cStreams))) {
            for (int i = 0, j = cStreams; i < j; i++) {
                DWORD dwFlags, dwGroup;
                LCID lcid = 0;
                CComHeapPtr<WCHAR> pszName;

                if (FAILED(pSS->Info(i, nullptr, &dwFlags, &lcid, &dwGroup, &pszName, nullptr, nullptr))
                        || !pszName) {
                    continue;
                }

                if (dwGroup != dwSelGroup) {
                    continue;
                }

                if (id == 0) {
                    pSS->Enable(i, AMSTREAMSELECTENABLE_ENABLE);
                    bSelected = true;
                    break;
                }

                id--;
            }
        }

        return bSelected;
    };

    CComQIPtr<IAMStreamSelect> pSS;

    BeginEnumFilters(m_pGB, pEF, pBF) {
        if (GetCLSID(pBF) == __uuidof(CAudioSwitcherFilter)) {
            continue;
        }

        if (pSS = pBF) {
            if (processStreamSelectFilter(pSS)) {
                return;
            }
        }
    }
    EndEnumFilters

    if (pSS = m_pGB) {
        processStreamSelectFilter(pSS);
    }
}

void CMainFrame::OnStreamSelect(bool bForward, DWORD dwSelGroup)
{
    ASSERT(dwSelGroup == 1 || dwSelGroup == 2);
    CComQIPtr<IAMStreamSelect> pSS;

    BeginEnumFilters(m_pGB, pEF, pBF) {
        if (GetCLSID(pBF) == __uuidof(CAudioSwitcherFilter)) {
            continue;
        }

        if (!(pSS = pBF)) {
            continue;
        }

        DWORD cStreams;
        if (FAILED(pSS->Count(&cStreams))) {
            continue;
        }

        std::vector<std::tuple<DWORD, LCID, CString>> streams;
        size_t currentSel = SIZE_MAX;
        for (DWORD i = 0; i < cStreams; i++) {
            DWORD dwFlags, dwGroup;
            LCID lcid = 0;
            CComHeapPtr<WCHAR> pszName;

            if (FAILED(pSS->Info(i, nullptr, &dwFlags, &lcid, &dwGroup, &pszName, nullptr, nullptr))
                    || !pszName) {
                continue;
            }

            if (dwGroup != dwSelGroup) {
                continue;
            }

            if (dwFlags) {
                currentSel = streams.size();
            }
            streams.emplace_back(i, lcid, CString(pszName));
        }

        size_t count = streams.size();
        if (count && currentSel != SIZE_MAX) {
            size_t requested = (bForward ? currentSel + 1 : currentSel - 1) % count;
            DWORD id;
            LCID lcid = 0;
            CString name;
            std::tie(id, lcid, name) = streams.at(requested);
            pSS->Enable(id, AMSTREAMSELECTENABLE_ENABLE);
            m_OSD.DisplayMessage(OSD_TOPLEFT, GetStreamOSDString(name, lcid, dwSelGroup));
            if (lcid && AfxGetAppSettings().fEnableSubtitles) {
                if (dwSelGroup == 2) {
                    GetLocaleString(lcid, LOCALE_SISO639LANGNAME2, currentSubLang);
                } else {
                    GetLocaleString(lcid, LOCALE_SISO639LANGNAME2, currentAudioLang);
                }
            }
            break;
        }
    }
    EndEnumFilters
}

CString CMainFrame::GetStreamOSDString(CString name, LCID lcid, DWORD dwSelGroup)
{
    name.Replace(_T("\t"), _T(" - "));
    CString sLcid;
    if (lcid && lcid != LCID(-1)) {
        GetLocaleString(lcid, LOCALE_SENGLANGUAGE, sLcid);
    }
    if (!sLcid.IsEmpty() && CString(name).MakeLower().Find(CString(sLcid).MakeLower()) < 0) {
        name += _T(" (") + sLcid + _T(")");
    }
    CString strMessage;
    if (dwSelGroup == 1) {
        int n = 0;
        if (name.Find(_T("A:")) == 0) {
            n = 2;
        }
        strMessage.Format(IDS_AUDIO_STREAM, name.Mid(n).Trim().GetString());
    } else if (dwSelGroup == 2) {
        int n = 0;
        if (name.Find(_T("S:")) == 0) {
            n = 2;
        }
        strMessage.Format(IDS_SUBTITLE_STREAM, name.Mid(n).Trim().GetString());
    }
    return strMessage;
}

void CMainFrame::SetupRecentFilesSubMenu()
{
    CMenu& subMenu = m_recentFilesMenu;
    // Empty the menu
    while (subMenu.RemoveMenu(0, MF_BYPOSITION));

    UINT id = ID_RECENT_FILE_START;
    auto& s = AfxGetAppSettings();
    auto& MRU = s.MRU;
    MRU.ReadList();

    bool bNoEmptyMRU = false;
    for (int i = 0; i < MRU.GetSize(); i++) {
        if (MRU[i].fns.GetCount() > 0 && !MRU[i].fns.GetHead().IsEmpty()) {
            bNoEmptyMRU = true;
            break;
        }
    }
    if (bNoEmptyMRU) {
        VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, ID_RECENT_FILES_CLEAR, ResStr(IDS_RECENT_FILES_CLEAR)));
        VERIFY(subMenu.AppendMenu(MF_SEPARATOR | MF_ENABLED));

        for (int i = 0; i < MRU.GetSize(); i++) {
            UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;
            if (!MRU[i].fns.GetHead().IsEmpty()) {
                CString p = MRU[i].cue.IsEmpty() ? MRU[i].fns.GetHead() : MRU[i].cue;
                if (s.bUseTitleInRecentFileList && !MRU[i].title.IsEmpty()) {
                    CString title(MRU[i].title);
                    if (title.GetLength() > 100) {
                        title = title.Left(40) + _T("~~~") + title.Right(57);
                    }
                    int targetlen = 150 - title.GetLength();
                    if (PathUtils::IsURL(p)) {
                        p.Format(_T("%s (%s)"), static_cast<LPCWSTR>(title), static_cast<LPCWSTR>(ShortenURL(p, targetlen, true)));
                    } else {
                        CString fn = PathUtils::StripPathOrUrl(p);
                        if (fn.GetLength() > targetlen) { // If file name is too long, cut middle part.
                            int l = fn.GetLength();
                            fn.Format(_T("%s~~~%s"), static_cast<LPCWSTR>(fn.Left(l / 2 - 2 + (l % 2))), static_cast<LPCWSTR>(fn.Right(l / 2 - 1)));
                        }
                        p.Format(_T("%s (%s)"), static_cast<LPCWSTR>(title), static_cast<LPCWSTR>(fn));
                    }
                }
                else {
                    if (PathUtils::IsURL(p)) {
                        p = ShortenURL(p, 150);
                    }
                    if (p.GetLength() > 150) {
                        p.Format(_T("%s~~~%s"), static_cast<LPCWSTR>(p.Left(60)), static_cast<LPCWSTR>(p.Right(87)));
                    }
                }
                p.Replace(_T("&"), _T("&&"));
                VERIFY(subMenu.AppendMenu(flags, id, p));
            }
            id++;
        }
    }
}

void CMainFrame::SetupFavoritesSubMenu()
{
    CMenu& subMenu = m_favoritesMenu;
    // Empty the menu
    while (subMenu.RemoveMenu(0, MF_BYPOSITION));

    const CAppSettings& s = AfxGetAppSettings();

    VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, ID_FAVORITES_ADD, ResStr(IDS_FAVORITES_ADD)));
    VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, ID_FAVORITES_ORGANIZE, ResStr(IDS_FAVORITES_ORGANIZE)));

    UINT nLastGroupStart = subMenu.GetMenuItemCount();
    UINT id = ID_FAVORITES_FILE_START;
    CAtlList<CString> favs;
    AfxGetAppSettings().GetFav(FAV_FILE, favs);
    POSITION pos = favs.GetHeadPosition();

    while (pos) {
        UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;

        CString f_str = favs.GetNext(pos);
        f_str.Replace(_T("&"), _T("&&"));
        f_str.Replace(_T("\t"), _T(" "));

        CAtlList<CString> sl;
        ExplodeEsc(f_str, sl, _T(';'), 3);

        f_str = sl.RemoveHead();

        CString str;

        if (!sl.IsEmpty()) {
            // pos
            REFERENCE_TIME rt = 0;
            if (1 == _stscanf_s(sl.GetHead(), _T("%I64d"), &rt) && rt > 0) {
                DVD_HMSF_TIMECODE hmsf = RT2HMSF(rt);
                str.Format(_T("[%02u:%02u:%02u]"), hmsf.bHours, hmsf.bMinutes, hmsf.bSeconds);
            }

            // relative drive
            if (sl.GetCount() > 1) {   // Here to prevent crash if old favorites settings are present
                sl.RemoveHead();

                BOOL bRelativeDrive = FALSE;
                if (_stscanf_s(sl.GetHead(), _T("%d"), &bRelativeDrive) == 1) {
                    if (bRelativeDrive) {
                        str = _T("[RD]") + str;
                    }
                }
            }
            if (!str.IsEmpty()) {
                f_str.AppendFormat(_T("\t%.14s"), str.GetString());
            }
        }

        if (!f_str.IsEmpty()) {
            VERIFY(subMenu.AppendMenu(flags, id, f_str));
        }

        id++;
    }

    if (id > ID_FAVORITES_FILE_START) {
        VERIFY(subMenu.InsertMenu(nLastGroupStart, MF_SEPARATOR | MF_ENABLED | MF_BYPOSITION));
    }

    nLastGroupStart = subMenu.GetMenuItemCount();

    id = ID_FAVORITES_DVD_START;
    s.GetFav(FAV_DVD, favs);
    pos = favs.GetHeadPosition();

    while (pos) {
        UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;

        CString str = favs.GetNext(pos);
        str.Replace(_T("&"), _T("&&"));

        CAtlList<CString> sl;
        ExplodeEsc(str, sl, _T(';'), 2);

        str = sl.RemoveHead();

        if (!sl.IsEmpty()) {
            // TODO
        }

        if (!str.IsEmpty()) {
            VERIFY(subMenu.AppendMenu(flags, id, str));
        }

        id++;
    }

    if (id > ID_FAVORITES_DVD_START) {
        VERIFY(subMenu.InsertMenu(nLastGroupStart, MF_SEPARATOR | MF_ENABLED | MF_BYPOSITION));
    }

    nLastGroupStart = subMenu.GetMenuItemCount();

    id = ID_FAVORITES_DEVICE_START;

    s.GetFav(FAV_DEVICE, favs);

    pos = favs.GetHeadPosition();
    while (pos) {
        UINT flags = MF_BYCOMMAND | MF_STRING | MF_ENABLED;

        CString str = favs.GetNext(pos);
        str.Replace(_T("&"), _T("&&"));

        CAtlList<CString> sl;
        ExplodeEsc(str, sl, _T(';'), 2);

        str = sl.RemoveHead();

        if (!str.IsEmpty()) {
            VERIFY(subMenu.AppendMenu(flags, id, str));
        }

        id++;
    }
}

bool CMainFrame::SetupShadersSubMenu()
{
    const auto& s = AfxGetAppSettings();

    CMenu& subMenu = m_shadersMenu;
    // Empty the menu
    while (subMenu.RemoveMenu(0, MF_BYPOSITION));

    if (!(s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM || s.iDSVideoRendererType == VIDRNDT_DS_SYNC
        || s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS || s.iDSVideoRendererType == VIDRNDT_DS_MADVR || s.iDSVideoRendererType == VIDRNDT_DS_MPCVR)) {
        return false;
    }        

    subMenu.AppendMenu(MF_BYCOMMAND | MF_STRING | MF_ENABLED, ID_PRESIZE_SHADERS_TOGGLE, ResStr(IDS_PRESIZE_SHADERS_TOGGLE));
    subMenu.AppendMenu(MF_BYCOMMAND | MF_STRING | MF_ENABLED, ID_POSTSIZE_SHADERS_TOGGLE, ResStr(IDS_POSTSIZE_SHADERS_TOGGLE));
    VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, ID_SHADERS_SELECT, ResStr(IDS_SHADERS_SELECT)));
    VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, ID_VIEW_DEBUGSHADERS, ResStr(IDS_SHADERS_DEBUG)));

    auto presets = s.m_Shaders.GetPresets();
    if (!presets.empty()) {
        CString current;
        bool selected = s.m_Shaders.GetCurrentPresetName(current);
        VERIFY(subMenu.AppendMenu(MF_SEPARATOR));
        UINT nID = ID_SHADERS_PRESETS_START;
        for (const auto& pair : presets) {
            if (nID > ID_SHADERS_PRESETS_END) {
                // too many presets
                ASSERT(FALSE);
                break;
            }
            VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, nID, pair.first));
            if (selected && pair.first == current) {
                VERIFY(subMenu.CheckMenuRadioItem(nID, nID, nID, MF_BYCOMMAND));
                selected = false;
            }
            nID++;
        }
    }
    return true;
}

/////////////

void CMainFrame::SetAlwaysOnTop(int iOnTop)
{
    CAppSettings& s = AfxGetAppSettings();

    if (!m_fFullScreen && !IsD3DFullScreenMode()) {
        const CWnd* pInsertAfter = nullptr;

        if (iOnTop == 0) {
            // We only want to disable "On Top" once so that
            // we don't interfere with other window manager
            if (s.iOnTop) {
                pInsertAfter = &wndNoTopMost;
            }
        } else if (iOnTop == 1) {
            pInsertAfter = &wndTopMost;
        } else if (iOnTop == 2) {
            pInsertAfter = (GetMediaState() == State_Running) ? &wndTopMost : &wndNoTopMost;
        } else { // if (iOnTop == 3)
            pInsertAfter = (GetMediaState() == State_Running && !m_fAudioOnly) ? &wndTopMost : &wndNoTopMost;
        }

        if (pInsertAfter) {
            SetWindowPos(pInsertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }

    s.iOnTop = iOnTop;
}

bool CMainFrame::WindowExpectedOnTop() {
    return (AfxGetAppSettings().iOnTop == 1 ||
        (AfxGetAppSettings().iOnTop == 2 && GetMediaState() == State_Running) ||
        (AfxGetAppSettings().iOnTop == 3 && GetMediaState() == State_Running && !m_fAudioOnly));
}

void CMainFrame::AddTextPassThruFilter()
{
    BeginEnumFilters(m_pGB, pEF, pBF) {
        if (!IsSplitter(pBF)) {
            continue;
        }

        BeginEnumPins(pBF, pEP, pPin) {
            CComPtr<IPin> pPinTo;
            AM_MEDIA_TYPE mt;
            if (SUCCEEDED(pPin->ConnectedTo(&pPinTo)) && pPinTo
                    && SUCCEEDED(pPin->ConnectionMediaType(&mt))
                    && (mt.majortype == MEDIATYPE_Text || mt.majortype == MEDIATYPE_Subtitle)) {
                InsertTextPassThruFilter(pBF, pPin, pPinTo);
            }
        }
        EndEnumPins;
    }
    EndEnumFilters;
}

HRESULT CMainFrame::InsertTextPassThruFilter(IBaseFilter* pBF, IPin* pPin, IPin* pPinTo)
{
    HRESULT hr;
    CComQIPtr<IBaseFilter> pTPTF = DEBUG_NEW CTextPassThruFilter(this);
    CStringW name;
    name.Format(L"TextPassThru%p", static_cast<void*>(pTPTF));
    if (FAILED(hr = m_pGB->AddFilter(pTPTF, name))) {
        return hr;
    }

    OAFilterState fs = GetMediaState();
    if (fs == State_Running || fs == State_Paused) {
        m_pMC->Stop();
    }

    hr = pPinTo->Disconnect();
    hr = pPin->Disconnect();

    if (FAILED(hr = m_pGB->ConnectDirect(pPin, GetFirstPin(pTPTF, PINDIR_INPUT), nullptr))
            || FAILED(hr = m_pGB->ConnectDirect(GetFirstPin(pTPTF, PINDIR_OUTPUT), pPinTo, nullptr))) {
        hr = m_pGB->ConnectDirect(pPin, pPinTo, nullptr);
    } else {
        SubtitleInput subInput(CComQIPtr<ISubStream>(pTPTF), pBF);
        m_pSubStreams.AddTail(subInput);
    }

    if (fs == State_Running) {
        m_pMC->Run();
    } else if (fs == State_Paused) {
        m_pMC->Pause();
    }

    return hr;
}

bool CMainFrame::LoadSubtitle(CString fn, SubtitleInput* pSubInput /*= nullptr*/, bool bAutoLoad /*= false*/)
{
    CAppSettings& s = AfxGetAppSettings();
    CComQIPtr<ISubStream> pSubStream;

    if (!s.IsISRAutoLoadEnabled() && (FindFilter(CLSID_VSFilter, m_pGB) || FindFilter(CLSID_XySubFilter, m_pGB))) {
        // Prevent ISR from loading if VSFilter is already in graph.
        // TODO: Support VSFilter natively (see ticket #4122)
        // Note that this doesn't affect ISR auto-loading if any sub renderer force loading itself into the graph.
        // VSFilter like filters can be blocked when building the graph and ISR auto-loading is enabled but some
        // users don't want that.
        return false;
    }

    if (GetPlaybackMode() == PM_FILE && !s.fDisableInternalSubtitles && !FindFilter(__uuidof(CTextPassThruFilter), m_pGB)) {
        // Add TextPassThru filter if it isn't already in the graph. (i.e ISR hasn't been loaded before)
        // This will load all embedded subtitle tracks when user triggers ISR (load external subtitle file) for the first time.
        AddTextPassThruFilter();
    }

    CString videoName;
    if (GetPlaybackMode() == PM_FILE) {
        videoName = m_wndPlaylistBar.GetCurFileName();
    }

    CString ext = CPath(fn).GetExtension().MakeLower();

    if (!pSubStream && (ext == _T(".idx") || !bAutoLoad && ext == _T(".sub"))) {
        CAutoPtr<CVobSubFile> pVSF(DEBUG_NEW CVobSubFile(&m_csSubLock));
        if (pVSF && pVSF->Open(fn) && pVSF->GetStreamCount() > 0) {
            pSubStream = pVSF.Detach();
        }
    }

    if (!pSubStream) {
        CAutoPtr<CRenderedTextSubtitle> pRTS(DEBUG_NEW CRenderedTextSubtitle(&m_csSubLock));
        if (pRTS) {
#if USE_LIBASS
            SubRendererSettings srs = AfxGetAppSettings().GetSubRendererSettings();
            pRTS->SetSubRenderSettings(srs);
#endif
            pRTS->SetDefaultStyle(s.subtitlesDefStyle);
            if (pRTS->Open(fn, DEFAULT_CHARSET, _T(""), videoName) && pRTS->GetStreamCount() > 0) {
#if USE_LIBASS
                pRTS->SetFilterGraph(m_pGB);
#endif
                pSubStream = pRTS.Detach();
            }
        }
    }

    if (!pSubStream) {
        CAutoPtr<CPGSSubFile> pPSF(DEBUG_NEW CPGSSubFile(&m_csSubLock));
        if (pPSF && pPSF->Open(fn, _T(""), videoName) && pPSF->GetStreamCount() > 0) {
            pSubStream = pPSF.Detach();
        }
    }

    if (pSubStream) {
        SubtitleInput subInput(pSubStream);
        m_ExternalSubstreams.push_back(pSubStream);
        m_pSubStreams.AddTail(subInput);

        // Temporarily load fonts from 'Fonts' folder - Begin
        CString path = PathUtils::DirName(fn) + L"\\fonts\\";

        if (::PathIsDirectory(path)) {
            WIN32_FIND_DATA fd = {0};
            HANDLE hFind;
            
            hFind = FindFirstFile(path + L"*.ttf", &fd);
            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    m_FontInstaller.InstallTempFontFile(path + fd.cFileName);
                } while (FindNextFile(hFind, &fd));
                
                FindClose(hFind);
            }

            hFind = FindFirstFile(path + L"*.otf", &fd);
            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    m_FontInstaller.InstallTempFontFile(path + fd.cFileName);
                } while (FindNextFile(hFind, &fd));
                
                FindClose(hFind);
            }
        }
        // Temporarily load fonts from 'Fonts' folder - End

        if (!m_posFirstExtSub) {
            m_posFirstExtSub = m_pSubStreams.GetTailPosition();
        }

        if (pSubInput) {
            *pSubInput = subInput;
        }
    }

    return !!pSubStream;
}

bool CMainFrame::LoadSubtitle(CYoutubeDLInstance::YDLSubInfo& sub) {
    CAppSettings& s = AfxGetAppSettings();
    CComQIPtr<ISubStream> pSubStream;
    CAtlList<CString> preferlist;
    if (!s.sYDLSubsPreference.IsEmpty()) {
        if (s.sYDLSubsPreference.Find(_T(',')) != -1) {
            ExplodeMin(s.sYDLSubsPreference, preferlist, ',');
        } else {
            ExplodeMin(s.sYDLSubsPreference, preferlist, ' ');
        }
    }
    if (!preferlist.IsEmpty() && !CYoutubeDLInstance::isPrefer(preferlist, sub.lang)) {
        return false;
    }

    if (!s.IsISRAutoLoadEnabled() && (FindFilter(CLSID_VSFilter, m_pGB) || FindFilter(CLSID_XySubFilter, m_pGB))) {
        // Prevent ISR from loading if VSFilter is already in graph.
        // TODO: Support VSFilter natively (see ticket #4122)
        // Note that this doesn't affect ISR auto-loading if any sub renderer force loading itself into the graph.
        // VSFilter like filters can be blocked when building the graph and ISR auto-loading is enabled but some
        // users don't want that.
        return false;
    }

    if (GetPlaybackMode() == PM_FILE && !s.fDisableInternalSubtitles && !FindFilter(__uuidof(CTextPassThruFilter), m_pGB)) {
        // Add TextPassThru filter if it isn't already in the graph. (i.e ISR hasn't been loaded before)
        // This will load all embedded subtitle tracks when user triggers ISR (load external subtitle file) for the first time.
        AddTextPassThruFilter();
    }

    CAutoPtr<CRenderedTextSubtitle> pRTS(DEBUG_NEW CRenderedTextSubtitle(&m_csSubLock));
    if (pRTS) {
#if USE_LIBASS
        SubRendererSettings srs = AfxGetAppSettings().GetSubRendererSettings();
        pRTS->SetSubRenderSettings(srs);
#endif
        pRTS->SetDefaultStyle(s.subtitlesDefStyle);
        bool opened = false;
        if (!sub.url.IsEmpty()) {
            SubtitlesProvidersUtils::stringMap strmap{};
            DWORD dwStatusCode;
            CT2CA tem(sub.url);
            std::string tem2(tem);
            std::string data("");
            SubtitlesProvidersUtils::StringDownload(tem2, strmap, data, true, &dwStatusCode);
            if (dwStatusCode != 200) {
                return false;
            }
            if (sub.ext.IsEmpty()) {
                int m2(sub.url.ReverseFind(_T('?')));
                int m3(sub.url.ReverseFind(_T('#')));
                int m = -1;
                if (m2 > -1 && m3 > -1) m = std::min(m2, m3);
                else if (m2 > -1) m = m2;
                else if (m3 > -1) m = m3;
                CString temp(sub.url);
                if (m > 0) temp = sub.url.Left(m);
                m = temp.ReverseFind(_T('.'));
                if (m >= 0) sub.ext = temp.Mid(m + 1);
            }
            CString langt = sub.isAutomaticCaptions ? sub.lang + _T("[Automatic]") : sub.lang;
            opened = pRTS->Open((BYTE*)data.c_str(), (int)data.length(), DEFAULT_CHARSET, _T("YoutubeDL"), langt, sub.ext);
        } else if (!sub.data.IsEmpty()) {
            CString langt = sub.isAutomaticCaptions ? sub.lang + _T("[Automatic]") : sub.lang;
            opened = pRTS->Open(sub.data, CTextFile::enc::UTF8, DEFAULT_CHARSET, _T("YoutubeDL"), langt, sub.ext);  // Do not modify charset, Now it wroks with Unicode char.
        }
        if (opened && pRTS->GetStreamCount() > 0) {
#if USE_LIBASS
            pRTS->SetFilterGraph(m_pGB);
#endif
            pSubStream = pRTS.Detach();
        }
    }

    if (pSubStream) {
        SubtitleInput subInput(pSubStream);
        m_ExternalSubstreams.push_back(pSubStream);
        m_pSubStreams.AddTail(subInput);

        if (!m_posFirstExtSub) {
            m_posFirstExtSub = m_pSubStreams.GetTailPosition();
        }
    }

    return !!pSubStream;
}

// Called from GraphThread
bool CMainFrame::SetSubtitle(int i, bool bIsOffset /*= false*/, bool bDisplayMessage /*= false*/)
{
    if (!m_pCAP) {
        return false;
    }
    if (GetLoadState() == MLS::CLOSING) {
        return false;
    }

    SubtitleInput* pSubInput = GetSubtitleInput(i, bIsOffset);
    bool success = false;

    if (pSubInput) {
        CComHeapPtr<WCHAR> pName;
        if (CComQIPtr<IAMStreamSelect> pSSF = pSubInput->pSourceFilter) {
            DWORD dwFlags;
            LCID lcid = 0;
            if (FAILED(pSSF->Info(i, nullptr, &dwFlags, &lcid, nullptr, &pName, nullptr, nullptr))) {
                dwFlags = 0;
            }
            if (lcid && AfxGetAppSettings().fEnableSubtitles) {
                GetLocaleString(lcid, LOCALE_SISO639LANGNAME2, currentSubLang);
            } else {
                currentSubLang.Empty();
            }

            // Enable the track only if it isn't already the only selected track in the group
            if (!(dwFlags & AMSTREAMSELECTINFO_EXCLUSIVE)) {
                pSSF->Enable(i, AMSTREAMSELECTENABLE_ENABLE);
            }
            i = 0;
        }
        {
            // m_csSubLock shouldn't be locked when using IAMStreamSelect::Enable or SetSubtitle
            CAutoLock cAutoLock(&m_csSubLock);
            pSubInput->pSubStream->SetStream(i);
        }
        SetSubtitle(*pSubInput, true);

        if (!pName) {
            LCID lcid = 0;
            pSubInput->pSubStream->GetStreamInfo(0, &pName, &lcid);
            if (lcid && AfxGetAppSettings().fEnableSubtitles) {
                GetLocaleString(lcid, LOCALE_SISO639LANGNAME2, currentSubLang);
            } else {
                currentSubLang.Empty();
            }
        }

        if (bDisplayMessage && pName) {
            m_OSD.DisplayMessage(OSD_TOPLEFT, GetStreamOSDString(CString(pName), LCID(-1), 2));
        }
        success = true;
    }

    return success;
}

void CMainFrame::SetSubtitle(const SubtitleInput& subInput, bool skip_lcid /* = false */)
{
    TRACE(_T("CMainFrame::SetSubtitle\n"));

    CAppSettings& s = AfxGetAppSettings();

    {
        CAutoLock cAutoLock(&m_csSubLock);

        if (subInput.pSubStream) {
            bool found = false;
            POSITION pos = m_pSubStreams.GetHeadPosition();
            while (pos) {
                if (subInput.pSubStream == m_pSubStreams.GetNext(pos).pSubStream) {
                    found = true;
                    break;
                }
            }
            // We are trying to set a subtitles stream that isn't in the list so we abort here.
            if (!found) {
                return;
            }

            CLSID clsid;
            subInput.pSubStream->GetClassID(&clsid);

            if (clsid == __uuidof(CVobSubFile) || clsid == __uuidof(CVobSubStream)) {
                if (auto pVSS = dynamic_cast<CVobSubSettings*>((ISubStream*)subInput.pSubStream)) {
                    pVSS->SetAlignment(s.fOverridePlacement, s.nHorPos, s.nVerPos);
                }
            } else if (clsid == __uuidof(CRenderedTextSubtitle)) {
                CRenderedTextSubtitle* pRTS = (CRenderedTextSubtitle*)(ISubStream*)subInput.pSubStream;

                STSStyle style = s.subtitlesDefStyle;

                if (pRTS->m_fUsingAutoGeneratedDefaultStyle) {
                    pRTS->SetDefaultStyle(style);
                } else if (pRTS->GetDefaultStyle(style) && style.relativeTo == STSStyle::AUTO && s.subtitlesDefStyle.relativeTo != STSStyle::AUTO) {
                    style.relativeTo = s.subtitlesDefStyle.relativeTo;
                    pRTS->SetDefaultStyle(style);
                }
                if (m_pCAP) {
                    bool bKeepAspectRatio = s.fKeepAspectRatio;
                    CSize szAspectRatio = m_pCAP->GetVideoSize(true);
                    CSize szVideoFrame;
                    if (m_pMVRI) {
                        // Use IMadVRInfo to get size. See http://bugs.madshi.net/view.php?id=180
                        m_pMVRI->GetSize("originalVideoSize", &szVideoFrame);
                        bKeepAspectRatio = true;
                    } else {
                        szVideoFrame = m_pCAP->GetVideoSize(false);
                    }

                    pRTS->m_ePARCompensationType = CSimpleTextSubtitle::EPARCompensationType::EPCTAccurateSize_ISR;
                    if (s.bSubtitleARCompensation && szAspectRatio.cx && szAspectRatio.cy && szVideoFrame.cx && szVideoFrame.cy && bKeepAspectRatio) {
                        pRTS->m_dPARCompensation = ((double)szAspectRatio.cx / szAspectRatio.cy) /
                                                   ((double)szVideoFrame.cx / szVideoFrame.cy);
                    } else {
                        pRTS->m_dPARCompensation = 1.0;
                    }
                }

                pRTS->SetOverride(s.fUseDefaultSubtitlesStyle, s.subtitlesDefStyle);
                pRTS->SetAlignment(s.fOverridePlacement, s.nHorPos, s.nVerPos);
                pRTS->Deinit();
            }

            CComQIPtr<ISubRenderOptions> pSRO = m_pCAP;

            LPWSTR yuvMatrix = nullptr;
            int nLen;
            if (m_pMVRI) {
                m_pMVRI->GetString("yuvMatrix", &yuvMatrix, &nLen);
            } else if (pSRO) {
                pSRO->GetString("yuvMatrix", &yuvMatrix, &nLen);
            }

            int targetBlackLevel = 0, targetWhiteLevel = 255;
            if (m_pMVRS) {
                m_pMVRS->SettingsGetInteger(L"Black", &targetBlackLevel);
                m_pMVRS->SettingsGetInteger(L"White", &targetWhiteLevel);
            } else if (pSRO) {
                int range = 0;
                pSRO->GetInt("supportedLevels", &range);
                if (range == 3) {
                    targetBlackLevel = 16;
                    targetWhiteLevel = 235;
                }
            }

            subInput.pSubStream->SetSourceTargetInfo(yuvMatrix, targetBlackLevel, targetWhiteLevel);
            LocalFree(yuvMatrix);
        }

        m_pCurrentSubInput = subInput;

        if (!skip_lcid) {
            LCID lcid = 0;
            if (m_pCurrentSubInput.pSubStream && s.fEnableSubtitles) {
                CComHeapPtr<WCHAR> pName;
                m_pCurrentSubInput.pSubStream->GetStreamInfo(0, &pName, &lcid);
            }
            if (lcid) {
                GetLocaleString(lcid, LOCALE_SISO639LANGNAME2, currentSubLang);
            } else {
                currentSubLang.Empty();
            }
        }

        if (m_pCAP) {
            g_bExternalSubtitle = (std::find(m_ExternalSubstreams.cbegin(), m_ExternalSubstreams.cend(), subInput.pSubStream) != m_ExternalSubstreams.cend());
            m_wndSubresyncBar.SetSubtitle(subInput.pSubStream, m_pCAP->GetFPS());
        }
    }

    if (m_pCAP && s.fEnableSubtitles) {
        m_pCAP->SetSubPicProvider(CComQIPtr<ISubPicProvider>(subInput.pSubStream));
    }
}

void CMainFrame::ToggleSubtitleOnOff(bool bDisplayMessage /*= false*/)
{
    CAppSettings& s = AfxGetAppSettings();
    s.fEnableSubtitles = !s.fEnableSubtitles;

    if (s.fEnableSubtitles) {
        SetSubtitle(0, true, bDisplayMessage);
    } else {
        m_pCAP->SetSubPicProvider(nullptr);
        currentSubLang.Empty();

        if (bDisplayMessage) {
            m_OSD.DisplayMessage(OSD_TOPLEFT, ResStr(IDS_SUBTITLE_STREAM_OFF));
        }
    }
}

void CMainFrame::ReplaceSubtitle(const ISubStream* pSubStreamOld, ISubStream* pSubStreamNew)
{
    POSITION pos = m_pSubStreams.GetHeadPosition();
    while (pos) {
        POSITION cur = pos;
        if (pSubStreamOld == m_pSubStreams.GetNext(pos).pSubStream) {
            m_pSubStreams.GetAt(cur).pSubStream = pSubStreamNew;
            if (m_pCurrentSubInput.pSubStream == pSubStreamOld) {
                SetSubtitle(m_pSubStreams.GetAt(cur), true);
            }
            break;
        }
    }
}

void CMainFrame::InvalidateSubtitle(DWORD_PTR nSubtitleId /*= DWORD_PTR_MAX*/, REFERENCE_TIME rtInvalidate /*= -1*/)
{
    if (m_pCAP) {
        if (nSubtitleId == DWORD_PTR_MAX || nSubtitleId == (DWORD_PTR)(ISubStream*)m_pCurrentSubInput.pSubStream) {
            m_pCAP->Invalidate(rtInvalidate);
        }
    }
}

void CMainFrame::ReloadSubtitle()
{
    {
        CAutoLock cAutoLock(&m_csSubLock);
        POSITION pos = m_pSubStreams.GetHeadPosition();
        while (pos) {
            m_pSubStreams.GetNext(pos).pSubStream->Reload();
        }
    }
    SetSubtitle(0, true);
    m_wndSubresyncBar.ReloadSubtitle();
}

void CMainFrame::SetSubtitleTrackIdx(int index)
{
    const CAppSettings& s = AfxGetAppSettings();

    if (GetLoadState() == MLS::LOADED && m_pCAP) {
        // Check if we want to change the enable/disable state
        if (s.fEnableSubtitles != (index >= 0)) {
            ToggleSubtitleOnOff();
        }
        // Set the new subtitles track if needed
        if (s.fEnableSubtitles) {
            SetSubtitle(index);
        }
    }
}

void CMainFrame::SetAudioTrackIdx(int index)
{
    if (GetLoadState() == MLS::LOADED) {
        CComQIPtr<IAMStreamSelect> pSS = FindFilter(__uuidof(CAudioSwitcherFilter), m_pGB);

        DWORD cStreams = 0;
        DWORD dwFlags = AMSTREAMSELECTENABLE_ENABLE;
        if (pSS && SUCCEEDED(pSS->Count(&cStreams)))
            if ((index >= 0) && (index < ((int)cStreams))) {
                pSS->Enable(index, dwFlags);
            }
    }
}

REFERENCE_TIME CMainFrame::GetPos() const
{
    return (GetLoadState() == MLS::LOADED ? m_wndSeekBar.GetPos() : 0);
}

REFERENCE_TIME CMainFrame::GetDur() const
{
    __int64 start, stop;
    m_wndSeekBar.GetRange(start, stop);
    return (GetLoadState() == MLS::LOADED ? stop : 0);
}

void CMainFrame::LoadKeyFrames()
{
    UINT nKFs = 0;
    m_kfs.clear();
    if (m_pKFI && S_OK == m_pKFI->GetKeyFrameCount(nKFs) && nKFs > 1) {
        UINT k = nKFs;
        m_kfs.resize(k);
        if (FAILED(m_pKFI->GetKeyFrames(&TIME_FORMAT_MEDIA_TIME, m_kfs.data(), k)) || k != nKFs) {
            m_kfs.clear();
        }
    }
}

bool CMainFrame::GetKeyFrame(REFERENCE_TIME rtTarget, REFERENCE_TIME rtMin, REFERENCE_TIME rtMax, bool nearest, REFERENCE_TIME& keyframetime) const
{
    ASSERT(rtTarget >= rtMin);
    ASSERT(rtTarget <= rtMax);
    if (!m_kfs.empty()) {
        const auto cbegin = m_kfs.cbegin();
        const auto cend = m_kfs.cend();
        ASSERT(std::is_sorted(cbegin, cend));

        auto foundkeyframe = std::lower_bound(cbegin, cend, rtTarget);

        if (foundkeyframe == cbegin) {
            // first keyframe
            keyframetime = *foundkeyframe;
            if ((keyframetime < rtMin) || (keyframetime > rtMax)) {
                keyframetime = rtTarget;
                return false;
            }
        } else if (foundkeyframe == cend) {
            // last keyframe
            keyframetime = *(--foundkeyframe);
            if (keyframetime < rtMin) {
                keyframetime = rtTarget;
                return false;
            }
        } else {
            keyframetime = *foundkeyframe;
            if (keyframetime == rtTarget) {
                return true;
            }
            if (keyframetime > rtMax) {
                // use preceding keyframe
                keyframetime = *(--foundkeyframe);
                if (keyframetime < rtMin) {
                    keyframetime = rtTarget;
                    return false;
                }
            } else {
                if (nearest) {
                    const auto& s = AfxGetAppSettings();
                    if (s.eFastSeekMethod == s.FASTSEEK_NEAREST_KEYFRAME) {
                        // use closest keyframe
                        REFERENCE_TIME prev_keyframetime = *(--foundkeyframe);
                        if ((prev_keyframetime >= rtMin)) {
                            if ((keyframetime - rtTarget) > (rtTarget - prev_keyframetime)) {
                                keyframetime = prev_keyframetime;
                            }
                        }
                    }
                }
            }
        }
        return true;
    } else {
        keyframetime = rtTarget;
    }
    return false;
}

REFERENCE_TIME CMainFrame::GetClosestKeyFrame(REFERENCE_TIME rtTarget, REFERENCE_TIME rtMaxForwardDiff, REFERENCE_TIME rtMaxBackwardDiff) const
{
    if (rtTarget < 0LL) return 0LL;
    REFERENCE_TIME duration = GetDur();
    if (rtTarget > duration) rtTarget = duration;

    REFERENCE_TIME rtKeyframe;
    REFERENCE_TIME rtMin = std::max(rtTarget - rtMaxBackwardDiff, 0LL);
    REFERENCE_TIME rtMax = std::min(rtTarget + rtMaxForwardDiff, duration);

    if (GetKeyFrame(rtTarget, rtMin, rtMax, true, rtKeyframe)) {
        return rtKeyframe;
    }
    return rtTarget;
}

REFERENCE_TIME CMainFrame::GetClosestKeyFramePreview(REFERENCE_TIME rtTarget) const
{
    return GetClosestKeyFrame(rtTarget, 200000000LL, 200000000LL);
}

void CMainFrame::SeekTo(REFERENCE_TIME rtPos, bool bShowOSD /*= true*/)
{
    if (m_pMS == nullptr) {
        return;
    }
    ASSERT(lastSeekFinish >= lastSeekStart); // ToDo: remove lastSeekStart variable if no regressions show up
    ULONGLONG curTime = GetTickCount64();
    ULONGLONG ticksSinceLastSeek = curTime - lastSeekFinish;
    ULONGLONG mindelay = (lastSeekFinish - lastSeekStart) > 40ULL ? 100ULL : 40ULL;
    //ASSERT(rtPos != queuedSeek.rtPos || queuedSeek.seekTime == 0 || (curTime < queuedSeek.seekTime + 500ULL));

    if (ticksSinceLastSeek < mindelay) {
        //TRACE(_T("Delay seek: %lu %lu\n"), rtPos, ticksSinceLastSeek);
        queuedSeek = { rtPos, curTime, bShowOSD };
        SetTimer(TIMER_DELAYEDSEEK, (UINT) (mindelay * 1.25 - ticksSinceLastSeek), nullptr);
    } else {
        KillTimerDelayedSeek();
        lastSeekStart = curTime;
        DoSeekTo(rtPos, bShowOSD);
        lastSeekFinish = GetTickCount64();
    }
}

void CMainFrame::DoSeekTo(REFERENCE_TIME rtPos, bool bShowOSD /*= true*/)
{
    //TRACE(_T("DoSeekTo: %lu\n"), rtPos);

    ASSERT(m_pMS != nullptr);
    if (m_pMS == nullptr) {
        return;
    }
    OAFilterState fs = GetMediaState();

    if (rtPos < 0) {
        rtPos = 0;
    }

    if (m_fFrameSteppingActive) {
        // Cancel pending frame steps
        m_pFS->CancelStep();
        m_fFrameSteppingActive = false;
        m_pBA->put_Volume(m_nVolumeBeforeFrameStepping);
    }
    m_nStepForwardCount = 0;

    // skip seeks when duration is unknown
    if (!m_wndSeekBar.HasDuration()) {
        return;
    }

    if (!IsPlaybackCaptureMode()) {
        __int64 start, stop;
        m_wndSeekBar.GetRange(start, stop);
        if (rtPos > stop) {
            rtPos = stop;
        }
        m_wndStatusBar.SetStatusTimer(rtPos, stop, IsSubresyncBarVisible(), GetTimeFormat());

        if (bShowOSD) {
            m_OSD.DisplayMessage(OSD_TOPLEFT, m_wndStatusBar.GetStatusTimer(), 1500);
        }
    }

    if (GetPlaybackMode() == PM_FILE) {
        if (fs == State_Stopped) {
            SendMessage(WM_COMMAND, ID_PLAY_PAUSE);
        }

        //SleepEx(5000, False); // artificial slow seek for testing purposes
        m_pMS->SetPositions(&rtPos, AM_SEEKING_AbsolutePositioning, nullptr, AM_SEEKING_NoPositioning);
        UpdateChapterInInfoBar();
    } else if (GetPlaybackMode() == PM_DVD && m_iDVDDomain == DVD_DOMAIN_Title) {
        if (fs == State_Stopped) {
            SendMessage(WM_COMMAND, ID_PLAY_PAUSE);
            fs = State_Paused;
        }

        const REFTIME refAvgTimePerFrame = GetAvgTimePerFrame();
        if (fs == State_Paused) {
            // Jump one more frame back, this is needed because we don't have any other
            // way to seek to specific time without running playback to refresh state.
            rtPos -= std::llround(refAvgTimePerFrame * 10000000i64);
            m_pFS->CancelStep();
        }

        DVD_HMSF_TIMECODE tc = RT2HMSF(rtPos, (1.0 / refAvgTimePerFrame));
        m_pDVDC->PlayAtTime(&tc, DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, nullptr);

        if (fs == State_Paused) {
            // Do frame step to update current position in paused state
            m_pFS->Step(1, nullptr);
        }
    } else {
        ASSERT(FALSE);
    }
    m_fEndOfStream = false;

    OnTimer(TIMER_STREAMPOSPOLLER);
    OnTimer(TIMER_STREAMPOSPOLLER2);

    if (abRepeatPositionAEnabled && rtPos < abRepeatPositionA || abRepeatPositionBEnabled && rtPos > abRepeatPositionB) {
        DisableABRepeat();
    }

    SendCurrentPositionToApi(true);
}

void CMainFrame::CleanGraph()
{
    if (!m_pGB) {
        return;
    }

    BeginEnumFilters(m_pGB, pEF, pBF) {
        CComQIPtr<IAMFilterMiscFlags> pAMMF(pBF);
        if (pAMMF && (pAMMF->GetMiscFlags()&AM_FILTER_MISC_FLAGS_IS_SOURCE)) {
            continue;
        }

        // some capture filters forget to set AM_FILTER_MISC_FLAGS_IS_SOURCE
        // or to implement the IAMFilterMiscFlags interface
        if (pBF == m_pVidCap || pBF == m_pAudCap) {
            continue;
        }

        // XySubFilter doesn't have any pins connected when it is reading
        // external subtitles
        if (GetCLSID(pBF) == CLSID_XySubFilter) {
            continue;
        }

        if (CComQIPtr<IFileSourceFilter>(pBF)) {
            continue;
        }

        int nIn, nOut, nInC, nOutC;
        if (CountPins(pBF, nIn, nOut, nInC, nOutC) > 0 && (nInC + nOutC) == 0) {
            TRACE(CStringW(L"Removing: ") + GetFilterName(pBF) + '\n');

            m_pGB->RemoveFilter(pBF);
            pEF->Reset();
        }
    }
    EndEnumFilters;
}

#define AUDIOBUFFERLEN 500

static void SetLatency(IBaseFilter* pBF, int cbBuffer)
{
    BeginEnumPins(pBF, pEP, pPin) {
        if (CComQIPtr<IAMBufferNegotiation> pAMBN = pPin) {
            ALLOCATOR_PROPERTIES ap;
            ap.cbAlign = -1;  // -1 means no preference.
            ap.cbBuffer = cbBuffer;
            ap.cbPrefix = -1;
            ap.cBuffers = -1;
            pAMBN->SuggestAllocatorProperties(&ap);
        }
    }
    EndEnumPins;
}

HRESULT CMainFrame::BuildCapture(IPin* pPin, IBaseFilter* pBF[3], const GUID& majortype, AM_MEDIA_TYPE* pmt)
{
    IBaseFilter* pBuff = pBF[0];
    IBaseFilter* pEnc = pBF[1];
    IBaseFilter* pMux = pBF[2];

    if (!pPin || !pMux) {
        return E_FAIL;
    }

    CString err;
    HRESULT hr = S_OK;
    CFilterInfo fi;

    if (FAILED(pMux->QueryFilterInfo(&fi)) || !fi.pGraph) {
        m_pGB->AddFilter(pMux, L"Multiplexer");
    }

    CStringW prefix;
    CString type;
    if (majortype == MEDIATYPE_Video) {
        prefix = L"Video ";
        type.LoadString(IDS_CAPTURE_ERROR_VIDEO);
    } else if (majortype == MEDIATYPE_Audio) {
        prefix = L"Audio ";
        type.LoadString(IDS_CAPTURE_ERROR_AUDIO);
    }

    if (pBuff) {
        hr = m_pGB->AddFilter(pBuff, prefix + L"Buffer");
        if (FAILED(hr)) {
            err.Format(IDS_CAPTURE_ERROR_ADD_BUFFER, type.GetString());
            MessageBox(err, ResStr(IDS_CAPTURE_ERROR), MB_ICONERROR | MB_OK);
            return hr;
        }

        hr = m_pGB->ConnectFilter(pPin, pBuff);
        if (FAILED(hr)) {
            err.Format(IDS_CAPTURE_ERROR_CONNECT_BUFF, type.GetString());
            MessageBox(err, ResStr(IDS_CAPTURE_ERROR), MB_ICONERROR | MB_OK);
            return hr;
        }

        pPin = GetFirstPin(pBuff, PINDIR_OUTPUT);
    }

    if (pEnc) {
        hr = m_pGB->AddFilter(pEnc, prefix + L"Encoder");
        if (FAILED(hr)) {
            err.Format(IDS_CAPTURE_ERROR_ADD_ENCODER, type.GetString());
            MessageBox(err, ResStr(IDS_CAPTURE_ERROR), MB_ICONERROR | MB_OK);
            return hr;
        }

        hr = m_pGB->ConnectFilter(pPin, pEnc);
        if (FAILED(hr)) {
            err.Format(IDS_CAPTURE_ERROR_CONNECT_ENC, type.GetString());
            MessageBox(err, ResStr(IDS_CAPTURE_ERROR), MB_ICONERROR | MB_OK);
            return hr;
        }

        pPin = GetFirstPin(pEnc, PINDIR_OUTPUT);

        if (CComQIPtr<IAMStreamConfig> pAMSC = pPin) {
            if (pmt->majortype == majortype) {
                hr = pAMSC->SetFormat(pmt);
                if (FAILED(hr)) {
                    err.Format(IDS_CAPTURE_ERROR_COMPRESSION, type.GetString());
                    MessageBox(err, ResStr(IDS_CAPTURE_ERROR), MB_ICONERROR | MB_OK);
                    return hr;
                }
            }
        }

    }

    //if (pMux)
    {
        hr = m_pGB->ConnectFilter(pPin, pMux);
        if (FAILED(hr)) {
            err.Format(IDS_CAPTURE_ERROR_MULTIPLEXER, type.GetString());
            MessageBox(err, ResStr(IDS_CAPTURE_ERROR), MB_ICONERROR | MB_OK);
            return hr;
        }
    }

    CleanGraph();

    return S_OK;
}

bool CMainFrame::BuildToCapturePreviewPin(
    IBaseFilter* pVidCap, IPin** ppVidCapPin, IPin** ppVidPrevPin,
    IBaseFilter* pAudCap, IPin** ppAudCapPin, IPin** ppAudPrevPin)
{
    HRESULT hr;
    *ppVidCapPin = *ppVidPrevPin = nullptr;
    *ppAudCapPin = *ppAudPrevPin = nullptr;
    CComPtr<IPin> pDVAudPin;

    if (pVidCap) {
        CComPtr<IPin> pPin;
        if (!pAudCap // only look for interleaved stream when we don't use any other audio capture source
                && SUCCEEDED(m_pCGB->FindPin(pVidCap, PINDIR_OUTPUT, &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Interleaved, TRUE, 0, &pPin))) {
            CComPtr<IBaseFilter> pDVSplitter;
            hr = pDVSplitter.CoCreateInstance(CLSID_DVSplitter);
            hr = m_pGB->AddFilter(pDVSplitter, L"DV Splitter");

            hr = m_pCGB->RenderStream(nullptr, &MEDIATYPE_Interleaved, pPin, nullptr, pDVSplitter);

            pPin = nullptr;
            hr = m_pCGB->FindPin(pDVSplitter, PINDIR_OUTPUT, nullptr, &MEDIATYPE_Video, TRUE, 0, &pPin);
            hr = m_pCGB->FindPin(pDVSplitter, PINDIR_OUTPUT, nullptr, &MEDIATYPE_Audio, TRUE, 0, &pDVAudPin);

            CComPtr<IBaseFilter> pDVDec;
            hr = pDVDec.CoCreateInstance(CLSID_DVVideoCodec);
            hr = m_pGB->AddFilter(pDVDec, L"DV Video Decoder");

            hr = m_pGB->ConnectFilter(pPin, pDVDec);

            pPin = nullptr;
            hr = m_pCGB->FindPin(pDVDec, PINDIR_OUTPUT, nullptr, &MEDIATYPE_Video, TRUE, 0, &pPin);
        } else if (FAILED(m_pCGB->FindPin(pVidCap, PINDIR_OUTPUT, &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, TRUE, 0, &pPin))) {
            MessageBox(ResStr(IDS_CAPTURE_ERROR_VID_CAPT_PIN), ResStr(IDS_CAPTURE_ERROR), MB_ICONERROR | MB_OK);
            return false;
        }

        CComPtr<IBaseFilter> pSmartTee;
        hr = pSmartTee.CoCreateInstance(CLSID_SmartTee);
        hr = m_pGB->AddFilter(pSmartTee, L"Smart Tee (video)");

        hr = m_pGB->ConnectFilter(pPin, pSmartTee);

        hr = pSmartTee->FindPin(L"Preview", ppVidPrevPin);
        hr = pSmartTee->FindPin(L"Capture", ppVidCapPin);
    }

    if (pAudCap || pDVAudPin) {
        CComPtr<IPin> pPin;
        if (pDVAudPin) {
            pPin = pDVAudPin;
        } else if (FAILED(m_pCGB->FindPin(pAudCap, PINDIR_OUTPUT, &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Audio, TRUE, 0, &pPin))) {
            MessageBox(ResStr(IDS_CAPTURE_ERROR_AUD_CAPT_PIN), ResStr(IDS_CAPTURE_ERROR), MB_ICONERROR | MB_OK);
            return false;
        }

        CComPtr<IBaseFilter> pSmartTee;
        hr = pSmartTee.CoCreateInstance(CLSID_SmartTee);
        hr = m_pGB->AddFilter(pSmartTee, L"Smart Tee (audio)");

        hr = m_pGB->ConnectFilter(pPin, pSmartTee);

        hr = pSmartTee->FindPin(L"Preview", ppAudPrevPin);
        hr = pSmartTee->FindPin(L"Capture", ppAudCapPin);
    }

    return true;
}

bool CMainFrame::BuildGraphVideoAudio(int fVPreview, bool fVCapture, int fAPreview, bool fACapture)
{
    if (!m_pCGB) {
        return false;
    }

    OAFilterState fs = GetMediaState();

    if (fs != State_Stopped) {
        SendMessage(WM_COMMAND, ID_PLAY_STOP);
    }

    HRESULT hr;

    m_pGB->NukeDownstream(m_pVidCap);
    m_pGB->NukeDownstream(m_pAudCap);

    CleanGraph();

    if (m_pAMVSCCap) {
        hr = m_pAMVSCCap->SetFormat(&m_wndCaptureBar.m_capdlg.m_mtv);
    }
    if (m_pAMVSCPrev) {
        hr = m_pAMVSCPrev->SetFormat(&m_wndCaptureBar.m_capdlg.m_mtv);
    }
    if (m_pAMASC) {
        hr = m_pAMASC->SetFormat(&m_wndCaptureBar.m_capdlg.m_mta);
    }

    CComPtr<IBaseFilter> pVidBuffer = m_wndCaptureBar.m_capdlg.m_pVidBuffer;
    CComPtr<IBaseFilter> pAudBuffer = m_wndCaptureBar.m_capdlg.m_pAudBuffer;
    CComPtr<IBaseFilter> pVidEnc = m_wndCaptureBar.m_capdlg.m_pVidEnc;
    CComPtr<IBaseFilter> pAudEnc = m_wndCaptureBar.m_capdlg.m_pAudEnc;
    CComPtr<IBaseFilter> pMux = m_wndCaptureBar.m_capdlg.m_pMux;
    CComPtr<IBaseFilter> pDst = m_wndCaptureBar.m_capdlg.m_pDst;
    CComPtr<IBaseFilter> pAudMux = m_wndCaptureBar.m_capdlg.m_pAudMux;
    CComPtr<IBaseFilter> pAudDst = m_wndCaptureBar.m_capdlg.m_pAudDst;

    bool fFileOutput = (pMux && pDst) || (pAudMux && pAudDst);
    bool fCapture = (fVCapture || fACapture);

    if (m_pAudCap) {
        AM_MEDIA_TYPE* pmt = &m_wndCaptureBar.m_capdlg.m_mta;
        int ms = (fACapture && fFileOutput && m_wndCaptureBar.m_capdlg.m_fAudOutput) ? AUDIOBUFFERLEN : 60;
        if (pMux != pAudMux && fACapture) {
            SetLatency(m_pAudCap, -1);
        } else if (pmt->pbFormat) {
            SetLatency(m_pAudCap, ((WAVEFORMATEX*)pmt->pbFormat)->nAvgBytesPerSec * ms / 1000);
        }
    }

    CComPtr<IPin> pVidCapPin, pVidPrevPin, pAudCapPin, pAudPrevPin;
    BuildToCapturePreviewPin(m_pVidCap, &pVidCapPin, &pVidPrevPin, m_pAudCap, &pAudCapPin, &pAudPrevPin);

    //if (m_pVidCap)
    {
        bool fVidPrev = pVidPrevPin && fVPreview;
        bool fVidCap = pVidCapPin && fVCapture && fFileOutput && m_wndCaptureBar.m_capdlg.m_fVidOutput;

        if (fVPreview == 2 && !fVidCap && pVidCapPin) {
            pVidPrevPin = pVidCapPin;
            pVidCapPin = nullptr;
        }

        if (fVidPrev) {
            CComPtr<IVMRMixerBitmap9>    pVMB;
            CComPtr<IMFVideoMixerBitmap> pMFVMB;
            CComPtr<IMadVRTextOsd>       pMVTO;

            m_pMVRS.Release();
            m_pMVRFG.Release();
            m_pMVRSR.Release();

            m_OSD.Stop();
            m_pCAP3.Release();
            m_pCAP2.Release();
            m_pCAP.Release();
            m_pVMRWC.Release();
            m_pVMRMC.Release();
            m_pMFVP.Release();
            m_pMFVDC.Release();
            m_pQP.Release();

            m_pGB->Render(pVidPrevPin);

            m_pGB->FindInterface(IID_PPV_ARGS(&m_pCAP), TRUE);
            m_pGB->FindInterface(IID_PPV_ARGS(&m_pCAP2), TRUE);
            m_pGB->FindInterface(IID_PPV_ARGS(&m_pCAP3), TRUE);
            m_pGB->FindInterface(IID_PPV_ARGS(&m_pVMRWC), FALSE);
            m_pGB->FindInterface(IID_PPV_ARGS(&m_pVMRMC), TRUE);
            m_pGB->FindInterface(IID_PPV_ARGS(&pVMB), TRUE);
            m_pGB->FindInterface(IID_PPV_ARGS(&pMFVMB), TRUE);
            m_pGB->FindInterface(IID_PPV_ARGS(&m_pMFVDC), TRUE);
            m_pGB->FindInterface(IID_PPV_ARGS(&m_pMFVP), TRUE);
            pMVTO = m_pCAP;
            m_pMVRSR = m_pCAP;
            m_pMVRS = m_pCAP;
            m_pMVRFG = m_pCAP;

            const CAppSettings& s = AfxGetAppSettings();
            m_pVideoWnd = &m_wndView;

            if (m_pMFVDC) {
                m_pMFVDC->SetVideoWindow(m_pVideoWnd->m_hWnd);
            } else if (m_pVMRWC) {
                m_pVMRWC->SetVideoClippingWindow(m_pVideoWnd->m_hWnd);
            }

            if (s.fShowOSD || s.fShowDebugInfo) {
                if (pVMB) {
                    m_OSD.Start(m_pVideoWnd, pVMB, IsD3DFullScreenMode());
                } else if (pMFVMB) {
                    m_OSD.Start(m_pVideoWnd, pMFVMB, IsD3DFullScreenMode());
                } else if (pMVTO) {
                    m_OSD.Start(m_pVideoWnd, pMVTO);
                }
            }
        }

        if (fVidCap) {
            IBaseFilter* pBF[3] = {pVidBuffer, pVidEnc, pMux};
            HRESULT hr2 = BuildCapture(pVidCapPin, pBF, MEDIATYPE_Video, &m_wndCaptureBar.m_capdlg.m_mtcv);
            UNREFERENCED_PARAMETER(hr2);
        }

        m_pAMDF.Release();
        if (FAILED(m_pCGB->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, m_pVidCap, IID_PPV_ARGS(&m_pAMDF)))) {
            TRACE(_T("Warning: No IAMDroppedFrames interface for vidcap capture"));
        }
    }

    //if (m_pAudCap)
    {
        bool fAudPrev = pAudPrevPin && fAPreview;
        bool fAudCap = pAudCapPin && fACapture && fFileOutput && m_wndCaptureBar.m_capdlg.m_fAudOutput;

        if (fAPreview == 2 && !fAudCap && pAudCapPin) {
            pAudPrevPin = pAudCapPin;
            pAudCapPin = nullptr;
        }

        if (fAudPrev) {
            m_pGB->Render(pAudPrevPin);
        }

        if (fAudCap) {
            IBaseFilter* pBF[3] = {pAudBuffer, pAudEnc, pAudMux ? pAudMux : pMux};
            HRESULT hr2 = BuildCapture(pAudCapPin, pBF, MEDIATYPE_Audio, &m_wndCaptureBar.m_capdlg.m_mtca);
            UNREFERENCED_PARAMETER(hr2);
        }
    }

    if ((m_pVidCap || m_pAudCap) && fCapture && fFileOutput) {
        if (pMux != pDst) {
            hr = m_pGB->AddFilter(pDst, L"File Writer V/A");
            hr = m_pGB->ConnectFilter(GetFirstPin(pMux, PINDIR_OUTPUT), pDst);
        }

        if (CComQIPtr<IConfigAviMux> pCAM = pMux) {
            int nIn, nOut, nInC, nOutC;
            CountPins(pMux, nIn, nOut, nInC, nOutC);
            pCAM->SetMasterStream(nInC - 1);
            //pCAM->SetMasterStream(-1);
            pCAM->SetOutputCompatibilityIndex(FALSE);
        }

        if (CComQIPtr<IConfigInterleaving> pCI = pMux) {
            //if (FAILED(pCI->put_Mode(INTERLEAVE_CAPTURE)))
            if (FAILED(pCI->put_Mode(INTERLEAVE_NONE_BUFFERED))) {
                pCI->put_Mode(INTERLEAVE_NONE);
            }

            REFERENCE_TIME rtInterleave = 10000i64 * AUDIOBUFFERLEN, rtPreroll = 0; //10000i64*500
            pCI->put_Interleaving(&rtInterleave, &rtPreroll);
        }

        if (pMux != pAudMux && pAudMux != pAudDst) {
            hr = m_pGB->AddFilter(pAudDst, L"File Writer A");
            hr = m_pGB->ConnectFilter(GetFirstPin(pAudMux, PINDIR_OUTPUT), pAudDst);
        }
    }

    REFERENCE_TIME stop = MAX_TIME;
    hr = m_pCGB->ControlStream(&PIN_CATEGORY_CAPTURE, nullptr, nullptr, nullptr, &stop, 0, 0); // stop in the infinite

    CleanGraph();

    OpenSetupVideo();
    OpenSetupAudio();
    OpenSetupStatsBar();
    OpenSetupStatusBar();
    RecalcLayout();

    SetupVMR9ColorControl();

    if (GetLoadState() == MLS::LOADED) {
        if (fs == State_Running) {
            SendMessage(WM_COMMAND, ID_PLAY_PLAY);
        } else if (fs == State_Paused) {
            SendMessage(WM_COMMAND, ID_PLAY_PAUSE);
        }
    }

    return true;
}

bool CMainFrame::StartCapture()
{
    if (!m_pCGB || m_fCapturing) {
        return false;
    }

    if (!m_wndCaptureBar.m_capdlg.m_pMux && !m_wndCaptureBar.m_capdlg.m_pDst) {
        return false;
    }

    HRESULT hr;

    ::SetPriorityClass(::GetCurrentProcess(), HIGH_PRIORITY_CLASS);

    // rare to see two capture filters to support IAMPushSource at the same time...
    //hr = CComQIPtr<IAMGraphStreams>(m_pGB)->SyncUsingStreamOffset(TRUE); // TODO:

    BuildGraphVideoAudio(
        m_wndCaptureBar.m_capdlg.m_fVidPreview, true,
        m_wndCaptureBar.m_capdlg.m_fAudPreview, true);

    hr = m_pME->CancelDefaultHandling(EC_REPAINT);
    SendMessage(WM_COMMAND, ID_PLAY_PLAY);
    m_fCapturing = true;

    return true;
}

bool CMainFrame::StopCapture()
{
    if (!m_pCGB || !m_fCapturing) {
        return false;
    }

    if (!m_wndCaptureBar.m_capdlg.m_pMux && !m_wndCaptureBar.m_capdlg.m_pDst) {
        return false;
    }

    m_wndStatusBar.SetStatusMessage(StrRes(IDS_CONTROLS_COMPLETING));
    m_fCapturing = false;

    BuildGraphVideoAudio(
        m_wndCaptureBar.m_capdlg.m_fVidPreview, false,
        m_wndCaptureBar.m_capdlg.m_fAudPreview, false);

    m_pME->RestoreDefaultHandling(EC_REPAINT);

    ::SetPriorityClass(::GetCurrentProcess(), AfxGetAppSettings().dwPriority);

    m_rtDurationOverride = -1;

    return true;
}

//

void CMainFrame::ShowOptions(int idPage/* = 0*/)
{
    // Disable the options dialog when using D3D fullscreen
    if (IsD3DFullScreenMode()) {
        return;
    }

    INT_PTR iRes;
    do {
        CPPageSheet options(ResStr(IDS_OPTIONS_CAPTION), m_pGB, GetModalParent(), idPage);
        iRes = options.DoModal();
        idPage = 0; // If we are to show the dialog again, always show the latest page
    } while (iRes == CPPageSheet::APPLY_LANGUAGE_CHANGE); // check if we exited the dialog so that the language change can be applied

    switch (iRes) {
        case CPPageSheet::RESET_SETTINGS:
            // Request MPC-HC to close itself
            SendMessage(WM_CLOSE);
            // and immediately reopen
            ShellExecute(nullptr, _T("open"), PathUtils::GetProgramPath(true), _T("/reset"), nullptr, SW_SHOWNORMAL);
            break;
        default:
            ASSERT(iRes > 0 && iRes != CPPageSheet::APPLY_LANGUAGE_CHANGE);
            break;
    }
}

void CMainFrame::StartWebServer(int nPort)
{
    if (!m_pWebServer) {
        m_pWebServer.Attach(DEBUG_NEW CWebServer(this, nPort));
    }
}

void CMainFrame::StopWebServer()
{
    if (m_pWebServer) {
        m_pWebServer.Free();
    }
}

void CMainFrame::SendStatusMessage(CString msg, int nTimeOut)
{
    const auto timerId = TimerOneTimeSubscriber::STATUS_ERASE;

    m_timerOneTime.Unsubscribe(timerId);

    m_tempstatus_msg.Empty();
    if (nTimeOut <= 0) {
        return;
    }

    m_tempstatus_msg = msg;
    m_timerOneTime.Subscribe(timerId, [this] { m_tempstatus_msg.Empty(); }, nTimeOut);

    m_Lcd.SetStatusMessage(msg, nTimeOut);
}

bool CMainFrame::CanPreviewUse() {
    return (m_bUseSeekPreview
        && m_eMediaLoadState == MLS::LOADED
        && (GetPlaybackMode() == PM_DVD || GetPlaybackMode() == PM_FILE)
        && !m_fAudioOnly
        && AfxGetAppSettings().fSeekPreview);
}

void CMainFrame::OpenCurPlaylistItem(REFERENCE_TIME rtStart, bool reopen)
{
    if (IsPlaylistEmpty()) {
        return;
    }

    CPlaylistItem pli;
    if (!m_wndPlaylistBar.GetCur(pli)) {
        m_wndPlaylistBar.SetFirstSelected();
        if (!m_wndPlaylistBar.GetCur(pli)) {
            return;
        }
    }

    if (pli.m_bYoutubeDL && (reopen || pli.m_fns.GetHead() == pli.m_ydlSourceURL && m_sydlLastProcessURL != pli.m_ydlSourceURL)) {
        if (ProcessYoutubeDLURL(pli.m_ydlSourceURL, false, true)) {
            OpenCurPlaylistItem(rtStart, false);
            return;
        }
    }

    CAutoPtr<OpenMediaData> p(m_wndPlaylistBar.GetCurOMD(rtStart));
    if (p) {
        OpenMedia(p);
    }
}

void CMainFrame::AddCurDevToPlaylist()
{
    if (GetPlaybackMode() == PM_ANALOG_CAPTURE) {
        m_wndPlaylistBar.Append(
            m_VidDispName,
            m_AudDispName,
            m_wndCaptureBar.m_capdlg.GetVideoInput(),
            m_wndCaptureBar.m_capdlg.GetVideoChannel(),
            m_wndCaptureBar.m_capdlg.GetAudioInput()
        );
    }
}

void CMainFrame::OpenMedia(CAutoPtr<OpenMediaData> pOMD)
{
    auto pFileData = dynamic_cast<const OpenFileData*>(pOMD.m_p);
    //auto pDVDData = dynamic_cast<const OpenDVDData*>(pOMD.m_p);
    auto pDeviceData = dynamic_cast<const OpenDeviceData*>(pOMD.m_p);

    // if the tuner graph is already loaded, we just change its channel
    if (pDeviceData) {
        if (GetLoadState() == MLS::LOADED && m_pAMTuner
                && m_VidDispName == pDeviceData->DisplayName[0] && m_AudDispName == pDeviceData->DisplayName[1]) {
            m_wndCaptureBar.m_capdlg.SetVideoInput(pDeviceData->vinput);
            m_wndCaptureBar.m_capdlg.SetVideoChannel(pDeviceData->vchannel);
            m_wndCaptureBar.m_capdlg.SetAudioInput(pDeviceData->ainput);
            SendNowPlayingToSkype();
            return;
        }
    }

    // Wait a little so we can maybe avoid having to close previous media while graph is still being created
    int wait = 3000;
    while (GetLoadState() == MLS::LOADING && wait > 0) {
        SleepEx(200, false);
        wait -= 200;
    }

    if (m_bSettingUpMenus) {
        SleepEx(500, false);
        ASSERT(!m_bSettingUpMenus);
    }

    ASSERT(!m_bOpenMediaActive);
    m_bOpenMediaActive = true;

    // close the current graph before opening new media
    if (GetLoadState() != MLS::CLOSED) {
        CloseMedia(true);
        ASSERT(GetLoadState() == MLS::CLOSED);
    }

    // if the file is on some removable drive and that drive is missing,
    // we yell at user before even trying to construct the graph
    if (pFileData) {
        CString fn = pFileData->fns.GetHead();
        int i = fn.Find(_T(":\\"));
        if (i > 0) {
            CString drive = fn.Left(i + 2);
            UINT type = GetDriveType(drive);
            CAtlList<CString> sl;
            if (type == DRIVE_REMOVABLE || type == DRIVE_CDROM && GetOpticalDiskType(drive[0], sl) != OpticalDisk_Audio) {
                int ret = IDRETRY;
                while (ret == IDRETRY) {
                    WIN32_FIND_DATA findFileData;
                    HANDLE h = FindFirstFile(fn, &findFileData);
                    if (h != INVALID_HANDLE_VALUE) {
                        FindClose(h);
                        ret = IDOK;
                    } else {
                        CString msg;
                        msg.Format(IDS_MAINFRM_114, fn.GetString());
                        ret = AfxMessageBox(msg, MB_RETRYCANCEL);
                    }
                }
                if (ret != IDOK) {
                    return;
                }
            }
        }
    }

    // clear BD playlist if we are not currently opening something from it
    if (!m_bIsBDPlay) {
        m_MPLSPlaylist.RemoveAll();
        m_LastOpenBDPath = _T("");
    }
    m_bIsBDPlay = false;

    // no need to try releasing external objects while playing
    KillTimer(TIMER_UNLOAD_UNUSED_EXTERNAL_OBJECTS);

    // we hereby proclaim
    SetLoadState(MLS::LOADING);

    const auto& s = AfxGetAppSettings();

    // use the graph thread only for some media types
    bool bDirectShow = pFileData && !pFileData->fns.IsEmpty() && s.m_Formats.GetEngine(pFileData->fns.GetHead()) == DirectShow;
    bool bUseThread = m_pGraphThread && s.fEnableWorkerThreadForOpening && (bDirectShow || !pFileData) && (s.iDefaultCaptureDevice == 1 || !pDeviceData);

    // create d3dfs window if launching in fullscreen and d3dfs is enabled
    if (s.IsD3DFullscreen() && m_fStartInD3DFullscreen) {
        CreateFullScreenWindow();
        m_pVideoWnd = m_pFullscreenWnd;
        m_fStartInD3DFullscreen = false;
    } else {
        m_pVideoWnd = &m_wndView;
    }

    // activate auto-fit logic upon exiting fullscreen if
    // we are opening new media in fullscreen mode
    if ((m_fFullScreen || IsD3DFullScreenMode()) && s.fRememberZoomLevel) {
        m_fFirstFSAfterLaunchOnFS = true;
    }

    // don't set video renderer output rect until the window is repositioned
    m_bDelaySetOutputRect = true;

#if 0
    // display corresponding media icon in status bar
    if (pFileData) {
        CString filename = m_wndPlaylistBar.GetCurFileName();
        CString ext = filename.Mid(filename.ReverseFind('.') + 1);
        m_wndStatusBar.SetMediaType(ext);
    } else if (pDVDData) {
        m_wndStatusBar.SetMediaType(_T(".ifo"));
    } else {
        // TODO: Create icons for pDeviceData
        m_wndStatusBar.SetMediaType(_T(".unknown"));
    }
#endif

    // initiate graph creation, OpenMediaPrivate() will call OnFilePostOpenmedia()
    if (bUseThread) {
        VERIFY(m_evOpenPrivateFinished.Reset());
        VERIFY(m_pGraphThread->PostThreadMessage(CGraphThread::TM_OPEN, 0, (LPARAM)pOMD.Detach()));
        m_bOpenedThroughThread = true;
    } else {
        OpenMediaPrivate(pOMD);
        m_bOpenedThroughThread = false;
    }
}

bool CMainFrame::ResetDevice()
{
    if (m_pCAP) {
        return m_pCAP->ResetDevice();
    }
    return true;
}

bool CMainFrame::DisplayChange()
{
    if (m_pCAP) {
        return m_pCAP->DisplayChange();
    }
    return true;
}

void CMainFrame::CloseMedia(bool bNextIsQueued/* = false*/)
{
    TRACE(_T("CMainFrame::CloseMedia\n"));

    m_dwLastPause = 0;

    if (GetLoadState() == MLS::CLOSING || GetLoadState() == MLS::CLOSED) {
        // double close, should be prevented
        ASSERT(FALSE);
        return;
    }

    if (m_bSettingUpMenus) {
        SleepEx(500, false);
        ASSERT(!m_bSettingUpMenus);
    }

    // delay showing auto-hidden controls if new media is queued
    if (bNextIsQueued) {
        m_controls.DelayShowNotLoaded(true);
    } else {
        m_controls.DelayShowNotLoaded(false);
    }

    // abort if loading
    bool bGraphTerminated = false;
    if (GetLoadState() == MLS::LOADING) {
        TRACE(_T("Media is still loading. Aborting graph.\n"));

        // should be possible only in graph thread
        ASSERT(m_bOpenedThroughThread);

        // tell OpenMediaPrivate() that we want to abort
        m_fOpeningAborted = true;

        // close pin connection error dialog
        if (mediaTypesErrorDlg) {
            mediaTypesErrorDlg->SendMessage(WM_EXTERNALCLOSE, 0, 0);
            // wait till error dialog has been closed
            CAutoLock lck(&lockModalDialog);
        }

        // abort current graph task
        if (m_pGB) {
            m_pGB->Abort(); // TODO: lock on graph objects somehow, this is not thread safe
        }

        BeginWaitCursor();
        if (WaitForSingleObject(m_evOpenPrivateFinished, 5000) == WAIT_TIMEOUT) { // TODO: it's really dangerous, decide if we should do it at all
            // graph thread is taking too long to respond, we take extreme measures and terminate it
            MessageBeep(MB_ICONEXCLAMATION);
            ASSERT(FALSE);
            ENSURE(TerminateThread(m_pGraphThread->m_hThread, DWORD_ERROR));
            // then we recreate graph thread
            bGraphTerminated = true;
            m_pGraphThread = (CGraphThread*)AfxBeginThread(RUNTIME_CLASS(CGraphThread));
            m_pGraphThread->SetMainFrame(this);
        }
        EndWaitCursor();

        MSG msg;
        // purge possible queued OnFilePostOpenmedia()
        if (PeekMessage(&msg, m_hWnd, WM_POSTOPEN, WM_POSTOPEN, PM_REMOVE | PM_NOYIELD)) {
            free((OpenMediaData*)msg.lParam);
        }
        // purge possible queued OnOpenMediaFailed()
        if (PeekMessage(&msg, m_hWnd, WM_OPENFAILED, WM_OPENFAILED, PM_REMOVE | PM_NOYIELD)) {
            free((OpenMediaData*)msg.lParam);
        }

        // abort finished, unset the flag
        m_fOpeningAborted = false;
    }

    auto& s = AfxGetAppSettings();
    if (s.fKeepHistory) {
        updateRecentFileListSub();
    }
    m_current_rfe = RecentFileEntry(); // Clear

    // we are on the way
    m_bSettingUpMenus = true;
    SetLoadState(MLS::CLOSING);

    // stop the graph before destroying it
    OnPlayStop();

    // clear any active osd messages
    //m_OSD.ClearMessage();

    // Ensure the dynamically added menu items are cleared and all references
    // on objects belonging to the DirectShow graph they might hold are freed.
    // Note that we need to be in closing state already when doing that
    if (m_hWnd) {
        SetupFiltersSubMenu();
        SetupAudioSubMenu();
        SetupSubtitlesSubMenu();
        SetupVideoStreamsSubMenu();
        SetupJumpToSubMenus();
    }

    m_bSettingUpMenus = false;

    m_pSubtitlesProviders->Abort(SubtitlesThreadType(STT_SEARCH | STT_DOWNLOAD));
    m_wndSubtitlesDownloadDialog.DoClear();

    // initiate graph destruction
    if (m_pGraphThread && m_bOpenedThroughThread && !bGraphTerminated) {
        // either opening or closing has to be blocked to prevent reentering them, closing is the better choice
        VERIFY(m_evClosePrivateFinished.Reset());
        VERIFY(m_pGraphThread->PostThreadMessage(CGraphThread::TM_CLOSE, 0, 0));

        HANDLE handle = m_evClosePrivateFinished;
        DWORD dwWait;
        // We don't want to block messages processing completely so we stop waiting
        // on new message received from another thread or application.
        while ((dwWait = MsgWaitForMultipleObjects(1, &handle, FALSE, INFINITE, QS_SENDMESSAGE)) != WAIT_OBJECT_0) {
            // If we haven't got the event we were waiting for, we ensure that we have got a new message instead
            if (dwWait == WAIT_OBJECT_0 + 1) {
                // and we make sure it is handled before resuming waiting
                MSG msg;
                PeekMessage(&msg, nullptr, 0, 0, PM_NOREMOVE);
            } else {
                ASSERT(FALSE);
            }
        }
    } else {
        CloseMediaPrivate();
    }

    // graph is destroyed, update stuff
    OnFilePostClosemedia(bNextIsQueued);

    TRACE(_T("Close media completed\n"));
}

void CMainFrame::StartTunerScan(CAutoPtr<TunerScanData> pTSD)
{
    // Remove the old info during the scan
    m_pDVBState->Reset();
    m_wndInfoBar.RemoveAllLines();
    m_wndNavigationBar.m_navdlg.SetChannelInfoAvailable(false);
    RecalcLayout();
    OpenSetupWindowTitle();
    SendNowPlayingToSkype();

    if (m_pGraphThread) {
        m_pGraphThread->PostThreadMessage(CGraphThread::TM_TUNER_SCAN, 0, (LPARAM)pTSD.Detach());
    } else {
        DoTunerScan(pTSD);
    }
}

void CMainFrame::StopTunerScan()
{
    m_bStopTunerScan = true;
}

HRESULT CMainFrame::SetChannel(int nChannel)
{
    CAppSettings& s = AfxGetAppSettings();
    HRESULT hr = S_OK;
    CComQIPtr<IBDATuner> pTun = m_pGB;
    CBDAChannel* pChannel = s.FindChannelByPref(nChannel);

    if (s.m_DVBChannels.empty() && nChannel == INT_ERROR) {
        hr = S_FALSE; // All channels have been cleared or it is the first start
    } else if (pTun && pChannel && !m_pDVBState->bSetChannelActive) {
        m_pDVBState->Reset();
        m_wndInfoBar.RemoveAllLines();
        m_wndNavigationBar.m_navdlg.SetChannelInfoAvailable(false);
        RecalcLayout();
        m_pDVBState->bSetChannelActive = true;

        // Skip n intermediate ZoomVideoWindow() calls while the new size is stabilized:
        switch (s.iDSVideoRendererType) {
            case VIDRNDT_DS_MADVR:
                if (s.nDVBStopFilterGraph == DVB_STOP_FG_ALWAYS) {
                    m_nLockedZoomVideoWindow = 3;
                } else {
                    m_nLockedZoomVideoWindow = 0;
                }
                break;
            case VIDRNDT_DS_EVR_CUSTOM:
                m_nLockedZoomVideoWindow = 0;
                break;
            default:
                m_nLockedZoomVideoWindow = 0;
        }
        if (SUCCEEDED(hr = pTun->SetChannel(nChannel))) {
            if (hr == S_FALSE) {
                // Re-create all
                m_nLockedZoomVideoWindow = 0;
                PostMessage(WM_COMMAND, ID_FILE_OPENDEVICE);
                return hr;
            }

            m_pDVBState->bActive = true;
            m_pDVBState->pChannel = pChannel;
            m_pDVBState->sChannelName = pChannel->GetName();

            m_wndInfoBar.SetLine(ResStr(IDS_INFOBAR_CHANNEL), m_pDVBState->sChannelName);
            RecalcLayout();

            if (s.fRememberZoomLevel && !(m_fFullScreen || IsZoomed() || IsIconic())) {
                ZoomVideoWindow();
            }
            MoveVideoWindow();

            // Add temporary flag to allow EC_VIDEO_SIZE_CHANGED event to stabilize window size
            // for 5 seconds since playback starts
            m_bAllowWindowZoom = true;
            m_timerOneTime.Subscribe(TimerOneTimeSubscriber::AUTOFIT_TIMEOUT, [this]
            { m_bAllowWindowZoom = false; }, 5000);

            UpdateCurrentChannelInfo();
        }
        m_pDVBState->bSetChannelActive = false;
    } else {
        hr = E_FAIL;
        ASSERT(FALSE);
    }

    return hr;
}

void CMainFrame::UpdateCurrentChannelInfo(bool bShowOSD /*= true*/, bool bShowInfoBar /*= false*/)
{
    const CBDAChannel* pChannel = m_pDVBState->pChannel;
    CComQIPtr<IBDATuner> pTun = m_pGB;

    if (!m_pDVBState->bInfoActive && pChannel && pTun) {
        if (m_pDVBState->infoData.valid()) {
            m_pDVBState->bAbortInfo = true;
            m_pDVBState->infoData.get();
        }
        m_pDVBState->bAbortInfo = false;
        m_pDVBState->bInfoActive = true;
        m_pDVBState->infoData = std::async(std::launch::async, [this, pChannel, pTun, bShowOSD, bShowInfoBar] {
            DVBState::EITData infoData;
            infoData.hr = pTun->UpdatePSI(pChannel, infoData.NowNext);
            infoData.bShowOSD = bShowOSD;
            infoData.bShowInfoBar = bShowInfoBar;
            if (m_pDVBState && !m_pDVBState->bAbortInfo)
            {
                PostMessage(WM_DVB_EIT_DATA_READY);
            }
            return infoData;
        });
    }
}

LRESULT CMainFrame::OnCurrentChannelInfoUpdated(WPARAM wParam, LPARAM lParam)
{
    if (!m_pDVBState->bAbortInfo && m_pDVBState->infoData.valid()) {
        EventDescriptor& NowNext = m_pDVBState->NowNext;
        const auto infoData = m_pDVBState->infoData.get();
        NowNext = infoData.NowNext;

        if (infoData.hr != S_FALSE) {
            // Set a timer to update the infos only if channel has now/next flag
            time_t tNow;
            time(&tNow);
            time_t tElapse = NowNext.duration - (tNow - NowNext.startTime);
            if (tElapse < 0) {
                tElapse = 0;
            }
            // We set a 15s delay to let some room for the program infos to change
            tElapse += 15;
            m_timerOneTime.Subscribe(TimerOneTimeSubscriber::DVBINFO_UPDATE,
                                     [this] { UpdateCurrentChannelInfo(false, false); },
                                     1000 * (UINT)tElapse);
            m_wndNavigationBar.m_navdlg.SetChannelInfoAvailable(true);
        } else {
            m_wndNavigationBar.m_navdlg.SetChannelInfoAvailable(false);
        }

        CString sChannelInfo = m_pDVBState->sChannelName;
        m_wndInfoBar.RemoveAllLines();
        m_wndInfoBar.SetLine(StrRes(IDS_INFOBAR_CHANNEL), sChannelInfo);

        if (infoData.hr == S_OK) {
            // EIT information parsed correctly
            if (infoData.bShowOSD) {
                sChannelInfo.AppendFormat(_T(" | %s (%s - %s)"), NowNext.eventName.GetString(), NowNext.strStartTime.GetString(), NowNext.strEndTime.GetString());
            }

            m_wndInfoBar.SetLine(StrRes(IDS_INFOBAR_TITLE), NowNext.eventName);
            m_wndInfoBar.SetLine(StrRes(IDS_INFOBAR_TIME), NowNext.strStartTime + _T(" - ") + NowNext.strEndTime);

            if (NowNext.parentalRating >= 0) {
                CString parentRating;
                if (!NowNext.parentalRating) {
                    parentRating.LoadString(IDS_NO_PARENTAL_RATING);
                } else {
                    parentRating.Format(IDS_PARENTAL_RATING, NowNext.parentalRating);
                }
                m_wndInfoBar.SetLine(StrRes(IDS_INFOBAR_PARENTAL_RATING), parentRating);
            }

            if (!NowNext.content.IsEmpty()) {
                m_wndInfoBar.SetLine(StrRes(IDS_INFOBAR_CONTENT), NowNext.content);
            }

            CString description = NowNext.eventDesc;
            if (!NowNext.extendedDescriptorsText.IsEmpty()) {
                if (!description.IsEmpty()) {
                    description += _T("; ");
                }
                description += NowNext.extendedDescriptorsText;
            }
            m_wndInfoBar.SetLine(StrRes(IDS_INFOBAR_DESCRIPTION), description);

            for (const auto& item : NowNext.extendedDescriptorsItems) {
                m_wndInfoBar.SetLine(item.first, item.second);
            }

            if (infoData.bShowInfoBar && !m_controls.ControlChecked(CMainFrameControls::Toolbar::INFO)) {
                m_controls.ToggleControl(CMainFrameControls::Toolbar::INFO);
            }
        }

        RecalcLayout();
        if (infoData.bShowOSD) {
            m_OSD.DisplayMessage(OSD_TOPLEFT, sChannelInfo, 3500);
        }

        // Update window title and skype status
        OpenSetupWindowTitle();
        SendNowPlayingToSkype();
    } else {
        ASSERT(FALSE);
    }

    m_pDVBState->bInfoActive = false;

    return 0;
}

// ==== Added by CASIMIR666
void CMainFrame::SetLoadState(MLS eState)
{
    m_eMediaLoadState = eState;
    SendAPICommand(CMD_STATE, L"%d", static_cast<int>(eState));
    if (eState == MLS::LOADED) {
        m_controls.DelayShowNotLoaded(false);
        m_eventc.FireEvent(MpcEvent::MEDIA_LOADED);
    }
    UpdateControlState(UPDATE_CONTROLS_VISIBILITY);
}

MLS CMainFrame::GetLoadState() const
{
    return m_eMediaLoadState;
}

void CMainFrame::SetPlayState(MPC_PLAYSTATE iState)
{
    m_Lcd.SetPlayState((CMPC_Lcd::PlayState)iState);
    SendAPICommand(CMD_PLAYMODE, L"%d", iState);

    if (m_fEndOfStream) {
        SendAPICommand(CMD_NOTIFYENDOFSTREAM, L"\0");     // do not pass NULL here!
    }

    // Prevent sleep when playing audio and/or video, but allow screensaver when only audio
    if (!m_fAudioOnly) {
        SetThreadExecutionState(iState == PS_PLAY ? ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED : ES_CONTINUOUS);
    } else {
        SetThreadExecutionState(iState == PS_PLAY ? ES_CONTINUOUS | ES_SYSTEM_REQUIRED : ES_CONTINUOUS);
    }

    UpdateThumbarButton(iState);
}

bool CMainFrame::CreateFullScreenWindow()
{
    const CAppSettings& s = AfxGetAppSettings();
    CMonitor monitor;

    if (m_pFullscreenWnd->IsWindow()) {
        m_pFullscreenWnd->DestroyWindow();
    }

    if (s.iMonitor == 0 && s.strFullScreenMonitor != _T("Current")) {
        CMonitors monitors;
        for (int i = 0; i < monitors.GetCount(); i++) {
            monitor = monitors.GetMonitor(i);
            if (monitor.IsMonitor()) {
                CString str;
                monitor.GetName(str);
                if (s.strFullScreenMonitor == str) {
                    break;
                }
                monitor.Detach();
            }
        }
    }

    if (!monitor.IsMonitor()) {
        monitor = CMonitors::GetNearestMonitor(this);
    }

    CRect monitorRect;
    monitor.GetMonitorRect(monitorRect);

    return !!m_pFullscreenWnd->CreateEx(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, _T(""), ResStr(IDS_MAINFRM_136),
                                        WS_POPUP | WS_VISIBLE, monitorRect, nullptr, 0);
}

bool CMainFrame::IsFrameLessWindow() const
{
    return (m_fFullScreen || AfxGetAppSettings().eCaptionMenuMode == MODE_BORDERLESS);
}

bool CMainFrame::IsCaptionHidden() const
{
    // If no caption, there is no menu bar. But if is no menu bar, then the caption can be.
    return (!m_fFullScreen && AfxGetAppSettings().eCaptionMenuMode > MODE_HIDEMENU); //!=MODE_SHOWCAPTIONMENU && !=MODE_HIDEMENU
}

bool CMainFrame::IsMenuHidden() const
{
    return (!m_fFullScreen && AfxGetAppSettings().eCaptionMenuMode != MODE_SHOWCAPTIONMENU);
}

bool CMainFrame::IsPlaylistEmpty() const
{
    return (m_wndPlaylistBar.GetCount() == 0);
}

bool CMainFrame::IsInteractiveVideo() const
{
    return m_fShockwaveGraph;
}

bool CMainFrame::IsD3DFullScreenMode() const
{
    return m_pFullscreenWnd && m_pFullscreenWnd->IsWindow();
};

bool CMainFrame::IsSubresyncBarVisible() const
{
    return !!m_wndSubresyncBar.IsWindowVisible();
}

void CMainFrame::SetupEVRColorControl()
{
    if (m_pMFVP) {
        CMPlayerCApp* pApp = AfxGetMyApp();
        CAppSettings& s = AfxGetAppSettings();

        if (FAILED(m_pMFVP->GetProcAmpRange(DXVA2_ProcAmp_Brightness, pApp->GetEVRColorControl(ProcAmp_Brightness)))) {
            return;
        }
        if (FAILED(m_pMFVP->GetProcAmpRange(DXVA2_ProcAmp_Contrast, pApp->GetEVRColorControl(ProcAmp_Contrast)))) {
            return;
        }
        if (FAILED(m_pMFVP->GetProcAmpRange(DXVA2_ProcAmp_Hue, pApp->GetEVRColorControl(ProcAmp_Hue)))) {
            return;
        }
        if (FAILED(m_pMFVP->GetProcAmpRange(DXVA2_ProcAmp_Saturation, pApp->GetEVRColorControl(ProcAmp_Saturation)))) {
            return;
        }

        pApp->UpdateColorControlRange(true);
        SetColorControl(ProcAmp_All, s.iBrightness, s.iContrast, s.iHue, s.iSaturation);
    }
}

// Called from GraphThread
void CMainFrame::SetupVMR9ColorControl()
{
    if (m_pVMRMC) {
        CMPlayerCApp* pApp = AfxGetMyApp();
        CAppSettings& s = AfxGetAppSettings();

        if (FAILED(m_pVMRMC->GetProcAmpControlRange(0, pApp->GetVMR9ColorControl(ProcAmp_Brightness)))) {
            return;
        }
        if (FAILED(m_pVMRMC->GetProcAmpControlRange(0, pApp->GetVMR9ColorControl(ProcAmp_Contrast)))) {
            return;
        }
        if (FAILED(m_pVMRMC->GetProcAmpControlRange(0, pApp->GetVMR9ColorControl(ProcAmp_Hue)))) {
            return;
        }
        if (FAILED(m_pVMRMC->GetProcAmpControlRange(0, pApp->GetVMR9ColorControl(ProcAmp_Saturation)))) {
            return;
        }

        pApp->UpdateColorControlRange(false);
        SetColorControl(ProcAmp_All, s.iBrightness, s.iContrast, s.iHue, s.iSaturation);
    }
}

void CMainFrame::SetColorControl(DWORD flags, int& brightness, int& contrast, int& hue, int& saturation)
{
    CMPlayerCApp* pApp = AfxGetMyApp();
    HRESULT hr = 0;

    static VMR9ProcAmpControl  ClrControl;
    static DXVA2_ProcAmpValues ClrValues;

    COLORPROPERTY_RANGE* cr = 0;
    if (flags & ProcAmp_Brightness) {
        cr = pApp->GetColorControl(ProcAmp_Brightness);
        brightness = std::min(std::max(brightness, cr->MinValue), cr->MaxValue);
    }
    if (flags & ProcAmp_Contrast) {
        cr = pApp->GetColorControl(ProcAmp_Contrast);
        contrast = std::min(std::max(contrast, cr->MinValue), cr->MaxValue);
    }
    if (flags & ProcAmp_Hue) {
        cr = pApp->GetColorControl(ProcAmp_Hue);
        hue = std::min(std::max(hue, cr->MinValue), cr->MaxValue);
    }
    if (flags & ProcAmp_Saturation) {
        cr = pApp->GetColorControl(ProcAmp_Saturation);
        saturation = std::min(std::max(saturation, cr->MinValue), cr->MaxValue);
    }

    if (m_pVMRMC) {
        ClrControl.dwSize     = sizeof(ClrControl);
        ClrControl.dwFlags    = flags;
        ClrControl.Brightness = (float)brightness;
        ClrControl.Contrast   = (float)(contrast + 100) / 100;
        ClrControl.Hue        = (float)hue;
        ClrControl.Saturation = (float)(saturation + 100) / 100;

        hr = m_pVMRMC->SetProcAmpControl(0, &ClrControl);
    } else if (m_pMFVP) {
        ClrValues.Brightness = IntToFixed(brightness);
        ClrValues.Contrast   = IntToFixed(contrast + 100, 100);
        ClrValues.Hue        = IntToFixed(hue);
        ClrValues.Saturation = IntToFixed(saturation + 100, 100);

        hr = m_pMFVP->SetProcAmpValues(flags, &ClrValues);

    }
    // Workaround: with Intel driver the minimum values of the supported range may not actually work
    if (FAILED(hr)) {
        if (flags & ProcAmp_Brightness) {
            cr = pApp->GetColorControl(ProcAmp_Brightness);
            if (brightness == cr->MinValue) {
                brightness = cr->MinValue + 1;
            }
        }
        if (flags & ProcAmp_Hue) {
            cr = pApp->GetColorControl(ProcAmp_Hue);
            if (hue == cr->MinValue) {
                hue = cr->MinValue + 1;
            }
        }
    }
}

void CMainFrame::SetClosedCaptions(bool enable)
{
    if (m_pLN21) {
        m_pLN21->SetServiceState(enable ? AM_L21_CCSTATE_On : AM_L21_CCSTATE_Off);
    }
}

LPCTSTR CMainFrame::GetDVDAudioFormatName(const DVD_AudioAttributes& ATR) const
{
    switch (ATR.AudioFormat) {
        case DVD_AudioFormat_AC3:
            return _T("AC3");
        case DVD_AudioFormat_MPEG1:
        case DVD_AudioFormat_MPEG1_DRC:
            return _T("MPEG1");
        case DVD_AudioFormat_MPEG2:
        case DVD_AudioFormat_MPEG2_DRC:
            return _T("MPEG2");
        case DVD_AudioFormat_LPCM:
            return _T("LPCM");
        case DVD_AudioFormat_DTS:
            return _T("DTS");
        case DVD_AudioFormat_SDDS:
            return _T("SDDS");
        case DVD_AudioFormat_Other:
        default:
            return MAKEINTRESOURCE(IDS_MAINFRM_137);
    }
}

afx_msg void CMainFrame::OnGotoSubtitle(UINT nID)
{
    if (!m_pSubStreams.IsEmpty() && !IsPlaybackCaptureMode()) {
        m_rtCurSubPos = m_wndSeekBar.GetPos();
        m_lSubtitleShift = 0;
        m_nCurSubtitle = m_wndSubresyncBar.FindNearestSub(m_rtCurSubPos, (nID == ID_GOTO_NEXT_SUB));
        if (m_nCurSubtitle >= 0 && m_pMS) {
            OnPlayPause();
            m_pMS->SetPositions(&m_rtCurSubPos, AM_SEEKING_AbsolutePositioning, nullptr, AM_SEEKING_NoPositioning);
        }
    }
}

afx_msg void CMainFrame::OnShiftSubtitle(UINT nID)
{
    if (m_nCurSubtitle >= 0) {
        long lShift = (nID == ID_SHIFT_SUB_DOWN) ? -100 : 100;
        CString strSubShift;

        if (m_wndSubresyncBar.ShiftSubtitle(m_nCurSubtitle, lShift, m_rtCurSubPos)) {
            m_lSubtitleShift += lShift;
            if (m_pMS) {
                m_pMS->SetPositions(&m_rtCurSubPos, AM_SEEKING_AbsolutePositioning, nullptr, AM_SEEKING_NoPositioning);
            }
        }

        strSubShift.Format(IDS_MAINFRM_138, m_lSubtitleShift);
        m_OSD.DisplayMessage(OSD_TOPLEFT, strSubShift);
    }
}

afx_msg void CMainFrame::OnSubtitleDelay(UINT nID)
{
    if (m_pCAP) {
        if (m_pSubStreams.IsEmpty()) {
            SendStatusMessage(StrRes(IDS_SUBTITLES_ERROR), 3000);
            return;
        }

        int newDelay;
        int oldDelay = m_pCAP->GetSubtitleDelay();
        int nDelayStep = AfxGetAppSettings().nSubDelayStep;

        if (nID == ID_SUB_DELAY_DOWN) {
            newDelay = oldDelay - nDelayStep;
        } else {
            newDelay = oldDelay + nDelayStep;
        }

        SetSubtitleDelay(newDelay);
    }
}

void CMainFrame::ProcessAPICommand(COPYDATASTRUCT* pCDS)
{
    CAtlList<CString> fns;
    REFERENCE_TIME rtPos = 0;

    switch (pCDS->dwData) {
        case CMD_OPENFILE:
            fns.AddHead((LPCWSTR)pCDS->lpData);
            m_wndPlaylistBar.Open(fns, false);
            OpenCurPlaylistItem();
            break;
        case CMD_STOP:
            OnPlayStop();
            break;
        case CMD_CLOSEFILE:
            CloseMedia();
            break;
        case CMD_PLAYPAUSE:
            OnPlayPlaypause();
            break;
        case CMD_PLAY:
            OnApiPlay();
            break;
        case CMD_PAUSE:
            OnApiPause();
            break;
        case CMD_ADDTOPLAYLIST:
            fns.AddHead((LPCWSTR)pCDS->lpData);
            m_wndPlaylistBar.Append(fns, true);
            break;
        case CMD_STARTPLAYLIST:
            OpenCurPlaylistItem();
            break;
        case CMD_CLEARPLAYLIST:
            m_wndPlaylistBar.Empty();
            break;
        case CMD_SETPOSITION:
            rtPos = 10000 * REFERENCE_TIME(_wtof((LPCWSTR)pCDS->lpData) * 1000); //with accuracy of 1 ms
            // imianz: quick and dirty trick
            // Pause->SeekTo->Play (in place of SeekTo only) seems to prevents in most cases
            // some strange video effects on avi files (ex. locks a while and than running fast).
            if (!m_fAudioOnly && GetMediaState() == State_Running) {
                SendMessage(WM_COMMAND, ID_PLAY_PAUSE);
                SeekTo(rtPos);
                SendMessage(WM_COMMAND, ID_PLAY_PLAY);
            } else {
                SeekTo(rtPos);
            }
            // show current position overridden by play command
            m_OSD.DisplayMessage(OSD_TOPLEFT, m_wndStatusBar.GetStatusTimer(), 2000);
            break;
        case CMD_SETAUDIODELAY:
            rtPos = (REFERENCE_TIME)_wtol((LPCWSTR)pCDS->lpData) * 10000;
            SetAudioDelay(rtPos);
            break;
        case CMD_SETSUBTITLEDELAY:
            SetSubtitleDelay(_wtoi((LPCWSTR)pCDS->lpData));
            break;
        case CMD_SETINDEXPLAYLIST:
            //m_wndPlaylistBar.SetSelIdx(_wtoi((LPCWSTR)pCDS->lpData));
            break;
        case CMD_SETAUDIOTRACK:
            SetAudioTrackIdx(_wtoi((LPCWSTR)pCDS->lpData));
            break;
        case CMD_SETSUBTITLETRACK:
            SetSubtitleTrackIdx(_wtoi((LPCWSTR)pCDS->lpData));
            break;
        case CMD_GETVERSION: {
            CStringW buff = AfxGetMyApp()->m_strVersion;
            SendAPICommand(CMD_VERSION, buff);
            break;
        }
        case CMD_GETSUBTITLETRACKS:
            SendSubtitleTracksToApi();
            break;
        case CMD_GETAUDIOTRACKS:
            SendAudioTracksToApi();
            break;
        case CMD_GETCURRENTPOSITION:
            SendCurrentPositionToApi();
            break;
        case CMD_GETNOWPLAYING:
            SendNowPlayingToApi();
            break;
        case CMD_JUMPOFNSECONDS:
            JumpOfNSeconds(_wtoi((LPCWSTR)pCDS->lpData));
            break;
        case CMD_GETPLAYLIST:
            SendPlaylistToApi();
            break;
        case CMD_JUMPFORWARDMED:
            OnPlaySeek(ID_PLAY_SEEKFORWARDMED);
            break;
        case CMD_JUMPBACKWARDMED:
            OnPlaySeek(ID_PLAY_SEEKBACKWARDMED);
            break;
        case CMD_TOGGLEFULLSCREEN:
            OnViewFullscreen();
            break;
        case CMD_INCREASEVOLUME:
            m_wndToolBar.m_volctrl.IncreaseVolume();
            break;
        case CMD_DECREASEVOLUME:
            m_wndToolBar.m_volctrl.DecreaseVolume();
            break;
		case CMD_SHADER_TOGGLE :
			OnShaderToggle1();
			break;
        case CMD_CLOSEAPP:
            PostMessage(WM_CLOSE);
            break;
        case CMD_SETSPEED:
            SetPlayingRate(_wtof((LPCWSTR)pCDS->lpData));
            break;
        case CMD_OSDSHOWMESSAGE:
            ShowOSDCustomMessageApi((MPC_OSDDATA*)pCDS->lpData);
            break;
    }
}

void CMainFrame::SendAPICommand(MPCAPI_COMMAND nCommand, LPCWSTR fmt, ...)
{
    const CAppSettings& s = AfxGetAppSettings();

    if (s.hMasterWnd) {
        COPYDATASTRUCT CDS;

        va_list args;
        va_start(args, fmt);

        int nBufferLen = _vsctprintf(fmt, args) + 1; // _vsctprintf doesn't count the null terminator
        TCHAR* pBuff = DEBUG_NEW TCHAR[nBufferLen];
        _vstprintf_s(pBuff, nBufferLen, fmt, args);

        CDS.cbData = (DWORD)nBufferLen * sizeof(TCHAR);
        CDS.dwData = nCommand;
        CDS.lpData = (LPVOID)pBuff;

        ::SendMessage(s.hMasterWnd, WM_COPYDATA, (WPARAM)GetSafeHwnd(), (LPARAM)&CDS);

        va_end(args);
        delete [] pBuff;
    }
}

void CMainFrame::SendNowPlayingToApi()
{
    if (!AfxGetAppSettings().hMasterWnd) {
        return;
    }

    if (GetLoadState() == MLS::LOADED) {
        CPlaylistItem pli;
        CString title, author, description;
        CString label;
        CString strDur;

        if (GetPlaybackMode() == PM_FILE) {
            m_wndInfoBar.GetLine(StrRes(IDS_INFOBAR_TITLE), title);
            m_wndInfoBar.GetLine(StrRes(IDS_INFOBAR_AUTHOR), author);
            m_wndInfoBar.GetLine(StrRes(IDS_INFOBAR_DESCRIPTION), description);

            m_wndPlaylistBar.GetCur(pli);
            if (!pli.m_fns.IsEmpty()) {
                label = !pli.m_label.IsEmpty() ? pli.m_label : pli.m_fns.GetHead();
                REFERENCE_TIME rtDur;
                m_pMS->GetDuration(&rtDur);
                strDur.Format(L"%.3f", rtDur / 10000000.0);
            }
        } else if (GetPlaybackMode() == PM_DVD) {
            DVD_DOMAIN DVDDomain;
            ULONG ulNumOfChapters = 0;
            DVD_PLAYBACK_LOCATION2 Location;

            // Get current DVD Domain
            if (SUCCEEDED(m_pDVDI->GetCurrentDomain(&DVDDomain))) {
                switch (DVDDomain) {
                    case DVD_DOMAIN_Stop:
                        title = _T("DVD - Stopped");
                        break;
                    case DVD_DOMAIN_FirstPlay:
                        title = _T("DVD - FirstPlay");
                        break;
                    case DVD_DOMAIN_VideoManagerMenu:
                        title = _T("DVD - RootMenu");
                        break;
                    case DVD_DOMAIN_VideoTitleSetMenu:
                        title = _T("DVD - TitleMenu");
                        break;
                    case DVD_DOMAIN_Title:
                        title = _T("DVD - Title");
                        break;
                }

                // get title information
                if (DVDDomain == DVD_DOMAIN_Title) {
                    // get current location (title number & chapter)
                    if (SUCCEEDED(m_pDVDI->GetCurrentLocation(&Location))) {
                        // get number of chapters in current title
                        VERIFY(SUCCEEDED(m_pDVDI->GetNumberOfChapters(Location.TitleNum, &ulNumOfChapters)));
                    }

                    // get total time of title
                    DVD_HMSF_TIMECODE tcDur;
                    ULONG ulFlags;
                    if (SUCCEEDED(m_pDVDI->GetTotalTitleTime(&tcDur, &ulFlags))) {
                        // calculate duration in seconds
                        strDur.Format(L"%d", tcDur.bHours * 60 * 60 + tcDur.bMinutes * 60 + tcDur.bSeconds);
                    }

                    // build string
                    // DVD - xxxxx|currenttitle|numberofchapters|currentchapter|titleduration
                    author.Format(L"%lu", Location.TitleNum);
                    description.Format(L"%lu", ulNumOfChapters);
                    label.Format(L"%lu", Location.ChapterNum);
                }
            }
        }

        title.Replace(L"|", L"\\|");
        author.Replace(L"|", L"\\|");
        description.Replace(L"|", L"\\|");
        label.Replace(L"|", L"\\|");

        CStringW buff;
        buff.Format(L"%s|%s|%s|%s|%s", title.GetString(), author.GetString(), description.GetString(), label.GetString(), strDur.GetString());

        SendAPICommand(CMD_NOWPLAYING, L"%s", static_cast<LPCWSTR>(buff));
        SendSubtitleTracksToApi();
        SendAudioTracksToApi();
    }
}

void CMainFrame::SendSubtitleTracksToApi()
{
    CStringW strSubs;

    if (GetLoadState() == MLS::LOADED) {
        POSITION pos = m_pSubStreams.GetHeadPosition();
        int i = 0, iSelected = -1;
        if (pos) {
            while (pos) {
                SubtitleInput& subInput = m_pSubStreams.GetNext(pos);

                if (CComQIPtr<IAMStreamSelect> pSSF = subInput.pSourceFilter) {
                    DWORD cStreams;
                    if (FAILED(pSSF->Count(&cStreams))) {
                        continue;
                    }

                    for (int j = 0, cnt = (int)cStreams; j < cnt; j++) {
                        DWORD dwFlags, dwGroup;
                        WCHAR* pszName = nullptr;

                        if (FAILED(pSSF->Info(j, nullptr, &dwFlags, nullptr, &dwGroup, &pszName, nullptr, nullptr))
                                || !pszName) {
                            continue;
                        }

                        CString name(pszName);
                        CoTaskMemFree(pszName);

                        if (dwGroup != 2) {
                            continue;
                        }

                        if (subInput.pSubStream == m_pCurrentSubInput.pSubStream
                                && dwFlags & (AMSTREAMSELECTINFO_ENABLED | AMSTREAMSELECTINFO_EXCLUSIVE)) {
                            iSelected = j;
                        }

                        if (!strSubs.IsEmpty()) {
                            strSubs.Append(L"|");
                        }
                        name.Replace(L"|", L"\\|");
                        strSubs.Append(name);

                        i++;
                    }
                } else {
                    CComPtr<ISubStream> pSubStream = subInput.pSubStream;
                    if (!pSubStream) {
                        continue;
                    }

                    if (subInput.pSubStream == m_pCurrentSubInput.pSubStream) {
                        iSelected = i + pSubStream->GetStream();
                    }

                    for (int j = 0, cnt = pSubStream->GetStreamCount(); j < cnt; j++) {
                        WCHAR* pName = nullptr;
                        if (SUCCEEDED(pSubStream->GetStreamInfo(j, &pName, nullptr))) {
                            CString name(pName);
                            CoTaskMemFree(pName);

                            if (!strSubs.IsEmpty()) {
                                strSubs.Append(L"|");
                            }
                            name.Replace(L"|", L"\\|");
                            strSubs.Append(name);
                        }
                        i++;
                    }
                }

            }
            if (AfxGetAppSettings().fEnableSubtitles) {
                strSubs.AppendFormat(L"|%d", iSelected);
            } else {
                strSubs.Append(L"|-1");
            }
        } else {
            strSubs.Append(L"-1");
        }
    } else {
        strSubs.Append(L"-2");
    }
    SendAPICommand(CMD_LISTSUBTITLETRACKS, L"%s", static_cast<LPCWSTR>(strSubs));
}

void CMainFrame::SendAudioTracksToApi()
{
    CStringW strAudios;

    if (GetLoadState() == MLS::LOADED) {
        CComQIPtr<IAMStreamSelect> pSS = FindFilter(__uuidof(CAudioSwitcherFilter), m_pGB);

        DWORD cStreams = 0;
        if (pSS && SUCCEEDED(pSS->Count(&cStreams))) {
            int currentStream = -1;
            for (int i = 0; i < (int)cStreams; i++) {
                AM_MEDIA_TYPE* pmt = nullptr;
                DWORD dwFlags = 0;
                LCID lcid = 0;
                DWORD dwGroup = 0;
                WCHAR* pszName = nullptr;
                if (FAILED(pSS->Info(i, &pmt, &dwFlags, &lcid, &dwGroup, &pszName, nullptr, nullptr))) {
                    return;
                }
                if (dwFlags == AMSTREAMSELECTINFO_EXCLUSIVE) {
                    currentStream = i;
                }
                CString name(pszName);
                if (!strAudios.IsEmpty()) {
                    strAudios.Append(L"|");
                }
                name.Replace(L"|", L"\\|");
                strAudios.AppendFormat(L"%s", name.GetString());
                if (pmt) {
                    DeleteMediaType(pmt);
                }
                if (pszName) {
                    CoTaskMemFree(pszName);
                }
            }
            strAudios.AppendFormat(L"|%d", currentStream);

        } else {
            strAudios.Append(L"-1");
        }
    } else {
        strAudios.Append(L"-2");
    }
    SendAPICommand(CMD_LISTAUDIOTRACKS, L"%s", static_cast<LPCWSTR>(strAudios));

}

void CMainFrame::SendPlaylistToApi()
{
    CStringW strPlaylist;
    POSITION pos = m_wndPlaylistBar.m_pl.GetHeadPosition(), pos2;

    while (pos) {
        CPlaylistItem& pli = m_wndPlaylistBar.m_pl.GetNext(pos);

        if (pli.m_type == CPlaylistItem::file) {
            pos2 = pli.m_fns.GetHeadPosition();
            while (pos2) {
                CString fn = pli.m_fns.GetNext(pos2);
                if (!strPlaylist.IsEmpty()) {
                    strPlaylist.Append(L"|");
                }
                fn.Replace(L"|", L"\\|");
                strPlaylist.AppendFormat(L"%s", fn.GetString());
            }
        }
    }
    if (strPlaylist.IsEmpty()) {
        strPlaylist.Append(L"-1");
    } else {
        strPlaylist.AppendFormat(L"|%d", m_wndPlaylistBar.GetSelIdx());
    }
    SendAPICommand(CMD_PLAYLIST, L"%s", static_cast<LPCWSTR>(strPlaylist));
}

void CMainFrame::SendCurrentPositionToApi(bool fNotifySeek)
{
    if (!AfxGetAppSettings().hMasterWnd) {
        return;
    }

    if (GetLoadState() == MLS::LOADED) {
        CStringW strPos;

        if (GetPlaybackMode() == PM_FILE) {
            REFERENCE_TIME rtCur;
            m_pMS->GetCurrentPosition(&rtCur);
            strPos.Format(L"%.3f", rtCur / 10000000.0);
        } else if (GetPlaybackMode() == PM_DVD) {
            DVD_PLAYBACK_LOCATION2 Location;
            // get current location while playing disc, will return 0, if at a menu
            if (m_pDVDI->GetCurrentLocation(&Location) == S_OK) {
                strPos.Format(L"%d", Location.TimeCode.bHours * 60 * 60 + Location.TimeCode.bMinutes * 60 + Location.TimeCode.bSeconds);
            }
        }

        SendAPICommand(fNotifySeek ? CMD_NOTIFYSEEK : CMD_CURRENTPOSITION, strPos);
    }
}

void CMainFrame::ShowOSDCustomMessageApi(const MPC_OSDDATA* osdData)
{
    m_OSD.DisplayMessage((OSD_MESSAGEPOS)osdData->nMsgPos, osdData->strMsg, osdData->nDurationMS);
}

void CMainFrame::JumpOfNSeconds(int nSeconds)
{
    if (GetLoadState() == MLS::LOADED) {
        REFERENCE_TIME rtCur;

        if (GetPlaybackMode() == PM_FILE) {
            m_pMS->GetCurrentPosition(&rtCur);
            DVD_HMSF_TIMECODE tcCur = RT2HMSF(rtCur);
            long lPosition = tcCur.bHours * 60 * 60 + tcCur.bMinutes * 60 + tcCur.bSeconds + nSeconds;

            // revert the update position to REFERENCE_TIME format
            tcCur.bHours   = (BYTE)(lPosition / 3600);
            tcCur.bMinutes = (lPosition / 60) % 60;
            tcCur.bSeconds = lPosition % 60;
            rtCur = HMSF2RT(tcCur);

            // quick and dirty trick:
            // pause->seekto->play seems to prevents some strange
            // video effect (ex. locks for a while and than running fast)
            if (!m_fAudioOnly) {
                SendMessage(WM_COMMAND, ID_PLAY_PAUSE);
            }
            SeekTo(rtCur);
            if (!m_fAudioOnly) {
                SendMessage(WM_COMMAND, ID_PLAY_PLAY);
                // show current position overridden by play command
                m_OSD.DisplayMessage(OSD_TOPLEFT, m_wndStatusBar.GetStatusTimer(), 2000);
            }
        }
    }
}

// TODO : to be finished !
//void CMainFrame::AutoSelectTracks()
//{
//  LCID DefAudioLanguageLcid    [2] = {MAKELCID( MAKELANGID(LANG_FRENCH, SUBLANG_DEFAULT), SORT_DEFAULT), MAKELCID( MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), SORT_DEFAULT)};
//  int  DefAudioLanguageIndex   [2] = {-1, -1};
//  LCID DefSubtitleLanguageLcid [2] = {0, MAKELCID( MAKELANGID(LANG_FRENCH, SUBLANG_DEFAULT), SORT_DEFAULT)};
//  int  DefSubtitleLanguageIndex[2] = {-1, -1};
//  LCID Language = MAKELCID(MAKELANGID(LANG_FRENCH, SUBLANG_DEFAULT), SORT_DEFAULT);
//
//  if ((m_iMediaLoadState == MLS::LOADING) || (m_iMediaLoadState == MLS::LOADED))
//  {
//      if (GetPlaybackMode() == PM_FILE)
//      {
//          CComQIPtr<IAMStreamSelect> pSS = FindFilter(__uuidof(CAudioSwitcherFilter), m_pGB);
//
//          DWORD cStreams = 0;
//          if (pSS && SUCCEEDED(pSS->Count(&cStreams)))
//          {
//              for (int i = 0; i < (int)cStreams; i++)
//              {
//                  AM_MEDIA_TYPE* pmt = nullptr;
//                  DWORD dwFlags = 0;
//                  LCID lcid = 0;
//                  DWORD dwGroup = 0;
//                  WCHAR* pszName = nullptr;
//                  if (FAILED(pSS->Info(i, &pmt, &dwFlags, &lcid, &dwGroup, &pszName, nullptr, nullptr)))
//                      return;
//              }
//          }
//
//          POSITION pos = m_pSubStreams.GetHeadPosition();
//          while (pos)
//          {
//              CComPtr<ISubStream> pSubStream = m_pSubStreams.GetNext(pos).subStream;
//              if (!pSubStream) continue;
//
//              for (int i = 0, j = pSubStream->GetStreamCount(); i < j; i++)
//              {
//                  WCHAR* pName = nullptr;
//                  if (SUCCEEDED(pSubStream->GetStreamInfo(i, &pName, &Language)))
//                  {
//                      if (DefAudioLanguageLcid[0] == Language)    DefSubtitleLanguageIndex[0] = i;
//                      if (DefSubtitleLanguageLcid[1] == Language) DefSubtitleLanguageIndex[1] = i;
//                      CoTaskMemFree(pName);
//                  }
//              }
//          }
//      }
//      else if (GetPlaybackMode() == PM_DVD)
//      {
//          ULONG ulStreamsAvailable, ulCurrentStream;
//          BOOL  bIsDisabled;
//
//          if (SUCCEEDED(m_pDVDI->GetCurrentSubpicture(&ulStreamsAvailable, &ulCurrentStream, &bIsDisabled)))
//          {
//              for (ULONG i = 0; i < ulStreamsAvailable; i++)
//              {
//                  DVD_SubpictureAttributes    ATR;
//                  if (SUCCEEDED(m_pDVDI->GetSubpictureLanguage(i, &Language)))
//                  {
//                      // Auto select forced subtitle
//                      if ((DefAudioLanguageLcid[0] == Language) && (ATR.LanguageExtension == DVD_SP_EXT_Forced))
//                          DefSubtitleLanguageIndex[0] = i;
//
//                      if (DefSubtitleLanguageLcid[1] == Language) DefSubtitleLanguageIndex[1] = i;
//                  }
//              }
//          }
//
//          if (SUCCEEDED(m_pDVDI->GetCurrentAudio(&ulStreamsAvailable, &ulCurrentStream)))
//          {
//              for (ULONG i = 0; i < ulStreamsAvailable; i++)
//              {
//                  if (SUCCEEDED(m_pDVDI->GetAudioLanguage(i, &Language)))
//                  {
//                      if (DefAudioLanguageLcid[0] == Language)    DefAudioLanguageIndex[0] = i;
//                      if (DefAudioLanguageLcid[1] == Language)    DefAudioLanguageIndex[1] = i;
//                  }
//              }
//          }
//
//          // Select best audio/subtitles tracks
//          if (DefAudioLanguageLcid[0] != -1)
//          {
//              m_pDVDC->SelectAudioStream(DefAudioLanguageIndex[0], DVD_CMD_FLAG_Block, nullptr);
//              if (DefSubtitleLanguageIndex[0] != -1)
//                  m_pDVDC->SelectSubpictureStream(DefSubtitleLanguageIndex[0], DVD_CMD_FLAG_Block, nullptr);
//          }
//          else if ((DefAudioLanguageLcid[1] != -1) && (DefSubtitleLanguageLcid[1] != -1))
//          {
//              m_pDVDC->SelectAudioStream      (DefAudioLanguageIndex[1],    DVD_CMD_FLAG_Block, nullptr);
//              m_pDVDC->SelectSubpictureStream (DefSubtitleLanguageIndex[1], DVD_CMD_FLAG_Block, nullptr);
//          }
//      }
//
//
//  }
//}

void CMainFrame::OnFileOpendirectory()
{
    if (GetLoadState() == MLS::LOADING || !IsWindow(m_wndPlaylistBar)) {
        return;
    }

    CString strTitle(StrRes(IDS_MAINFRM_DIR_TITLE));
    CString path;

    CFileDialog dlg(TRUE);
    dlg.AddCheckButton(IDS_MAINFRM_DIR_CHECK, ResStr(IDS_MAINFRM_DIR_CHECK), TRUE);
    IFileOpenDialog* openDlgPtr = dlg.GetIFileOpenDialog();

    if (openDlgPtr != nullptr) {
        openDlgPtr->SetTitle(strTitle);
        openDlgPtr->SetOptions(FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
        if (FAILED(openDlgPtr->Show(m_hWnd))) {
            openDlgPtr->Release();
            return;
        }
        openDlgPtr->Release();

        path = dlg.GetFolderPath();

        BOOL recur = TRUE;
        dlg.GetCheckButtonState(IDS_MAINFRM_DIR_CHECK, recur);
        COpenDirHelper::m_incl_subdir = !!recur;
    } else {
        return;
    }

    // If we got a link file that points to a directory, follow the link
    if (PathUtils::IsLinkFile(path)) {
        CString resolvedPath = PathUtils::ResolveLinkFile(path);
        if (PathUtils::IsDir(resolvedPath)) {
            path = resolvedPath;
        }
    }

    if (path[path.GetLength() - 1] != '\\') {
        path += _T('\\');
    }

    CAtlList<CString> sl;
    sl.AddTail(path);
    if (COpenDirHelper::m_incl_subdir) {
        PathUtils::RecurseAddDir(path, sl);
    }

    m_wndPlaylistBar.Open(sl, true);
    OpenCurPlaylistItem();
}

HRESULT CMainFrame::CreateThumbnailToolbar()
{
    if (!AfxGetAppSettings().bUseEnhancedTaskBar || !IsWindows7OrGreater()) {
        return E_FAIL;
    }

    if (m_pTaskbarList || SUCCEEDED(m_pTaskbarList.CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER))) {
        bool bRTLLayout = false; // Assume left-to-right layout by default
        // Try to locate the window used to display the task bar
        if (CWnd* pTaskBarWnd = FindWindow(_T("TaskListThumbnailWnd"), nullptr)) {
            bRTLLayout = !!(pTaskBarWnd->GetExStyle() & WS_EX_LAYOUTRTL);
        }

        CMPCPngImage image;
        if (!image.Load(IDF_WIN7_TOOLBAR)) {
            return E_FAIL;
        }

        CSize size = image.GetSize();

        if (bRTLLayout) { // We don't want the buttons to be mirrored so we pre-mirror them
            // Create memory DCs for the source and destination bitmaps
            CDC sourceDC, destDC;
            sourceDC.CreateCompatibleDC(nullptr);
            destDC.CreateCompatibleDC(nullptr);
            // Swap the source bitmap with an empty one
            CBitmap sourceImg;
            sourceImg.Attach(image.Detach());
            // Create a temporary DC
            CClientDC clientDC(nullptr);
            // Create the destination bitmap
            image.CreateCompatibleBitmap(&clientDC, size.cx, size.cy);
            // Select the bitmaps into the DCs
            HGDIOBJ oldSourceDCObj = sourceDC.SelectObject(sourceImg);
            HGDIOBJ oldDestDCObj = destDC.SelectObject(image);
            // Actually flip the bitmap
            destDC.StretchBlt(0, 0, size.cx, size.cy,
                              &sourceDC, size.cx, 0, -size.cx, size.cy,
                              SRCCOPY);
            // Reselect the old objects back into their DCs
            sourceDC.SelectObject(oldSourceDCObj);
            destDC.SelectObject(oldDestDCObj);

            sourceDC.DeleteDC();
            destDC.DeleteDC();
        }

        CImageList imageList;
        imageList.Create(size.cy, size.cy, ILC_COLOR32, size.cx / size.cy, 0);
        imageList.Add(&image, nullptr);

        if (SUCCEEDED(m_pTaskbarList->ThumbBarSetImageList(m_hWnd, imageList.GetSafeHandle()))) {
            THUMBBUTTON buttons[5] = {};

            for (size_t i = 0; i < _countof(buttons); i++) {
                buttons[i].dwMask = THB_BITMAP | THB_TOOLTIP | THB_FLAGS;
                buttons[i].dwFlags = THBF_DISABLED;
                buttons[i].iBitmap = 0; // Will be set later
            }

            // PREVIOUS
            buttons[0].iId = IDTB_BUTTON3;
            // STOP
            buttons[1].iId = IDTB_BUTTON1;
            // PLAY/PAUSE
            buttons[2].iId = IDTB_BUTTON2;
            // NEXT
            buttons[3].iId = IDTB_BUTTON4;
            // FULLSCREEN
            buttons[4].iId = IDTB_BUTTON5;

            if (bRTLLayout) { // We don't want the buttons to be mirrored so we pre-mirror them
                std::reverse(buttons, buttons + _countof(buttons));
            }

            if (SUCCEEDED(m_pTaskbarList->ThumbBarAddButtons(m_hWnd, _countof(buttons), buttons))) {
                return UpdateThumbarButton();
            }
        }
    }

    return E_FAIL;
}

HRESULT CMainFrame::UpdateThumbarButton()
{
    MPC_PLAYSTATE state = PS_STOP;
    if (GetLoadState() == MLS::LOADED) {
        switch (GetMediaState()) {
            case State_Running:
                state = PS_PLAY;
                break;
            case State_Paused:
                state = PS_PAUSE;
                break;
        }
    }
    return UpdateThumbarButton(state);
}

HRESULT CMainFrame::UpdateThumbarButton(MPC_PLAYSTATE iPlayState)
{
    if (!m_pTaskbarList) {
        return E_FAIL;
    }

    THUMBBUTTON buttons[5] = {};

    for (size_t i = 0; i < _countof(buttons); i++) {
        buttons[i].dwMask = THB_BITMAP | THB_TOOLTIP | THB_FLAGS;
    }

    buttons[0].iId = IDTB_BUTTON3;
    buttons[1].iId = IDTB_BUTTON1;
    buttons[2].iId = IDTB_BUTTON2;
    buttons[3].iId = IDTB_BUTTON4;
    buttons[4].iId = IDTB_BUTTON5;

    const CAppSettings& s = AfxGetAppSettings();

    if (!s.bUseEnhancedTaskBar) {
        m_pTaskbarList->SetOverlayIcon(m_hWnd, nullptr, L"");
        m_pTaskbarList->SetProgressState(m_hWnd, TBPF_NOPROGRESS);

        for (size_t i = 0; i < _countof(buttons); i++) {
            buttons[i].dwFlags = THBF_HIDDEN;
        }
    } else {
        buttons[0].iBitmap = 0;
        StringCchCopy(buttons[0].szTip, _countof(buttons[0].szTip), ResStr(IDS_AG_PREVIOUS));

        buttons[1].iBitmap = 1;
        StringCchCopy(buttons[1].szTip, _countof(buttons[1].szTip), ResStr(IDS_AG_STOP));

        buttons[2].iBitmap = 3;
        StringCchCopy(buttons[2].szTip, _countof(buttons[2].szTip), ResStr(IDS_AG_PLAYPAUSE));

        buttons[3].iBitmap = 4;
        StringCchCopy(buttons[3].szTip, _countof(buttons[3].szTip), ResStr(IDS_AG_NEXT));

        buttons[4].iBitmap = 5;
        StringCchCopy(buttons[4].szTip, _countof(buttons[4].szTip), ResStr(IDS_AG_FULLSCREEN));

        if (GetLoadState() == MLS::LOADED) {
            HICON hIcon = nullptr;

            buttons[0].dwFlags = (!s.fUseSearchInFolder && m_wndPlaylistBar.GetCount() <= 1 && (m_pCB && m_pCB->ChapGetCount() <= 1)) ? THBF_DISABLED : THBF_ENABLED;
            buttons[3].dwFlags = (!s.fUseSearchInFolder && m_wndPlaylistBar.GetCount() <= 1 && (m_pCB && m_pCB->ChapGetCount() <= 1)) ? THBF_DISABLED : THBF_ENABLED;
            buttons[4].dwFlags = THBF_ENABLED;

            if (iPlayState == PS_PLAY) {
                buttons[1].dwFlags = THBF_ENABLED;
                buttons[2].dwFlags = THBF_ENABLED;
                buttons[2].iBitmap = 2;

                hIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDR_TB_PLAY), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
                m_pTaskbarList->SetProgressState(m_hWnd, m_wndSeekBar.HasDuration() ? TBPF_NORMAL : TBPF_NOPROGRESS);
            } else if (iPlayState == PS_STOP) {
                buttons[1].dwFlags = THBF_DISABLED;
                buttons[2].dwFlags = THBF_ENABLED;
                buttons[2].iBitmap = 3;

                hIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDR_TB_STOP), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
                m_pTaskbarList->SetProgressState(m_hWnd, TBPF_NOPROGRESS);
            } else if (iPlayState == PS_PAUSE) {
                buttons[1].dwFlags = THBF_ENABLED;
                buttons[2].dwFlags = THBF_ENABLED;
                buttons[2].iBitmap = 3;

                hIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDR_TB_PAUSE), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
                m_pTaskbarList->SetProgressState(m_hWnd, m_wndSeekBar.HasDuration() ? TBPF_PAUSED : TBPF_NOPROGRESS);
            }

            if (GetPlaybackMode() == PM_DVD && m_iDVDDomain != DVD_DOMAIN_Title) {
                buttons[0].dwFlags = THBF_DISABLED;
                buttons[1].dwFlags = THBF_DISABLED;
                buttons[2].dwFlags = THBF_DISABLED;
                buttons[3].dwFlags = THBF_DISABLED;
            }

            m_pTaskbarList->SetOverlayIcon(m_hWnd, hIcon, L"");

            if (hIcon != nullptr) {
                DestroyIcon(hIcon);
            }
        } else {
            for (size_t i = 0; i < _countof(buttons); i++) {
                buttons[i].dwFlags = THBF_DISABLED;
            }

            m_pTaskbarList->SetOverlayIcon(m_hWnd, nullptr, L"");
            m_pTaskbarList->SetProgressState(m_hWnd, TBPF_NOPROGRESS);
        }

        UpdateThumbnailClip();
    }

    // Try to locate the window used to display the task bar to check if it is RTLed
    if (CWnd* pTaskBarWnd = FindWindow(_T("TaskListThumbnailWnd"), nullptr)) {
        // We don't want the buttons to be mirrored so we pre-mirror them
        if (pTaskBarWnd->GetExStyle() & WS_EX_LAYOUTRTL) {
            for (UINT i = 0; i < _countof(buttons); i++) {
                buttons[i].iBitmap = _countof(buttons) - buttons[i].iBitmap;
            }
            std::reverse(buttons, buttons + _countof(buttons));
        }
    }

    return m_pTaskbarList->ThumbBarUpdateButtons(m_hWnd, _countof(buttons), buttons);
}

HRESULT CMainFrame::UpdateThumbnailClip()
{
    if (!m_pTaskbarList || !m_hWnd) {
        return E_FAIL;
    }

    const CAppSettings& s = AfxGetAppSettings();

    CRect r;
    m_wndView.GetClientRect(&r);
    if (s.eCaptionMenuMode == MODE_SHOWCAPTIONMENU) {
        r.OffsetRect(0, GetSystemMetrics(SM_CYMENU));
    }

    if (!s.bUseEnhancedTaskBar || (GetLoadState() != MLS::LOADED) || m_fFullScreen || IsD3DFullScreenMode() || r.Width() <= 0 || r.Height() <= 0) {
        return m_pTaskbarList->SetThumbnailClip(m_hWnd, nullptr);
    }

    return m_pTaskbarList->SetThumbnailClip(m_hWnd, &r);
}

BOOL CMainFrame::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, LPCTSTR lpszMenuName, DWORD dwExStyle, CCreateContext* pContext)
{
    if (defaultMPCThemeMenu == nullptr) {
        defaultMPCThemeMenu = new CMPCThemeMenu();
    }
    if (lpszMenuName != NULL) {
        defaultMPCThemeMenu->LoadMenu(lpszMenuName);

        if (!CreateEx(dwExStyle, lpszClassName, lpszWindowName, dwStyle,
                      rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, pParentWnd->GetSafeHwnd(), defaultMPCThemeMenu->m_hMenu, (LPVOID)pContext)) {
            return FALSE;
        }
        defaultMPCThemeMenu->fulfillThemeReqs(true);

        return TRUE;
    }
    return FALSE;
}

void CMainFrame::enableFileDialogHook(CMPCThemeUtil* helper)
{
    if (AfxGetAppSettings().bWindows10DarkThemeActive) { //hard coded behavior for windows 10 dark theme file dialogs, irrespsective of theme loaded by user (fixing windows bugs)
        watchingFileDialog = true;
        fileDialogHookHelper = helper;
        fileDialogHandle = nullptr;
    }
}

LRESULT CMainFrame::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    if (!m_hWnd) {
        ASSERT(false);
        return 0;
    }

    if ((message == WM_COMMAND) && (THBN_CLICKED == HIWORD(wParam))) {
        int const wmId = LOWORD(wParam);
        switch (wmId) {
            case IDTB_BUTTON1:
                SendMessage(WM_COMMAND, ID_PLAY_STOP);
                break;
            case IDTB_BUTTON2:
                SendMessage(WM_COMMAND, ID_PLAY_PLAYPAUSE);
                break;
            case IDTB_BUTTON3:
                SendMessage(WM_COMMAND, ID_NAVIGATE_SKIPBACK);
                break;
            case IDTB_BUTTON4:
                SendMessage(WM_COMMAND, ID_NAVIGATE_SKIPFORWARD);
                break;
            case IDTB_BUTTON5:
                WINDOWPLACEMENT wp;
                GetWindowPlacement(&wp);
                if (wp.showCmd == SW_SHOWMINIMIZED) {
                    SendMessage(WM_SYSCOMMAND, SC_RESTORE, -1);
                }
                SetForegroundWindow();
                SendMessage(WM_COMMAND, ID_VIEW_FULLSCREEN);
                break;
            default:
                break;
        }
        return 0;
    } else if (watchingFileDialog && message == WM_ACTIVATE && LOWORD(wParam) == WA_INACTIVE) {
        fileDialogHandle = (HWND)lParam;
        watchingFileDialog = false;
        //capture but process message normally
    } else if (message == WM_GETICON && nullptr != fileDialogHandle) {
        HWND duiview = ::FindWindowEx(fileDialogHandle, NULL, _T("DUIViewWndClassName"), NULL);
        HWND duihwnd = ::FindWindowEx(duiview, NULL, _T("DirectUIHWND"), NULL);
        HWND firstchild = ::GetWindow(duihwnd, GW_CHILD);
        if (nullptr != firstchild) {
            fileDialogHookHelper->subClassFileDialog(this, duihwnd);
            ::RedrawWindow(duiview, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
            fileDialogHandle = nullptr;
        }
    }

    LRESULT ret = 0;
    bool bCallOurProc = true;
    if (m_pMVRSR) {
        // call madVR window proc directly when the interface is available
        switch (message) {
            case WM_MOUSEMOVE:
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
                // CMouseWnd will call madVR window proc
                break;
            default:
                bCallOurProc = !m_pMVRSR->ParentWindowProc(m_hWnd, message, &wParam, &lParam, &ret);
        }
    }
    if (bCallOurProc && m_hWnd) {
        ret = __super::WindowProc(message, wParam, lParam);
    }

    return ret;
}

bool CMainFrame::IsAeroSnapped()
{
    bool ret = false;
    WINDOWPLACEMENT wp = { sizeof(wp) };
    if (IsWindowVisible() && !IsZoomed() && !IsIconic() && GetWindowPlacement(&wp)) {
        CRect rect;
        GetWindowRect(rect);
        if (HMONITOR hMon = MonitorFromRect(rect, MONITOR_DEFAULTTONULL)) {
            MONITORINFO mi = { sizeof(mi) };
            if (GetMonitorInfo(hMon, &mi)) {
                CRect wpRect(wp.rcNormalPosition);
                wpRect.OffsetRect(mi.rcWork.left - mi.rcMonitor.left, mi.rcWork.top - mi.rcMonitor.top);
                ret = !!(rect != wpRect);
            } else {
                ASSERT(FALSE);
            }
        }
    }
    return ret;
}

UINT CMainFrame::OnPowerBroadcast(UINT nPowerEvent, LPARAM nEventData)
{
    static BOOL bWasPausedBeforeSuspention;

    switch (nPowerEvent) {
        case PBT_APMSUSPEND:            // System is suspending operation.
            TRACE(_T("OnPowerBroadcast - suspending\n"));   // For user tracking
            bWasPausedBeforeSuspention = FALSE;             // Reset value

            if (GetMediaState() == State_Running) {
                bWasPausedBeforeSuspention = TRUE;
                SendMessage(WM_COMMAND, ID_PLAY_PAUSE);     // Pause
            }
            break;
        case PBT_APMRESUMESUSPEND:     // System is resuming operation
            TRACE(_T("OnPowerBroadcast - resuming\n"));     // For user tracking

            // force seek to current position when resuming playback to re-initialize the video decoder
            m_dwLastPause = 1;

            // Resume if we paused before suspension.
            if (bWasPausedBeforeSuspention) {
                SendMessage(WM_COMMAND, ID_PLAY_PLAY);      // Resume
            }
            break;
    }

    return __super::OnPowerBroadcast(nPowerEvent, nEventData);
}

#define NOTIFY_FOR_THIS_SESSION 0

void CMainFrame::OnSessionChange(UINT nSessionState, UINT nId)
{
    static BOOL bWasPausedBeforeSessionChange;

    switch (nSessionState) {
        case WTS_SESSION_LOCK:
            TRACE(_T("OnSessionChange - Lock session\n"));
            bWasPausedBeforeSessionChange = FALSE;

            if (GetMediaState() == State_Running && !m_fAudioOnly) {
                bWasPausedBeforeSessionChange = TRUE;
                SendMessage(WM_COMMAND, ID_PLAY_PAUSE);
            }
            break;
        case WTS_SESSION_UNLOCK:
            TRACE(_T("OnSessionChange - UnLock session\n"));

            if (bWasPausedBeforeSessionChange) {
                SendMessage(WM_COMMAND, ID_PLAY_PLAY);
            }
            break;
    }
}

void CMainFrame::WTSRegisterSessionNotification()
{
    const WinapiFunc<BOOL WINAPI(HWND, DWORD)>
    fnWtsRegisterSessionNotification = { _T("wtsapi32.dll"), "WTSRegisterSessionNotification" };

    if (fnWtsRegisterSessionNotification) {
        fnWtsRegisterSessionNotification(m_hWnd, NOTIFY_FOR_THIS_SESSION);
    }
}

void CMainFrame::WTSUnRegisterSessionNotification()
{
    const WinapiFunc<BOOL WINAPI(HWND)>
    fnWtsUnRegisterSessionNotification = { _T("wtsapi32.dll"), "WTSUnRegisterSessionNotification" };

    if (fnWtsUnRegisterSessionNotification) {
        fnWtsUnRegisterSessionNotification(m_hWnd);
    }
}

void CMainFrame::UpdateSkypeHandler()
{
    const auto& s = AfxGetAppSettings();
    if (s.bNotifySkype && !m_pSkypeMoodMsgHandler) {
        m_pSkypeMoodMsgHandler.Attach(DEBUG_NEW SkypeMoodMsgHandler());
        m_pSkypeMoodMsgHandler->Connect(m_hWnd);
    } else if (!s.bNotifySkype && m_pSkypeMoodMsgHandler) {
        m_pSkypeMoodMsgHandler.Free();
    }
}

void CMainFrame::UpdateSeekbarChapterBag()
{
    const auto& s = AfxGetAppSettings();
    if (s.fShowChapters && m_pCB) {
        m_wndSeekBar.SetChapterBag(m_pCB);
        m_OSD.SetChapterBag(m_pCB);
    } else {
        m_wndSeekBar.RemoveChapters();
        m_OSD.RemoveChapters();
    }
}

void CMainFrame::UpdateAudioSwitcher()
{
    CAppSettings& s = AfxGetAppSettings();
    CComQIPtr<IAudioSwitcherFilter> pASF = FindFilter(__uuidof(CAudioSwitcherFilter), m_pGB);

    if (pASF) {
        pASF->SetSpeakerConfig(s.fCustomChannelMapping, s.pSpeakerToChannelMap);
        pASF->EnableDownSamplingTo441(s.fDownSampleTo441);
        pASF->SetAudioTimeShift(s.fAudioTimeShift ? 10000i64 * s.iAudioTimeShift : 0);
        pASF->SetNormalizeBoost2(s.fAudioNormalize, s.nAudioMaxNormFactor, s.fAudioNormalizeRecover, s.nAudioBoost);
    }
}

void CMainFrame::UpdateControlState(UpdateControlTarget target)
{
    const auto& s = AfxGetAppSettings();
    switch (target) {
        case UPDATE_VOLUME_STEP:
            m_wndToolBar.m_volctrl.SetPageSize(s.nVolumeStep);
            break;
        case UPDATE_LOGO:
            if (GetLoadState() == MLS::LOADED && m_fAudioOnly && s.bEnableCoverArt) {
                CString filename = m_wndPlaylistBar.GetCurFileName();
                CString filename_no_ext;
                CString filedir;
                CPath path = CPath(filename);
                if (path.FileExists()) {
                    path.RemoveExtension();
                    filename_no_ext = path.m_strPath;
                    path.RemoveFileSpec();
                    filedir = path.m_strPath;
                }

                CString author;
                m_wndInfoBar.GetLine(StrRes(IDS_INFOBAR_AUTHOR), author);

                CComQIPtr<IFilterGraph> pFilterGraph = m_pGB;
                CPlaylistItem* pli = m_wndPlaylistBar.GetCur();
                std::vector<BYTE> internalCover;
                if (CoverArt::FindEmbedded(pFilterGraph, internalCover)) {
                    m_wndView.LoadImg(internalCover);
                    m_currentCoverPath = filename;
                    m_currentCoverAuthor = author;
                } else if (pli && !pli->m_cover.IsEmpty() && CPath(pli->m_cover).FileExists()) {
                    m_wndView.LoadImg(pli->m_cover);
                } else if (!filedir.IsEmpty() && (m_currentCoverPath != filedir || m_currentCoverAuthor != author || currentCoverIsFileArt)) {
                    m_wndView.LoadImg(CoverArt::FindExternal(filename_no_ext, filedir, author, currentCoverIsFileArt));
                    m_currentCoverPath = filedir;
                    m_currentCoverAuthor = author;
                } else if (!m_wndView.IsCustomImgLoaded()) {
                    m_wndView.LoadImg();
                }
            } else {
                m_currentCoverPath.Empty();
                m_currentCoverAuthor.Empty();
                m_wndView.LoadImg();
            }
            break;
        case UPDATE_SKYPE:
            UpdateSkypeHandler();
            break;
        case UPDATE_SEEKBAR_CHAPTERS:
            UpdateSeekbarChapterBag();
            break;
        case UPDATE_WINDOW_TITLE:
            OpenSetupWindowTitle();
            break;
        case UPDATE_AUDIO_SWITCHER:
            UpdateAudioSwitcher();
            break;
        case UPDATE_CONTROLS_VISIBILITY:
            m_controls.UpdateToolbarsVisibility();
            break;
        case UPDATE_CHILDVIEW_CURSOR_HACK:
            // HACK: windowed (not renderless) video renderers created in graph thread do not
            // produce WM_MOUSEMOVE message when we release mouse capture on top of it, here's a workaround
            m_timerOneTime.Subscribe(TimerOneTimeSubscriber::CHILDVIEW_CURSOR_HACK, std::bind(&CChildView::Invalidate, &m_wndView, FALSE), 16);
            break;
        default:
            ASSERT(FALSE);
    }
}

void CMainFrame::UpdateUILanguage()
{
    //    CMenu  defaultMenu;
    CMenu* oldMenu;

    // Destroy the dynamic menus before reloading the main menus
    DestroyDynamicMenus();

    // Reload the main menus
    m_popupMenu.DestroyMenu();
    m_popupMenu.LoadMenu(IDR_POPUP);
    m_mainPopupMenu.DestroyMenu();
    m_mainPopupMenu.LoadMenu(IDR_POPUPMAIN);

    oldMenu = GetMenu();
    defaultMPCThemeMenu = new CMPCThemeMenu(); //will have been destroyed
    defaultMPCThemeMenu->LoadMenu(IDR_MAINFRAME);
    if (oldMenu) {
        // Attach the new menu to the window only if there was a menu before
        SetMenu(defaultMPCThemeMenu);
        // and then destroy the old one
        oldMenu->DestroyMenu();
    }
    //we don't detach because we retain the cmenu
    //m_hMenuDefault = defaultMenu.Detach();
    m_hMenuDefault = defaultMPCThemeMenu->GetSafeHmenu();

    m_popupMenu.fulfillThemeReqs();
    m_mainPopupMenu.fulfillThemeReqs();
    defaultMPCThemeMenu->fulfillThemeReqs(true);

    // Reload the dynamic menus
    CreateDynamicMenus();

    // Reload the static bars
    OpenSetupInfoBar();
    if (GetPlaybackMode() == PM_DIGITAL_CAPTURE) {
        UpdateCurrentChannelInfo(false, false);
    }
    OpenSetupStatsBar();

    // Reload the debug shaders dialog if need be
    if (m_pDebugShaders && IsWindow(m_pDebugShaders->m_hWnd)) {
        BOOL bWasVisible = m_pDebugShaders->IsWindowVisible();
        VERIFY(m_pDebugShaders->DestroyWindow());
        m_pDebugShaders = std::make_unique<CDebugShadersDlg>();
        if (bWasVisible) {
            m_pDebugShaders->ShowWindow(SW_SHOWNA);
            // Don't steal focus from main frame
            SetActiveWindow();
        }
    }
}

bool CMainFrame::OpenBD(CString Path)
{
    CHdmvClipInfo ClipInfo;
    CString strPlaylistFile;
    CAtlList<CHdmvClipInfo::PlaylistItem> MainPlaylist;

#if INTERNAL_SOURCEFILTER_MPEG
    const CAppSettings& s = AfxGetAppSettings();
    bool InternalMpegSplitter = s.SrcFilters[SRC_MPEG] || s.SrcFilters[SRC_MPEGTS];
#else
    bool InternalMpegSplitter = false;
#endif

    m_LastOpenBDPath = Path;

    CString ext = CPath(Path).GetExtension();
    ext.MakeLower();

    if ((CPath(Path).IsDirectory() && Path.Find(_T("\\BDMV")))
            || CPath(Path + _T("\\BDMV")).IsDirectory()
            || (!ext.IsEmpty() && ext == _T(".bdmv"))) {
        if (!ext.IsEmpty() && ext == _T(".bdmv")) {
            Path.Replace(_T("\\BDMV\\"), _T("\\"));
            CPath _Path(Path);
            _Path.RemoveFileSpec();
            Path = CString(_Path);
        } else if (Path.Find(_T("\\BDMV"))) {
            Path.Replace(_T("\\BDMV"), _T("\\"));
        }
        if (SUCCEEDED(ClipInfo.FindMainMovie(Path, strPlaylistFile, MainPlaylist, m_MPLSPlaylist))) {
            m_bIsBDPlay = true;

            m_bHasBDMeta = ClipInfo.ReadMeta(Path, m_BDMeta);

            if (!InternalMpegSplitter && !ext.IsEmpty() && ext == _T(".bdmv")) {
                return false;
            } else {
                m_wndPlaylistBar.Empty();
                CAtlList<CString> sl;

                if (InternalMpegSplitter) {
                    sl.AddTail(CString(strPlaylistFile));
                } else {
                    sl.AddTail(CString(Path + _T("\\BDMV\\index.bdmv")));
                }

                m_wndPlaylistBar.Append(sl, false);
                OpenCurPlaylistItem();
                return true;
            }
        }
    }

    m_LastOpenBDPath = _T("");
    return false;
}

// Returns the the corresponding subInput or nullptr in case of error.
// i is modified to reflect the locale index of track
SubtitleInput* CMainFrame::GetSubtitleInput(int& i, bool bIsOffset /*= false*/)
{
    // Only 1, 0 and -1 are supported offsets
    if ((bIsOffset && (i < -1 || i > 1)) || (!bIsOffset && i < 0)) {
        return nullptr;
    }

    POSITION pos = m_pSubStreams.GetHeadPosition();
    SubtitleInput* pSubInput = nullptr, *pSubInputPrec = nullptr;
    int iLocalIdx = -1, iLocalIdxPrec = -1;
    bool bNextTrack = false;

    while (pos && !pSubInput) {
        SubtitleInput& subInput = m_pSubStreams.GetNext(pos);

        if (CComQIPtr<IAMStreamSelect> pSSF = subInput.pSourceFilter) {
            DWORD cStreams;
            if (FAILED(pSSF->Count(&cStreams))) {
                continue;
            }

            for (int j = 0, cnt = (int)cStreams; j < cnt; j++) {
                DWORD dwFlags, dwGroup;

                if (FAILED(pSSF->Info(j, nullptr, &dwFlags, nullptr, &dwGroup, nullptr, nullptr, nullptr))) {
                    continue;
                }

                if (dwGroup != 2) {
                    continue;
                }

                if (bIsOffset) {
                    if (bNextTrack) { // We detected previously that the next subtitles track is the one we want to select
                        pSubInput = &subInput;
                        iLocalIdx = j;
                        break;
                    } else if (subInput.pSubStream == m_pCurrentSubInput.pSubStream
                               && dwFlags & (AMSTREAMSELECTINFO_ENABLED | AMSTREAMSELECTINFO_EXCLUSIVE)) {
                        if (i == 0) {
                            pSubInput = &subInput;
                            iLocalIdx = j;
                            break;
                        } else if (i > 0) {
                            bNextTrack = true; // We want to the select the next subtitles track
                        } else {
                            // We want the previous subtitles track and we know which one it is
                            if (pSubInputPrec) {
                                pSubInput = pSubInputPrec;
                                iLocalIdx = iLocalIdxPrec;
                                break;
                            }
                        }
                    }

                    pSubInputPrec = &subInput;
                    iLocalIdxPrec = j;
                } else {
                    if (i == 0) {
                        pSubInput = &subInput;
                        iLocalIdx = j;
                        break;
                    }

                    i--;
                }
            }
        } else {
            if (bIsOffset) {
                if (bNextTrack) { // We detected previously that the next subtitles track is the one we want to select
                    pSubInput = &subInput;
                    iLocalIdx = 0;
                    break;
                } else if (subInput.pSubStream == m_pCurrentSubInput.pSubStream) {
                    iLocalIdx = subInput.pSubStream->GetStream() + i;
                    if (iLocalIdx >= 0 && iLocalIdx < subInput.pSubStream->GetStreamCount()) {
                        // The subtitles track we want to select is part of this substream
                        pSubInput = &subInput;
                    } else if (i > 0) { // We want to the select the next subtitles track
                        bNextTrack = true;
                    } else {
                        // We want the previous subtitles track and we know which one it is
                        if (pSubInputPrec) {
                            pSubInput = pSubInputPrec;
                            iLocalIdx = iLocalIdxPrec;
                        }
                    }
                } else {
                    pSubInputPrec = &subInput;
                    iLocalIdxPrec = subInput.pSubStream->GetStreamCount() - 1;
                }
            } else {
                if (i < subInput.pSubStream->GetStreamCount()) {
                    pSubInput = &subInput;
                    iLocalIdx = i;
                } else {
                    i -= subInput.pSubStream->GetStreamCount();
                }
            }
        }

        // Handle special cases
        if (!pos && !pSubInput && bIsOffset) {
            if (bNextTrack) { // The last subtitles track was selected and we want the next one
                // Let's restart the loop to select the first subtitles track
                pos = m_pSubStreams.GetHeadPosition();
            } else if (i < 0) { // The first subtitles track was selected and we want the previous one
                pSubInput = pSubInputPrec; // We select the last track
                iLocalIdx = iLocalIdxPrec;
            }
        }
    }

    i = iLocalIdx;

    return pSubInput;
}

CString CMainFrame::GetFileName()
{
    CPlaylistItem* pli = m_wndPlaylistBar.GetCur();
    if (pli) {
        CString path(m_wndPlaylistBar.GetCurFileName());
        if (!pli->m_bYoutubeDL && m_pFSF) {
            CComHeapPtr<OLECHAR> pFN;
            if (SUCCEEDED(m_pFSF->GetCurFile(&pFN, nullptr))) {
                path = pFN;
            }
        }
        if (PathUtils::IsURL(path)) {
            path = ShortenURL(path);
        }
        return pli->m_bYoutubeDL ? path : PathUtils::StripPathOrUrl(path);
    }
    return _T("");
}

CString CMainFrame::GetCaptureTitle()
{
    CString title;

    title.LoadString(IDS_CAPTURE_LIVE);
    if (GetPlaybackMode() == PM_ANALOG_CAPTURE) {
        CString devName = GetFriendlyName(m_VidDispName);
        if (!devName.IsEmpty()) {
            title.AppendFormat(_T(" | %s"), devName.GetString());
        }
    } else {
        CString& eventName = m_pDVBState->NowNext.eventName;
        if (m_pDVBState->bActive) {
            title.AppendFormat(_T(" | %s"), m_pDVBState->sChannelName.GetString());
            if (!eventName.IsEmpty()) {
                title.AppendFormat(_T(" - %s"), eventName.GetString());
            }
        } else {
            title += _T(" | DVB");
        }
    }
    return title;
}

GUID CMainFrame::GetTimeFormat()
{
    GUID ret;
    if (!m_pMS || !SUCCEEDED(m_pMS->GetTimeFormat(&ret))) {
        ASSERT(FALSE);
        ret = TIME_FORMAT_NONE;
    }
    return ret;
}

void CMainFrame::UpdateDXVAStatus()
{
    CString DXVADecoderDescription = GetDXVADecoderDescription();
    m_HWAccelType = GetDXVAVersion();
    m_bUsingDXVA = (_T("Not using DXVA") != DXVADecoderDescription && _T("Unknown") != DXVADecoderDescription);

    CString DXVAInfo;
    DXVAInfo.Format(_T("%-13s: %s"), m_HWAccelType, DXVADecoderDescription.GetString());

    // If LAV Video is in the graph, we query it since it's always more reliable than the hook.
    if (CComQIPtr<ILAVVideoStatus> pLAVVideoStatus = FindFilter(GUID_LAVVideo, m_pGB)) {
        const LPCWSTR decoderName = pLAVVideoStatus->GetActiveDecoderName();
        ASSERT(decoderName != nullptr);
        if (wcscmp(decoderName, L"avcodec") != 0 && wcscmp(decoderName, L"wmv9 mft") != 0 && wcscmp(decoderName, L"msdk mvc") != 0) {
            m_HWAccelType = CFGFilterLAVVideo::GetUserFriendlyDecoderName(decoderName);
            CString LAVDXVAInfo;
            LAVDXVAInfo.Format(_T("LAV Video Decoder (%s)"), m_HWAccelType);

            if (!m_bUsingDXVA) { // Don't trust the hook
                m_bUsingDXVA = true;
                DXVAInfo.Format(_T("H/W Decoder  : %s"), LAVDXVAInfo.GetString());
            } else {
                DXVAInfo.AppendFormat(_T(" [%s]"), LAVDXVAInfo.GetString());
            }
        }
    }
    GetRenderersData()->m_strDXVAInfo = DXVAInfo;
}

bool CMainFrame::GetDecoderType(CString& type) const
{
    if (!m_fAudioOnly) {
        if (m_bUsingDXVA) {
            type = m_HWAccelType;
        } else {
            type.LoadString(IDS_TOOLTIP_SOFTWARE_DECODING);
        }
        return true;
    }
    return false;
}

void CMainFrame::UpdateSubOverridePlacement()
{
    const CAppSettings& s = AfxGetAppSettings();
    if (auto pRTS = dynamic_cast<CRenderedTextSubtitle*>((ISubStream*)m_pCurrentSubInput.pSubStream)) {
        {
            CAutoLock cAutoLock(&m_csSubLock);

            pRTS->SetAlignment(s.fOverridePlacement, s.nHorPos, s.nVerPos);
            pRTS->Deinit();
        }
        InvalidateSubtitle();
    } else if (auto pVSS = dynamic_cast<CVobSubSettings*>((ISubStream*)m_pCurrentSubInput.pSubStream)) {
        {
            CAutoLock cAutoLock(&m_csSubLock);

            pVSS->SetAlignment(s.fOverridePlacement, s.nHorPos, s.nVerPos);
        }
        InvalidateSubtitle();
    }
}

void CMainFrame::UpdateSubDefaultStyle()
{
    const CAppSettings& s = AfxGetAppSettings();
    if (auto pRTS = dynamic_cast<CRenderedTextSubtitle*>((ISubStream*)m_pCurrentSubInput.pSubStream)) {
        {
            CAutoLock cAutoLock(&m_csSubLock);

            pRTS->SetOverride(s.fUseDefaultSubtitlesStyle, s.subtitlesDefStyle);
            pRTS->Deinit();
        }
        InvalidateSubtitle();
    }
}

void CMainFrame::UpdateSubAspectRatioCompensation()
{
    const CAppSettings& s = AfxGetAppSettings();
    if (auto pRTS = dynamic_cast<CRenderedTextSubtitle*>((ISubStream*)m_pCurrentSubInput.pSubStream)) {
        CAutoLock autoLockSubtitleManagement(&m_csSubtitleManagementLock);

        bool bInvalidate = false;
        double dPARCompensation = 1.0;
        if (m_pCAP) {
            bool bKeepAspectRatio = s.fKeepAspectRatio;
            CSize szAspectRatio = m_pCAP->GetVideoSize(true);
            CSize szVideoFrame;
            if (m_pMVRI) {
                // Use IMadVRInfo to get size. See http://bugs.madshi.net/view.php?id=180
                m_pMVRI->GetSize("originalVideoSize", &szVideoFrame);
                bKeepAspectRatio = true;
            } else {
                szVideoFrame = m_pCAP->GetVideoSize(false);
            }

            if (s.bSubtitleARCompensation && szAspectRatio.cx && szAspectRatio.cy && szVideoFrame.cx && szVideoFrame.cy && bKeepAspectRatio) {
                dPARCompensation = ((double)szAspectRatio.cx / szAspectRatio.cy) /
                                   ((double)szVideoFrame.cx / szVideoFrame.cy);
            }
        }

        pRTS->m_ePARCompensationType = CSimpleTextSubtitle::EPARCompensationType::EPCTAccurateSize_ISR;
        if (pRTS->m_dPARCompensation != dPARCompensation) {
            CAutoLock autoLock(&m_csSubLock);
            bInvalidate = true;
            pRTS->m_dPARCompensation = dPARCompensation;
            pRTS->Deinit();
        }

        if (bInvalidate) {
            InvalidateSubtitle();
        }
    }
}

REFTIME CMainFrame::GetAvgTimePerFrame() const
{
    REFTIME refAvgTimePerFrame = 0.0;

    if (FAILED(m_pBV->get_AvgTimePerFrame(&refAvgTimePerFrame))) {
        if (m_pCAP) {
            refAvgTimePerFrame = 1.0 / m_pCAP->GetFPS();
        }

        BeginEnumFilters(m_pGB, pEF, pBF) {
            if (refAvgTimePerFrame > 0.0) {
                break;
            }

            BeginEnumPins(pBF, pEP, pPin) {
                AM_MEDIA_TYPE mt;
                if (SUCCEEDED(pPin->ConnectionMediaType(&mt))) {
                    if (mt.majortype == MEDIATYPE_Video && mt.formattype == FORMAT_VideoInfo) {
                        refAvgTimePerFrame = (REFTIME)((VIDEOINFOHEADER*)mt.pbFormat)->AvgTimePerFrame / 10000000i64;
                        break;
                    } else if (mt.majortype == MEDIATYPE_Video && mt.formattype == FORMAT_VideoInfo2) {
                        refAvgTimePerFrame = (REFTIME)((VIDEOINFOHEADER2*)mt.pbFormat)->AvgTimePerFrame / 10000000i64;
                        break;
                    }
                }
            }
            EndEnumPins;
        }
        EndEnumFilters;
    }

    // Double-check that the detection is correct for DVDs
    DVD_VideoAttributes VATR;
    if (m_pDVDI && SUCCEEDED(m_pDVDI->GetCurrentVideoAttributes(&VATR))) {
        double ratio;
        if (VATR.ulFrameRate == 50) {
            ratio = 25.0 * refAvgTimePerFrame;
            // Accept 25 or 50 fps
            if (!IsNearlyEqual(ratio, 1.0, 1e-2) && !IsNearlyEqual(ratio, 2.0, 1e-2)) {
                refAvgTimePerFrame = 1.0 / 25.0;
            }
        } else {
            ratio = 29.97 * refAvgTimePerFrame;
            // Accept 29,97, 59.94, 23.976 or 47.952 fps
            if (!IsNearlyEqual(ratio, 1.0, 1e-2) && !IsNearlyEqual(ratio, 2.0, 1e-2)
                    && !IsNearlyEqual(ratio, 1.25, 1e-2) && !IsNearlyEqual(ratio, 2.5, 1e-2)) {
                refAvgTimePerFrame = 1.0 / 29.97;
            }
        }
    }

    return refAvgTimePerFrame;
}

void CMainFrame::OnVideoSizeChanged(const bool bWasAudioOnly /*= false*/)
{
    const auto& s = AfxGetAppSettings();
    if (GetLoadState() == MLS::LOADED &&
            ((s.fRememberZoomLevel && (s.fLimitWindowProportions || m_bAllowWindowZoom)) || m_fAudioOnly || bWasAudioOnly) &&
            !(m_fFullScreen || IsD3DFullScreenMode() || IsZoomed() || IsIconic() || IsAeroSnapped())) {
        CSize videoSize;
        if (!m_fAudioOnly && !m_bAllowWindowZoom) {
            videoSize = GetVideoSize();
        }
        if (videoSize.cx && videoSize.cy) {
            ZoomVideoWindow(m_dLastVideoScaleFactor * std::sqrt((static_cast<double>(m_lastVideoSize.cx) * m_lastVideoSize.cy)
                                                                / (static_cast<double>(videoSize.cx) * videoSize.cy)));
        } else {
            ZoomVideoWindow();
        }
    }
    MoveVideoWindow();
}

typedef struct {
    SubtitlesInfo* pSubtitlesInfo;
    BOOL bActivate;
    std::string fileName;
    std::string fileContents;
} SubtitlesData;

LRESULT CMainFrame::OnLoadSubtitles(WPARAM wParam, LPARAM lParam)
{
    SubtitlesData& data = *(SubtitlesData*)lParam;

    CAutoPtr<CRenderedTextSubtitle> pRTS(DEBUG_NEW CRenderedTextSubtitle(&m_csSubLock));
    if (pRTS) {
#if USE_LIBASS
        SubRendererSettings srs = AfxGetAppSettings().GetSubRendererSettings();
        pRTS->SetSubRenderSettings(srs);
#endif
        pRTS->SetDefaultStyle(AfxGetAppSettings().subtitlesDefStyle);
        if (pRTS->Open(CString(data.pSubtitlesInfo->Provider()->Name().c_str()),
            (BYTE*)(LPCSTR)data.fileContents.c_str(), (int)data.fileContents.length(), DEFAULT_CHARSET,
            UTF8To16(data.fileName.c_str()), Subtitle::HearingImpairedType(data.pSubtitlesInfo->hearingImpaired),
            ISOLang::ISO6391ToLcid(data.pSubtitlesInfo->languageCode.c_str())) && pRTS->GetStreamCount() > 0) {
            m_wndSubtitlesDownloadDialog.DoDownloaded(*data.pSubtitlesInfo);
#if USE_LIBASS
            pRTS->SetFilterGraph(m_pGB);
#endif

            SubtitleInput subElement = pRTS.Detach();
            m_pSubStreams.AddTail(subElement);
            if (data.bActivate) {
            m_ExternalSubstreams.push_back(subElement.pSubStream);
                SetSubtitle(subElement.pSubStream);
            }
            return TRUE;
        }
    }

    return FALSE;
}

LRESULT CMainFrame::OnGetSubtitles(WPARAM, LPARAM lParam)
{
    CheckPointer(lParam, FALSE);

    int n = 0;
    SubtitleInput* pSubInput = GetSubtitleInput(n, true);
    CheckPointer(pSubInput, FALSE);

    CLSID clsid;
    if (FAILED(pSubInput->pSubStream->GetClassID(&clsid)) || clsid != __uuidof(CRenderedTextSubtitle)) {
        return FALSE;
    }

    CRenderedTextSubtitle* pRTS = (CRenderedTextSubtitle*)(ISubStream*)pSubInput->pSubStream;
    // Only for external text subtitles
    if (pRTS->m_path.IsEmpty()) {
        return FALSE;
    }

    SubtitlesInfo* pSubtitlesInfo = reinterpret_cast<SubtitlesInfo*>(lParam);

    pSubtitlesInfo->GetFileInfo();
    pSubtitlesInfo->releaseNames.emplace_back(UTF16To8(pRTS->m_name));
    if (pSubtitlesInfo->hearingImpaired == Subtitle::HI_UNKNOWN) {
        pSubtitlesInfo->hearingImpaired = pRTS->m_eHearingImpaired;
    }

    if (!pSubtitlesInfo->languageCode.length() && pRTS->m_lcid && pRTS->m_lcid != LCID(-1)) {
        CString str;
        GetLocaleString(pRTS->m_lcid, LOCALE_SISO639LANGNAME, str);
        pSubtitlesInfo->languageCode = UTF16To8(str);
    }

    pSubtitlesInfo->frameRate = m_pCAP->GetFPS();
    int delay = m_pCAP->GetSubtitleDelay();
    if (pRTS->m_mode == FRAME) {
        delay = std::lround(delay * pSubtitlesInfo->frameRate / 1000.0);
    }

    const CStringW fmt(L"%d\n%02d:%02d:%02d,%03d --> %02d:%02d:%02d,%03d\n%s\n\n");
    CStringW content;
    CAutoLock cAutoLock(&m_csSubLock);
    for (int i = 0, j = int(pRTS->GetCount()), k = 0; i < j; i++) {
        int t1 = (int)(RT2MS(pRTS->TranslateStart(i, pSubtitlesInfo->frameRate)) + delay);
        if (t1 < 0) {
            k++;
            continue;
        }

        int t2 = (int)(RT2MS(pRTS->TranslateEnd(i, pSubtitlesInfo->frameRate)) + delay);

        int hh1 = (t1 / 60 / 60 / 1000);
        int mm1 = (t1 / 60 / 1000) % 60;
        int ss1 = (t1 / 1000) % 60;
        int ms1 = (t1) % 1000;
        int hh2 = (t2 / 60 / 60 / 1000);
        int mm2 = (t2 / 60 / 1000) % 60;
        int ss2 = (t2 / 1000) % 60;
        int ms2 = (t2) % 1000;

        content.AppendFormat(fmt, i - k + 1, hh1, mm1, ss1, ms1, hh2, mm2, ss2, ms2, pRTS->GetStrW(i, false).GetString());
    }

    pSubtitlesInfo->fileContents = UTF16To8(content);
    return TRUE;
}

static const CString ydl_whitelist[] = {
    _T("youtube.com/"),
    _T("youtu.be/"),
    _T("twitch.com/")
};

static const CString ydl_blacklist[] = {
    _T("googlevideo.com/videoplayback"), // already processed URL
    _T("googlevideo.com/api/manifest"),
    _T("saunalahti.fi/")
};

bool CMainFrame::IsOnYDLWhitelist(CString url) {
    for (int i = 0; i < _countof(ydl_whitelist); i++) {
        if (url.Find(ydl_whitelist[i]) >= 0) {
            return true;
        }
    }
    return false;
}

bool CMainFrame::CanSendToYoutubeDL(const CString url)
{
    if (url.Left(4).MakeLower() == _T("http") && AfxGetAppSettings().bUseYDL) {
        // Blacklist: don't use for IP addresses
        std::wcmatch regmatch;
        std::wregex regexp(LR"(https?:\/\/(\d{1,3}\.){3}\d{1,3}.*)");
        if (std::regex_match(url.GetString(), regmatch, regexp)) {
            return false;
        }

        // Whitelist: popular supported sites
        if (IsOnYDLWhitelist(url)) {
            return true;
        }

        // Blacklist: unsupported sites where YDL causes an error or long delay
        for (int i = 0; i < _countof(ydl_blacklist); i++) {
            if (url.Find(ydl_blacklist[i], 7) > 0) {
                return false;
            }
        }

        // Blacklist: URL points to a file
        CString baseurl;
        int q = url.Find(_T('?'));
        if (q > 0) {
            baseurl = url.Left(q);
        } else {
            baseurl = url;
            q = url.GetLength();
        }
        int p = baseurl.ReverseFind(_T('.'));
        if (p > 0 && (q - p <= 6)) {
            CString ext = baseurl.Mid(p);
            if (AfxGetAppSettings().m_Formats.FindExt(ext)) {
                return false;
            }
        }

        return true;
    }
    return false;
}

bool CMainFrame::ProcessYoutubeDLURL(CString url, bool append, bool replace)
{
    auto& s = AfxGetAppSettings();
    CAtlList<CYoutubeDLInstance::YDLStreamURL> streams;
    CAtlList<CString> filenames;
    CYoutubeDLInstance ydl;
    CYoutubeDLInstance::YDLPlaylistInfo listinfo;

    m_wndStatusBar.SetStatusMessage(ResStr(IDS_CONTROLS_YOUTUBEDL));

    if (!ydl.Run(url)) {
        return false;
    }
    if (!ydl.GetHttpStreams(streams, listinfo)) {
        return false;
    }

    m_sydlLastProcessURL = url;

    if (!append && !replace) {
        m_wndPlaylistBar.Empty();
    }

    CString f_title;

    for (unsigned int i = 0; i < streams.GetCount(); i++) {
        auto stream = streams.GetAt(streams.FindIndex(i));
        CString v_url = stream.video_url;
        CString a_url = stream.audio_url;
        filenames.RemoveAll();
        if (!v_url.IsEmpty() && (!s.bYDLAudioOnly || a_url.IsEmpty())) {
            filenames.AddTail(v_url);

        }
        if (!a_url.IsEmpty()) {
            filenames.AddTail(a_url);
        }
        CString title = stream.title;
        CString seasonid;
        if (stream.season_number != -1) {
            seasonid.Format(_T("S%02d"), stream.season_number);
        }
        CString episodeid;
        if (stream.episode_number != -1) {
            episodeid.Format(_T("E%02d"), stream.episode_number);
        }
        CString epiid;
        if (!seasonid.IsEmpty() || !episodeid.IsEmpty()) {
            epiid.Format(_T("%s%s. "), static_cast<LPCWSTR>(seasonid), static_cast<LPCWSTR>(episodeid));
        }
        CString season;
        if (!stream.series.IsEmpty()) {
            season = stream.series;
            CString t(stream.season.Left(6));
            if (!stream.season.IsEmpty() && (t.MakeLower() != _T("season") || stream.season_number == -1)) {
                season += _T(" ") + stream.season;
            }
            season += _T(" - ");
        }
        else if (!stream.season.IsEmpty()) {
            CString t(stream.season.Left(6));
            if (t.MakeLower() != _T("season") || stream.season_number == -1) {
                season = stream.season + _T(" - ");
            }
        }
        title.Format(_T("%s%s%s"), static_cast<LPCWSTR>(epiid), static_cast<LPCWSTR>(season), static_cast<LPCWSTR>(title));
        if (i == 0) f_title = title;
        int targetlen = title.GetLength() > 100 ? 50 : 150 - title.GetLength();
        CString short_url;
        if (streams.GetCount() > 1 && !stream.webpage_url.IsEmpty()) {
            short_url = ShortenURL(stream.webpage_url, targetlen, true);
        } else {
            short_url = ShortenURL(url, targetlen, true);
        }
        CString ydl_src = url;
        if (url == filenames.GetHead()) {
            // Processed URL is same as input, can happen for DASH manifest files. Clear source URL to avoid reprocessing.
            ydl_src = _T("");
        }
        if (replace) {
            m_wndPlaylistBar.ReplaceCurrentItem(filenames, nullptr, title + " (" + short_url + ")", ydl_src, _T(""), &stream.subtitles);
            break;
        } else {
            m_wndPlaylistBar.Append(filenames, false, nullptr, title + " (" + short_url + ")", ydl_src, _T(""), &stream.subtitles);
        }
    }

    if (s.fKeepHistory) {
        auto* mru = &s.MRU;
        mru->ReadList();
        RecentFileEntry r;
        r.fns.AddTail(url);
        if (streams.GetCount() > 1) {
            auto h = streams.GetHead();
            if (!h.series.IsEmpty()) {
                r.title = h.series;
                if (!h.season.IsEmpty()) {
                    r.title += _T(" - ") + h.season;
                }
            }
            else if (!h.season.IsEmpty()) {
                r.title = h.season;
            }
            else if (!listinfo.title.IsEmpty()) {
                if (!listinfo.uploader.IsEmpty()) r.title.Format(_T("%s - %s"), static_cast<LPCWSTR>(listinfo.uploader), static_cast<LPCWSTR>(listinfo.title));
                else r.title = listinfo.title;
            }
            else r.title = f_title;
        }
        else if (streams.GetCount() == 1) {
            r.title = f_title;
        }
        m_current_rfe = r;
        mru->Add(r);
        mru->WriteList();
    }

    if (!append && (!replace || !m_wndPlaylistBar.GetCur())) {
        m_wndPlaylistBar.SetFirst();
    }
    return true;
}

bool CMainFrame::DownloadWithYoutubeDL(CString url, CString filename)
{
    PROCESS_INFORMATION proc_info;
    STARTUPINFO startup_info;
    const auto& s = AfxGetAppSettings();

    CString args = _T("youtube-dl --console-title \"") + url + _T("\"");
    if (!s.sYDLCommandLine.IsEmpty()) {
        args.Append(_T(" "));
        args.Append(s.sYDLCommandLine);
    }
    if (s.bYDLAudioOnly && (s.sYDLCommandLine.Find(_T("-f ")) < 0)) {
        args.Append(_T(" -f bestaudio"));
    }
    if (s.sYDLCommandLine.Find(_T("-o ")) < 0) {
        args.Append(_T(" -o \"" + filename + "\""));
    }

    ZeroMemory(&proc_info, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&startup_info, sizeof(STARTUPINFO));
    startup_info.cb = sizeof(STARTUPINFO);

    if (!CreateProcess(NULL, args.GetBuffer(), NULL, NULL, false, 0,
                       NULL, NULL, &startup_info, &proc_info)) {
        AfxMessageBox(_T("An error occurred while attempting to run youtube-dl"), MB_ICONERROR, 0);
        return false;
    }

    CloseHandle(proc_info.hProcess);
    CloseHandle(proc_info.hThread);

    return true;
}

void CMainFrame::OnSettingChange(UINT uFlags, LPCTSTR lpszSection)
{
    __super::OnSettingChange(uFlags, lpszSection);
    if (SPI_SETNONCLIENTMETRICS == uFlags) {
        CMPCThemeMenu::clearDimensions();
        if (nullptr != defaultMPCThemeMenu) {
            UpdateUILanguage(); //cheap way to rebuild menus--we want to do this to force them to re-measure
        }
        RecalcLayout();
    }
}

BOOL CMainFrame::OnMouseWheel(UINT nFlags, short zDelta, CPoint point) {
    if (m_bUseSeekPreview && m_wndPreView.IsWindowVisible()) {

        int seek =
            nFlags == MK_SHIFT ? 10 :
            nFlags == MK_CONTROL ? 1 : 5;

        zDelta > 0 ? SetCursorPos(point.x + seek, point.y) :
            zDelta < 0 ? SetCursorPos(point.x - seek, point.y) : SetCursorPos(point.x, point.y);

        return 0;
    }
    return __super::OnMouseWheel(nFlags, zDelta, point);
}

void CMainFrame::OnMouseHWheel(UINT nFlags, short zDelta, CPoint pt) {
    if (m_wndView && m_wndView.OnMouseHWheelImpl(nFlags, zDelta, pt)) {
        //HWHEEL is sent to active window, so we have to manually pass it to CMouseWnd to trap hotkeys
        return;
    }
    __super::OnMouseHWheel(nFlags, zDelta, pt);
}

CHdmvClipInfo::BDMVMeta CMainFrame::GetBDMVMeta()
{
    if (m_BDMeta.GetCount() == 1) return m_BDMeta.GetHead();
    else {
        CAppSettings& s = AfxGetAppSettings();
        LCID langid = s.language;
        if (langid == 0) langid = ISOLang::ISO6392ToLcid("eng");
        POSITION p = m_BDMeta.GetHeadPosition();
        CHdmvClipInfo::BDMVMeta meta;
        bool b = true;
        do {
            if (p == m_BDMeta.GetTailPosition()) b = false;
            meta = m_BDMeta.GetNext(p);
            CW2A temlang(meta.langcode.GetString());
            LCID lang = ISOLang::ISO6392ToLcid(temlang);
            if (lang == langid) return meta;
        } while (b);
        return m_BDMeta.GetHead();
    }
}

void CMainFrame::updateRecentFileListSub(CString fn) {
    auto& MRU = AfxGetAppSettings().MRU;
    MRU.ReadList();
    RecentFileEntry r = m_current_rfe;
    bool found(r.subs.Find(fn));
    if (!found && !fn.IsEmpty()) r.subs.AddTail(fn);
    MRU.Add(r);
    MRU.WriteList();
    m_current_rfe = RecentFileEntry(); // Clear
}
