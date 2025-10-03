/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2017 see Authors.txt
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

#include "ChildView.h"
#include "DVBChannel.h"
#include "DebugShadersDlg.h"
#include "DropTarget.h"
#include "EditListEditor.h"
#include "IBufferInfo.h"
#include "IKeyFrameInfo.h"
#include "MainFrmControls.h"
#include "MouseTouch.h"
#include "MpcApi.h"
#include "PlayerCaptureBar.h"
#include "PlayerInfoBar.h"
#include "PlayerNavigationBar.h"
#include "PlayerPlaylistBar.h"
#include "PlayerSeekBar.h"
#include "PlayerStatusBar.h"
#include "PlayerSubresyncBar.h"
#include "PlayerPreView.h"
#include "PlayerToolBar.h"
#include "SubtitleDlDlg.h"
#include "TimerWrappers.h"
#include "OSD.h"
#include "CMPCThemeMenu.h"
#include "../SubPic/MemSubPic.h"
#include <initguid.h>
#include <qnetwork.h>
#include "../DSUtil/FontInstaller.h"
#include "AppSettings.h"
#include "../filters/transform/VSFilter/IDirectVobSub.h"
#include "MediaTransControls.h"
#include "FavoriteOrganizeDlg.h"
#include "AllocatorCommon.h"

class CDebugShadersDlg;
class CFullscreenWnd;
class SkypeMoodMsgHandler;
struct DisplayMode;
enum MpcCaptionState;
class CMediaTypesDlg;

interface IDSMChapterBag;
interface IGraphBuilder2;
interface IMFVideoDisplayControl;
interface IMFVideoProcessor;
interface IMadVRCommand;
interface IMadVRInfo;
interface IMadVRFrameGrabber;
interface IMadVRSettings;
interface IMadVRSubclassReplacement;
interface ISubClock;
interface ISubPicAllocatorPresenter2;
interface ISubPicAllocatorPresenter;
interface ISubStream;
interface ISyncClock;
DECLARE_INTERFACE_IID(IAMLine21Decoder_2, "6E8D4A21-310C-11d0-B79A-00AA003767A7");

enum class MLS {
    CLOSED,
    LOADING,
    LOADED,
    CLOSING,
    FAILING,
};

enum {
    PM_NONE,
    PM_FILE,
    PM_DVD,
    PM_ANALOG_CAPTURE,
    PM_DIGITAL_CAPTURE
};

class OpenMediaData
{
public:
    //  OpenMediaData() {}
    virtual ~OpenMediaData() {} // one virtual funct is needed to enable rtti
    CString title;
    CAtlList<CString> subs;
};

class OpenFileData : public OpenMediaData
{
public:
    OpenFileData() : rtStart(0), bAddToRecent(true) {}
    CAtlList<CString> fns;
    REFERENCE_TIME rtStart;
    ABRepeat abRepeat;
    bool bAddToRecent;
    CString useragent;
    CString referrer;
};

class OpenDVDData : public OpenMediaData
{
public:
    //  OpenDVDData() {}
    CString path;
    CComPtr<IDvdState> pDvdState;
};

class OpenDeviceData : public OpenMediaData
{
public:
    OpenDeviceData() {
        vinput = vchannel = ainput = -1;
    }
    CStringW DisplayName[2];
    int vinput, vchannel, ainput;
};

class TunerScanData
{
public:
    ULONG FrequencyStart;
    ULONG FrequencyStop;
    ULONG Bandwidth;
    ULONG SymbolRate;
    LONG  Offset;
    HWND  Hwnd;
};

struct SeekToCommand {
    REFERENCE_TIME rtPos;
    ULONGLONG seekTime;
    bool bShowOSD;
};

struct SubtitleInput {
    CComQIPtr<ISubStream> pSubStream;
    CComPtr<IBaseFilter> pSourceFilter;

    SubtitleInput() {};
    SubtitleInput(CComQIPtr<ISubStream> pSubStream) : pSubStream(pSubStream) {};
    SubtitleInput(CComQIPtr<ISubStream> pSubStream, CComPtr<IBaseFilter> pSourceFilter)
        : pSubStream(pSubStream), pSourceFilter(pSourceFilter) {};
};

struct FileFavorite {
    CString Name;
    REFERENCE_TIME Start;
    REFERENCE_TIME MarkA;
    REFERENCE_TIME MarkB;
    BOOL RelativeDrive;

    FileFavorite() {
        Start = MarkA = MarkB = 0;
        RelativeDrive = FALSE;
    }

    static bool TryParse(const CString& fav, FileFavorite& ff);
    static bool TryParse(const CString& fav, FileFavorite& ff, CAtlList<CString>& parts);

    CString ToString() const;
};

/////////////////////////////////////////////////////////////////////////////
// CMainFrame

class CMainFrame : public CFrameWnd, public CDropClient
{
public:

    DpiHelper m_dpi;

    enum class TimerHiderSubscriber {
        TOOLBARS_HIDER,
        CURSOR_HIDER,
        CURSOR_HIDER_D3DFS,
    };
    OnDemandTimer<TimerHiderSubscriber> m_timerHider;

    enum class TimerOneTimeSubscriber {
        TOOLBARS_DELAY_NOTLOADED,
        CHILDVIEW_CURSOR_HACK,
        DELAY_IDLE,
        ACTIVE_SHADER_FILES_CHANGE_COOLDOWN,
        DELAY_PLAYPAUSE_AFTER_AUTOCHANGE_MODE,
        DVBINFO_UPDATE,
        STATUS_ERASE,
        PLACE_FULLSCREEN_UNDER_ACTIVE_WINDOW,
        AUTOFIT_TIMEOUT
    };
    OneTimeTimerPool<TimerOneTimeSubscriber> m_timerOneTime;

private:
    EventClient m_eventc;
    void EventCallback(MpcEvent ev);

    CMainFrameMouseHook m_mouseHook;

    enum {
        TIMER_STREAMPOSPOLLER = 1,
        TIMER_STREAMPOSPOLLER2,
        TIMER_STATS,
        TIMER_UNLOAD_UNUSED_EXTERNAL_OBJECTS,
        TIMER_HIDER,
        TIMER_WINDOW_FULLSCREEN,
        TIMER_DELAYEDSEEK,
        TIMER_ONETIME_START,
        TIMER_ONETIME_END = TIMER_ONETIME_START + 127,
    };
    enum {
        SEEK_DIRECTION_NONE,
        SEEK_DIRECTION_BACKWARD,
        SEEK_DIRECTION_FORWARD
    };
    enum {
        ZOOM_DEFAULT_LEVEL = 0,
        ZOOM_AUTOFIT = -1,
        ZOOM_AUTOFIT_LARGER = -2
    };


    typedef std::map<int, float> PlaybackRateMap;
    static PlaybackRateMap filePlaybackRates;
    static PlaybackRateMap dvdPlaybackRates;

    friend class CPPageFileInfoSheet;
    friend class CPPageLogo;
    friend class CMouse;
    friend class CPlayerSeekBar; // for accessing m_controls.ControlChecked(), m_pMainFrame->m_CachedFilterState, m_pMainFrame->m_pGB_preview
    friend class CChildView; // for accessing m_controls.DelayShowNotLoaded()
    friend class CFullscreenWnd; // for accessing m_controls.DelayShowNotLoaded()
    friend class CMouseWndWithArtView; // for accessing m_controls.DelayShowNotLoaded()
    friend class SubtitlesProvider;

    // TODO: wrap these graph objects into a class to make it look cleaner

    CComPtr<IGraphBuilder2> m_pGB;
    CComQIPtr<IMediaControl> m_pMC;
    CComQIPtr<IMediaEventEx> m_pME;
    CComQIPtr<IVideoWindow> m_pVW;
    CComQIPtr<IBasicVideo> m_pBV;
    CComQIPtr<IBasicAudio> m_pBA;
    CComQIPtr<IMediaSeeking> m_pMS;
    CComQIPtr<IVideoFrameStep> m_pFS;
    CComQIPtr<IFileSourceFilter> m_pFSF;
    CComQIPtr<IKeyFrameInfo> m_pKFI;
    CComQIPtr<IQualProp, &IID_IQualProp> m_pQP;
    CComQIPtr<IBufferInfo> m_pBI;
    CComQIPtr<IAMOpenProgress> m_pAMOP;
    CComQIPtr<IAMMediaContent, &IID_IAMMediaContent> m_pAMMC[2];
    CComQIPtr<IAMNetworkStatus, &IID_IAMNetworkStatus> m_pAMNS;
    CComQIPtr<IAMStreamSelect> m_pAudioSwitcherSS;
    CComQIPtr<IAMStreamSelect> m_pSplitterSS;
    CComQIPtr<IAMStreamSelect> m_pSplitterDubSS;
    CComQIPtr<IAMStreamSelect> m_pOtherSS[2];
    // SmarkSeek
    CComPtr<IGraphBuilder2>         m_pGB_preview;
    CComQIPtr<IMediaControl>        m_pMC_preview;
    //CComQIPtr<IMediaEventEx>        m_pME_preview;
    CComQIPtr<IMediaSeeking>        m_pMS_preview;
    CComQIPtr<IVideoWindow>         m_pVW_preview;
    CComQIPtr<IBasicVideo>          m_pBV_preview;
    //CComQIPtr<IVideoFrameStep>      m_pFS_preview;
    CComQIPtr<IDvdControl2>         m_pDVDC_preview;
    CComQIPtr<IDvdInfo2>            m_pDVDI_preview; // VtX: usually not necessary but may sometimes be necessary.
    CComPtr<IMFVideoDisplayControl> m_pMFVDC_preview;
    CComPtr<IVMRWindowlessControl9> m_pVMR9C_preview;
    CComPtr<IMFVideoProcessor>      m_pMFVP_preview;
    CComPtr<ISubPicAllocatorPresenter2> m_pCAP2_preview;
    int defaultVideoAngle;
    //
    CComPtr<IVMRMixerControl9> m_pVMRMC;
    CComPtr<IMFVideoDisplayControl> m_pMFVDC;
    CComPtr<IMFVideoProcessor> m_pMFVP;
    CComPtr<IVMRMixerBitmap9>    m_pVMB;
    CComPtr<IMFVideoMixerBitmap>    m_pMFVMB;
    CComPtr<IVMRWindowlessControl9> m_pVMRWC;

    CComPtr<ISubPicAllocatorPresenter> m_pCAP;
    CComPtr<ISubPicAllocatorPresenter2> m_pCAP2;
    CComPtr<ISubPicAllocatorPresenter3> m_pCAP3;

    CComPtr<IMadVRSettings> m_pMVRS;
    CComPtr<IMadVRSubclassReplacement> m_pMVRSR;
    CComPtr<IMadVRCommand> m_pMVRC;
    CComPtr<IMadVRInfo> m_pMVRI;
    CComPtr<IMadVRFrameGrabber> m_pMVRFG;
    CComPtr<IMadVRTextOsd> m_pMVTO;

    CComPtr<ID3DFullscreenControl> m_pD3DFSC;

    CComQIPtr<IDvdControl2> m_pDVDC;
    CComQIPtr<IDvdInfo2> m_pDVDI;
    CComPtr<IAMLine21Decoder_2> m_pLN21;

    CComPtr<ICaptureGraphBuilder2> m_pCGB;
    CStringW m_VidDispName, m_AudDispName;
    CComPtr<IBaseFilter> m_pVidCap, m_pAudCap;
    CComPtr<IAMVideoCompression> m_pAMVCCap, m_pAMVCPrev;
    CComPtr<IAMStreamConfig> m_pAMVSCCap, m_pAMVSCPrev, m_pAMASC;
    CComPtr<IAMCrossbar> m_pAMXBar;
    CComPtr<IAMTVTuner> m_pAMTuner;
    CComPtr<IAMDroppedFrames> m_pAMDF;

    CComPtr<IUnknown> m_pProv;

    CComQIPtr<IDirectVobSub> m_pDVS;
    CComQIPtr<IDirectVobSub2> m_pDVS2;

    bool m_bUsingDXVA;
    LPCTSTR m_HWAccelType;
    void UpdateDXVAStatus();

    void SetVolumeBoost(UINT nAudioBoost);
    void SetBalance(int balance);
	
	// temp fonts loader
	CFontInstaller m_FontInstaller;

    // subtitles

    CCritSec m_csSubLock;
    CCritSec m_csSubtitleManagementLock;

    CList<SubtitleInput> m_pSubStreams;
    std::list<ISubStream*> m_ExternalSubstreams;
    POSITION m_posFirstExtSub;
    SubtitleInput m_pCurrentSubInput;

    // StatusBar message text parts
    CString currentAudioLang;
    CString currentSubLang;
    CString m_statusbarVideoFormat;
    CString m_statusbarAudioFormat;
    CString m_statusbarVideoSize;

    SubtitleInput* GetSubtitleInput(int& i, bool bIsOffset = false);
    int UpdateSelectedAudioStreamInfo(int index, AM_MEDIA_TYPE* pmt, LCID lcid);
    bool IsValidSubtitleStream(int i);
    int GetSelectedSubtitleTrackIndex();

    friend class CTextPassThruFilter;

    // windowing

    bool m_bDelaySetOutputRect;

    CRect m_lastWindowRect;

    void SetDefaultWindowRect(int iMonitor = 0);
    void SetDefaultFullscreenState();
    void RestoreDefaultWindowRect();
    CRect GetInvisibleBorderSize() const;
    CSize GetVideoOrArtSize(MINMAXINFO& mmi);
    CSize GetZoomWindowSize(double dScale, bool ignore_video_size = false);
    bool GetWorkAreaRect(CRect& work);
    CRect GetZoomWindowRect(const CSize& size, bool ignoreSavedPosition = false);
    void ZoomVideoWindow(double dScale = ZOOM_DEFAULT_LEVEL, bool ignore_video_size = false);
    double GetZoomAutoFitScale();

    bool alwaysOnTopZOrderInitialized = false;
    void SetAlwaysOnTop(int iOnTop);
    bool WindowExpectedOnTop();

    // dynamic menus

    void CreateDynamicMenus();
    void DestroyDynamicMenus();
    void LoadDynamicMenus();
    void SetupOpenCDSubMenu();
    void SetupFiltersSubMenu();
    void SetupAudioSubMenu();
    void SetupSubtitlesSubMenu();
    void SetupVideoStreamsSubMenu();
    void SetupJumpToSubMenus(CMenu* parentMenu = nullptr, int iInsertPos = -1);
    void SetupFavoritesSubMenu();
    bool SetupShadersSubMenu();
    void SetupRecentFilesSubMenu();

    DWORD SetupNavStreamSelectSubMenu(CMenu& subMenu, UINT id, DWORD dwSelGroup);
    void OnNavStreamSelectSubMenu(UINT id, DWORD dwSelGroup);
    void OnStreamSelect(bool forward, DWORD dwSelGroup);
    static CString GetStreamOSDString(CString name, LCID lcid, DWORD dwSelGroup);

    void CreateOSDBar();
    bool OSDBarSetPos();
    void DestroyOSDBar();

    CMPCThemeMenu m_mainPopupMenu, m_popupMenu;
    CMPCThemeMenu m_openCDsMenu;
    CMPCThemeMenu m_filtersMenu, m_subtitlesMenu, m_audiosMenu, m_videoStreamsMenu;
    CMPCThemeMenu m_chaptersMenu, m_titlesMenu, m_playlistMenu, m_BDPlaylistMenu, m_channelsMenu;
    CMPCThemeMenu m_favoritesMenu;
    CMPCThemeMenu m_shadersMenu;
    CMPCThemeMenu m_recentFilesMenu;
    int recentFilesMenuFromMRUSequence;

    UINT m_nJumpToSubMenusCount;

    CInterfaceArray<IUnknown, &IID_IUnknown> m_pparray;
    CInterfaceArray<IAMStreamSelect> m_ssarray;

    // chapters (file mode)
    CComPtr<IDSMChapterBag> m_pCB;
    void SetupChapters();
    void SetupCueChapters(CString fn);

    // chapters (DVD mode)
    void SetupDVDChapters();

    bool SeekToFileChapter(int iChapter, bool bRelative = false);
    bool SeekToDVDChapter(int iChapter, bool bRelative = false);

    void AddTextPassThruFilter();

    int m_nLoops;
    ABRepeat abRepeat, reloadABRepeat;
    UINT m_nLastSkipDirection;

    int m_iStreamPosPollerInterval;

    bool m_fCustomGraph;
    bool m_fShockwaveGraph;

    int m_iGraphID;

    CComPtr<ISubClock> m_pSubClock;

    bool m_fFrameSteppingActive;
    int m_nStepForwardCount;
    REFERENCE_TIME m_rtStepForwardStart;
    int m_nVolumeBeforeFrameStepping;

    bool m_fEndOfStream;
    ULONGLONG m_dwLastPause;
    ULONGLONG m_dwReloadPos;
    int m_iReloadAudioIdx;
    int m_iReloadSubIdx;

    bool m_bRememberFilePos;

    ULONGLONG m_dwLastRun;

    bool m_bBuffering;

    bool m_fLiveWM;

    bool delayingFullScreen;

    bool m_bIsMPCVRExclusiveMode = false;

    void SendStatusMessage(CString msg, int nTimeOut);
    CString m_tempstatus_msg, m_closingmsg;

    REFERENCE_TIME m_rtDurationOverride;

    void CleanGraph();

    void ShowOptions(int idPage = 0);

    HRESULT GetDisplayedImage(std::vector<BYTE>& dib, CString& errmsg);
    HRESULT GetCurrentFrame(std::vector<BYTE>& dib, CString& errmsg);
    HRESULT GetOriginalFrame(std::vector<BYTE>& dib, CString& errmsg);
    HRESULT RenderCurrentSubtitles(BYTE* pData);
    bool GetDIB(BYTE** ppData, long& size, bool fSilent = false);
    void SaveDIB(LPCTSTR fn, BYTE* pData, long size);
    CString MakeSnapshotFileName(BOOL thumbnails);
    BOOL IsRendererCompatibleWithSaveImage();
    void SaveImage(LPCTSTR fn, bool displayed, bool includeSubtitles);
    void SaveThumbnails(LPCTSTR fn);

    //

    friend class CWebClientSocket;
    friend class CWebServer;
    CAutoPtr<CWebServer> m_pWebServer;
    int m_iPlaybackMode;
    ULONG m_lCurrentChapter;
    ULONG m_lChapterStartTime;

    CString m_currentCoverAuthor;
    CString m_currentCoverPath;
    bool currentCoverIsFileArt = false;

    CAutoPtr<SkypeMoodMsgHandler> m_pSkypeMoodMsgHandler;
    void SendNowPlayingToSkype();

    MLS m_eMediaLoadState;
    OAFilterState m_CachedFilterState;

    bool m_bSettingUpMenus;
    bool m_bOpenMediaActive;
    int m_OpenMediaFailedCount;

    bool m_bTBDropdownActive;

    REFTIME GetAvgTimePerFrame() const;
    void OnVideoSizeChanged(const bool bWasAudioOnly = false);

    CDropTarget m_dropTarget;
    void OnDropFiles(CAtlList<CStringW>& slFiles, DROPEFFECT dropEffect) override;
    DROPEFFECT OnDropAccept(COleDataObject* pDataObject, DWORD dwKeyState, CPoint point) override;

public:
    void StartWebServer(int nPort);
    void StopWebServer();

    int GetPlaybackMode() const {
        return m_iPlaybackMode;
    }
    bool IsPlaybackCaptureMode() const {
        return GetPlaybackMode() == PM_ANALOG_CAPTURE || GetPlaybackMode() == PM_DIGITAL_CAPTURE;
    }
    void SetPlaybackMode(int iNewStatus);
    bool IsMuted() {
        return m_wndToolBar.GetVolume() == -10000;
    }
    int GetVolume() {
        return m_wndToolBar.m_volctrl.GetPos();
    }
    double GetPlayingRate() const {
        return m_dSpeedRate;
    }

public:
    CMainFrame();
    DECLARE_DYNAMIC(CMainFrame)

    // Attributes
public:
    bool m_fFullScreen;
    bool m_bNeedZoomAfterFullscreenExit;
    bool m_fStartInD3DFullscreen;
    bool m_fStartInFullscreenSeparate;
    bool m_bFullScreenWindowIsD3D;
    bool m_bFullScreenWindowIsOnSeparateDisplay;

    CComPtr<IBaseFilter> m_pRefClock; // Adjustable reference clock. GothSync
    CComPtr<ISyncClock> m_pSyncClock;

    bool IsFrameLessWindow() const;
    bool IsCaptionHidden() const;
    bool IsMenuHidden() const;
    bool IsPlaylistEmpty() const;
    bool IsInteractiveVideo() const;
    bool IsFullScreenMode() const;
    bool IsFullScreenMainFrame() const;
    bool IsFullScreenMainFrameExclusiveMPCVR() const;
    bool IsFullScreenSeparate() const;
    bool HasDedicatedFSVideoWindow() const;
    bool IsD3DFullScreenMode() const;
    bool IsSubresyncBarVisible() const;

    CControlBar* m_pLastBar;

protected:
    bool m_bUseSeekPreview;
    bool m_bFirstPlay;
    bool m_bOpeningInAutochangedMonitorMode;
    bool m_bPausedForAutochangeMonitorMode;
    bool restoringWindowRect;

    bool m_fAudioOnly;
    bool m_fValidDVDOpen;
    CString m_LastOpenBDPath;
    CAutoPtr<OpenMediaData> m_lastOMD;

    DVD_DOMAIN  m_iDVDDomain;
    DWORD       m_iDVDTitle;
    bool        m_bDVDStillOn;
    int         m_loadedAudioTrackIndex = -1;
    int         m_loadedSubtitleTrackIndex = -1;
    int         m_audioTrackCount = 0;

    double m_dSpeedRate;
    double m_ZoomX, m_ZoomY, m_PosX, m_PosY;
    int m_AngleX, m_AngleY, m_AngleZ;
    int m_iDefRotation;

    void ForceCloseProcess();

    // Operations
    bool OpenMediaPrivate(CAutoPtr<OpenMediaData> pOMD);
    void CloseMediaPrivate();
    void DoTunerScan(TunerScanData* pTSD);

    CWnd* GetModalParent();

    CCritSec lockModalDialog;
    CMediaTypesDlg* mediaTypesErrorDlg;
    void ShowMediaTypesDialog();

    void OpenCreateGraphObject(OpenMediaData* pOMD);
    void OpenFile(OpenFileData* pOFD);
    void OpenDVD(OpenDVDData* pODD);
    void OpenCapture(OpenDeviceData* pODD);
    HRESULT OpenBDAGraph();
    void OpenCustomizeGraph();
    CSize OpenSetupGetVideoSize();
    void OpenSetupVideo();
    void OpenSetupAudio();
    void OpenSetupInfoBar(bool bClear = true);
    void UpdateChapterInInfoBar();
    void OpenSetupStatsBar();
    void CheckSelectedAudioStream();
    void CheckSelectedVideoStream();
    void OpenSetupStatusBar();
    void OpenSetupCaptureBar();
    void OpenSetupWindowTitle(bool reset = false);

public:
    static bool GetCurDispMode(const CString& displayName, DisplayMode& dm);
    static bool GetDispMode(CString displayName, int i, DisplayMode& dm);

protected:
    void SetDispMode(CString displayName, const DisplayMode& dm, int msAudioDelay);
    void AutoChangeMonitorMode();

    void GraphEventComplete();

    friend class CGraphThread;
    CGraphThread* m_pGraphThread;
    bool m_bOpenedThroughThread;
    ATL::CEvent m_evOpenPrivateFinished;
    ATL::CEvent m_evClosePrivateFinished;

    void LoadKeyFrames();
    std::vector<REFERENCE_TIME> m_kfs;

    bool m_fOpeningAborted;
    bool m_bWasSnapped;

protected:
    friend class CSubtitleDlDlg;
    CSubtitleDlDlg m_wndSubtitlesDownloadDialog;
    CFavoriteOrganizeDlg m_wndFavoriteOrganizeDialog;
    friend class CPPageSubMisc;

    friend class SubtitlesProviders;
    std::unique_ptr<SubtitlesProviders> m_pSubtitlesProviders;
    friend struct SubtitlesInfo;
    friend class SubtitlesTask;
    friend class SubtitlesThread;

public:
    void OpenCurPlaylistItem(REFERENCE_TIME rtStart = 0, bool reopen = false, ABRepeat abRepeat = ABRepeat());
    void OpenMedia(CAutoPtr<OpenMediaData> pOMD);
    void PlayFavoriteFile(const CString& fav);
    void PlayFavoriteDVD(CString fav);
    FileFavorite ParseFavoriteFile(const CString& fav, CAtlList<CString>& args, REFERENCE_TIME* prtStart = nullptr);
    bool ResetDevice();
    bool DisplayChange();
    bool CloseMediaBeforeOpen();
    void CloseMedia(bool bNextIsQueued = false, bool bPendingFileDelete = false);
    void StartTunerScan(CAutoPtr<TunerScanData> pTSD);
    void StopTunerScan();
    HRESULT SetChannel(int nChannel);

    void AddCurDevToPlaylist();

    bool m_bTrayIcon;
    void ShowTrayIcon(bool bShow);
    void SetTrayTip(const CString& str);

    CSize GetVideoSize() const;
    CSize GetVideoSizeWithRotation(bool forPreview = false) const;
    void HidePlaylistFullScreen(bool force = false);
    void ToggleFullscreen(bool fToNearest, bool fSwitchScreenResWhenHasTo);
    void ToggleD3DFullscreen(bool fSwitchScreenResWhenHasTo);
    void MoveVideoWindow(bool fShowStats = false, bool bSetStoppedVideoRect = false);
    void SetPreviewVideoPosition();

    void RepaintVideo(const bool bForceRepaint = false);
    void HideVideoWindow(bool fHide);

    CCritSec lockGraphAccess;
    OAFilterState GetMediaStateDirect();
    OAFilterState GetMediaState();
    OAFilterState UpdateCachedMediaState();
    bool MediaControlRun(bool waitforcompletion = false);
    bool MediaControlPause(bool waitforcompletion = false);
    bool MediaControlStop(bool waitforcompletion = false);
    bool MediaControlStopPreview();

    REFERENCE_TIME GetPos() const;
    REFERENCE_TIME GetDur() const;
    bool GetKeyFrame(REFERENCE_TIME rtTarget, REFERENCE_TIME rtMin, REFERENCE_TIME rtMax, bool nearest, REFERENCE_TIME& keyframetime) const;
    REFERENCE_TIME GetClosestKeyFrame(REFERENCE_TIME rtTarget, REFERENCE_TIME rtMaxForwardDiff, REFERENCE_TIME rtMaxBackwardDiff) const;
    REFERENCE_TIME GetClosestKeyFramePreview(REFERENCE_TIME rtTarget) const;
    void SeekTo(REFERENCE_TIME rt, bool bShowOSD = true);
    void DoSeekTo(REFERENCE_TIME rt, bool bShowOSD = true);
    SeekToCommand queuedSeek;
    ULONGLONG lastSeekStart;
    ULONGLONG lastSeekFinish;
    void SetPlayingRate(double rate);

    int SetupAudioStreams();
    int SetupSubtitleStreams();

    bool LoadSubtitle(CString fn, SubtitleInput* pSubInput = nullptr, bool bAutoLoad = false);
    bool LoadSubtitle(CYoutubeDLInstance::YDLSubInfo& sub);
    bool SetSubtitle(int i, bool bIsOffset = false, bool bDisplayMessage = false);
    void SetSubtitle(const SubtitleInput& subInput, bool skip_lcid = false);
    void UpdateSubtitleColorInfo();
    void ToggleSubtitleOnOff(bool bDisplayMessage = false);
    void ReplaceSubtitle(const ISubStream* pSubStreamOld, ISubStream* pSubStreamNew);
    void InvalidateSubtitle(DWORD_PTR nSubtitleId = DWORD_PTR_MAX, REFERENCE_TIME rtInvalidate = -1);
    void ReloadSubtitle();
    void UpdateSubtitleRenderingParameters();
    HRESULT InsertTextPassThruFilter(IBaseFilter* pBF, IPin* pPin, IPin* pPinto);

    void SetAudioTrackIdx(int index);
    void SetSubtitleTrackIdx(int index);
    int GetCurrentAudioTrackIdx(CString *pstrName = nullptr);
    int GetCurrentSubtitleTrackIdx(CString *pstrName = nullptr);

    void AddFavorite(bool fDisplayMessage = false, bool fShowDialog = true);

    CString GetFileName();
    CString GetCaptureTitle();

    // shaders
    void SetShaders(bool bSetPreResize = true, bool bSetPostResize = true);
	
	bool m_bToggleShader;
	bool m_bToggleShaderScreenSpace;
	std::list<ShaderC> m_ShaderCache;
	ShaderC* GetShader(CString path, bool bD3D11);
	bool SaveShaderFile(ShaderC* shader);
	bool DeleteShaderFile(LPCWSTR label);
	void TidyShaderCache();

    // capturing
    bool m_fCapturing;
    HRESULT BuildCapture(IPin* pPin, IBaseFilter* pBF[3], const GUID& majortype, AM_MEDIA_TYPE* pmt); // pBF: 0 buff, 1 enc, 2 mux, pmt is for 1 enc
    bool BuildToCapturePreviewPin(IBaseFilter* pVidCap, IPin** pVidCapPin, IPin** pVidPrevPin,
                                  IBaseFilter* pAudCap, IPin** pAudCapPin, IPin** pAudPrevPin);
    bool BuildGraphVideoAudio(int fVPreview, bool fVCapture, int fAPreview, bool fACapture);
    bool DoCapture(), StartCapture(), StopCapture();

    void DoAfterPlaybackEvent();
    bool SearchInDir(bool bDirForward, bool bLoop = false);
    bool WildcardFileSearch(CString searchstr, std::set<CString, CStringUtils::LogicalLess>& results, bool recurse_dirs);
    CString lastOpenFile;
    bool CanSkipFromClosedFile();

    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
    virtual BOOL PreTranslateMessage(MSG* pMsg);
    virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
    virtual void RecalcLayout(BOOL bNotify = TRUE);
    void EnableDocking(DWORD dwDockStyle);

    // DVB capture
    void UpdateCurrentChannelInfo(bool bShowOSD = true, bool bShowInfoBar = false);
    LRESULT OnCurrentChannelInfoUpdated(WPARAM wParam, LPARAM lParam);

    bool CheckABRepeat(REFERENCE_TIME& aPos, REFERENCE_TIME& bPos);
    void PerformABRepeat();
    void DisableABRepeat();

    struct DVBState {
        struct EITData {
            HRESULT hr        = E_FAIL;
            EventDescriptor NowNext;
            bool bShowOSD     = true;
            bool bShowInfoBar = false;
        };

        CString         sChannelName;                // Current channel name
        CBDAChannel*    pChannel          = nullptr; // Pointer to current channel object
        EventDescriptor NowNext;                     // Current channel EIT
        bool            bActive           = false;   // True when channel is active
        bool            bSetChannelActive = false;   // True when channel change is in progress
        bool            bInfoActive       = false;   // True when EIT data update is in progress
        bool            bAbortInfo        = true;    // True when aborting current EIT update
        std::future<DVBState::EITData> infoData;

        void Reset() {
            sChannelName.Empty();
            pChannel          = nullptr;
            NowNext           = EventDescriptor();
            bActive           = false;
            bSetChannelActive = false;
            bInfoActive       = false;
            bAbortInfo        = true;
        }

        void Join() {
            if (infoData.valid()) {
                bAbortInfo = true;
                infoData.wait();
            }
        }

        ~DVBState() {
            bAbortInfo = true;
        }
    };

    std::unique_ptr<DVBState> m_pDVBState = nullptr;

    // Implementation
public:
    virtual ~CMainFrame();
#ifdef _DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

protected:  // control bar embedded members
    friend class CMainFrameControls;
    friend class CPPageToolBarLayout;
    CMainFrameControls m_controls;
    friend class CPlayerBar; // it notifies m_controls of panel re-dock

    CChildView m_wndView;

    CPlayerSeekBar m_wndSeekBar;
    CPlayerToolBar m_wndToolBar;
    CPlayerInfoBar m_wndInfoBar;
    CPlayerInfoBar m_wndStatsBar;
    CPlayerStatusBar m_wndStatusBar;

    CPlayerSubresyncBar m_wndSubresyncBar;
    CPlayerPlaylistBar m_wndPlaylistBar;
    CPlayerCaptureBar m_wndCaptureBar;
    CPlayerNavigationBar m_wndNavigationBar;
    CEditListEditor m_wndEditListEditor;

    std::unique_ptr<CDebugShadersDlg> m_pDebugShaders;

    LPCTSTR GetRecentFile() const;

    friend class CPPagePlayback; // TODO
    friend class CPPageAudioSwitcher; // TODO
    friend class CMPlayerCApp; // TODO

    // Generated message map functions

    DECLARE_MESSAGE_MAP()

public:
    afx_msg int OnNcCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnDestroy();

    afx_msg LRESULT OnTaskBarRestart(WPARAM, LPARAM);
    afx_msg LRESULT OnNotifyIcon(WPARAM, LPARAM);
    afx_msg LRESULT OnTaskBarThumbnailsCreate(WPARAM, LPARAM);

    afx_msg LRESULT OnSkypeAttach(WPARAM wParam, LPARAM lParam);

    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
    afx_msg void OnMove(int x, int y);
    afx_msg void OnEnterSizeMove();
    afx_msg void OnMoving(UINT fwSide, LPRECT pRect);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnSizing(UINT nSide, LPRECT lpRect);
    afx_msg void OnExitSizeMove();
    afx_msg void OnDisplayChange();
    afx_msg void OnWindowPosChanging(WINDOWPOS* lpwndpos);

    LRESULT OnDpiChanged(WPARAM wParam, LPARAM lParam);

    afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
    afx_msg void OnActivateApp(BOOL bActive, DWORD dwThreadID);
    afx_msg LRESULT OnAppCommand(WPARAM wParam, LPARAM lParam);
    afx_msg void OnRawInput(UINT nInputcode, HRAWINPUT hRawInput);

    afx_msg LRESULT OnHotKey(WPARAM wParam, LPARAM lParam);

    afx_msg void OnTimer(UINT_PTR nIDEvent);

    afx_msg LRESULT OnGraphNotify(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnResetDevice(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnRepaintRenderLess(WPARAM wParam, LPARAM lParam);

    afx_msg void SaveAppSettings();

    afx_msg LRESULT OnNcHitTest(CPoint point);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);

    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);

    void RestoreFocus();

    afx_msg void OnInitMenu(CMenu* pMenu);
    afx_msg void OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu);
    afx_msg void OnUnInitMenuPopup(CMenu* pPopupMenu, UINT nFlags);
    afx_msg void OnEnterMenuLoop(BOOL bIsTrackPopupMenu);

    afx_msg BOOL OnQueryEndSession();
    afx_msg void OnEndSession(BOOL bEnding);

    BOOL OnMenu(CMenu* pMenu);
    CMPCThemeMenu* GetShortMenu();
    afx_msg void OnMenuPlayerShort();
    afx_msg void OnMenuPlayerLong();
    afx_msg void OnMenuFilters();

    afx_msg void OnUpdatePlayerStatus(CCmdUI* pCmdUI);

    afx_msg LRESULT OnFilePostOpenmedia(WPARAM wParam, LPARAM lparam);
    afx_msg LRESULT OnOpenMediaFailed(WPARAM wParam, LPARAM lParam);
    void OnFilePostClosemedia(bool bNextIsQueued = false);

    afx_msg void OnBossKey();

    void ToolbarContextMenu(int iItem, int nIndex, CRect buttonRect);

    afx_msg void OnUpdateAudiosButton(CCmdUI* pCmdUI);
    afx_msg void OnUpdateSubtitlesButton(CCmdUI* pCmdUI);

    afx_msg void OnStreamAudio(UINT nID);
    afx_msg void OnStreamSub(UINT nID);
    afx_msg void OnStreamSubOnOff();
    afx_msg void OnAudioShiftOnOff();
    afx_msg void OnDvdAngle(UINT nID);
    afx_msg void OnDvdAudio(UINT nID);
    afx_msg void OnDvdSub(UINT nID);
    afx_msg void OnDvdSubOnOff();

    afx_msg LRESULT OnLoadSubtitles(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnGetSubtitles(WPARAM, LPARAM lParam);

    // menu item handlers

    INT_PTR DoFileDialogWithLastFolder(CFileDialog& fd, CStringW& lastPath);
    void OpenDVDOrBD(CStringW path);

    afx_msg void OnFileOpenQuick();
    afx_msg void OnFileOpenmedia();
    afx_msg void OnUpdateFileOpen(CCmdUI* pCmdUI);
    afx_msg BOOL OnCopyData(CWnd* pWnd, COPYDATASTRUCT* pCopyDataStruct);
    afx_msg void OnFileOpendvd();
    afx_msg void OnFileOpendevice();
    afx_msg void OnFileOpenOpticalDisk(UINT nID);
    afx_msg void OnFileReopen();
    afx_msg void OnFileRecycle();
    afx_msg void OnFileSaveAs();
    afx_msg void OnUpdateFileSaveAs(CCmdUI* pCmdUI);
    afx_msg void OnFileSaveImage();
    afx_msg void OnFileSaveImageAuto();
    afx_msg void OnUpdateFileSaveImage(CCmdUI* pCmdUI);
    afx_msg void OnCmdLineSaveThumbnails();
    afx_msg void OnFileSaveThumbnails();
    afx_msg void OnUpdateFileSaveThumbnails(CCmdUI* pCmdUI);
    afx_msg void OnFileSubtitlesLoad();
    afx_msg void OnUpdateFileSubtitlesLoad(CCmdUI* pCmdUI);
    afx_msg void OnFileSubtitlesSave() { SubtitlesSave(); }
    afx_msg void OnUpdateFileSubtitlesSave(CCmdUI* pCmdUI);
    //afx_msg void OnFileSubtitlesUpload();
    //afx_msg void OnUpdateFileSubtitlesUpload(CCmdUI* pCmdUI);
    afx_msg void OnFileSubtitlesDownload();
    afx_msg void OnUpdateFileSubtitlesDownload(CCmdUI* pCmdUI);
    afx_msg void OnFileProperties();
    afx_msg void OnUpdateFileProperties(CCmdUI* pCmdUI);
    afx_msg void OnFileOpenLocation();
    afx_msg void OnFileCloseAndRestore();
    afx_msg void OnFileCloseMedia(); // no menu item
    afx_msg void OnUpdateFileClose(CCmdUI* pCmdUI);

    void SetCaptionState(MpcCaptionState eState);
    afx_msg void OnViewCaptionmenu();

    afx_msg void OnViewNavigation();
    afx_msg void OnUpdateViewCaptionmenu(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewNavigation(CCmdUI* pCmdUI);
    afx_msg void OnViewControlBar(UINT nID);
    afx_msg void OnUpdateViewControlBar(CCmdUI* pCmdUI);
    afx_msg void OnViewSubresync();
    afx_msg void OnUpdateViewSubresync(CCmdUI* pCmdUI);
    void UpdatePlaylistButton();
    afx_msg void OnViewPlaylist();
    afx_msg void OnPlaylistToggleShuffle();
    afx_msg void OnUpdateViewPlaylist(CCmdUI* pCmdUI);
    afx_msg void OnViewEditListEditor();
    afx_msg void OnEDLIn();
    afx_msg void OnUpdateEDLIn(CCmdUI* pCmdUI);
    afx_msg void OnEDLOut();
    afx_msg void OnUpdateEDLOut(CCmdUI* pCmdUI);
    afx_msg void OnEDLNewClip();
    afx_msg void OnUpdateEDLNewClip(CCmdUI* pCmdUI);
    afx_msg void OnEDLSave();
    afx_msg void OnUpdateEDLSave(CCmdUI* pCmdUI);
    afx_msg void OnViewCapture();
    afx_msg void OnUpdateViewCapture(CCmdUI* pCmdUI);
    afx_msg void OnViewDebugShaders();
    afx_msg void OnUpdateViewDebugShaders(CCmdUI* pCmdUI);
    afx_msg void OnViewMinimal();
    afx_msg void OnUpdateViewMinimal(CCmdUI* pCmdUI);
    afx_msg void OnViewCompact();
    afx_msg void OnUpdateViewCompact(CCmdUI* pCmdUI);
    afx_msg void OnViewNormal();
    afx_msg void OnUpdateViewNormal(CCmdUI* pCmdUI);
    afx_msg void OnViewFullscreen();
    afx_msg void OnViewFullscreenSecondary();
    afx_msg void OnUpdateViewFullscreen(CCmdUI* pCmdUI);
    afx_msg void OnViewZoom(UINT nID);
    afx_msg void OnUpdateViewZoom(CCmdUI* pCmdUI);
    afx_msg void OnViewZoomAutoFit();
    afx_msg void OnViewZoomAutoFitLarger();
    afx_msg void OnViewModifySize(UINT nID);
    afx_msg void OnViewDefaultVideoFrame(UINT nID);
    afx_msg void OnUpdateViewDefaultVideoFrame(CCmdUI* pCmdUI);
    afx_msg void OnViewSwitchVideoFrame();
    afx_msg void OnUpdateViewSwitchVideoFrame(CCmdUI* pCmdUI);
    afx_msg void OnViewCompMonDeskARDiff();
    afx_msg void OnUpdateViewCompMonDeskARDiff(CCmdUI* pCmdUI);
    afx_msg void OnViewPanNScan(UINT nID);
    afx_msg void OnUpdateViewPanNScan(CCmdUI* pCmdUI);
    afx_msg void OnViewPanNScanPresets(UINT nID);
    afx_msg void OnUpdateViewPanNScanPresets(CCmdUI* pCmdUI);
    afx_msg void OnViewRotate(UINT nID);
    afx_msg void OnUpdateViewRotate(CCmdUI* pCmdUI);
    afx_msg void OnViewAspectRatio(UINT nID);
    afx_msg void OnUpdateViewAspectRatio(CCmdUI* pCmdUI);
    afx_msg void OnViewAspectRatioNext();
    afx_msg void OnViewOntop(UINT nID);
    afx_msg void OnUpdateViewOntop(CCmdUI* pCmdUI);
    afx_msg void OnViewOptions();
    afx_msg void OnUpdateViewTearingTest(CCmdUI* pCmdUI);
    afx_msg void OnViewTearingTest();
    afx_msg void OnUpdateViewDisplayRendererStats(CCmdUI* pCmdUI);
    afx_msg void OnViewResetRendererStats();
    afx_msg void OnViewDisplayRendererStats();
    afx_msg void OnUpdateViewVSync(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewVSyncOffset(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewVSyncAccurate(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewFlushGPU(CCmdUI* pCmdUI);

    afx_msg void OnUpdateViewSynchronizeVideo(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewSynchronizeDisplay(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewSynchronizeNearest(CCmdUI* pCmdUI);

    afx_msg void OnUpdateViewD3DFullscreen(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewDisableDesktopComposition(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewAlternativeVSync(CCmdUI* pCmdUI);

    afx_msg void OnUpdateViewColorManagementEnable(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewColorManagementInput(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewColorManagementAmbientLight(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewColorManagementIntent(CCmdUI* pCmdUI);

    afx_msg void OnUpdateViewEVROutputRange(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewFullscreenGUISupport(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewHighColorResolution(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewForceInputHighColorResolution(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewFullFloatingPointProcessing(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewHalfFloatingPointProcessing(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewEnableFrameTimeCorrection(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewVSyncOffsetIncrease(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewVSyncOffsetDecrease(CCmdUI* pCmdUI);
    afx_msg void OnViewVSync();
    afx_msg void OnViewVSyncAccurate();

    afx_msg void OnViewSynchronizeVideo();
    afx_msg void OnViewSynchronizeDisplay();
    afx_msg void OnViewSynchronizeNearest();

    afx_msg void OnViewColorManagementEnable();
    afx_msg void OnViewColorManagementInputAuto();
    afx_msg void OnViewColorManagementInputHDTV();
    afx_msg void OnViewColorManagementInputSDTV_NTSC();
    afx_msg void OnViewColorManagementInputSDTV_PAL();
    afx_msg void OnViewColorManagementAmbientLightBright();
    afx_msg void OnViewColorManagementAmbientLightDim();
    afx_msg void OnViewColorManagementAmbientLightDark();
    afx_msg void OnViewColorManagementIntentPerceptual();
    afx_msg void OnViewColorManagementIntentRelativeColorimetric();
    afx_msg void OnViewColorManagementIntentSaturation();
    afx_msg void OnViewColorManagementIntentAbsoluteColorimetric();

    afx_msg void OnViewEVROutputRange_0_255();
    afx_msg void OnViewEVROutputRange_16_235();

    afx_msg void OnViewFlushGPUBeforeVSync();
    afx_msg void OnViewFlushGPUAfterVSync();
    afx_msg void OnViewFlushGPUWait();

    afx_msg void OnViewD3DFullScreen();
    afx_msg void OnViewDisableDesktopComposition();
    afx_msg void OnViewAlternativeVSync();
    afx_msg void OnViewResetDefault();
    afx_msg void OnViewResetOptimal();

    afx_msg void OnViewFullscreenGUISupport();
    afx_msg void OnViewHighColorResolution();
    afx_msg void OnViewForceInputHighColorResolution();
    afx_msg void OnViewFullFloatingPointProcessing();
    afx_msg void OnViewHalfFloatingPointProcessing();
    afx_msg void OnViewEnableFrameTimeCorrection();
    afx_msg void OnViewVSyncOffsetIncrease();
    afx_msg void OnViewVSyncOffsetDecrease();
	afx_msg void OnUpdateShaderToggle1(CCmdUI* pCmdUI);
	afx_msg void OnUpdateShaderToggle2(CCmdUI* pCmdUI);
	afx_msg void OnShaderToggle1();
	afx_msg void OnShaderToggle2();
    afx_msg void OnUpdateViewOSDDisplayTime(CCmdUI* pCmdUI);
    afx_msg void OnViewOSDDisplayTime();
    afx_msg void OnUpdateViewOSDShowFileName(CCmdUI* pCmdUI);
    afx_msg void OnViewOSDShowFileName();
    afx_msg void OnD3DFullscreenToggle();
    afx_msg void OnGotoSubtitle(UINT nID);
    afx_msg void OnSubresyncShiftSub(UINT nID);
    afx_msg void OnSubtitleDelay(UINT nID);
    afx_msg void OnSubtitlePos(UINT nID);
    afx_msg void OnSubtitleFontSize(UINT nID);

    afx_msg void OnPlayPlay();
    afx_msg void OnPlayPause();
    afx_msg void OnPlayPlaypause();
    afx_msg void OnApiPlay();
    afx_msg void OnApiPause();
    afx_msg void OnPlayStop();
            void OnPlayStop(bool is_closing);
    afx_msg void OnUpdatePlayPauseStop(CCmdUI* pCmdUI);
    afx_msg void OnPlayFramestep(UINT nID);
    afx_msg void OnUpdatePlayFramestep(CCmdUI* pCmdUI);
    afx_msg void OnPlaySeek(UINT nID);
    afx_msg void OnPlaySeekSet();
    afx_msg void OnPlaySeekKey(UINT nID); // no menu item
    afx_msg void OnUpdatePlaySeek(CCmdUI* pCmdUI);
    afx_msg void OnPlayChangeRate(UINT nID);
    afx_msg void OnUpdatePlayChangeRate(CCmdUI* pCmdUI);
    afx_msg void OnPlayResetRate();
    afx_msg void OnUpdatePlayResetRate(CCmdUI* pCmdUI);
    afx_msg void OnPlayChangeAudDelay(UINT nID);
    afx_msg void OnUpdatePlayChangeAudDelay(CCmdUI* pCmdUI);
    afx_msg void OnPlayFiltersCopyToClipboard();
    afx_msg void OnPlayFilters(UINT nID);
    afx_msg void OnUpdatePlayFilters(CCmdUI* pCmdUI);
    afx_msg void OnPlayShadersSelect();
    afx_msg void OnPlayShadersPresetNext();
    afx_msg void OnPlayShadersPresetPrev();
    afx_msg void OnPlayShadersPresets(UINT nID);
    afx_msg void OnPlayAudio(UINT nID);
    afx_msg void OnSubtitlesDefaultStyle();
    afx_msg void OnPlaySubtitles(UINT nID);
    afx_msg void OnPlayVideoStreams(UINT nID);
    afx_msg void OnPlayFiltersStreams(UINT nID);
    afx_msg void OnPlayVolume(UINT nID);
    afx_msg void OnPlayVolumeBoost(UINT nID);
    afx_msg void OnUpdatePlayVolumeBoost(CCmdUI* pCmdUI);
    afx_msg void OnCustomChannelMapping();
    afx_msg void OnUpdateCustomChannelMapping(CCmdUI* pCmdUI);
    afx_msg void OnNormalizeRegainVolume(UINT nID);
    afx_msg void OnUpdateNormalizeRegainVolume(CCmdUI* pCmdUI);
    afx_msg void OnPlayColor(UINT nID);
    afx_msg void OnAfterplayback(UINT nID);
    afx_msg void OnUpdateAfterplayback(CCmdUI* pCmdUI);
    afx_msg void OnPlayRepeat(UINT nID);
    afx_msg void OnUpdatePlayRepeat(CCmdUI* pCmdUI);
    afx_msg void OnABRepeat(UINT nID);
    afx_msg void OnUpdateABRepeat(CCmdUI* pCmdUI);
    afx_msg void OnPlayRepeatForever();
    afx_msg void OnUpdatePlayRepeatForever(CCmdUI* pCmdUI);

    afx_msg void OnNavigateSkip(UINT nID);
    afx_msg void OnUpdateNavigateSkip(CCmdUI* pCmdUI);
    afx_msg void OnNavigateSkipFile(UINT nID);
    afx_msg void OnUpdateNavigateSkipFile(CCmdUI* pCmdUI);
    afx_msg void OnNavigateGoto();
    afx_msg void OnUpdateNavigateGoto(CCmdUI* pCmdUI);
    afx_msg void OnNavigateMenu(UINT nID);
    afx_msg void OnUpdateNavigateMenu(CCmdUI* pCmdUI);
    afx_msg void OnNavigateJumpTo(UINT nID);
    afx_msg void OnNavigateMenuItem(UINT nID);
    afx_msg void OnUpdateNavigateMenuItem(CCmdUI* pCmdUI);
    afx_msg void OnTunerScan();
    afx_msg void OnUpdateTunerScan(CCmdUI* pCmdUI);

    afx_msg void OnFavoritesAdd();
    afx_msg void OnUpdateFavoritesAdd(CCmdUI* pCmdUI);
    afx_msg void OnFavoritesQuickAddFavorite();
    afx_msg void OnFavoritesOrganize();
    afx_msg void OnUpdateFavoritesOrganize(CCmdUI* pCmdUI);
    afx_msg void OnFavoritesFile(UINT nID);
    afx_msg void OnUpdateFavoritesFile(CCmdUI* pCmdUI);
    afx_msg void OnFavoritesDVD(UINT nID);
    afx_msg void OnUpdateFavoritesDVD(CCmdUI* pCmdUI);
    afx_msg void OnFavoritesDevice(UINT nID);
    afx_msg void OnUpdateFavoritesDevice(CCmdUI* pCmdUI);
    afx_msg void OnRecentFileClear();
    afx_msg void OnUpdateRecentFileClear(CCmdUI* pCmdUI);
    afx_msg void OnRecentFile(UINT nID);
    afx_msg void OnUpdateRecentFile(CCmdUI* pCmdUI);

    afx_msg void OnHelpHomepage();
    afx_msg void OnHelpCheckForUpdate();
    afx_msg void OnHelpToolbarImages();
    afx_msg void OnHelpDonate();

    afx_msg void OnClose();


    bool FilterSettingsByClassID(CLSID clsid, CWnd* parent);
    void FilterSettings(CComPtr<IUnknown> pUnk, CWnd* parent);

    LRESULT OnMPCVRSwitchFullscreen(WPARAM wParam, LPARAM lParam);

    CMPC_Lcd m_Lcd;

    CMouseWndWithArtView*  m_pVideoWnd;            // Current Video (main display screen or 2nd)
    CWnd*                  m_pOSDWnd;
    CPreView               m_wndPreView;           // SeekPreview


    void ReleasePreviewGraph();
    HRESULT PreviewWindowHide();
    HRESULT PreviewWindowShow(REFERENCE_TIME rtCur2);
    HRESULT HandleMultipleEntryRar(CStringW fn);
    bool CanPreviewUse();

    CFullscreenWnd* m_pDedicatedFSVideoWnd;
    COSD        m_OSD;
    int         m_nCurSubtitle;
    long        m_lSubtitleShift;
    REFERENCE_TIME m_rtCurSubPos;
    bool        m_bScanDlgOpened;
    bool        m_bStopTunerScan;
    bool        m_bLockedZoomVideoWindow;
    int         m_nLockedZoomVideoWindow;

    void        SetLoadState(MLS eState);
    MLS         GetLoadState() const;
    void        SetPlayState(MPC_PLAYSTATE iState);
    bool        CreateFullScreenWindow(bool isD3D=true);
    void        SetupEVRColorControl();
    void        SetupVMR9ColorControl();
    void        SetColorControl(DWORD flags, int& brightness, int& contrast, int& hue, int& saturation);
    void        SetClosedCaptions(bool enable);
    LPCTSTR     GetDVDAudioFormatName(const DVD_AudioAttributes& ATR) const;
    void        SetAudioDelay(REFERENCE_TIME rtShift);
    void        SetSubtitleDelay(int delay_ms, bool relative = false);
    //void      AutoSelectTracks();
    void        SetTimersPlay();
    void        KillTimerDelayedSeek();
    void        KillTimersStop();
    void        AdjustStreamPosPoller(bool restart);
    void        ResetSubtitlePosAndSize(bool repaint = false);

    // MPC API functions
    void        ProcessAPICommand(COPYDATASTRUCT* pCDS);
    void        SendAPICommand(MPCAPI_COMMAND nCommand, LPCWSTR fmt, ...);
    void        SendNowPlayingToApi(bool sendtrackinfo = true);
    void        SendSubtitleTracksToApi();
    void        SendAudioTracksToApi();
    void        SendPlaylistToApi();
    afx_msg void OnFileOpendirectory();

    void        SendCurrentPositionToApi(bool fNotifySeek = false);
    void        ShowOSDCustomMessageApi(const MPC_OSDDATA* osdData);
    void        JumpOfNSeconds(int seconds);

    CString GetVidPos() const;

    CComPtr<ITaskbarList3> m_pTaskbarList;
    HRESULT CreateThumbnailToolbar();
    HRESULT UpdateThumbarButton();
    HRESULT UpdateThumbarButton(MPC_PLAYSTATE iPlayState);
    HRESULT UpdateThumbnailClip();
    BOOL Create(LPCTSTR lpszClassName,
                LPCTSTR lpszWindowName,
                DWORD dwStyle = WS_OVERLAPPEDWINDOW,
                const RECT& rect = rectDefault,
                CWnd* pParentWnd = NULL,        // != NULL for popups
                LPCTSTR lpszMenuName = NULL,
                DWORD dwExStyle = 0,
                CCreateContext* pContext = NULL);
    CMPCThemeMenu* defaultMPCThemeMenu = nullptr;

    bool isSafeZone(CPoint pt);

protected:
    afx_msg void OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT lpMeasureItemStruct);
    // GDI+
    virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
    void WTSRegisterSessionNotification();
    void WTSUnRegisterSessionNotification();

    CMenu* m_pActiveContextMenu;
    CMenu* m_pActiveSystemMenu;

    void UpdateSkypeHandler();
    void UpdateSeekbarChapterBag();
    void UpdateAudioSwitcher();

    void LoadArtToViews(const CString& imagePath);
    void LoadArtToViews(std::vector<BYTE> buffer);
    void ClearArtFromViews();

    void UpdateUILanguage();

    bool PerformFlipRotate();

    bool m_bAltDownClean;
    bool m_bShowingFloatingMenubar;
    virtual void OnShowMenuBar() override {
        m_bShowingFloatingMenubar = (GetMenuBarVisibility() != AFX_MBV_KEEPVISIBLE);
    };
    virtual void OnHideMenuBar() override {
        m_bShowingFloatingMenubar = false;
    };
    virtual void SetMenuBarVisibility(DWORD dwStyle) override {
        __super::SetMenuBarVisibility(dwStyle);
        if (dwStyle & AFX_MBV_KEEPVISIBLE) {
            m_bShowingFloatingMenubar = false;
        }
    };

    bool IsAeroSnapped();

    CPoint m_snapStartPoint;
    CRect m_snapStartRect;

    bool m_bAllowWindowZoom;
    double m_dLastVideoScaleFactor;
    CSize m_lastVideoSize;

    bool m_bExtOnTop; // 'true' if the "on top" flag was set by an external tool

    CString m_sydlLastProcessURL;

    bool IsImageFile(CStringW fn);
    bool IsPlayableFormatExt(CStringW ext);
    bool IsAudioFileExt(CStringW ext);
    bool IsImageFileExt(CStringW ext);
    bool IsPlaylistFile(CStringW fn);
    bool IsPlaylistFileExt(CStringW ext);
    bool IsAudioOrVideoFileExt(CStringW ext);
    bool CanSkipToExt(CStringW ext, CStringW curExt);
    bool IsAudioFilename(CString filename);


    // Handles MF_DEFAULT and escapes '&'
    static BOOL AppendMenuEx(CMenu& menu, UINT nFlags, UINT nIDNewItem, CString& text);

    void SubtitlesSave(const TCHAR* directory = nullptr, bool silent = false);

    void OnSizingFixWndToVideo(UINT nSide, LPRECT lpRect, bool bCtrl = false);
    void OnSizingSnapToScreen(UINT nSide, LPRECT lpRect, bool bCtrl = false);

public:
    afx_msg UINT OnPowerBroadcast(UINT nPowerEvent, LPARAM nEventData);
    afx_msg void OnSessionChange(UINT nSessionState, UINT nId);

    enum UpdateControlTarget {
        UPDATE_VOLUME_STEP,
        UPDATE_LOGO,
        UPDATE_SKYPE,
        UPDATE_SEEKBAR_CHAPTERS,
        UPDATE_WINDOW_TITLE,
        UPDATE_AUDIO_SWITCHER,
        UPDATE_CONTROLS_VISIBILITY,
        UPDATE_CHILDVIEW_CURSOR_HACK,
    };

    void UpdateControlState(UpdateControlTarget target);

    void ReloadMenus();

    // TODO: refactor it outside of MainFrm
    GUID GetTimeFormat();

    CHdmvClipInfo::HdmvPlaylist m_MPLSPlaylist;
    bool m_bIsBDPlay;
    bool OpenBD(CString Path);
    bool m_bHasBDMeta;
    CAtlList<CHdmvClipInfo::BDMVMeta> m_BDMeta;
    CHdmvClipInfo::BDMVMeta GetBDMVMeta();

    bool GetDecoderType(CString& type) const;

    static bool IsOnYDLWhitelist(const CString url);

    bool CanSendToYoutubeDL(const CString url);
    bool ProcessYoutubeDLURL(CString url, bool append, bool replace = false);
    bool DownloadWithYoutubeDL(CString url, CString filename);

    /**
     * @brief Get title of file
     * @param fTitleBarTextTitle 
     * @return 
    */
    CString getBestTitle(bool fTitleBarTextTitle = true);
    MediaTransControls m_media_trans_control;

    void MediaTransportControlSetMedia();
    void MediaTransportControlUpdateState(OAFilterState state);

    enum themableDialogTypes {
        None,
        windowsFileDialog,
    };
    void enableFileDialogHook(CMPCThemeUtil* helper);
    void enableDialogHook(CMPCThemeUtil* helper, themableDialogTypes type);
private:
    themableDialogTypes watchingDialog, foundDialog;
    CMPCThemeUtil* dialogHookHelper;

public:
    afx_msg void OnSettingChange(UINT uFlags, LPCTSTR lpszSection);
    afx_msg void OnMouseHWheel(UINT nFlags, short zDelta, CPoint pt);
private:
    void SetupExternalChapters();
    void ApplyPanNScanPresetString();
    void ValidatePanNScanParameters();
};
