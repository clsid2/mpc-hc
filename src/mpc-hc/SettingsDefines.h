/*
 * (C) 2009-2018 see Authors.txt
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

#define IDS_R_SETTINGS                      _T("Settings")
#define IDS_R_SETTINGS_FULLSCREEN_AUTOCHANGE_MODE IDS_R_SETTINGS _T("\\FullscreenAutoChangeMode")
#define IDS_R_VERSION                       _T("SettingsVersion")
#define IDS_R_FILTERS                       _T("Filters")
#define IDS_R_EXTERNAL_FILTERS_x86          _T("Filters\\x86")
#define IDS_R_EXTERNAL_FILTERS_x64          _T("Filters\\x64")
#ifndef _WIN64
#define IDS_R_EXTERNAL_FILTERS              IDS_R_EXTERNAL_FILTERS_x86
#else
#define IDS_R_EXTERNAL_FILTERS              IDS_R_EXTERNAL_FILTERS_x64
#endif
#define IDS_R_INTERNAL_FILTERS              _T("Internal Filters")
#define IDS_R_FAVFILES                      _T("Favorites\\Files")
#define IDS_R_FAVDVDS                       _T("Favorites\\DVDs")
#define IDS_R_FAVDEVICES                    _T("Favorites\\Devices")
#define IDS_R_COMMANDS                      _T("Commands2")
#define IDS_R_LOGINS                        _T("Logins")
#define IDS_R_FAVORITES                     _T("Favorites")

#define IDS_RS_FAV_REMEMBERPOS              _T("RememberPosition")
#define IDS_RS_FAV_RELATIVEDRIVE            _T("RelativeDrive")
#define IDS_RS_FAV_REMEMBERABMARKS          _T("RememberABMarks")
#define IDS_RS_DVDPOS                       _T("RememberDVDPos")
#define IDS_RS_FILEPOS                      _T("RememberFilePos")
#define IDS_RS_FILEPOSLONGER                _T("RememberPosForLongerThan")
#define IDS_RS_FILEPOSAUDIO                 _T("RememberPosForAudioFiles")
#define IDS_RS_FILEPOS_PLAYLIST             _T("RememberExternalPlaylistPos")
#define IDS_RS_FILEPOS_TRACK_SELECTION      _T("RememberTrackSelection")
#define IDS_RS_LASTFULLSCREEN               _T("LastFullScreen")
#define IDS_RS_EVR_BUFFERS                  _T("EVRBuffers")
#define IDS_RS_SHOWOSD                      _T("ShowOSD")
#define IDS_RS_SHOW_CURRENT_TIME_OSD        _T("ShowCurrentTimeInOSD")
#define IDS_RS_LANGUAGE                     _T("InterfaceLanguage")
#define IDS_RS_GLOBALMEDIA                  _T("UseGlobalMedia")
#define IDS_RS_DXVAFILTERS                  _T("DXVAFilters")
#define IDS_RS_FFMPEGFILTERS                _T("FFmpegFilters")
#define IDS_RS_TITLEBARTEXTSTYLE            _T("TitleBarTextStyle")
#define IDS_RS_CONTROLSTATE                 _T("ControlState")
#define IDS_RS_LOOP                         _T("Loop")
#define IDS_RS_LOOPMODE                     _T("LoopMode")
#define IDS_RS_LOOPNUM                      _T("LoopNum")
#define IDS_RS_SNAPTODESKTOPEDGES           _T("SnapToDesktopEdges")
#define IDS_RS_ENABLESUBTITLES              _T("EnableSubtitles")
#define IDS_RS_PREFER_FORCED_DEFAULT_SUBTITLES _T("PreferForcedDefaultSubtitles")
#define IDS_RS_PRIORITIZEEXTERNALSUBTITLES  _T("PrioritizeExternalSubtitles")
#define IDS_RS_DISABLEINTERNALSUBTITLES     _T("DisableInternalSubtitles")
#define IDS_RS_ALLOW_OVERRIDING_EXT_SPLITTER _T("AllowOverridingExternalSplitterSubtitleChoice")
#define IDS_RS_AUTODOWNLOADSUBTITLES        _T("AutoDownloadSubtitles")
#define IDS_RS_AUTOSAVEDOWNLOADEDSUBTITLES  _T("AutoSaveDownloadedSubtitles")
#define IDS_RS_AUTODOWNLOADSCOREMOVIES      _T("AutoDownloadScoreMovies")
#define IDS_RS_AUTODOWNLOADSCORESERIES      _T("AutoDownloadScoreSeries")
#define IDS_RS_AUTODOWNLOADSUBTITLESEXCLUDE _T("AutoDownloadSubtitlesExclude")
#define IDS_RS_AUTOUPLOADSUBTITLES          _T("AutoUploadSubtitles")
#define IDS_RS_SUBTITLESPROVIDERS           _T("SubtitlesProviders")
#define IDS_RS_PREFERHEARINGIMPAIREDSUBTITLES _T("PreferHearingImpairedSubtitles")
#define IDS_RS_MPCTHEME                     _T("MPCTheme")
#define IDS_RS_MODERNSEEKBARHEIGHT          _T("ModernSeekbarHeight")
#define IDS_RS_MODERNTHEMEMODE              _T("ModernThemeMode")
#define IDS_RS_FULLSCREEN_DELAY             _T("FullscreenDelay")
#define IDS_RS_VERTICALALIGNVIDEO          _T("VerticalAlignVideo")
#define IDS_RS_SUBTITLEPATHS                _T("SubtitlePaths")
#define IDS_RS_OVERRIDE_DEFAULT_STYLE       _T("OverrideDefaultStyle")
#define IDS_RS_OVERRIDE_ALL_STYLES          _T("OverrideAllStyles")
#define IDS_RS_THUMBWIDTH                   _T("ThumbWidth")
#define IDS_RS_SUBSAVEEXTERNALSTYLEFILE     _T("SubSaveExternalStyleFile")
#define IDS_RS_D3DFULLSCREEN                _T("D3DFullScreen")
//#define IDS_RS_MONITOR_AUTOREFRESHRATE      _T("MonitorAutoRefreshRate")
#define IDS_RS_SPEEDSTEP                    _T("SpeedStep")
#define IDS_RS_DEFAULTTOOLBARSIZE           _T("DefaultToolbarSize")
#define IDS_R_PLAYERTOOLBAR                 L"Toolbars\\PlayerToolBar"
#define IDS_RS_TOOLBARACTION1                L"ToolbarAction1"
#define IDS_RS_TOOLBARACTION2                L"ToolbarAction2"
#define IDS_RS_TOOLBARACTION3                L"ToolbarAction3"
#define IDS_RS_TOOLBARACTION4                L"ToolbarAction4"
#define IDS_RS_TOOLBARRIGHTACTION1           L"ToolbarRightAction1"
#define IDS_RS_TOOLBARRIGHTACTION2           L"ToolbarRightAction2"
#define IDS_RS_TOOLBARRIGHTACTION3           L"ToolbarRightAction3"
#define IDS_RS_TOOLBARRIGHTACTION4           L"ToolbarRightAction4"
#define IDS_RS_TOOLBAR_TYPE                  L"ToolbarType"
#define IDS_RS_TOOLBAR_NAME                  L"ToolbarName"
#define IDS_RS_SAVEIMAGE_POSITION           _T("SaveImagePosition")
#define IDS_RS_SAVEIMAGE_CURRENTTIME        _T("SaveImageCurrentTime")
#define IDS_RS_ALLOW_INACCURATE_FASTSEEK    _T("AllowInaccurateFastseek")
#define IDS_RS_LOOP_FOLDER_NEXT_FILE        _T("LoopFolderOnPlayNextFile")
#define IDS_RS_TIME_REFRESH_INTERVAL        _T("TimeRefreshInterval")
#define IDS_RS_SHOW_ABMARKS_STATUSBAR       _T("ShowABMarksInStatusbar")
#define IDS_RS_SHOW_LANG_STATUSBAR          _T("ShowLangInStatusbar")
#define IDS_RS_SHOW_FPS_STATUSBAR           _T("ShowFPSInStatusbar")
#define IDS_RS_SHOW_VIDEOINFO_STATUSBAR     _T("ShowVideoInfoInStatusbar")
#define IDS_RS_SHOW_AUDIOFORMAT_STATUSBAR   _T("ShowAudioFormatInStatusbar")
#define IDS_RS_ADD_LANGCODE_WHEN_SAVE_SUBTITLES _T("AddLanguageCodeWhenSaveSubtitles")
#define IDS_RS_USE_TITLE_IN_RECENT_FILE_LIST    _T("UseTitleInRecentFileList")
#define IDS_RS_YDL_SUBS_PREFERENCE          _T("YDLSubsPreference")
#define IDS_RS_USE_AUTOMATIC_CAPTIONS       _T("UseAutomaticCaptions")
#define IDS_RS_LOCK_NOPAUSE                 _T("LockNoPause")
#define IDS_RS_PREVENT_DISPLAY_SLEEP        _T("PreventDisplaySleep")
#define IDS_RS_USE_SMTC                     _T("UseSMTC")
#define IDS_RS_RELOAD_AFTER_LONG_PAUSE      _T("ReloadAfterLongPause")
#define IDS_RS_OPEN_REC_PANEL_WHEN_OPENING_DEVICE _T("OpenRecordingPanelWhenOpeningDevice")
#define IDS_RS_REDIRECT_OPEN_TO_APPEND_THRESHOLD _T("RedirectOpenToAppendThreshold")
#define IDS_RS_FULLSCREEN_SEPARATE_CONTROLS _T("FullscreenSeparateControls")
#define IDS_RS_ALWAYS_USE_SHORT_MENU        _T("AlwaysUseShortMenu")
#define IDS_RS_STILL_VIDEO_DURATION         _T("StillVideoDuration")
#define IDS_RS_MOUSE_LEFTUP_DELAY           _T("MouseLeftUpDelay")
#define IDS_RS_USE_FREETYPE                 _T("UseFreetype")
#define IDS_RS_USE_MEDIAINFO_LOAD_FILE_DURATION _T("UseMediainfoLoadFileDuration")
#define IDS_RS_CAPTURE_DEINTERLACE          _T("CaptureDeinterlace")
#define IDS_RS_PAUSE_WHILE_DRAGGING_SEEKBAR _T("PauseWhileDraggingSeekbar")
#define IDS_RS_CONFIRM_FILE_DELETE          _T("ConfirmFileDelete")
#define IDS_RS_LIBASS_FOR_SRT               _T("UseLibassForSRT")
#define IDS_RS_SHOW_VOLUME_PERCENTAGE       _T("ShowVolumePercentage")

// Audio
#define IDS_RS_VOLUME                       _T("Volume")
#define IDS_RS_MUTE                         _T("Mute")
#define IDS_RS_BALANCE                      _T("Balance")
#define IDS_RS_VOLUMESTEP                   _T("VolumeStep")

// AudioSwitcher
#define IDS_RS_ENABLEAUDIOSWITCHER          _T("EnableAudioSwitcher")
#define IDS_RS_AUDIONORMALIZE               _T("AudioNormalize")
#define IDS_RS_AUDIOMAXNORMFACTOR           _T("AudioMaxNormFactor")
#define IDS_RS_AUDIONORMALIZERECOVER        _T("AudioNormalizeRecover")
#define IDS_RS_AUDIOBOOST                   _T("AudioBoost")
#define IDS_RS_AUDIOBOOSTWARNED             _T("AudioBoostWarned")
#define IDS_RS_ENABLEAUDIOTIMESHIFT         _T("EnableAudioTimeShift")
#define IDS_RS_AUDIOTIMESHIFT               _T("AudioTimeShift")
#define IDS_RS_CUSTOMCHANNELMAPPING         _T("CustomChannelMapping")
#define IDS_RS_SPEAKERCHANNELS              _T("SpeakerChannels")
#define IDS_RS_SPEAKERTOCHANNELMAPPING      _T("SpeakerToChannelMapping")

// Video
#define IDS_RS_COLOR_BRIGHTNESS             _T("VideoBrightness")
#define IDS_RS_COLOR_CONTRAST               _T("VideoContrast")
#define IDS_RS_COLOR_HUE                    _T("VideoHue")
#define IDS_RS_COLOR_SATURATION             _T("VideoSaturation")

// DVD
#define IDS_RS_DVDPATH                      _T("DVDPath")
#define IDS_RS_USEDVDPATH                   _T("UseDVDPath")
#define IDS_RS_MENULANG                     _T("MenuLang")
#define IDS_RS_AUDIOLANG                    _T("AudioLang")
#define IDS_RS_SUBTITLESLANG                _T("SubtitlesLang")
#define IDS_RS_RENDERSSAUSINGLIBASS         _T("RenderSSAUsingLibass")
#define IDS_RS_RENDERSRTUSINGLIBASS         _T("RenderSRTUsingLibass")
#define IDS_RS_OPENTYPELANGHINT             _T("OpenTypeLangHint")
#define IDS_RS_AUTOSPEAKERCONF              _T("AutoSpeakerConf")
#define IDS_RS_CLOSEDCAPTIONS               _T("ClosedCaptions")

#define IDS_RS_TITLEBARTEXTTITLE            _T("TitleBarTextTitle")
#define IDS_RS_VMR9MIXERYUV                 _T("VMRMixerYUV")
#define IDS_RS_ZOOM                         _T("Zoom")
#define IDS_RS_MULTIINST                    _T("AllowMultipleInstances")
#define IDS_RS_ALWAYSONTOP                  _T("AlwaysOnTop")
#define IDS_RS_AUTOZOOM                     _T("AutoZoom")
#define IDS_RS_AUTOFITFACTOR                _T("AutoFitFactor")
#define IDS_RS_AUTOFITFACTOR_MIN            _T("AutoFitFactorMin")
#define IDS_RS_AUTOFITFACTOR_MAX            _T("AutoFitFactorMax")
#define IDS_RS_AFTER_PLAYBACK               _T("AfterPlayback")
#define IDS_RS_SPSTYLE                      _T("SPDefaultStyle")
#define IDS_RS_SPOVERRIDEPLACEMENT          _T("SPOverridePlacement")
#define IDS_RS_SPHORPOS                     _T("SPHorPos")
#define IDS_RS_SPVERPOS                     _T("SPVerPos")
#define IDS_RS_SUBTITLEARCOMPENSATION       _T("SubtitleARCompensation")
#define IDS_RS_SPCSIZE                      _T("SPCSize2")
#define IDS_RS_SPCMAXRESX                   _T("SPCMaxResX")
#define IDS_RS_SPCMAXRESY                   _T("SPCMaxResY")
#define IDS_RS_DISABLE_SUBTITLE_ANIMATION   _T("DisableSubtitleAnimation")
#define IDS_RS_RENDER_AT_WHEN_ANIM_DISABLED _T("RenderAtWhenSubtitleAnimationIsDisabled")
#define IDS_RS_SUBTITLE_ANIMATION_RATE      _T("SubtitleAnimationRate")
#define IDS_RS_ALLOW_DROPPING_SUBPIC        _T("AllowDroppingSubpic")
#define IDS_RS_EXITFULLSCREENATTHEEND       _T("ExitFullscreenAtTheEnd")
#define IDS_RS_REMEMBERWINDOWPOS            _T("RememberWindowPos")
#define IDS_RS_LASTWINDOWRECT               _T("LastWindowRect")
#define IDS_RS_AUDIORENDERERTYPE            _T("AudioRendererType")
#define IDS_RS_HIDECAPTIONMENU              _T("HideCaptionMenu")
#define IDS_RS_HIDENAVIGATION               _T("HideNavigation")
#define IDS_RS_DEFAULTVIDEOFRAME            _T("DefaultVideoFrame")
#define IDS_RS_REMEMBERWINDOWSIZE           _T("RememberWindowSize")
#define IDS_RS_PANSCANZOOM                  _T("PanScanZoom")
#define IDS_RS_SUBDELAYINTERVAL             _T("SubDelayInterval")
#define IDS_RS_LOGOFILE                     _T("LogoFile")
#define IDS_RS_ENABLEWORKERTHREADFOROPENING _T("EnableWorkerThreadForOpening")
#define IDS_RS_PNSPRESETS                   _T("PnSPresets")
#define IDS_RS_AUTOLOADAUDIO                _T("AutoloadAudio")
#define IDS_RS_SUBTITLESLANGORDER           _T("SubtitlesLanguageOrder")
#define IDS_RS_AUDIOSLANGORDER              _T("AudiosLanguageOrder")
#define IDS_RS_BLOCKVSFILTER                _T("BlockVSFilter")
#define IDS_RS_BLOCKRDP                     _T("BlockRDP")
#define IDS_RS_ACCELTBL                     _T("AccelTbl")
#define IDS_RS_WINLIRCADDR                  _T("WinLircAddr")
#define IDS_RS_WINLIRC                      _T("UseWinLirc")
#define IDS_RS_TRAYICON                     _T("TrayIcon")
#define IDS_RS_KEEPASPECTRATIO              _T("KeepAspectRatio")
#define IDS_RS_UICEADDR                     _T("UICEAddr")
#define IDS_RS_UICE                         _T("UseUICE")
#define IDS_RS_JUMPDISTS                    _T("JumpDistS")
#define IDS_RS_JUMPDISTM                    _T("JumpDistM")
#define IDS_RS_JUMPDISTL                    _T("JumpDistL")
#define IDS_RS_REPORTFAILEDPINS             _T("ReportFailedPins")
#define IDS_RS_SRCFILTERS                   _T("SrcFilters")
#define IDS_RS_KEEPHISTORY                  _T("KeepHistory")
#define IDS_RS_RECENT_FILES_NUMBER          _T("RecentFilesNumber")
#define IDS_RS_LOGOID                       _T("LogoID2")
#define IDS_RS_LOGOEXT                      _T("LogoExt")
#define IDS_RS_LOGOCOLORPROFILE             _T("LogoColorProfile")
#define IDS_RS_TRAFILTERS                   _T("TraFilters")
#define IDS_RS_COMPMONDESKARDIFF            _T("CompMonDeskARDiff")
#define IDS_RS_HIDECDROMSSUBMENU            _T("HideCDROMsSubMenu")
#define IDS_RS_VMRTEXTURE                   _T("VMRTexture")
#define IDS_RS_VMR3D                        _T("VMR3D")
#define IDS_RS_DSVIDEORENDERERTYPE          _T("DSVidRen")
#define IDS_RS_SHUFFLEPLAYLISTITEMS         _T("ShufflePlaylistItems")
#define IDS_RS_REMEMBERPLAYLISTITEMS        _T("RememberPlaylistItems")
#define IDS_RS_HIDEPLAYLISTFULLSCREEN       _T("HidePlaylistFullScreen")
#define IDS_RS_APSURACEFUSAGE               _T("APSurfaceUsage")
#define IDS_RS_ENABLEWEBSERVER              _T("EnableWebServer")
#define IDS_RS_WEBSERVERPORT                _T("WebServerPort")
#define IDS_RS_LASTWINDOWTYPE               _T("LastWindowType")
#define IDS_RS_ONTOP                        _T("OnTop")
#define IDS_RS_WEBSERVERPRINTDEBUGINFO      _T("WebServerPrintDebugIfo")
#define IDS_RS_WEBSERVERUSECOMPRESSION      _T("WebServerUseCompression")
#define IDS_RS_WEBUI_ENABLE_PREVIEW         _T("WebUIEnablePreview")
#define IDS_RS_SNAPSHOTPATH                 _T("SnapshotPath")
#define IDS_RS_PRIORITY                     _T("Priority")
#define IDS_RS_SNAPSHOTEXT                  _T("SnapshotExt")
#define IDS_RS_SNAPSHOTSUBTITLES            _T("SnapshotSubtitles")
#define IDS_RS_SNAPSHOTKEEPVIDEOEXTENSION   _T("SnapshotKeepVideoExtension")
#define IDS_RS_LAUNCHFULLSCREEN             _T("LaunchFullScreen")
#define IDS_RS_WEBROOT                      _T("WebRoot")
#define IDS_RS_WEBSERVERLOCALHOSTONLY       _T("WebServerLocalhostOnly")
#define IDS_RS_ASPECTRATIO_X                _T("AspectRatioX")
#define IDS_RS_ASPECTRATIO_Y                _T("AspectRatioY")
#define IDS_RS_DX9_RESIZER                  _T("DX9Resizer")
#define IDS_RS_WEBSERVERCGI                 _T("WebServerCGI")
#define IDS_RS_WEBDEFINDEX                  _T("WebDefIndex")
#define IDS_RS_LIMITWINDOWPROPORTIONS       _T("LimitWindowProportions")
#define IDS_RS_LASTUSEDPAGE                 _T("LastUsedPage")
#define IDS_RS_VMR9MIXERMODE                _T("VMR9MixerMode")
#define IDS_RS_THUMBROWS                    _T("ThumbRows")
#define IDS_RS_THUMBCOLS                    _T("ThumbCols")
#define IDS_RS_ENABLEEDLEDITOR              _T("EnableEDLEditor")
#define IDS_RS_FULLSCREENMONITOR            _T("FullScreenMonitor")
#define IDS_RS_FULLSCREENMONITORDEVICE      _T("FullScreenMonitorDevice")
#define IDS_RS_PREVENT_MINIMIZE             _T("PreventMinimize")
#define IDS_RS_ENHANCED_TASKBAR             _T("UseWin7TaskBar")
#define IDS_RS_SEARCH_IN_FOLDER             _T("UseSearchInFolder")
#define IDS_RS_USE_TIME_TOOLTIP             _T("UseTimeTooltip")
#define IDS_RS_TIME_TOOLTIP_POSITION        _T("TimeTooltipPosition")
#define IDS_RS_MPC_OSD_SIZE                 _T("OSDSize")
#define IDS_RS_MPC_OSD_FONT                 _T("OSDFont")
#define IDS_RS_LAST_OPEN_DIR                _T("LastOpenDir")
#define IDS_RS_ASSOCIATED_WITH_ICON         _T("AssociatedWithIcon")
#define IDS_RS_ICON_LIB_VERSION             _T("IconLibVersion")
#define IDS_RS_OSD_TRANSPARENCY             _T("OSDTransparency")
#define IDS_RS_OSD_BORDER                   _T("OSDBorder")

#define IDS_RS_HIDE_FULLSCREEN_CONTROLS        _T("HideFullscreenControls")
#define IDS_RS_HIDE_FULLSCREEN_CONTROLS_POLICY _T("HideFullscreenControlsPolicy")
#define IDS_RS_HIDE_FULLSCREEN_CONTROLS_DELAY  _T("HideFullscreenControlsDelay")
#define IDS_RS_HIDE_FULLSCREEN_DOCKED_PANELS   _T("HideFullscreenDockedPanels")
#define IDS_RS_HIDE_WINDOWED_CONTROLS          _T("HideWindowedControls")

#define IDS_RS_HIDE_WINDOWED_MOUSE_POINTER  _T("HideWindowedMousePointer")

#define IDS_RS_FULLSCREEN_AUTOCHANGE_MODE_ENABLE               _T("Enable")
#define IDS_RS_FULLSCREEN_AUTOCHANGE_MODE_APPLYDEFMODEATFSEXIT _T("ApplyDefaultModeAtFSExit")
#define IDS_RS_FULLSCREEN_AUTOCHANGE_MODE_RESTORERESAFTEREXIT  _T("RestoreResAfterExit")
#define IDS_RS_FULLSCREEN_AUTOCHANGE_MODE_DELAY                _T("Delay")
#define IDS_RS_FULLSCREEN_AUTOCHANGE_MODE_MODE                 IDS_R_SETTINGS_FULLSCREEN_AUTOCHANGE_MODE _T("\\Mode%Iu")
#define IDS_RS_FULLSCREEN_AUTOCHANGE_MODE_MODE_CHECKED         _T("Checked")
#define IDS_RS_FULLSCREEN_AUTOCHANGE_MODE_MODE_FRAMERATESTART  _T("FrameRateStart")
#define IDS_RS_FULLSCREEN_AUTOCHANGE_MODE_MODE_FRAMERATESTOP   _T("FrameRateStop")
#define IDS_RS_FULLSCREEN_AUTOCHANGE_MODE_MODE_AUDIODELAY      _T("AudioDelay")
#define IDS_RS_FULLSCREEN_AUTOCHANGE_MODE_MODE_DM_BPP          _T("BPP")
#define IDS_RS_FULLSCREEN_AUTOCHANGE_MODE_MODE_DM_FREQ         _T("Freq")
#define IDS_RS_FULLSCREEN_AUTOCHANGE_MODE_MODE_DM_SIZEX        _T("SizeX")
#define IDS_RS_FULLSCREEN_AUTOCHANGE_MODE_MODE_DM_SIZEY        _T("SizeY")
#define IDS_RS_FULLSCREEN_AUTOCHANGE_MODE_MODE_DM_FLAGS        _T("Flags")

#define IDS_RS_DEFAULT_CAPTURE              _T("DefaultCapture")
#define IDS_R_CAPTURE                       _T("Capture")
#define IDS_RS_VIDEO_DISP_NAME              _T("VidDispName")
#define IDS_RS_AUDIO_DISP_NAME              _T("AudDispName")
#define IDS_RS_COUNTRY                      _T("Country")

#define IDS_R_DVB                           _T("DVBConfiguration")
//#define IDS_RS_BDA_NETWORKPROVIDER          _T("BDANetworkProvider")
#define IDS_RS_BDA_TUNER                    _T("BDATuner")
#define IDS_RS_BDA_RECEIVER                 _T("BDAReceiver")
#define IDS_RS_BDA_STANDARD                 _T("BDAStandard")
#define IDS_RS_BDA_SCAN_FREQ_START          _T("BDAScanFreqStart")
#define IDS_RS_BDA_SCAN_FREQ_END            _T("BDAScanFreqEnd")
#define IDS_RS_BDA_BANDWIDTH                _T("BDABandWidth")
#define IDS_RS_BDA_SYMBOLRATE               _T("BDASymbolRate")
#define IDS_RS_BDA_USE_OFFSET               _T("BDAUseOffset")
#define IDS_RS_BDA_OFFSET                   _T("BDAOffset")
#define IDS_RS_BDA_IGNORE_ENCRYPTED_CHANNELS _T("BDAIgnoreEncryptedChannels")
#define IDS_RS_DVB_LAST_CHANNEL             _T("LastChannel")
#define IDS_RS_DVB_REBUILD_FG               _T("RebuildFilterGraph")
#define IDS_RS_DVB_STOP_FG                  _T("StopFilterGraph")

#define IDS_RS_D3D9RENDERDEVICE             _T("D3D9RenderDevice")

#define IDS_RS_FASTSEEK                     _T("FastSeek")
#define IDS_RS_FASTSEEK_METHOD              _T("FastSeekMethod")
#define IDS_RS_SHOW_CHAPTERS                _T("ShowChapters")

#define IDS_RS_LCD_SUPPORT                  _T("LcdSupport")

#define IDS_RS_SEEKPREVIEW                    _T("UseSeekPreview")
#define IDS_RS_SEEKPREVIEW_SIZE               _T("SeekPreviewSize")

#define IDS_RS_TOGGLESHADER                 _T("PreSizeShadersEnabled")
#define IDS_RS_TOGGLESHADERSSCREENSPACE     _T("PostSizeShadersEnabled")

#define IDS_RS_REMAINING_TIME               _T("RemainingTime")
#define IDS_RS_HIGH_PRECISION_TIMER         _T("HighPrecisionTimer")
#define IDS_RS_TIMER_SHOW_PERCENTAGE        _T("TimerShowPercentage")

#define IDS_RS_UPDATER_AUTO_CHECK           _T("UpdaterAutoCheck")
#define IDS_RS_UPDATER_LAST_CHECK           _T("UpdaterLastCheck")
#define IDS_RS_UPDATER_DELAY                _T("UpdaterDelay")
#define IDS_RS_UPDATER_IGNORE_VERSION       _T("UpdaterIgnoreVersion")

#define IDS_RS_NOTIFY_SKYPE                 _T("NotifySkype")
#define IDS_RS_JPEG_QUALITY                 _T("JpegQuality")

#define IDS_RS_GOTO_LAST_USED               _T("GoToLastUsed")
#define IDS_RS_GOTO_FPS                     _T("GoToFPS")

#define IDS_R_DLG_SUBTITLEDL                _T("Dialogs\\SubtitleDl")
#define IDS_RS_DLG_SUBTITLEDL_COLWIDTH      _T("ColWidth")
#define IDS_RS_DLG_SUBTITLEDL_SORTCOLUMN    _T("SortColumn")
#define IDS_RS_DLG_SUBTITLEDL_SORTORDER     _T("SortOrder")

#define IDS_R_DLG_ORGANIZE_FAV              _T("Dialogs\\OrganizeFavorites")

#define IDS_R_DLG_CMD_LINE_HELP             _T("Dialogs\\CmdLineHelp")
#define IDS_R_DLG_RAR_ENTRY_SELECTOR        _T("Dialogs\\RarEntrySelector")

#define IDS_R_SHADERS                       _T("Shaders")
#define IDS_RS_SHADERS_EXTRA                _T("Extra")
#define IDS_RS_SHADERS_PRERESIZE            _T("PreResize")
#define IDS_RS_SHADERS_POSTRESIZE           _T("PostResize")
#define IDS_RS_SHADERS_LASTPRESET           _T("LastPreset")
#define IDS_R_SHADER_PRESETS                _T("Shaders\\Presets")
#define IDS_R_DEBUG_SHADERS                 _T("Dialogs\\DebugShaders")
#define IDS_RS_DEBUG_SHADERS_LASTVERSION    _T("LastVersion")
#define IDS_RS_DEBUG_SHADERS_LASTFILE       _T("LastFile")
#define IDS_RS_DEBUG_SHADERS_FIRSTRUN       _T("FirstRun")
#define IDS_RS_CACHESHADERS                 _T("CacheShaders")
#define IDS_R_SHADER_CACHE                  _T("ShaderCache")

#define IDS_RS_COVER_ART                    _T("EnableCoverArt")
#define IDS_RS_COVER_ART_SIZE_LIMIT         _T("CoverArtSizeLimit")
#define IDS_RS_LOGGING                      _T("EnableLogging")

#define IDS_RS_SUBTITLE_RENDERER            _T("SubtitleRenderer")

#define IDS_R_SANEAR                        IDS_R_INTERNAL_FILTERS _T("\\Audio Renderer")
#define IDS_RS_SANEAR_DEVICE_ID             _T("DeviceId")
#define IDS_RS_SANEAR_DEVICE_EXCLUSIVE      _T("DeviceExclusive")
#define IDS_RS_SANEAR_DEVICE_BUFFER         _T("DeviceBufferDuration")
#define IDS_RS_SANEAR_CROSSFEED_ENABLED     _T("CrossfeedEnabled")
#define IDS_RS_SANEAR_CROSSFEED_CUTOFF_FREQ _T("CrossfeedCutoffFrequency")
#define IDS_RS_SANEAR_CROSSFEED_LEVEL       _T("CrossfeedLevel")

#define IDS_RS_SANEAR_IGNORE_SYSTEM_MIXER   _T("IgnoreSystemChannelMixer")

#define IDS_RS_USE_YDL                      _T("UseYDL")
#define IDS_RS_YDL_MAX_HEIGHT               _T("YDLMaxHeight")
#define IDS_RS_YDL_VIDEO_FORMAT             _T("YDLVideoFormat")
#define IDS_RS_YDL_AUDIO_FORMAT             _T("YDLAudioFormat")
#define IDS_RS_YDL_AUDIO_ONLY               _T("YDLAudioOnly")
#define IDS_RS_YDL_EXEPATH                  _T("YDLExePath")
#define IDS_RS_YDL_COMMAND_LINE             _T("YDLCommandLine")

#define IDS_RS_ENABLE_CRASH_REPORTER        _T("EnableCrashReporter")

#define IDS_LAST_QUICKOPEN_PATH             _T("LastQuickOpenPath")
#define IDS_LAST_FILESAVECOPY_PATH          _T("LastFileSaveCopyPath")
#define IDS_LAST_FILEOPENDIR_PATH           _T("LastFileOpenDirPath")
#define IDS_EXTERNAL_PLAYLIST_PATH          _T("ExternalPlayListPath")

// Mouse
#define IDS_R_MOUSE                         L"Mouse"
#define IDS_RS_MOUSE_BTN_LEFT               L"ButtonLeft"
#define IDS_RS_MOUSE_BTN_LEFT_DBLCLICK      L"ButtonLeftDblClick"
#define IDS_RS_MOUSE_BTN_RIGHT              L"ButtonRight"
#define IDS_RS_MOUSE_BTN_MIDDLE             L"ButtonMiddle"
#define IDS_RS_MOUSE_BTN_X1                 L"ButtonX1"
#define IDS_RS_MOUSE_BTN_X2                 L"ButtonX2"
#define IDS_RS_MOUSE_WHEEL_UP               L"WheelUp"
#define IDS_RS_MOUSE_WHEEL_DOWN             L"WheelDown"
#define IDS_RS_MOUSE_WHEEL_LEFT             L"WheelLeft"
#define IDS_RS_MOUSE_WHEEL_RIGHT            L"WheelRight"
