@ECHO OFF
REM (C) 2009-2019 see Authors.txt
REM
REM This file is part of MPC-HC.
REM
REM MPC-HC is free software; you can redistribute it and/or modify
REM it under the terms of the GNU General Public License as published by
REM the Free Software Foundation; either version 3 of the License, or
REM (at your option) any later version.
REM
REM MPC-HC is distributed in the hope that it will be useful,
REM but WITHOUT ANY WARRANTY; without even the implied warranty of
REM MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
REM GNU General Public License for more details.
REM
REM You should have received a copy of the GNU General Public License
REM along with this program.  If not, see <http://www.gnu.org/licenses/>.


SETLOCAL EnableDelayedExpansion

SET ARG=/%*
SET ARG=%ARG:/=%
SET ARG=%ARG:-=%
SET ARGB=0
SET ARGBC=0
SET ARGC=0
SET ARGPL=0
SET INPUT=0
SET VALID=0

IF /I "%ARG%" == "?"            GOTO ShowHelp

FOR %%G IN (%ARG%) DO (
  IF /I "%%G" == "help"         GOTO ShowHelp
  IF /I "%%G" == "GetVersion"   ENDLOCAL & SET "FORCE_VER_UPDATE=True" & CALL "%~dp0common.bat" :SubGetVersion & EXIT /B
  IF /I "%%G" == "Build"        SET "BUILDTYPE=Build"    & SET /A ARGB+=1
  IF /I "%%G" == "Clean"        SET "BUILDTYPE=Clean"    & SET /A ARGB+=1  & SET "NO_INST=True" & SET /A "NO_ZIP=True" & SET "NO_LAV=True"
  IF /I "%%G" == "Rebuild"      SET "BUILDTYPE=Rebuild"  & SET /A ARGB+=1  & SET "NO_LAV=True"
  IF /I "%%G" == "Both"         SET "PPLATFORM=Both"     & SET /A ARGPL+=1
  IF /I "%%G" == "Win32"        SET "PPLATFORM=Win32"    & SET /A ARGPL+=1
  IF /I "%%G" == "x86"          SET "PPLATFORM=Win32"    & SET /A ARGPL+=1
  IF /I "%%G" == "x64"          SET "PPLATFORM=x64"      & SET /A ARGPL+=1
  IF /I "%%G" == "All"          SET "CONFIG=All"         & SET /A ARGC+=1  & SET "NO_LITE=True"
  IF /I "%%G" == "Main"         SET "CONFIG=Main"        & SET /A ARGC+=1  & SET "NO_INST=True" & SET "NO_ZIP=True"
  IF /I "%%G" == "Filters"      SET "CONFIG=Filters"     & SET /A ARGC+=1  & SET "NO_INST=True" & SET "NO_LITE=True"
  IF /I "%%G" == "Filter"       SET "CONFIG=Filters"     & SET /A ARGC+=1  & SET "NO_INST=True" & SET "NO_LITE=True"
  IF /I "%%G" == "API"          SET "CONFIG=API"         & SET /A ARGC+=1  & SET "NO_INST=True" & SET "NO_ZIP=True" & SET "NO_LITE=True"
  IF /I "%%G" == "MPCHC"        SET "CONFIG=MPCHC"       & SET /A ARGC+=1
  IF /I "%%G" == "MPC-HC"       SET "CONFIG=MPCHC"       & SET /A ARGC+=1
  IF /I "%%G" == "Resources"    SET "CONFIG=Resources"   & SET /A ARGC+=1  & SET "NO_INST=True" & SET "NO_ZIP=True" & SET "NO_LITE=True"
  IF /I "%%G" == "MPCIconLib"   SET "CONFIG=IconLib"     & SET /A ARGC+=1  & SET "NO_INST=True" & SET "NO_ZIP=True" & SET "NO_LITE=True"
  IF /I "%%G" == "IconLib"      SET "CONFIG=IconLib"     & SET /A ARGC+=1  & SET "NO_INST=True" & SET "NO_ZIP=True" & SET "NO_LITE=True"
  IF /I "%%G" == "Translations" SET "CONFIG=Translation" & SET /A ARGC+=1  & SET "NO_INST=True" & SET "NO_ZIP=True" & SET "NO_LITE=True"
  IF /I "%%G" == "Debug"        SET "BUILDCFG=Debug"     & SET /A ARGBC+=1 & SET "NO_INST=True"
  IF /I "%%G" == "Release"      SET "BUILDCFG=Release"   & SET /A ARGBC+=1
  IF /I "%%G" == "Packages"     SET "PACKAGES=True"      & SET /A VALID+=1
  IF /I "%%G" == "Installer"    SET "INSTALLER=True"     & SET /A VALID+=1
  IF /I "%%G" == "7z"           SET "ZIP=True"           & SET /A VALID+=1
  IF /I "%%G" == "Lite"         SET "MPCHC_LITE=True"    & SET /A VALID+=1
  IF /I "%%G" == "LAVFilters"   SET "CLEAN=LAVFilters"   & SET /A VALID+=1
  IF /I "%%G" == "Silent"       SET "SILENT=True"        & SET /A VALID+=1
  IF /I "%%G" == "Nocolors"     SET "NOCOLORS=True"      & SET /A VALID+=1
  IF /I "%%G" == "Analyze"      SET "ANALYZE=True"       & SET /A VALID+=1
  IF /I "%%G" == "MINGWLIB"     ENDLOCAL & SET "FORCE_MINGW_UPDATE=True" & CALL "%~dp0common.bat" :SubMINGWLIB & EXIT /B
)

SET "FILE_DIR=%~dp0"
PUSHD "%FILE_DIR%"

SET "COMMON=%FILE_DIR%\common.bat"

CALL "%COMMON%" :SubPreBuild
IF %ERRORLEVEL% NEQ 0 GOTO MissingVar

FOR %%G IN (%*) DO SET /A INPUT+=1
SET /A VALID+=%ARGB%+%ARGPL%+%ARGC%+%ARGBC%
IF %VALID% NEQ %INPUT% GOTO UnsupportedSwitch

IF %ARGB%    GTR 1 (GOTO UnsupportedSwitch) ELSE IF %ARGB% == 0    (SET "BUILDTYPE=Build")
IF %ARGPL%   GTR 1 (GOTO UnsupportedSwitch) ELSE IF %ARGPL% == 0   (SET "PPLATFORM=Both")
IF %ARGC%    GTR 1 (GOTO UnsupportedSwitch) ELSE IF %ARGC% == 0    (SET "CONFIG=MPCHC")
IF %ARGBC%   GTR 1 (GOTO UnsupportedSwitch) ELSE IF %ARGBC% == 0   (SET "BUILDCFG=Release")

IF /I "%PACKAGES%" == "True" SET "INSTALLER=True" & SET "ZIP=True"

IF /I "%INSTALLER%" == "True"   IF "%NO_INST%" == "True" GOTO UnsupportedSwitch
IF /I "%ZIP%" == "True"         IF "%NO_ZIP%" == "True"  GOTO UnsupportedSwitch
IF /I "%MPCHC_LITE%" == "True"  IF "%NO_LITE%" == "True" GOTO UnsupportedSwitch
IF /I "%CLEAN%" == "LAVFilters" IF "%NO_LAV%" == "True"  GOTO UnsupportedSwitch

IF NOT EXIST "%MPCHC_VS_PATH%" CALL "%COMMON%" :SubVSPath
IF NOT EXIST "!MPCHC_VS_PATH!" GOTO MissingVar
SET "TOOLSET=!MPCHC_VS_PATH!\Common7\Tools\vsdevcmd"
SET "BIN_DIR=bin"
IF NOT EXIST "%TOOLSET%" GOTO MissingVar

IF EXIST "%FILE_DIR%signinfo.txt" (
  IF /I "%INSTALLER%" == "True" SET "SIGN=True"
  IF /I "%ZIP%" == "True"       SET "SIGN=True"
)

REM Set version for DX libraries
CALL "%COMMON%" :SubParseConfig

:Start
REM Check if the %LOG_DIR% folder exists otherwise MSBuild will fail
SET "LOG_DIR=%BIN_DIR%\logs"
IF NOT EXIST "%LOG_DIR%" MD "%LOG_DIR%"

IF DEFINED MPCHC_LITE SET "BUILDCFG=%BUILDCFG% Lite"

SET "MSBUILD_SWITCHES=/nologo /consoleloggerparameters:Verbosity=minimal /maxcpucount /nodeReuse:true"

SET START_TIME=%TIME%
SET START_DATE=%DATE%

IF /I "%PPLATFORM%" == "Both" (
  SETLOCAL
  SET "PPLATFORM=Win32" & CALL :Main
  ENDLOCAL

  SETLOCAL
  SET "PPLATFORM=x64"   & CALL :Main
  ENDLOCAL
) ELSE (
  CALL :Main
)
GOTO End


:Main
IF %ERRORLEVEL% NEQ 0 EXIT /B

IF /I "%PPLATFORM%" == "x64" (
  SET "LAVFILTERSDIR=LAVFilters64"
) ELSE (
  SET "LAVFILTERSDIR=LAVFilters"
)

IF /I "%CLEAN%" == "LAVFilters" CALL "src\thirdparty\LAVFilters\build_lavfilters.bat" Clean %PPLATFORM% %BUILDCFG% %COMPILER%
IF %ERRORLEVEL% NEQ 0 ENDLOCAL & EXIT /B

IF /I "%PPLATFORM%" == "Win32" (SET ARCH=x86) ELSE (SET ARCH=amd64)
CALL "%TOOLSET%" -no_logo -arch=%ARCH% -winsdk=%MPCHC_WINSDK_VER%
IF %ERRORLEVEL% NEQ 0 GOTO MissingVar

IF /I "%CONFIG%" == "Filters" (
  CALL :SubFilters %PPLATFORM%
  IF /I "%ZIP%" == "True" CALL :SubCreatePackages Filters %PPLATFORM%
  EXIT /B
)

IF /I "%CONFIG%" == "IconLib" (
  CALL :SubMPCIconLib %PPLATFORM%
  EXIT /B
)

IF /I "%CONFIG%" == "Translation" (
  CALL :SubMPCRresources %PPLATFORM%
  EXIT /B
)

IF /I "%CONFIG%" == "API" (
  CALL :SubMPCTestAPI %PPLATFORM%
  EXIT /B
)

IF /I "%CONFIG%" NEQ "Resources" CALL :SubMPCHC %PPLATFORM%
IF /I "%CONFIG%" NEQ "Main"      CALL :SubResources %PPLATFORM%

IF /I "%INSTALLER%" == "True" CALL :SubCreateInstaller %PPLATFORM%
IF /I "%ZIP%" == "True"       CALL :SubCreatePackages MPC-HC %PPLATFORM%

IF /I "%CONFIG%" == "All" (
  CALL :SubFilters %PPLATFORM%
  IF /I "%ZIP%" == "True" CALL :SubCreatePackages Filters %PPLATFORM%
)
EXIT /B


:End
IF %ERRORLEVEL% NEQ 0 EXIT /B
TITLE Compiling MPC-HC %COMPILER% [FINISHED]
SET END_TIME=%TIME%
CALL "%COMMON%" :SubGetDuration
CALL "%COMMON%" :SubMsg "INFO" "Compilation started on %START_DATE%-%START_TIME% and completed on %DATE%-%END_TIME% [%DURATION%]"
ENDLOCAL
EXIT /B


:SubFilters
IF %ERRORLEVEL% NEQ 0 EXIT /B

TITLE Compiling MPC-HC Filters %COMPILER% - %BUILDCFG% Filter^|%1...
CALL "update_version.bat"

MSBuild.exe mpc-hc.sln %MSBUILD_SWITCHES%^
 /target:%BUILDTYPE% /property:Configuration="%BUILDCFG% Filter";Platform=%1^
 /flp1:LogFile=%LOG_DIR%\filters_errors_%BUILDCFG%_%1.log;errorsonly;Verbosity=diagnostic^
 /flp2:LogFile=%LOG_DIR%\filters_warnings_%BUILDCFG%_%1.log;warningsonly;Verbosity=diagnostic
IF %ERRORLEVEL% NEQ 0 (
  CALL "%COMMON%" :SubMsg "ERROR" "mpc-hc.sln %BUILDCFG% Filter %1 - Compilation failed!"
  EXIT /B
) ELSE (
  CALL "%COMMON%" :SubMsg "INFO" "mpc-hc.sln %BUILDCFG% Filter %1 compiled successfully"
)
IF /I "%SIGN%" == "True" CALL :SubSign Filters *.ax
IF /I "%SIGN%" == "True" CALL :SubSign Filters VSFilter.dll
EXIT /B


:SubMPCHC
IF %ERRORLEVEL% NEQ 0 EXIT /B

TITLE Compiling MPC-HC %COMPILER% - %BUILDCFG%^|%1...
CALL "update_version.bat"

MSBuild.exe mpc-hc.sln %MSBUILD_SWITCHES%^
 /target:%BUILDTYPE% /property:Configuration="%BUILDCFG%";Platform=%1^
 /flp1:LogFile="%LOG_DIR%\mpc-hc_errors_%BUILDCFG%_%1.log";errorsonly;Verbosity=diagnostic^
 /flp2:LogFile="%LOG_DIR%\mpc-hc_warnings_%BUILDCFG%_%1.log";warningsonly;Verbosity=diagnostic
IF %ERRORLEVEL% NEQ 0 (
  CALL "%COMMON%" :SubMsg "ERROR" "mpc-hc.sln %BUILDCFG% %1 - Compilation failed!"
  EXIT /B
) ELSE (
  CALL "%COMMON%" :SubMsg "INFO" "mpc-hc.sln %BUILDCFG% %1 compiled successfully"
)
IF /I "%SIGN%" == "True" CALL :SubSign MPC-HC mpc-hc*.exe
IF /I "%SIGN%" == "True" CALL :SubSign MPC-HC *.dll %LAVFILTERSDIR%
IF /I "%SIGN%" == "True" CALL :SubSign MPC-HC *.ax %LAVFILTERSDIR%
IF /I "%SIGN%" == "True" CALL :SubSign MPC-HC CrashReporterDialog.dll CrashReporter

EXIT /B


:SubResources
IF %ERRORLEVEL% NEQ 0 EXIT /B

IF /I "%BUILDCFG%" == "Debug" (
  CALL "%COMMON%" :SubMsg "WARNING" "/debug was used, resources will not be built"
  EXIT /B
)

CALL :SubMPCIconLib %1

IF DEFINED MPCHC_LITE (
  CALL "%COMMON%" :SubMsg "WARNING" "/lite was used, translations will not be built"
  EXIT /B
)

CALL :SubMPCRresources %1
EXIT /B


:SubMPCIconLib
IF %ERRORLEVEL% NEQ 0 EXIT /B

TITLE Compiling mpciconlib %COMPILER% - Release^|%1...
MSBuild.exe mpciconlib.sln %MSBUILD_SWITCHES%^
 /target:%BUILDTYPE% /property:Configuration=Release;Platform=%1
IF %ERRORLEVEL% NEQ 0 (
  CALL "%COMMON%" :SubMsg "ERROR" "mpciconlib.sln %1 - Compilation failed!"
  EXIT /B
) ELSE (
  CALL "%COMMON%" :SubMsg "INFO" "mpciconlib.sln %1 compiled successfully"
)
IF /I "%SIGN%" == "True" CALL :SubSign MPC-HC mpciconlib.dll

IF /I "%1" == "Win32" (SET "VS_OUT_DIR=mpc-hc_x86") ELSE (SET "VS_OUT_DIR=mpc-hc_x64")
IF DEFINED MPCHC_LITE (
  PUSHD "%BIN_DIR%"
  COPY /Y /V "%VS_OUT_DIR%\mpciconlib.dll" "%VS_OUT_DIR% Lite" >NUL
  POPD
)

EXIT /B


:SubMPCRresources
IF %ERRORLEVEL% NEQ 0 EXIT /B

TITLE Compiling mpcresources %COMPILER%...
MSBuild.exe mpcresources.sln %MSBUILD_SWITCHES%^
 /target:%BUILDTYPE% /property:Configuration="Release";Platform=%1
IF %ERRORLEVEL% NEQ 0 CALL "%COMMON%" :SubMsg "ERROR" "Compilation failed!" & EXIT /B
IF /I "%SIGN%" == "True" CALL :SubSign MPC-HC mpcresources.??.dll Lang
IF /I "%SIGN%" == "True" CALL :SubSign MPC-HC mpcresources.??_??.dll Lang
EXIT /B


:SubMPCTestAPI
IF %ERRORLEVEL% NEQ 0 EXIT /B

PUSHD "src\MPCTestAPI"
TITLE Compiling MPCTestAPI %COMPILER% - %BUILDCFG%^|%1...
MSBuild.exe MPCTestAPI.sln %MSBUILD_SWITCHES%^
 /target:%BUILDTYPE% /property:Configuration=%BUILDCFG%;Platform=%1
IF %ERRORLEVEL% NEQ 0 (
  CALL "%COMMON%" :SubMsg "ERROR" "MPCTestAPI.sln %1 - Compilation failed!"
  EXIT /B
) ELSE (
  CALL "%COMMON%" :SubMsg "INFO" "MPCTestAPI.sln %1 compiled successfully"
)
POPD
EXIT /B


:SubSign
IF %ERRORLEVEL% NEQ 0 EXIT /B
REM %1 is Filters or MPC-HC
REM %2 is name of the file to sign
REM %3 is the subfolder

IF /I "%PPLATFORM%" == "Win32" PUSHD "%BIN_DIR%\%~1_x86\%3"
IF /I "%PPLATFORM%" == "x64"   PUSHD "%BIN_DIR%\%~1_x64\%3"

FOR /F "delims=" %%A IN ('DIR "%2" /b') DO (
  CALL "%FILE_DIR%contrib\sign.bat" "%%A" || (CALL "%COMMON%" :SubMsg "ERROR" "Problem signing %%A" & GOTO Break)
)
CALL "%COMMON%" :SubMsg "INFO" "%2 signed successfully."

:Break
POPD
EXIT /B


:SubCopyDXDll
REM SubCopyDXDll skipped
EXIT /B
IF /I "%BUILDCFG%" == "Debug" EXIT /B
PUSHD "%BIN_DIR%"
COPY /Y /V "%WindowsSdkDir%\Redist\D3D\%~1\d3dcompiler_%MPC_D3D_COMPILER_VERSION%.dll" "mpc-hc_%~1%~2" >NUL
IF %ERRORLEVEL% NEQ 0 CALL "%COMMON%" :SubMsg "ERROR" "Problem when copying %WindowsSdkDir%\Redist\D3D\%~1\d3dcompiler_%MPC_D3D_COMPILER_VERSION%.dll" & EXIT /B
EXPAND "%DXSDK_DIR%\Redist\Jun2010_d3dx9_%MPC_DX_SDK_NUMBER%_%~1.cab" -F:d3dx9_%MPC_DX_SDK_NUMBER%.dll "mpc-hc_%~1%~2"
IF %ERRORLEVEL% NEQ 0 CALL "%COMMON%" :SubMsg "ERROR" "Problem when extracting Jun2010_d3dx9_%MPC_DX_SDK_NUMBER%_%~1.cab" & EXIT /B
POPD
EXIT /B


:SubCreateInstaller
IF %ERRORLEVEL% NEQ 0 EXIT /B

IF /I "%~1" == "x64" (
  SET MPCHC_INNO_DEF=%MPCHC_INNO_DEF% /Dx64Build
  SET MPCHC_COPY_DX_DLL_ARGS=x64
) ELSE SET MPCHC_COPY_DX_DLL_ARGS=x86


IF DEFINED MPCHC_LITE (
  SET MPCHC_INNO_DEF=%MPCHC_INNO_DEF% /DMPCHC_LITE
  SET MPCHC_COPY_DX_DLL_ARGS=%MPCHC_COPY_DX_DLL_ARGS% " Lite"
)

CALL :SubCopyDXDll %MPCHC_COPY_DX_DLL_ARGS%

CALL "%COMMON%" :SubDetectInnoSetup

IF NOT DEFINED InnoSetupPath (
  CALL "%COMMON%" :SubMsg "WARNING" "Inno Setup wasn't found, the %1 installer wasn't built"
  EXIT /B
)

TITLE Compiling %1 %COMPILER% installer...
"%InnoSetupPath%" /SMySignTool="cmd /c "%FILE_DIR%contrib\sign.bat" $f" /Q /O"%BIN_DIR%"^
 "distrib\mpc-hc_setup.iss" %MPCHC_INNO_DEF%
IF %ERRORLEVEL% NEQ 0 CALL "%COMMON%" :SubMsg "ERROR" "Compilation failed!" & EXIT /B
CALL "%COMMON%" :SubMsg "INFO" "%1 installer successfully built"

EXIT /B


:SubCreatePackages
IF %ERRORLEVEL% NEQ 0 EXIT /B

CALL "%COMMON%" :SubDetectSevenzipPath
CALL "%COMMON%" :SubGetVersion
CALL "%COMMON%" :SubMINGWLIB

IF NOT DEFINED SEVENZIP (
  CALL "%COMMON%" :SubMsg "WARNING" "7-Zip wasn't found, the %1 %2 package wasn't built"
  EXIT /B
)

IF /I "%~1" == "Filters" (SET "NAME=MPC-HC_standalone_filters") ELSE (SET "NAME=MPC-HC")
IF /I "%~2" == "Win32" (
  SET ARCH=x86
) ELSE (
  SET ARCH=x64
)

IF DEFINED MPCHC_LITE (
  CALL :SubCopyDXDll %ARCH% " Lite"
) ELSE IF /I "%NAME%" == "MPC-HC" (
  CALL :SubCopyDXDll %ARCH%
)

PUSHD "%BIN_DIR%"

SET "VS_OUT_DIR=%~1_%ARCH%"
SET "PCKG_NAME=%NAME%.%MPCHC_VER%.%ARCH%"
IF DEFINED MPCHC_LITE (
  SET "VS_OUT_DIR=%VS_OUT_DIR% Lite"
  SET "PCKG_NAME=%PCKG_NAME%.Lite"
)
IF /I "%BUILDCFG%" == "Debug" (
  SET "PCKG_NAME=%PCKG_NAME%.dbg"
  SET "VS_OUT_DIR=%VS_OUT_DIR%_Debug"
)

IF EXIST "%PCKG_NAME%.7z"     DEL "%PCKG_NAME%.7z"
IF EXIST "%PCKG_NAME%.pdb.7z" DEL "%PCKG_NAME%.pdb.7z"
IF EXIST "%PCKG_NAME%"        RD /Q /S "%PCKG_NAME%"

SET "PDB_FILES=*.pdb"
IF NOT DEFINED MPCHC_LITE (SET "PDB_FILES=%PDB_FILES% %LAVFILTERSDIR%\*.pdb")

REM Compress the pdb file for mpc-hc only
IF /I "%NAME%" == "MPC-HC" (
  PUSHD "%VS_OUT_DIR%"
  TITLE Creating archive %PCKG_NAME%.pdb.7z...
  START "7z" /B /WAIT "%SEVENZIP%" a -t7z "%PCKG_NAME%.pdb.7z" %PDB_FILES% -m0=LZMA2^
   -mmt=%NUMBER_OF_PROCESSORS% -mx9 -ms=on
  IF %ERRORLEVEL% NEQ 0 CALL "%COMMON%" :SubMsg "ERROR" "Unable to create %PCKG_NAME%.pdb.7z!" & EXIT /B
  CALL "%COMMON%" :SubMsg "INFO" "%PCKG_NAME%.pdb.7z successfully created"
  IF EXIST "%PCKG_NAME%.pdb.7z" MOVE /Y "%PCKG_NAME%.pdb.7z" ".." >NUL
  POPD
)

TITLE Copying %PCKG_NAME%...
IF NOT EXIST "%PCKG_NAME%" MD "%PCKG_NAME%"

IF /I "%NAME%" == "MPC-HC" (
  IF NOT DEFINED MPCHC_LITE (
    IF /I "%BUILDCFG%" NEQ "Debug" (
      IF NOT EXIST "%PCKG_NAME%\Lang"          MD "%PCKG_NAME%\Lang"
    )
    IF NOT EXIST "%PCKG_NAME%\%LAVFILTERSDIR%" MD "%PCKG_NAME%\%LAVFILTERSDIR%"
  )
  IF /I "%ARCH%" == "x64" (
    COPY /Y /V "%VS_OUT_DIR%\mpc-hc64.exe" "%PCKG_NAME%\mpc-hc64.exe" >NUL
  ) ELSE (
    COPY /Y /V "%VS_OUT_DIR%\mpc-hc.exe"   "%PCKG_NAME%\mpc-hc.exe" >NUL
  )
  COPY /Y /V "%VS_OUT_DIR%\mpciconlib.dll"                "%PCKG_NAME%\*.dll" >NUL
  IF NOT DEFINED MPCHC_LITE (
    COPY /Y /V "%VS_OUT_DIR%\Lang\mpcresources.??.dll"    "%PCKG_NAME%\Lang\" >NUL
    COPY /Y /V "%VS_OUT_DIR%\Lang\mpcresources.??_??.dll" "%PCKG_NAME%\Lang\" >NUL
    COPY /Y /V "%VS_OUT_DIR%\%LAVFILTERSDIR%\*.ax"        "%PCKG_NAME%\%LAVFILTERSDIR%" >NUL
    COPY /Y /V "%VS_OUT_DIR%\%LAVFILTERSDIR%\*.dll"       "%PCKG_NAME%\%LAVFILTERSDIR%" >NUL
    COPY /Y /V "%VS_OUT_DIR%\%LAVFILTERSDIR%\*.manifest"  "%PCKG_NAME%\%LAVFILTERSDIR%" >NUL
  )
  COPY /Y /V "%VS_OUT_DIR%\d3dcompiler_%MPC_D3D_COMPILER_VERSION%.dll" "%PCKG_NAME%\d3dcompiler_%MPC_D3D_COMPILER_VERSION%.dll" >NUL
  COPY /Y /V "%VS_OUT_DIR%\d3dx9_%MPC_DX_SDK_NUMBER%.dll"              "%PCKG_NAME%\d3dx9_%MPC_DX_SDK_NUMBER%.dll" >NUL
  IF NOT EXIST "%PCKG_NAME%\Shaders" MD "%PCKG_NAME%\Shaders"
  COPY /Y /V "..\src\mpc-hc\res\shaders\dx9\*.hlsl" "%PCKG_NAME%\Shaders" >NUL
  IF NOT EXIST "%PCKG_NAME%\Shaders11" MD "%PCKG_NAME%\Shaders11"
  COPY /Y /V "..\src\mpc-hc\res\shaders\dx11\*.hlsl" "%PCKG_NAME%\Shaders11" >NUL
  IF /I "%BUILDCFG%" NEQ "Debug" IF /I "%BUILDCFG%" NEQ "Debug Lite" IF EXIST "%VS_OUT_DIR%\CrashReporter\crashrpt.dll" (
    IF NOT EXIST "%PCKG_NAME%\CrashReporter" MD "%PCKG_NAME%\CrashReporter"
    COPY /Y /V "%VS_OUT_DIR%\CrashReporter\crashrpt.dll"            "%PCKG_NAME%\CrashReporter"
    COPY /Y /V "%VS_OUT_DIR%\CrashReporter\dbghelp.dll"             "%PCKG_NAME%\CrashReporter"
    COPY /Y /V "%VS_OUT_DIR%\CrashReporter\sendrpt.exe"             "%PCKG_NAME%\CrashReporter"
    COPY /Y /V "%VS_OUT_DIR%\CrashReporter\CrashReporterDialog.dll" "%PCKG_NAME%\CrashReporter"
  )
) ELSE (
  COPY /Y /V "%VS_OUT_DIR%\*.ax"           "%PCKG_NAME%\*.ax" >NUL
  COPY /Y /V "%VS_OUT_DIR%\VSFilter.dll"   "%PCKG_NAME%\VSFilter.dll" >NUL
)

COPY /Y /V "..\COPYING.txt"         "%PCKG_NAME%" >NUL

TITLE Creating archive %PCKG_NAME%.7z...
START "7z" /B /WAIT "%SEVENZIP%" a -t7z "%PCKG_NAME%.7z" "%PCKG_NAME%" -m0=LZMA2^
 -mmt=%NUMBER_OF_PROCESSORS% -mx9 -ms=on
IF %ERRORLEVEL% NEQ 0 CALL "%COMMON%" :SubMsg "ERROR" "Unable to create %PCKG_NAME%.7z!" & EXIT /B
CALL "%COMMON%" :SubMsg "INFO" "%PCKG_NAME%.7z successfully created"

IF EXIST "%PCKG_NAME%" RD /Q /S "%PCKG_NAME%"

POPD
EXIT /B


:ShowHelp
TITLE %~nx0 Help
ECHO.
ECHO Usage:
ECHO %~nx0 [Clean^|Build^|Rebuild] [x86^|x64^|Both] [Main^|Resources^|MPCHC^|IconLib^|Translations^|Filters^|API^|All] [Debug^|Release] [Lite] [Packages^|Installer^|7z] [LAVFilters] [Analyze]
ECHO.
ECHO Notes: You can also prefix the commands with "-", "--" or "/".
ECHO        Debug only applies to mpc-hc.sln.
ECHO        The arguments are not case sensitive and can be ommitted.
ECHO. & ECHO.
ECHO Executing %~nx0 without any arguments will use the default ones:
ECHO "%~nx0 Build Both MPCHC Release"
ECHO. & ECHO.
ECHO Examples:
ECHO %~nx0 x86 Resources     -Builds the x86 resources
ECHO %~nx0 Resources         -Builds both x86 and x64 resources
ECHO %~nx0 x86               -Builds x86 Main exe and the x86 resources
ECHO %~nx0 x86 Debug         -Builds x86 Main Debug exe and x86 resources
ECHO %~nx0 x86 Filters       -Builds x86 Filters
ECHO %~nx0 x86 All           -Builds x86 Main exe, x86 Filters and the x86 resources
ECHO %~nx0 x86 Packages      -Builds x86 Main exe, x86 resources and creates the installer and the .7z package
ECHO %~nx0 x64 LAVFilters 7z -Rebuilds LAVFilters, builds x64 Main exe, x64 resources and creates the .7z package
ECHO.
ENDLOCAL
EXIT /B


:MissingVar
TITLE Compiling MPC-HC %COMPILER% [ERROR]
ECHO Not all build dependencies were found.
ECHO.
ECHO See "docs\Compilation.md" for more information.
CALL "%COMMON%" :SubMsg "ERROR" "Compilation failed!" & EXIT /B


:UnsupportedSwitch
ECHO.
ECHO Unsupported commandline switch!
ECHO.
ECHO "%~nx0 %*"
ECHO.
ECHO Run "%~nx0 help" for details about the commandline switches.
CALL "%COMMON%" :SubMsg "ERROR" "Compilation failed!" & EXIT /B
