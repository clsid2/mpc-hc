; (C) 2009-2018 see Authors.txt
;
; This file is part of MPC-HC.
;
; MPC-HC is free software; you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation; either version 3 of the License, or
; (at your option) any later version.
;
; MPC-HC is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program.  If not, see <http://www.gnu.org/licenses/>.


; Requirements:
; Inno Setup Unicode: http://www.jrsoftware.org/isdl.php


#if VER < EncodeVer(6,3,2)
  #error Update your Inno Setup version (6.3.2 or newer)
#endif

#ifndef UNICODE
  #error Use the Unicode Inno Setup
#endif

; If you want to compile the 64-bit version define "x64build" (uncomment the define below or use build.bat)
;#define x64Build
;#define MPCHC_LITE

; Include translations by default. You can bypass this by defining localize=whatever or false etc in build.bat or here
#if !defined(localize)
  #if defined(MPCHC_LITE)
    #define localize = "false"
  #else
    #define localize = "true"
  #endif
#endif

#if GetEnv('MPC_DRDUMP') == '1'
#define USE_DRDUMP_CRASH_REPORTER 1
#endif

; From now on you shouldn't need to change anything

#include "..\include\mpc-hc_config.h"
#include "..\include\version.h"

#define copyright_str   str(MPC_COPYRIGHT_STR)
#define app_name        "MPC-HC"

#if MPC_NIGHTLY_RELEASE
  #define app_ver       str(MPC_VERSION_MAJOR) + "." + str(MPC_VERSION_MINOR) + "." + str(MPC_VERSION_PATCH) + "." + str(MPC_VERSION_REV)
#else
  #define app_ver       str(MPC_VERSION_MAJOR) + "." + str(MPC_VERSION_MINOR) + "." + str(MPC_VERSION_PATCH)
#endif

#define app_vername     = app_name + " " + app_ver
#define quick_launch    "{userappdata}\Microsoft\Internet Explorer\Quick Launch"

#define base_bindir     = "..\bin"

#ifdef x64Build
  #define bindir        = AddBackslash(base_bindir) + "mpc-hc_x64"
  #define mpchc_exe     = "mpc-hc64.exe"
  #define mpchc_ini     = "mpc-hc64.ini"
  #define lavfiltersdir = "LAVFilters64"
  #define OutFilename   = app_name + "." + app_ver + ".x64"
  #define platform      = "x64"
  #define mpcvr_ax      = "MpcVideoRenderer64.ax"
  #define mediainfo_dll = "..\distrib\x64\MediaInfo.dll"
#else
  #define bindir        = AddBackslash(base_bindir) + "mpc-hc_x86"
  #define mpchc_exe     = "mpc-hc.exe"
  #define mpchc_ini     = "mpc-hc.ini"
  #define lavfiltersdir = "LAVFilters"
  #define OutFilename   = app_name + "." + app_ver + ".x86"
  #define platform      = "x86"
  #define mpcvr_ax      = "MpcVideoRenderer.ax"
  #define mediainfo_dll = "..\distrib\x86\MediaInfo.dll"
#endif

#if defined(MPCHC_LITE)
  #define bindir        = bindir + " Lite"
#endif

#ifnexist AddBackslash(bindir) + mpchc_exe
  #error Compile MPC-HC first
#endif

#if localize != "true"
  #if defined(MPCHC_LITE)
    #define OutFilename  = OutFilename + ".Lite"
  #else
    #define OutFilename  = OutFilename + ".en"
  #endif
#endif

#if MPC_NIGHTLY_RELEASE
  #define FullAppNameVer = app_vername + " " + "(" + str(MPCHC_HASH) + ")"
#else
  #define FullAppNameVer = app_vername
#endif

#if MPC_NIGHTLY_RELEASE
  #define FullAppNameVer = FullAppNameVer + " " + str(MPC_VERSION_NIGHTLY)
#endif
#ifdef MPCHC_LITE
  #define FullAppNameVer = FullAppNameVer + " " + "Lite"
#endif
#ifdef x64Build
  #define FullAppNameVer = FullAppNameVer + " " + "(64-bit)"
#endif

#ifexist "..\distrib\mpcvr\MpcVideoRenderer.ax"
#define INCLUDE_MPCVR = true
#else
#define INCLUDE_MPCVR = false
#endif

#ifexist mediainfo_dll
	#if !defined(MPCHC_LITE)
#define INCLUDE_MEDIAINFO = true
	#else
#define INCLUDE_MEDIAINFO = false
	#endif
#else
#define INCLUDE_MEDIAINFO = false
#endif


[Setup]
#ifdef x64Build
AppId                     = {{2ACBF1FA-F5C3-4B19-A774-B22A31F231B9}
DefaultGroupName          = {#app_name} x64
ArchitecturesAllowed      = x64compatible
ArchitecturesInstallIn64BitMode = x64compatible
#else
AppId                     = {{2624B969-7135-4EB1-B0F6-2D8C397B45F7}
DefaultGroupName          = {#app_name}
#endif

AppName                   = {#app_name}
AppVersion                = {#app_ver}
AppVerName                = {#app_vername}
AppPublisher              = MPC-HC Team
AppPublisherURL           = {#WEBSITE_URL}
AppCopyright              = {#copyright_str}
VersionInfoVersion        = {#app_ver}
UninstallDisplayIcon      = {app}\{#mpchc_exe}
UninstallDisplayName      = {#FullAppNameVer}
OutputBaseFilename        = {#OutFilename}
DefaultDirName            = {code:GetInstallFolder}
LicenseFile               = ..\COPYING.txt
OutputDir                 = .
SetupIconFile             = ..\src\mpc-hc\res\icon.ico
WizardImageFile           = WizardImageFile.bmp
WizardSmallImageFile      = WizardSmallImageFile.bmp
Compression               = lzma2/ultra
InternalCompressLevel     = ultra
SolidCompression          = yes
AllowNoIcons              = yes
ShowTasksTreeLines        = yes
DisableDirPage            = auto
DisableProgramGroupPage   = auto
MinVersion                = 6.1
CloseApplications         = true
#ifexist "..\signinfo.txt"
SignTool                  = MySignTool
#endif
SetupMutex                = 'mpchc_setup_mutex'

[Languages]
Name: en;    MessagesFile: compiler:Default.isl

#if localize == "true"
Name: ar;    MessagesFile: Languages\Arabic.isl
Name: be;    MessagesFile: Languages\Belarusian.isl
Name: bn;    MessagesFile: Languages\Bengali.islu
Name: bs_BA; MessagesFile: Languages\Bosnian.isl
Name: ca;    MessagesFile: compiler:Languages\Catalan.isl
Name: cs;    MessagesFile: compiler:Languages\Czech.isl
Name: da;    MessagesFile: compiler:Languages\Danish.isl
Name: de;    MessagesFile: compiler:Languages\German.isl
Name: en_GB; MessagesFile: Languages\EnglishBritish.isl
Name: es;    MessagesFile: compiler:Languages\Spanish.isl
Name: eu;    MessagesFile: Languages\Basque.isl
Name: fi;    MessagesFile: compiler:Languages\Finnish.isl
Name: fr;    MessagesFile: compiler:Languages\French.isl
Name: gl;    MessagesFile: Languages\Galician.isl
Name: he;    MessagesFile: compiler:Languages\Hebrew.isl
Name: hr;    MessagesFile: Languages\Croatian.isl
Name: hu;    MessagesFile: compiler:Languages\Hungarian.isl
Name: hy;    MessagesFile: compiler:Languages\Armenian.isl
Name: id;    MessagesFile: Languages\Indonesian.isl
Name: it;    MessagesFile: compiler:Languages\Italian.isl
Name: ja;    MessagesFile: compiler:Languages\Japanese.isl
Name: ko;    MessagesFile: Languages\Korean.isl
Name: lt;    MessagesFile: Languages\Lithuanian.isl
Name: ms_MY; MessagesFile: Languages\Malaysian.isl
Name: nl;    MessagesFile: compiler:Languages\Dutch.isl
Name: pl;    MessagesFile: compiler:Languages\Polish.isl
Name: pt_BR; MessagesFile: compiler:Languages\BrazilianPortuguese.isl
Name: pt_PT; MessagesFile: compiler:Languages\Portuguese.isl
Name: ro;    MessagesFile: Languages\Romanian.isl
Name: ru;    MessagesFile: compiler:Languages\Russian.isl
Name: sk;    MessagesFile: Languages\Slovak.isl
Name: sl;    MessagesFile: compiler:Languages\Slovenian.isl
Name: sv;    MessagesFile: Languages\Swedish.isl
Name: th_TH; MessagesFile: Languages\Thai.isl
Name: tt;    MessagesFile: Languages\Tatar.isl
Name: tr;    MessagesFile: compiler:Languages\Turkish.isl
Name: uk;    MessagesFile: compiler:Languages\Ukrainian.isl
Name: vi;    MessagesFile: Languages\Vietnamese.isl
Name: zh_CN; MessagesFile: Languages\ChineseSimplified.isl
Name: zh_TW; MessagesFile: Languages\ChineseTraditional.isl
#endif

; Include installer's custom messages
#include "custom_messages.iss"


[Types]
Name: default;            Description: {cm:types_DefaultInstallation}
Name: custom;             Description: {cm:types_CustomInstallation};                     Flags: iscustom


[Components]
Name: main;               Description: {#app_vername};             Types: default custom; Flags: fixed
Name: mpciconlib;         Description: {cm:comp_mpciconlib};       Types: default custom
#if localize == "true"
Name: mpcresources;       Description: {cm:comp_mpcresources};     Types: default custom; Flags: disablenouninstallwarning
#endif


[Tasks]
Name: desktopicon;        Description: {cm:CreateDesktopIcon};     GroupDescription: {cm:AdditionalIcons}
Name: desktopicon\user;   Description: {cm:tsk_CurrentUser};       GroupDescription: {cm:AdditionalIcons}; Flags: exclusive
Name: desktopicon\common; Description: {cm:tsk_AllUsers};          GroupDescription: {cm:AdditionalIcons}; Flags: unchecked exclusive
Name: quicklaunchicon;    Description: {cm:CreateQuickLaunchIcon}; GroupDescription: {cm:AdditionalIcons}; Flags: unchecked;             OnlyBelowVersion: 6.01
Name: reset_settings;     Description: {cm:tsk_ResetSettings};     GroupDescription: {cm:tsk_Other};       Flags: checkedonce unchecked; Check: SettingsExist()


[Files]
Source: {#bindir}\{#mpchc_exe};                    DestDir: {app};                  Components: main;         Flags: ignoreversion
	#if localize == "true"
Source: {#bindir}\Lang\mpcresources.??.dll;        DestDir: {app}\Lang;             Components: mpcresources; Flags: ignoreversion
Source: {#bindir}\Lang\mpcresources.??_??.dll;     DestDir: {app}\Lang;             Components: mpcresources; Flags: ignoreversion
	#endif
Source: {#bindir}\mpciconlib.dll;                  DestDir: {app};                  Components: mpciconlib;   Flags: ignoreversion
	#ifndef MPCHC_LITE
Source: {#bindir}\{#lavfiltersdir}\*.dll;          DestDir: {app}\{#lavfiltersdir}; Components: main;         Flags: ignoreversion
Source: {#bindir}\{#lavfiltersdir}\*.ax;           DestDir: {app}\{#lavfiltersdir}; Components: main;         Flags: ignoreversion
Source: {#bindir}\{#lavfiltersdir}\*.manifest;     DestDir: {app}\{#lavfiltersdir}; Components: main;         Flags: ignoreversion
	#endif
	#if INCLUDE_MPCVR
Source: ..\distrib\mpcvr\{#mpcvr_ax};              DestDir: {app}\MPCVR;            Components: main;         Flags: ignoreversion
	#endif
Source: {#platform}\d3dcompiler_{#MPC_D3D_COMPILER_VERSION}.dll; DestDir: {app};    Components: main;         Flags: ignoreversion
Source: {#platform}\d3dx9_{#MPC_DX_SDK_NUMBER}.dll;              DestDir: {app};    Components: main;         Flags: ignoreversion
	#if INCLUDE_MEDIAINFO
Source: {#platform}\mediainfo.dll;                 DestDir: {app};                  Components: main;         Flags: ignoreversion
	#endif
Source: ..\src\mpc-hc\res\shaders\dx9\*.hlsl;      DestDir: {app}\Shaders;          Components: main;         Flags: onlyifdoesntexist
Source: ..\src\mpc-hc\res\shaders\dx11\*.hlsl;     DestDir: {app}\Shaders11;        Components: main;         Flags: onlyifdoesntexist
Source: ..\distrib\Toolbars\*.*;                   DestDir: {app}\Toolbars;         Components: main;         Flags: onlyifdoesntexist recursesubdirs
Source: ..\COPYING.txt;                            DestDir: {app};                  Components: main;         Flags: ignoreversion
Source: ..\docs\Authors.txt;                       DestDir: {app};                  Components: main;         Flags: ignoreversion
	#if USE_DRDUMP_CRASH_REPORTER
Source: {#platform}\crashrpt.dll;                  DestDir: {app}\CrashReporter;    Components: main;         Flags: ignoreversion
Source: {#platform}\dbghelp.dll;                   DestDir: {app}\CrashReporter;    Components: main;         Flags: ignoreversion
Source: {#platform}\sendrpt.exe;                   DestDir: {app}\CrashReporter;    Components: main;         Flags: ignoreversion
Source: CrashReporter_LICENSE.txt;                 DestDir: {app}\CrashReporter;    Components: main;         Flags: ignoreversion
	#endif


[Icons]
#ifdef x64Build
Name: {group}\{#app_name} x64;                   Filename: {app}\{#mpchc_exe}; Comment: {#app_vername} (64-bit); WorkingDir: {app}; IconFilename: {app}\{#mpchc_exe}; IconIndex: 0
Name: {commondesktop}\{#app_name} x64;           Filename: {app}\{#mpchc_exe}; Comment: {#app_vername} (64-bit); WorkingDir: {app}; IconFilename: {app}\{#mpchc_exe}; IconIndex: 0; Tasks: desktopicon\common
Name: {userdesktop}\{#app_name} x64;             Filename: {app}\{#mpchc_exe}; Comment: {#app_vername} (64-bit); WorkingDir: {app}; IconFilename: {app}\{#mpchc_exe}; IconIndex: 0; Tasks: desktopicon\user
Name: {#quick_launch}\{#app_name} x64;           Filename: {app}\{#mpchc_exe}; Comment: {#app_vername} (64-bit); WorkingDir: {app}; IconFilename: {app}\{#mpchc_exe}; IconIndex: 0; Tasks: quicklaunchicon
#else
Name: {group}\{#app_name};                       Filename: {app}\{#mpchc_exe}; Comment: {#app_vername}; WorkingDir: {app}; IconFilename: {app}\{#mpchc_exe}; IconIndex: 0
Name: {commondesktop}\{#app_name};               Filename: {app}\{#mpchc_exe}; Comment: {#app_vername}; WorkingDir: {app}; IconFilename: {app}\{#mpchc_exe}; IconIndex: 0; Tasks: desktopicon\common
Name: {userdesktop}\{#app_name};                 Filename: {app}\{#mpchc_exe}; Comment: {#app_vername}; WorkingDir: {app}; IconFilename: {app}\{#mpchc_exe}; IconIndex: 0; Tasks: desktopicon\user
Name: {#quick_launch}\{#app_name};               Filename: {app}\{#mpchc_exe}; Comment: {#app_vername}; WorkingDir: {app}; IconFilename: {app}\{#mpchc_exe}; IconIndex: 0; Tasks: quicklaunchicon
#endif
Name: {group}\{cm:UninstallProgram,{#app_name}}; Filename: {uninstallexe};      Comment: {cm:UninstallProgram,{#app_name}}; WorkingDir: {app}


[Run]
Filename: {app}\{#mpchc_exe};                    Description: {cm:LaunchProgram,{#app_name}}; WorkingDir: {app}; Flags: nowait postinstall skipifsilent unchecked


[InstallDelete]
Type: files; Name: {userdesktop}\{#app_name}.lnk;   Check: not IsTaskSelected('desktopicon\user')   and IsUpgrade()
Type: files; Name: {commondesktop}\{#app_name}.lnk; Check: not IsTaskSelected('desktopicon\common') and IsUpgrade()
Type: files; Name: {#quick_launch}\{#app_name}.lnk; Check: not IsTaskSelected('quicklaunchicon')    and IsUpgrade(); OnlyBelowVersion: 6.01
Type: files; Name: {app}\AUTHORS;                   Check: IsUpgrade()
Type: files; Name: {app}\COPYING;                   Check: IsUpgrade()
	#if !USE_DRDUMP_CRASH_REPORTER
Type: filesandordirs; Name: {app}\CrashReporter;    Check: IsUpgrade()
	#endif

; old shortcuts
#ifdef x64Build
Type: files; Name: {group}\Media Player Classic - Home Cinema x64.lnk;                   Check: IsUpgrade()
Type: files; Name: {commondesktop}\Media Player Classic - Home Cinema x64.lnk;           Check: IsUpgrade()
Type: files; Name: {userdesktop}\Media Player Classic - Home Cinema x64.lnk;             Check: IsUpgrade()
Type: files; Name: {#quick_launch}\Media Player Classic - Home Cinema x64.lnk;           Check: IsUpgrade()
#else
Type: files; Name: {group}\Media Player Classic - Home Cinema.lnk;                       Check: IsUpgrade()
Type: files; Name: {commondesktop}\Media Player Classic - Home Cinema.lnk;               Check: IsUpgrade()
Type: files; Name: {userdesktop}\Media Player Classic - Home Cinema.lnk;                 Check: IsUpgrade()
Type: files; Name: {#quick_launch}\Media Player Classic - Home Cinema.lnk;               Check: IsUpgrade()
#endif
Type: files; Name: {group}\{cm:ProgramOnTheWeb,Media Player Classic - Home Cinema}.url;  Check: IsUpgrade()
Type: files; Name: {group}\{cm:UninstallProgram,Media Player Classic - Home Cinema}.lnk; Check: IsUpgrade()

Type: files; Name: {userdesktop}\Media Player Classic - Home Cinema.lnk;   Check: not IsTaskSelected('desktopicon\user')   and IsUpgrade()
Type: files; Name: {commondesktop}\Media Player Classic - Home Cinema.lnk; Check: not IsTaskSelected('desktopicon\common') and IsUpgrade()
Type: files; Name: {#quick_launch}\Media Player Classic - Home Cinema.lnk; Check: not IsTaskSelected('quicklaunchicon')    and IsUpgrade(); OnlyBelowVersion: 6.01

; Old ffmpeg dlls from LAV Filters
Type: files; Name: {app}\{#lavfiltersdir}\avcodec-lav-61.dll;   Check: IsUpgrade()
Type: files; Name: {app}\{#lavfiltersdir}\avcodec-lav-60.dll;   Check: IsUpgrade()
Type: files; Name: {app}\{#lavfiltersdir}\avcodec-lav-59.dll;   Check: IsUpgrade()
Type: files; Name: {app}\{#lavfiltersdir}\avcodec-lav-58.dll;   Check: IsUpgrade()
Type: files; Name: {app}\{#lavfiltersdir}\avcodec-lav-57.dll;   Check: IsUpgrade()
Type: files; Name: {app}\{#lavfiltersdir}\avfilter-lav-10.dll;   Check: IsUpgrade()
Type: files; Name: {app}\{#lavfiltersdir}\avfilter-lav-9.dll;   Check: IsUpgrade()
Type: files; Name: {app}\{#lavfiltersdir}\avfilter-lav-8.dll;   Check: IsUpgrade()
Type: files; Name: {app}\{#lavfiltersdir}\avfilter-lav-7.dll;   Check: IsUpgrade()
Type: files; Name: {app}\{#lavfiltersdir}\avformat-lav-61.dll;  Check: IsUpgrade()
Type: files; Name: {app}\{#lavfiltersdir}\avformat-lav-60.dll;  Check: IsUpgrade()
Type: files; Name: {app}\{#lavfiltersdir}\avformat-lav-59.dll;  Check: IsUpgrade()
Type: files; Name: {app}\{#lavfiltersdir}\avformat-lav-58.dll;  Check: IsUpgrade()
Type: files; Name: {app}\{#lavfiltersdir}\avformat-lav-57.dll;  Check: IsUpgrade()
Type: files; Name: {app}\{#lavfiltersdir}\avutil-lav-59.dll;    Check: IsUpgrade()
Type: files; Name: {app}\{#lavfiltersdir}\avutil-lav-58.dll;    Check: IsUpgrade()
Type: files; Name: {app}\{#lavfiltersdir}\avutil-lav-57.dll;    Check: IsUpgrade()
Type: files; Name: {app}\{#lavfiltersdir}\avutil-lav-56.dll;    Check: IsUpgrade()
Type: files; Name: {app}\{#lavfiltersdir}\avutil-lav-55.dll;    Check: IsUpgrade()
Type: files; Name: {app}\{#lavfiltersdir}\swresample-lav-5.dll; Check: IsUpgrade()
Type: files; Name: {app}\{#lavfiltersdir}\swresample-lav-4.dll; Check: IsUpgrade()
Type: files; Name: {app}\{#lavfiltersdir}\swscale-lav-8.dll;    Check: IsUpgrade()
Type: files; Name: {app}\{#lavfiltersdir}\swscale-lav-7.dll;    Check: IsUpgrade()
Type: files; Name: {app}\{#lavfiltersdir}\swscale-lav-6.dll;    Check: IsUpgrade()
Type: files; Name: {app}\{#lavfiltersdir}\swscale-lav-5.dll;    Check: IsUpgrade()
Type: files; Name: {app}\{#lavfiltersdir}\swscale-lav-4.dll;    Check: IsUpgrade()
Type: files; Name: {app}\{#lavfiltersdir}\avresample-lav-4.dll; Check: IsUpgrade()
Type: files; Name: {app}\{#lavfiltersdir}\avresample-lav-3.dll; Check: IsUpgrade()

#ifdef x64Build
; Super old LAV files
Type: files; Name: {app}\LAVFilters\avcodec-lav-??.dll;                    Check: IsUpgrade()
Type: files; Name: {app}\LAVFilters\avfilter-lav-?.dll;                    Check: IsUpgrade()
Type: files; Name: {app}\LAVFilters\avformat-lav-??.dll;                   Check: IsUpgrade()
Type: files; Name: {app}\LAVFilters\avresample-lav-?.dll;                  Check: IsUpgrade()
Type: files; Name: {app}\LAVFilters\avutil-lav-??.dll;                     Check: IsUpgrade()
Type: files; Name: {app}\LAVFilters\IntelQuickSyncDecoder.dll;             Check: IsUpgrade()
Type: files; Name: {app}\LAVFilters\LAVAudio.ax;                           Check: IsUpgrade()
Type: files; Name: {app}\LAVFilters\LAVFilters.Dependencies.manifest;      Check: IsUpgrade()
Type: files; Name: {app}\LAVFilters\LAVSplitter.ax;                        Check: IsUpgrade()
Type: files; Name: {app}\LAVFilters\LAVVideo.ax;                           Check: IsUpgrade()
Type: files; Name: {app}\LAVFilters\libbluray.dll;                         Check: IsUpgrade()
Type: files; Name: {app}\LAVFilters\swscale-lav-?.dll;                     Check: IsUpgrade()
Type: dirifempty; Name: {app}\LAVFilters\;                                 Check: IsUpgrade()
#endif


#if localize == "true"
; remove the old language dlls when upgrading
Type: files; Name: {app}\mpcresources.br.dll
Type: files; Name: {app}\mpcresources.by.dll
Type: files; Name: {app}\mpcresources.ca.dll
Type: files; Name: {app}\mpcresources.cz.dll
Type: files; Name: {app}\mpcresources.de.dll
Type: files; Name: {app}\mpcresources.es.dll
Type: files; Name: {app}\mpcresources.fr.dll
Type: files; Name: {app}\mpcresources.he.dll
Type: files; Name: {app}\mpcresources.hu.dll
Type: files; Name: {app}\mpcresources.hy.dll
Type: files; Name: {app}\mpcresources.it.dll
Type: files; Name: {app}\mpcresources.ja.dll
Type: files; Name: {app}\mpcresources.kr.dll
Type: files; Name: {app}\mpcresources.nl.dll
Type: files; Name: {app}\mpcresources.pl.dll
Type: files; Name: {app}\mpcresources.ru.dll
Type: files; Name: {app}\mpcresources.sc.dll
Type: files; Name: {app}\mpcresources.sk.dll
Type: files; Name: {app}\mpcresources.sv.dll
Type: files; Name: {app}\mpcresources.tc.dll
Type: files; Name: {app}\mpcresources.tr.dll
Type: files; Name: {app}\mpcresources.ua.dll
Type: files; Name: {app}\Lang\mpcresources.br.dll
Type: files; Name: {app}\Lang\mpcresources.by.dll
Type: files; Name: {app}\Lang\mpcresources.cz.dll
Type: files; Name: {app}\Lang\mpcresources.en-GB.dll
Type: files; Name: {app}\Lang\mpcresources.kr.dll
Type: files; Name: {app}\Lang\mpcresources.sc.dll
Type: files; Name: {app}\Lang\mpcresources.tc.dll
Type: files; Name: {app}\Lang\mpcresources.ua.dll
#endif


[Code]
function IsProcessorFeaturePresent(Feature: Integer): Boolean;
external 'IsProcessorFeaturePresent@kernel32.dll stdcall';

function GetInstallFolder(Default: String): String;
var
  sInstallPath: String;
begin
  if not RegQueryStringValue(HKCU, 'SOFTWARE\Gabest\Media Player Classic', 'ExePath', sInstallPath)
  or not RegQueryStringValue(HKCU, 'SOFTWARE\MPC-HC\MPC-HC', 'ExePath', sInstallPath) then begin
    Result := ExpandConstant('{pf}\MPC-HC');
  end
  else begin
    RegQueryStringValue(HKCU, 'SOFTWARE\MPC-HC\MPC-HC', 'ExePath', sInstallPath);
    Result := ExtractFileDir(sInstallPath);
    if (Result = '') or not DirExists(Result) then begin
      Result := ExpandConstant('{pf}\MPC-HC');
    end;
  end;
end;

function Is_SSE2_Supported(): Boolean;
begin
  // PF_XMMI64_INSTRUCTIONS_AVAILABLE
  Result := IsProcessorFeaturePresent(10);
end;

function IsUpgrade(): Boolean;
var
  sPrevPath: String;
begin
  sPrevPath := WizardForm.PrevAppDir;
  Result := (sPrevPath <> '');
end;


// Check if MPC-HC's settings exist
function SettingsExist(): Boolean;
begin
  if RegKeyExists(HKEY_CURRENT_USER, 'Software\Gabest\Media Player Classic') or
  RegKeyExists(HKEY_CURRENT_USER, 'Software\MPC-HC\MPC-HC') or
  FileExists(ExpandConstant('{app}\{#mpchc_ini}')) then
    Result := True
  else
    Result := False;
end;


function ShouldSkipPage(PageID: Integer): Boolean;
begin
  // Hide the License page
  if IsUpgrade() and (PageID = wpLicense) then
    Result := True;
end;


procedure CleanUpSettingsAndFiles();
var
  ResultCode: Integer;
begin
  try
    Exec(ExpandConstant('{app}\{#mpchc_exe}'), '/unregall', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  except
  end;
  DeleteFile(ExpandConstant('{app}\{#mpchc_ini}'));
  DelTree(ExpandConstant('{userappdata}\MPC-HC\ShaderCache'), True, True, True);  
  DeleteFile(ExpandConstant('{userappdata}\MPC-HC\default.mpcpl'));
  RemoveDir(ExpandConstant('{userappdata}\MPC-HC'));
  RegDeleteKeyIncludingSubkeys(HKCU, 'Software\MPC-HC\Filters');
  RegDeleteKeyIncludingSubkeys(HKCU, 'Software\MPC-HC\MPC-HC');
  RegDeleteKeyIfEmpty(HKCU, 'Software\MPC-HC');

  #if INCLUDE_MPCVR
  RegDeleteKeyIncludingSubkeys(HKCU, 'Software\MPC-BE Filters\MPC Video Renderer');
  RegDeleteKeyIfEmpty(HKCU, 'Software\MPC-BE Filters');  
  #endif
end;


procedure CleanUpOldSettingsAndFiles();
begin
  DeleteFile(ExpandConstant('{userappdata}\Media Player Classic\default.mpcpl'));
  RemoveDir(ExpandConstant('{userappdata}\Media Player Classic'));
  RegDeleteKeyIncludingSubkeys(HKCU, 'Software\Gabest\Filters');
  RegDeleteKeyIncludingSubkeys(HKCU, 'Software\Gabest\Media Player Classic');
  RegDeleteKeyIfEmpty(HKCU, 'Software\Gabest');
  RegDeleteValue(HKLM, 'SOFTWARE\Gabest\Media Player Classic', 'ExePath')
  RegDeleteKeyIfEmpty(HKLM, 'SOFTWARE\Gabest\Media Player Classic');
  RegDeleteKeyIfEmpty(HKLM, 'SOFTWARE\Gabest');
end;


procedure InitializeWizard();
begin
  WizardForm.LicenseAcceptedRadio.Checked := True;
end;


procedure CurStepChanged(CurStep: TSetupStep);
var
  iLanguage: Integer;
begin
  if CurStep = ssPostInstall then begin
    if IsTaskSelected('reset_settings') then begin
      CleanUpSettingsAndFiles();
      RegWriteStringValue(HKCU, 'Software\MPC-HC\MPC-HC', 'ExePath', ExpandConstant('{app}\{#mpchc_exe}'));
    end;

    iLanguage := StrToInt(ExpandConstant('{cm:langid}'));
    if IsComponentSelected('mpcresources') then begin
      if FileExists(ExpandConstant('{app}\{#mpchc_ini}')) then
        SetIniInt('Settings', 'InterfaceLanguage', iLanguage, ExpandConstant('{app}\{#mpchc_ini}'))
      else
        RegWriteDWordValue(HKCU, 'Software\MPC-HC\MPC-HC\Settings', 'InterfaceLanguage', iLanguage);
    end;
  end;

end;


procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  // When uninstalling, ask the user to delete MPC-HC settings
  if (CurUninstallStep = usUninstall) and SettingsExist() then begin
    if SuppressibleMsgBox(CustomMessage('msg_DeleteSettings'), mbConfirmation, MB_YESNO or MB_DEFBUTTON2, IDNO) = IDYES then begin
      CleanUpSettingsAndFiles();
      CleanUpOldSettingsAndFiles();
    end;

    RegDeleteValue(HKLM, 'SOFTWARE\MPC-HC\MPC-HC', 'ExePath')
    RegDeleteKeyIfEmpty(HKLM, 'SOFTWARE\MPC-HC\MPC-HC');
    RegDeleteKeyIfEmpty(HKLM, 'SOFTWARE\MPC-HC');

  end;
end;


function InitializeSetup(): Boolean;
begin
    Result := True;

    if not Is_SSE2_Supported() then begin
      SuppressibleMsgBox(CustomMessage('msg_simd_sse2'), mbCriticalError, MB_OK, MB_OK);
      Result := False;
    end;
end;
