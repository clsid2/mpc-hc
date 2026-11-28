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
#include "Snapshot.h"
#include "CaptureController.h"
#include "DiscController.h"
#include "FavoritesRecent.h"
#include "VideoGeometry.h"
#include "ApiBridge.h"
#include "SubtitleController.h"
#include "ShaderController.h"
#include "CommandLine.h"
#include "TimerWrappers.h"
#include "OSD.h"
#include "CMPCThemeMenu.h"
#include "../SubPic/MemSubPic.h"
#include <initguid.h>
#include <qnetwork.h>
#include "PlayerGraph.h" // after initguid.h and qnetwork.h, so the qnetwork IIDs are defined here
#include "../DSUtil/FontInstaller.h"
#include "AppSettings.h"
#include "MediaTransControls.h"
#include "FavoriteOrganizeDlg.h"
#include "AllocatorCommon.h"
#include <deque>
#include <functional>
#include <optional>
#include <vector>

#define PREV_CHAP_THRESHOLD 2 // used by the file and the DVD chapter skip

class CDebugShadersDlg;
class CColorControlsDlg;
class CHistoryDlg;
class UpdateCheckerDlg;
class CFullscreenWnd;
struct DisplayMode;
enum MpcCaptionState;

interface IDSMChapterBag;
interface IGraphBuilder2;
interface IMFVideoDisplayControl;
interface IMFVideoProcessor;
interface IMadVRCommand;
interface IMadVRInfo;
interface IMadVRFrameGrabber;
interface IMadVRSettings;
interface IMadVRSubclassReplacement;
interface IMPCVRSubclassReplacement;
interface ISubClock;
interface ISubPicAllocatorPresenter2;
interface ISubPicAllocatorPresenter;
interface ISubStream;
interface ISyncClock;

enum class MLS {
    CLOSED,
    LOADING,
    LOADED,
    CLOSING,
    FAILING,
    ABORTING,
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
    std::list<CString> subs;
};

class OpenFileData : public OpenMediaData
{
public:
    OpenFileData() : rtStart(0), bAddToRecent(true), rarEntryIndex(-1) {}
    std::list<CString> fns;
    REFERENCE_TIME rtStart;
    ABRepeat abRepeat;
    bool bAddToRecent;
    CString useragent;
    CString referrer;
    int rarEntryIndex;
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

struct SeekToCommand {
    REFERENCE_TIME rtPos;
    ULONGLONG seekTime;
    bool bShowOSD;
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
public:
    enum {
        ZOOM_DEFAULT_LEVEL = 0,
        ZOOM_AUTOFIT = -1,
        ZOOM_AUTOFIT_LARGER = -2
    };
private:


    typedef std::map<int, float> PlaybackRateMap;
    static PlaybackRateMap filePlaybackRates;
    static PlaybackRateMap dvdPlaybackRates;

    CPlayerGraph m_graph;
    // SmarkSeek
    CPlayerGraph m_previewGraph;
    int defaultVideoAngle;

    CComPtr<IUnknown> m_pProv;

    bool m_bUsingDXVA;
    LPCTSTR m_HWAccelType;
    void UpdateDXVAStatus();

    // ReplayGain values found in the tags of the currently open file
    struct ReplayGainInfo {
        bool bHasTrackGain = false, bHasAlbumGain = false;
        float fTrackGain = 0.0f, fAlbumGain = 0.0f; // dB
        float fTrackPeak = 0.0f, fAlbumPeak = 0.0f; // 0 = unknown
    } m_replayGain;
    static bool ParseReplayGainValue(LPCWSTR str, float& value);
    void ApplyReplayGain();

	// temp fonts loader
	CFontInstaller m_FontInstaller;

    // StatusBar message text parts
    CString currentAudioLang;
    CString currentSubLang;
    CString m_statusbarVideoFormat;
    CString m_statusbarAudioFormat;
    CString m_statusbarVideoSize;

    int UpdateSelectedAudioStreamInfo(int index, AM_MEDIA_TYPE* pmt, LCID lcid);

    // windowing

    bool m_bDelaySetOutputRect;

    CRect m_lastWindowRect;

    void RestoreDefaultWindowRect();
    CSize GetZoomWindowSize(double dScale, bool ignore_video_size = false);
    double GetZoomAutoFitScale();

    bool alwaysOnTopZOrderInitialized = false;
    void SetAlwaysOnTop(int iOnTop);
    bool WindowExpectedOnTop();

    // dynamic menus

    void CreateDynamicMenus();
    void DestroyDynamicMenus();
    void LoadDynamicMenus();
    void SetupOpenCDSubMenu();
    void SetupAudioSubMenu();
    void SetupVideoStreamsSubMenu();
    void SetupJumpToSubMenus(CMenu* parentMenu = nullptr, int iInsertPos = -1);

    void CreateOSDBar();
    bool OSDBarSetPos();
    void DestroyOSDBar();

    CMPCThemeMenu m_mainPopupMenu, m_popupMenu;
    CMPCThemeMenu m_openCDsMenu;
    CMPCThemeMenu m_filtersMenu, m_subtitlesMenu, m_subtitlesSecondaryMenu, m_audiosMenu, m_videoStreamsMenu;
    CMPCThemeMenu m_chaptersMenu, m_titlesMenu, m_playlistMenu, m_BDPlaylistMenu, m_channelsMenu;
    CMPCThemeMenu m_favoritesMenu;
    CMPCThemeMenu m_shadersMenu;
    CMPCThemeMenu m_recentFilesMenu;

    UINT m_nJumpToSubMenusCount;

    std::vector<CComPtr<IUnknown>> m_pparray;
    std::vector<CComPtr<IAMStreamSelect>> m_ssarray;

    // chapters (file mode)
    CComPtr<IDSMChapterBag> m_pCB;
    void SetupChapters();
    void SetupCueChapters(CString fn);

    bool SeekToFileChapter(int iChapter, bool bRelative = false);

    int m_nLoops;
    bool m_bKeepLoopCountOnStop = false; // set before a skip that closes the file, so OnPlayStop keeps m_nLoops
    ABRepeat abRepeat, reloadABRepeat;
    UINT m_nLastSkipDirection;

    int m_iStreamPosPollerInterval;

    bool m_fCustomGraph;
    bool m_fShockwaveGraph;

    bool m_fFrameSteppingActive;
    int m_nStepForwardCount;
    REFERENCE_TIME m_rtStepForwardStart;
    int m_nVolumeBeforeFrameStepping;

    bool m_fEndOfStream;
    ULONGLONG m_dwLastPause;

    CString m_reloadFilename;
    REFERENCE_TIME m_rtReloadPos;
    int m_iReloadAudioIdx;
    int m_iReloadSubIdx;

    bool m_bRememberFilePos;

    bool m_bBuffering;

    bool m_fLiveWM;

    bool delayingFullScreen;

    bool m_bIsMPCVRExclusiveMode = false;

    CString m_tempstatus_msg, m_closingmsg;
    bool m_bKeepTempStatusBarVisibleOnMediaLoad = false;

    int m_lastProcessedVolume = -1;
    int m_lastProcessedMute = -1;

    REFERENCE_TIME m_rtDurationOverride;

public:
    CSnapshot m_snapshot;
    CCaptureController m_capture;
    CDiscController m_disc;
    CFavoritesRecent m_favorites;
    CVideoGeometry m_geometry;
    CApiBridge m_api;
    CSubtitleController m_subs;
    CShaderController m_shaders;
    CCommandLine m_cmdline;
private:

    //

    int m_iPlaybackMode;
    ULONG m_lCurrentChapter;
    ULONG m_lChapterStartTime;

    CString m_currentCoverAuthor;
    CString m_currentCoverPath;
    bool currentCoverIsFileArt = false;

    volatile MLS m_eMediaLoadState;
    OAFilterState m_CachedFilterState;

    volatile LONG m_ActiveGraphNotifyEvCode = 0;
    volatile bool m_OnClose_called = false;
    volatile bool m_OnClose_queued = false;

    // OnTimer, OnGraphNotify, OpenMedia, CloseMedia, OnClose and the MediaControl*
    // functions hold graph interfaces across a possible nested message pump (a
    // dialog raised inside them, or quartz pumping inside an IMediaControl call).
    // Each holds one of these; while the depth is nonzero, OpenMedia, CloseMedia,
    // OnClose and the handlers that close before they open are recorded below and
    // run from the top-level pump once the outermost holder has returned, instead
    // of underneath it. See DeferIfNested and OnRunDeferredActions.
public:
    class CDeferredActionScope {
        CMainFrame& m_frame;
    public:
        explicit CDeferredActionScope(CMainFrame& frame);
        ~CDeferredActionScope();
    };
private:
    int m_nDeferredActionDepth = 0;
    // requests recorded while a holder is on the stack, run in arrival order by
    // OnRunDeferredActions. A recorded OnClose discards them and exits instead.
    // Only the last Open or Close entry still matters when they run: an open
    // closes first, and a close after an open makes that open pointless. Other
    // entries (appends to the playlist) always run, or their items would be lost
public:
    enum class DeferredActionType { Close, Open, Other };
private:
    struct DeferredAction {
        DeferredActionType type;
        std::function<void()> run;
    };
    std::vector<DeferredAction> m_deferredActions;
    bool m_bDeferredOnClose = false;
    // the options sheet while ShowOptions has it up. Its modal pump is not a holder,
    // since an open from in there is fine, but an exit would delete the frame under
    // ShowOptions, so OnClose defers that one and cancels the sheet
    CWnd* m_pOptionsSheet = nullptr;
    // set from the INI warning through the last sheet ShowOptions loops over. Both
    // pump messages, and a hotkey, remote or modeless dialog reaching ShowOptions
    // from there stacked a second sheet over the first, which crashed in comctl32
    bool m_bInShowOptions = false;
public:
    bool DeferIfNested(DeferredActionType type, std::function<void()> action);

    // Work the web server's thread needs done here, because it reads lists this
    // thread rebuilds whenever a file opens or the playlist changes: the subtitle
    // streams (m_pSubStreams, written without m_csSubLock in LoadSubtitle and
    // OnGetSubtitles) and the playlist (never locked). The web thread sends
    // WM_MPC_UI_REQUEST with one of these and waits a bounded time for OnUIRequest
    // to run build. A send that times out is still delivered later, so the
    // request lives on the heap with a reference from each side, and whichever
    // side lets go last frees it. See CWebClientSocket::BuildOnUIThread.
    struct UIRequest {
        std::function<void(CMainFrame*, CStringA&)> build;
        CStringA body;
        LONG refs = 2;
        void Release() {
            if (InterlockedDecrement(&refs) == 0) {
                delete this;
            }
        }
    };
private:

    // a tracked popup or the menu bar is showing menus that a media change rebuilds
    // (the stream, filter and recent submenus). Windows owns those popup windows
    // while they are up, and emptying one frees the per item data it hands back to
    // DrawItem. So while one is tracked an open or close is recorded like any other
    // nested request, the menu is ended, and the rebuild runs from the top level
    // pump with nothing on screen. See DeferIfNested.
    class CTrackedMenuScope {
        CMainFrame& m_frame;
        // declared after m_frame, and destroyed only once the destructor body has
        // run, so the tracked depth is already back down when this one posts
        // WM_MPC_RUN_DEFERRED. Keep the order
        CDeferredActionScope m_defer;
    public:
        explicit CTrackedMenuScope(CMainFrame& frame);
        ~CTrackedMenuScope();
    };
    int m_nTrackedMenuDepth = 0;
    // the menu bar's modal loop spans OnEnterMenuLoop and OnExitMenuLoop, so its
    // scope cannot live on the stack like the tracked popups' ones do
    std::optional<CTrackedMenuScope> m_menuBarMenuScope;

    // bodies of OpenMedia and CloseMedia; the public wrappers record the request
    // while a holder is on the stack, these always run
    void OpenMediaInternal(std::unique_ptr<OpenMediaData> pOMD);
    void CloseMediaInternal(bool bNextIsQueued = false, bool bPendingFileDelete = false);

    bool m_bSettingUpMenus;
    volatile bool m_bOpenMediaActive;
    int m_OpenMediaFailedCount;

    bool m_bTBDropdownActive;

    REFTIME GetAvgTimePerFrame() const;

    CDropTarget m_dropTarget;
    // client coordinates of the last OnDropAccept call; OnDropFiles gets no point
    CPoint m_lastDropPoint;
    bool IsPlaylistDropWhileDisabled(CPoint ptClient) const;
    void OnDropFiles(std::list<CStringW>& slFiles, DROPEFFECT dropEffect) override;
    DROPEFFECT OnDropAccept(COleDataObject* pDataObject, DWORD dwKeyState, CPoint point) override;

public:
    int GetPlaybackMode() const {
        return m_iPlaybackMode;
    }
    bool IsPlaybackCaptureMode() const {
        return GetPlaybackMode() == PM_ANALOG_CAPTURE || GetPlaybackMode() == PM_DIGITAL_CAPTURE;
    }
    void SetPlaybackMode(int iNewStatus);
    bool IsMuted() {
        return m_wndToolBar.IsMuted();
    }
    int GetVolume() {
        return m_wndToolBar.m_volctrl.GetPos();
    }
    double GetPlayingRate() const {
        return m_dSpeedRate;
    }

    CPlayerGraph& Graph() {
        return m_graph;
    }
    CPlayerGraph& PreviewGraph() {
        return m_previewGraph;
    }

    // The bars, the view and the controls, for the windows and pages that drive
    // them directly (fill, toggle, lay out or reposition), the same way Graph()
    // hands out the graph.
    CPlayerPlaylistBar& PlaylistBar() {
        return m_wndPlaylistBar;
    }
    CPlayerToolBar& ToolBar() {
        return m_wndToolBar;
    }
    CPlayerSeekBar& SeekBar() {
        return m_wndSeekBar;
    }
    CPlayerStatusBar& StatusBar() {
        return m_wndStatusBar;
    }
    CChildView& View() {
        return m_wndView;
    }
    CMainFrameControls& Controls() {
        return m_controls;
    }

    bool IsAudioOnly() const {
        return m_fAudioOnly;
    }
    bool IsMPCVRExclusiveMode() const {
        return m_bIsMPCVRExclusiveMode;
    }
    bool IsToolBarDropdownActive() const {
        return m_bTBDropdownActive;
    }
    OAFilterState GetCachedFilterState() const {
        return m_CachedFilterState;
    }
    bool IsSeekPreviewEnabled() const {
        return m_bUseSeekPreview;
    }
    void SetSeekPreviewEnabled(bool bEnabled) {
        m_bUseSeekPreview = bEnabled;
    }

    void SendStatusMessage(CString msg, int nTimeOut, bool bRevealStatusBar = false,
                           bool bKeepVisibleOnMediaLoad = false);
    void ShowOptions(int idPage = 0);
    void SetVolumeBoost(UINT nAudioBoost);
    void SetBalance(int balance);
    bool ForwardMessageToRenderer(HWND hWnd, UINT message, WPARAM& wParam, LPARAM& lParam, LRESULT& ret);
    void SetDefaultWindowRect(int iMonitor = 0);
    void SetDefaultFullscreenState();
    CRect GetInvisibleBorderSize() const;

    // For the composition controllers (m_snapshot through m_cmdline). The bars and
    // the dynamic menus are handed out as references because the controllers fill
    // and drive them; the menu objects themselves stay with the other dynamic menus.
    CPlayerInfoBar& InfoBar() {
        return m_wndInfoBar;
    }
    CPlayerCaptureBar& CaptureBar() {
        return m_wndCaptureBar;
    }
    CPlayerNavigationBar& NavigationBar() {
        return m_wndNavigationBar;
    }
    CPlayerSubresyncBar& SubresyncBar() {
        return m_wndSubresyncBar;
    }
    CMPCThemeMenu& FavoritesMenu() {
        return m_favoritesMenu;
    }
    CMPCThemeMenu& RecentFilesMenu() {
        return m_recentFilesMenu;
    }
    CMPCThemeMenu& ShadersMenu() {
        return m_shadersMenu;
    }
    CMPCThemeMenu& SubtitlesMenu() {
        return m_subtitlesMenu;
    }
    CMPCThemeMenu& SubtitlesSecondaryMenu() {
        return m_subtitlesSecondaryMenu;
    }

    bool IsShockwaveGraph() const {
        return m_fShockwaveGraph;
    }
    int GetDefaultVideoAngle() const {
        return defaultVideoAngle;
    }
    ULONG GetChapterStartTime() const {
        return m_lChapterStartTime;
    }
    const OpenMediaData* GetLastOpenMediaData() const {
        return m_lastOMD.get();
    }
    const ABRepeat& GetABRepeat() const {
        return abRepeat;
    }
    void SetABRepeatMarks(REFERENCE_TIME rtA, REFERENCE_TIME rtB) {
        abRepeat.positionA = rtA;
        abRepeat.positionB = rtB;
    }
    // the chapter bag of the open file or disc; no reference is added
    IDSMChapterBag* GetChapterBag() const {
        return m_pCB;
    }
    void ResetChapterBag();
    int GetVolumeBeforeFrameStepping() const {
        return m_nVolumeBeforeFrameStepping;
    }
    void SetVolumeBeforeFrameStepping(int nVolume) {
        m_nVolumeBeforeFrameStepping = nVolume;
    }
    // the stored rate only; the graph is not touched
    void ResetPlayingRateValue() {
        m_dSpeedRate = 1.0;
    }
    void ResetLoopCount() {
        m_nLoops = 0;
    }
    void SetAllowWindowZoom(bool bAllow) {
        m_bAllowWindowZoom = bAllow;
    }
    void ClearDurationOverride() {
        m_rtDurationOverride = -1;
    }
    void SetCurrentAudioLang(const CString& lang) {
        currentAudioLang = lang;
    }
    void ClearCurrentAudioLang() {
        currentAudioLang.Empty();
    }
    void SetCurrentSubLang(const CString& lang) {
        currentSubLang = lang;
    }
    void ClearCurrentSubLang() {
        currentSubLang.Empty();
    }
    bool InstallTempFont(LPCTSTR fn) {
        return m_FontInstaller.InstallTempFontFile(fn);
    }
    // sets the closing message and shows it in the status bar
    void ShowClosingMessage(const CString& msg) {
        m_closingmsg = msg;
        m_wndStatusBar.SetStatusMessage(m_closingmsg);
    }
    // what a reopen restores: the file, position and A-B marks, with the audio and
    // subtitle track choices cleared
    void SetReloadState(const CString& fn, REFERENCE_TIME rtPos, const ABRepeat& ab) {
        m_reloadFilename = fn;
        m_rtReloadPos = rtPos;
        reloadABRepeat = ab;
        m_iReloadAudioIdx = -1;
        m_iReloadSubIdx = -1;
    }
    int GetReloadSubIdx() const {
        return m_iReloadSubIdx;
    }
    void ClearReloadSubIdx() {
        m_iReloadSubIdx = -1;
    }
    bool IsGraphThreadRunning() const;
    BOOL PostGraphThreadMessage(UINT message, WPARAM wParam, LPARAM lParam);

    CWnd* GetModalParent();
    void ShowMediaTypesDialog();
    void CleanGraph();
    void OpenSetupVideo();
    void OpenSetupAudio();
    void OpenSetupStatsBar();
    void OpenSetupStatusBar();
    void OpenSetupWindowTitle(bool reset = false);
    void SetupFiltersSubMenu();
    void UpdateSeekbarChapterBag();
    bool IsPlaylistFile(CStringW fn);
    CSize GetVideoOrArtSize(MINMAXINFO& mmi);
    bool GetWorkAreaRect(CRect& work);
    CRect GetZoomWindowRect(const CSize& size, bool ignoreSavedPosition = false);
    void ZoomVideoWindow(double dScale = ZOOM_DEFAULT_LEVEL, bool ignore_video_size = false);
    void OnVideoSizeChanged(const bool bWasAudioOnly = false);
    DWORD SetupNavStreamSelectSubMenu(CMenu& subMenu, UINT id, DWORD dwSelGroup);
    void OnNavStreamSelectSubMenu(UINT id, DWORD dwSelGroup);
    void OnStreamSelect(bool forward, DWORD dwSelGroup);
    static CString GetStreamOSDString(CString name, LCID lcid, DWORD dwSelGroup);
    // Handles MF_DEFAULT and escapes '&'
    static BOOL AppendMenuEx(CMenu& menu, UINT nFlags, UINT nIDNewItem, CString& text);

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
    bool m_bPausedByMinimize;
    bool m_bThemeChangePending;
    bool restoringWindowRect;

    bool m_fAudioOnly;
    std::unique_ptr<OpenMediaData> m_lastOMD;

    int         m_loadedAudioTrackIndex = -1;
    int         m_audioTrackCount = 0;

    double m_dSpeedRate;

    void ForceCloseProcess();
    void ThrowAndForceClose();

    // Operations
    bool OpenMediaPrivate(std::unique_ptr<OpenMediaData> pOMD);
    void CloseMediaPrivate();

    CCritSec lockModalDialog; // guards the two members below, never held across a modal loop
    CDialog* openingModalDlg;
    DWORD openingModalDlgThread;
    INT_PTR DoOpeningModalDialog(CDialog& dlg);
    void CloseOpeningModalDialog();

    void OpenCreateGraphObject(OpenMediaData* pOMD);
    void OpenFile(OpenFileData* pOFD);
    void OpenCustomizeGraph();
    CSize OpenSetupGetVideoSize();
    void OpenSetupInfoBar(bool bClear = true);
    bool UpdateChapterInInfoBar(bool bRecalcLayout = true);
    void UpdateDVDInfoBar();
    void CheckSelectedAudioStream();
    void CheckSelectedVideoStream();

public:
    static bool GetCurDispMode(const CString& displayName, DisplayMode& dm);
    static bool GetDispMode(CString displayName, int i, DisplayMode& dm);

protected:
    void SetDispMode(CString displayName, const DisplayMode& dm, int msAudioDelay);
    void AutoChangeMonitorMode();

    void GraphEventComplete();

    friend class CGraphThread; // runs OpenMediaPrivate and CloseMediaPrivate for the open path
    CGraphThread* m_pGraphThread;
    bool m_bOpenedThroughThread;
    ATL::CEvent m_evOpenPrivateFinished;
    ATL::CEvent m_evClosePrivateFinished;

    void LoadKeyFrames();
    std::vector<REFERENCE_TIME> m_kfs;

    volatile bool m_fOpeningAborted;
    bool m_bWasSnapped;

public:
    void OpenCurPlaylistItem(REFERENCE_TIME rtStart = 0, bool reopen = false, ABRepeat abRepeat = ABRepeat());
    void OpenMedia(std::unique_ptr<OpenMediaData> pOMD);
    bool ResetDevice();
    bool DisplayChange();
    bool CloseMediaBeforeOpen();
    void CloseMedia(bool bNextIsQueued = false, bool bPendingFileDelete = false);
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

    void SetAudioTrackIdx(int index);
    int GetCurrentAudioTrackIdx(CString *pstrName = nullptr);

    CString GetFileName();

    void DoAfterPlaybackEvent();
    bool SearchInDir(bool bDirForward, bool bLoop = false);
    bool WildcardFileSearch(CString searchstr, std::set<CString, CStringUtils::LogicalLess>& results, bool recurse_dirs, std::map<CString, ULONGLONG>* creationTimes = nullptr);
    CString lastOpenFile;
    bool CanSkipFromClosedFile();

    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
    virtual BOOL PreTranslateMessage(MSG* pMsg);
    virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
    virtual void RecalcLayout(BOOL bNotify = TRUE);
    void EnableDocking(DWORD dwDockStyle);

    // DVB capture
    LRESULT OnCurrentChannelInfoUpdated(WPARAM wParam, LPARAM lParam);

    bool CheckABRepeat(REFERENCE_TIME& aPos, REFERENCE_TIME& bPos);
    void PerformABRepeat();
    void DisableABRepeat();

    // Implementation
public:
    virtual ~CMainFrame();
#ifdef _DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

protected:  // control bar embedded members
    CMainFrameControls m_controls;

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

    std::unique_ptr<CColorControlsDlg> m_pColorControls;
    std::unique_ptr<UpdateCheckerDlg> m_pUpdateCheckerDlg;

    // Generated message map functions

    DECLARE_MESSAGE_MAP()

public:
    afx_msg int OnNcCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnDestroy();
    afx_msg LRESULT OnLAVPropPageCallback(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnInfoBarShown(WPARAM wParam, LPARAM lParam);
    bool m_bLAVPropPageOpen = false;

    afx_msg LRESULT OnTaskBarRestart(WPARAM, LPARAM);
    afx_msg LRESULT OnNotifyIcon(WPARAM, LPARAM);
    afx_msg LRESULT OnTaskBarThumbnailsCreate(WPARAM, LPARAM);

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

    afx_msg LRESULT OnDoStandby(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnDoHibernate(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnDoShutdown(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnDoLogOff(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnDoOpenCurPlaylist(WPARAM wParam, LPARAM lParam);

    afx_msg void SaveAppSettings();

    afx_msg LRESULT OnNcHitTest(CPoint point);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);

    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);

    void RestoreFocus();

    afx_msg void OnInitMenu(CMenu* pMenu);
    afx_msg void OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu);
    afx_msg void OnUnInitMenuPopup(CMenu* pPopupMenu, UINT nFlags);
    afx_msg void OnEnterMenuLoop(BOOL bIsTrackPopupMenu);
    afx_msg void OnExitMenuLoop(BOOL bIsTrackPopupMenu);

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
    afx_msg LRESULT OnRunDeferredActions(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnUpdateCheckDone(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnUIRequest(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnHelpPromptAddr(WPARAM wParam, LPARAM lParam);

    // Only reached in headless scan mode: with the dialog running these go to
    // it instead, because DoTunerScan sends to the HWND it was handed.
    afx_msg LRESULT OnHeadlessScanNewChannel(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnHeadlessScanEnd(WPARAM wParam, LPARAM lParam);
    void OnFilePostClosemedia(bool bNextIsQueued = false);

    afx_msg void OnBossKey();

    void ToolbarContextMenu(int iItem, int nIndex, CRect buttonRect);

    afx_msg void OnUpdateAudiosButton(CCmdUI* pCmdUI);

    afx_msg void OnStreamAudio(UINT nID);
    afx_msg void OnAudioShiftOnOff();

    afx_msg LRESULT OnLoadSubtitles(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnGetSubtitles(WPARAM, LPARAM lParam);

    // menu item handlers

    INT_PTR DoFileDialogWithLastFolder(CFileDialog& fd, CStringW& lastPath);

    afx_msg void OnFileOpenQuick();
    afx_msg void OnFileOpenmedia();
    afx_msg void OnUpdateFileOpen(CCmdUI* pCmdUI);
    afx_msg BOOL OnCopyData(CWnd* pWnd, COPYDATASTRUCT* pCopyDataStruct);
    afx_msg LRESULT OnCommandLineReceived(WPARAM wParam, LPARAM lParam);
    afx_msg void OnFileOpendevice();
    afx_msg void OnFileOpenOpticalDisk(UINT nID);
    afx_msg void OnFileReopen();
    afx_msg void OnFileRecycle();
    afx_msg void OnFileSaveAs();
    afx_msg void OnUpdateFileSaveAs(CCmdUI* pCmdUI);
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
    afx_msg void OnViewColorControls();
    afx_msg void OnUpdateViewColorControls(CCmdUI* pCmdUI);
    afx_msg void OnViewMinimal();
    afx_msg void OnUpdateViewMinimal(CCmdUI* pCmdUI);
    afx_msg void OnViewCompact();
    afx_msg void OnUpdateViewCompact(CCmdUI* pCmdUI);
    afx_msg void OnViewNormal();
    afx_msg void OnUpdateViewNormal(CCmdUI* pCmdUI);
    afx_msg void OnViewCustom();
    afx_msg void OnUpdateViewCustom(CCmdUI* pCmdUI);
    afx_msg void OnViewFullscreen();
    afx_msg void OnViewFullscreenSecondary();
    afx_msg void OnUpdateViewFullscreen(CCmdUI* pCmdUI);
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

    afx_msg void OnViewFullscreenGUISupport();
    afx_msg void OnViewHighColorResolution();
    afx_msg void OnViewForceInputHighColorResolution();
    afx_msg void OnViewFullFloatingPointProcessing();
    afx_msg void OnViewHalfFloatingPointProcessing();
    afx_msg void OnViewEnableFrameTimeCorrection();
    afx_msg void OnViewVSyncOffsetIncrease();
    afx_msg void OnViewVSyncOffsetDecrease();
    afx_msg void OnUpdateViewOSDDisplayTime(CCmdUI* pCmdUI);
    afx_msg void OnViewOSDDisplayTime();
    afx_msg void OnUpdateViewOSDShowFileName(CCmdUI* pCmdUI);
    afx_msg void OnViewOSDShowFileName();
    afx_msg void OnD3DFullscreenToggle();

    afx_msg void OnPlayPlay();
    afx_msg void OnPlayPause();
    afx_msg void OnPlayPlaypause();
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
    afx_msg void OnPlayAudio(UINT nID);
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
    afx_msg void OnNavigateJumpTo(UINT nID);
    afx_msg void OnNavigateMenuItem(UINT nID);
    afx_msg void OnUpdateNavigateMenuItem(CCmdUI* pCmdUI);

    afx_msg void OnHelpHomepage();
    afx_msg void OnHelpCheckForUpdate();
    afx_msg void OnHelpToolbarImages();
    afx_msg void OnHelpDonate();

    afx_msg void OnClose();


    bool FilterSettingsByClassID(CLSID clsid, CWnd* parent);
    void FilterSettings(CComPtr<IUnknown> pUnk, CWnd* parent);

    LRESULT OnMPCVRSwitchFullscreen(WPARAM wParam, LPARAM lParam);

    CMouseWndWithArtView*  m_pVideoWnd;            // Current Video (main display screen or 2nd)
    CWnd*                  m_pOSDWnd;
    CPreView               m_wndPreView;           // SeekPreview


    void ReleasePreviewGraph();
    HRESULT PreviewWindowHide();
    HRESULT PreviewWindowShow(REFERENCE_TIME rtCur2);
    void SyncPreviewEdition();
    HRESULT HandleMultipleEntryRar(CStringW fn, int* pEntryIndex = nullptr);
    bool TrySkipWithinRar(bool forward);
    bool CanPreviewUse();

    CFullscreenWnd* m_pDedicatedFSVideoWnd;
    COSD        m_OSD;
    bool        m_bLockedZoomVideoWindow;
    int         m_nLockedZoomVideoWindow;

    void        SetLoadState(MLS eState);
    MLS         GetLoadState() const;
    bool        IsStateLoaded();
    bool        IsStateLoadedOrLoading();
    bool        IsStateClosed();
    bool        IsStateClosedOrLoaded();
    bool        IsStateClosingAborting();
    void        SetPlayState(MPC_PLAYSTATE iState);
    bool        CreateFullScreenWindow(bool isD3D=true);
    void        SetupEVRColorControl();
    void        SetupVMR9ColorControl();
    void        SetColorControl(DWORD flags, int& brightness, int& contrast, int& hue, int& saturation);
    void        SetClosedCaptions(bool enable);
    void        SetAudioDelay(REFERENCE_TIME rtShift);
    //void      AutoSelectTracks();
    void        SetTimersPlay();
    void        KillTimerDelayedSeek();
    void        KillTimersStop();
    void        AdjustStreamPosPoller(bool restart);

    afx_msg LRESULT OnApiIntMessage(WPARAM wParam, LPARAM lParam);
    afx_msg void OnFileOpendirectory();

    void        JumpOfNSeconds(int seconds);

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

    void UpdateAudioSwitcher();

    void LoadArtToViews(const CString& imagePath);
    void LoadArtToViews(std::vector<BYTE> buffer);
    void ClearArtFromViews();

    void UpdateUILanguage();
    void ApplyThemeChange();

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

    bool IsImageFile(CStringW fn);
    bool IsPlayableFormatExt(CStringW ext);
    bool IsAudioFileExt(CStringW ext);
    bool IsImageFileExt(CStringW ext);
    bool IsPlaylistFileExt(CStringW ext);
    bool IsAudioOrVideoFileExt(CStringW ext);
    bool CanSkipToExt(CStringW ext, CStringW curExt);
    bool IsAudioFilename(CString filename);

    void OnSizingFixWndToVideo(UINT nSide, LPRECT lpRect, bool bCtrl = false);
    void OnSizingSnapToScreen(UINT nSide, LPRECT lpRect, bool bCtrl = false);

public:
    afx_msg UINT OnPowerBroadcast(UINT nPowerEvent, LPARAM nEventData);
    afx_msg void OnSessionChange(UINT nSessionState, UINT nId);

    enum UpdateControlTarget {
        UPDATE_VOLUME_STEP,
        UPDATE_LOGO,
        UPDATE_MEDIA_ART,
        UPDATE_SEEKBAR_CHAPTERS,
        UPDATE_WINDOW_TITLE,
        UPDATE_AUDIO_SWITCHER,
        UPDATE_CONTROLS_VISIBILITY,
        UPDATE_CHILDVIEW_CURSOR_HACK,
    };

    void UpdateControlState(UpdateControlTarget target);

    UINT GetNormalPresetCS() const;
    void ApplyTimeOnSeekBarChange();
    void ApplyCustomPresetChange();
    void ApplyStartupPreset();
    int m_nActiveViewPreset = 0; // command id of the last-applied view preset, 0 = none (#3256)

    // Reveal the status bar for a message when a preset hides it; stays until next media load (issue #3256).
    bool m_bStatusBarForcedForMessage = false;
    bool IsStatusBarForcedForMessage() const { return m_bStatusBarForcedForMessage; }
    void ShowStatusBarForMessage();
    void RestoreStatusBarMessageHold();
    void SetClosingError(const CString& msg); // set the closing message and reveal the status bar (errors)
    void SetClosingError(UINT nIDmsg);

    void ReloadMenus();

    // TODO: refactor it outside of MainFrm
    GUID GetTimeFormat();

    bool GetDecoderType(CString& type) const;
    CString GetAudioInfo() const;

    /**
     * @brief Get title of file
     * @param fTitleBarTextTitle 
     * @return 
    */
    CString getBestTitle(bool fTitleBarTextTitle = true);

    afx_msg LRESULT OnSmtcSeek(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnSmtcAutoRepeat(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnSmtcShuffle(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnSmtcRate(WPARAM wParam, LPARAM lParam);

    afx_msg void OnSettingChange(UINT uFlags, LPCTSTR lpszSection);
    afx_msg void OnMouseHWheel(UINT nFlags, short zDelta, CPoint pt);
private:
    void SetupExternalChapters();
};
