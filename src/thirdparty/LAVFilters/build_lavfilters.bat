@ECHO OFF
REM (C) 2013-2017 see Authors.txt
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
SET "FILE_DIR=%~dp0"
PUSHD "%FILE_DIR%"

SET ROOT_DIR=..\..\..
SET "COMMON=%FILE_DIR%%ROOT_DIR%\common.bat"

SET ARG=/%*
SET ARG=%ARG:/=%
SET ARG=%ARG:-=%
SET ARGB=0
SET ARGBC=0
SET ARGCOMP=0
SET ARGPL=0
SET ARGTC=0
SET INPUT=0
SET VALID=0

IF /I "%ARG%" == "?"        GOTO ShowHelp

FOR %%G IN (%ARG%) DO (
  IF /I "%%G" == "help"     GOTO ShowHelp
  IF /I "%%G" == "Build"    SET "BUILDTYPE=Build"     & SET /A ARGB+=1
  IF /I "%%G" == "Clean"    SET "BUILDTYPE=Clean"     & SET /A ARGB+=1
  IF /I "%%G" == "Rebuild"  SET "BUILDTYPE=Rebuild"   & SET /A ARGB+=1
  IF /I "%%G" == "Both"     SET "ARCH=Both"           & SET /A ARGPL+=1
  IF /I "%%G" == "Win32"    SET "ARCH=x86"            & SET /A ARGPL+=1
  IF /I "%%G" == "x86"      SET "ARCH=x86"            & SET /A ARGPL+=1
  IF /I "%%G" == "x64"      SET "ARCH=x64"            & SET /A ARGPL+=1
  IF /I "%%G" == "Debug"    SET "RELEASETYPE=Debug"   & SET /A ARGBC+=1
  IF /I "%%G" == "Release"  SET "RELEASETYPE=Release" & SET /A ARGBC+=1
  IF /I "%%G" == "VS2017"   SET "COMPILER=VS2017"     & SET /A ARGCOMP+=1
  IF /I "%%G" == "VS2019"   SET "COMPILER=VS2019"     & SET /A ARGCOMP+=1
  IF /I "%%G" == "MSVC"     SET "TOOLCHAIN=MSVC"      & SET /A ARGTC+=1
  IF /I "%%G" == "GCC"      SET "TOOLCHAIN=GCC"       & SET /A ARGTC+=1
  IF /I "%%G" == "Silent"   SET "SILENT=True"         & SET /A VALID+=1
  IF /I "%%G" == "Nocolors" SET "NOCOLORS=True"       & SET /A VALID+=1
)

FOR %%X IN (%*) DO SET /A INPUT+=1
SET /A VALID+=%ARGB%+%ARGPL%+%ARGBC%+%ARGCOMP%+%ARGTC%

IF %VALID% NEQ %INPUT% GOTO UnsupportedSwitch

IF %ARGB%    GTR 1 (GOTO UnsupportedSwitch) ELSE IF %ARGB% == 0    (SET "BUILDTYPE=Build")
IF %ARGPL%   GTR 1 (GOTO UnsupportedSwitch) ELSE IF %ARGPL% == 0   (SET "ARCH=Both")
IF %ARGBC%   GTR 1 (GOTO UnsupportedSwitch) ELSE IF %ARGBC% == 0   (SET "RELEASETYPE=Release")
IF %ARGCOMP% GTR 1 (GOTO UnsupportedSwitch) ELSE IF %ARGCOMP% == 0 (SET "COMPILER=VS2019")
IF %ARGTC%   GTR 1 GOTO UnsupportedSwitch

REM Which toolchain builds ffmpeg:
REM   GCC  - the historical path, ffmpeg cross-compiled with MinGW-w64 through MSYS2 (build_ffmpeg.sh);
REM          this is how upstream releases are built.
REM   MSVC - ffmpeg and its external libraries built by the MSBuild projects in msvc\ (see
REM          msvc\README.md); needs nasm.exe on PATH and nothing else outside Visual Studio.
REM Chosen by the MSVC/GCC switch, else by MPCHC_LAV_TOOLCHAIN (build.user.bat), else GCC when a
REM MinGW-w64 gcc is configured and MSVC when it is not.
IF EXIST "%ROOT_DIR%\build.user.bat" CALL "%ROOT_DIR%\build.user.bat"
IF NOT DEFINED TOOLCHAIN IF DEFINED MPCHC_LAV_TOOLCHAIN SET "TOOLCHAIN=%MPCHC_LAV_TOOLCHAIN%"
IF NOT DEFINED TOOLCHAIN (
  IF NOT DEFINED MPCHC_MINGW64 IF DEFINED MINGW64 SET "MPCHC_MINGW64=%MINGW64%"
  IF NOT DEFINED MPCHC_MINGW64 SET "MPCHC_MINGW64=C:\msys64\mingw64"
  IF EXIST "!MPCHC_MINGW64!\bin\gcc.exe" (SET "TOOLCHAIN=GCC") ELSE (SET "TOOLCHAIN=MSVC")
)
IF /I NOT "%TOOLCHAIN%" == "GCC" IF /I NOT "%TOOLCHAIN%" == "MSVC" (
  ECHO ERROR: unknown toolchain "%TOOLCHAIN%" ^(use MSVC or GCC^)
  EXIT /B 1
)
ECHO Building LAV Filters' ffmpeg with the %TOOLCHAIN% toolchain

IF /I "%TOOLCHAIN%" == "GCC" (
  CALL "%COMMON%" :SubSetPath
  IF !ERRORLEVEL! NEQ 0 EXIT /B 1
  CALL "%COMMON%" :SubDoesExist gcc.exe
  IF !ERRORLEVEL! NEQ 0 (
    ECHO ERROR: gcc.exe not found in your MinGW installation
    EXIT /B 1
  )
) ELSE (
  IF EXIST "%ROOT_DIR%\build.user.bat" CALL "%ROOT_DIR%\build.user.bat"
  CALL "%COMMON%" :SubDoesExist nasm.exe
  IF !ERRORLEVEL! NEQ 0 (
    ECHO ERROR: nasm.exe not found in PATH. See "%ROOT_DIR%\docs\Compilation.md".
    EXIT /B 1
  )
  REM No shell is needed: Directory.Build.targets replaces LAV's bash-based version stamp
  REM (DSUtilLite's pre-build step) with msvc\genversion.cmd.
)

IF NOT EXIST "%MPCHC_VS_PATH%" CALL "%COMMON%" :SubVSPath
IF NOT EXIST "!MPCHC_VS_PATH!" (
  ECHO ERROR: Visual Studio install path not found or invalid. You should add MPCHC_VS_PATH to build.user.bat
  GOTO MissingVar
)

SET "TOOLSET=!MPCHC_VS_PATH!\Common7\Tools\vsdevcmd"
IF NOT EXIST "%TOOLSET%" (
  ECHO ERROR: Visual Studio tool path invalid
  GOTO MissingVar
)

SET "BIN_DIR=%ROOT_DIR%\bin"

CALL "%COMMON%" :SubParseConfig

IF /I "%ARCH%" == "Both" (
  SET "ARCH=x86" & CALL :Main
  SET "ARCH=x64" & CALL :Main
) ELSE (
  CALL :Main
)
GOTO End


:Main
IF %ERRORLEVEL% NEQ 0 EXIT /B

IF /I "%ARCH%" == "x86" (SET TOOLSETARCH=x86) ELSE (SET TOOLSETARCH=amd64)
CALL "%TOOLSET%" -no_logo -arch=%TOOLSETARCH%

SET START_TIME=%TIME%
SET START_DATE=%DATE%

IF /I "%BUILDTYPE%" == "Rebuild" (
  SET "BUILDTYPE=Clean" & CALL :SubMake
  SET "BUILDTYPE=Build" & CALL :SubMake
  SET "BUILDTYPE=Rebuild"
  EXIT /B
)

IF /I "%BUILDTYPE%" == "Clean" (CALL :SubMake & EXIT /B)

CALL :SubMake

EXIT /B


:End
IF %ERRORLEVEL% NEQ 0 EXIT /B %ERRORLEVEL%
TITLE Compiling LAV Filters %TOOLCHAIN% [FINISHED]
SET END_TIME=%TIME%
CALL "%COMMON%" :SubGetDuration
CALL "%COMMON%" :SubMsg "INFO" "LAV Filters compilation started on %START_DATE%-%START_TIME% and completed on %DATE%-%END_TIME% [%DURATION%]"
POPD
ENDLOCAL
EXIT /B


:SubMake
IF %ERRORLEVEL% NEQ 0 EXIT /B

IF /I "%ARCH%" == "x86" (SET "ARCHVS=Win32") ELSE (SET "ARCHVS=x64")

REM Build FFmpeg (and, on the MSVC path, the external libraries it links against)
IF /I "%TOOLCHAIN%" == "GCC" (
  REM The MSVC path leaves its ffmpeg DLLs in src\bin_<plat>[d]; the copy step below must not pick them up
  IF /I "%RELEASETYPE%" == "Debug" (SET "FFDLLDIR=src\bin_%ARCHVS%d") ELSE (SET "FFDLLDIR=src\bin_%ARCHVS%")
  IF EXIST "!FFDLLDIR!\avutil-lav-*.dll" DEL /Q "!FFDLLDIR!\*-lav-*.dll" "!FFDLLDIR!\*-lav-*.pdb" >NUL 2>&1
  sh build_ffmpeg.sh %ARCH% %RELEASETYPE% %BUILDTYPE% %COMPILER%
  IF !ERRORLEVEL! NEQ 0 (
    CALL "%COMMON%" :SubMsg "ERROR" "'sh build_ffmpeg.sh %ARCH% %RELEASETYPE% %BUILDTYPE% %COMPILER%' failed!"
    EXIT /B
  )
) ELSE (
  MSBuild.exe msvc\ffmpeg\avfilter.vcxproj /nologo /consoleloggerparameters:Verbosity=minimal /nodeReuse:true /m /t:%BUILDTYPE% /property:Configuration=%RELEASETYPE%;Platform=%ARCHVS%
  IF !ERRORLEVEL! NEQ 0 (
    CALL "%COMMON%" :SubMsg "ERROR" "'MSBuild.exe msvc\ffmpeg\avfilter.vcxproj /t:%BUILDTYPE% /property:Configuration=%RELEASETYPE%;Platform=%ARCHVS%' failed!"
    EXIT /B
  )
)

PUSHD src

REM Build LAVFilters
IF /I "%ARCH%" == "x86" (SET "ARCHVS=Win32") ELSE (SET "ARCHVS=x64")

MSBuild.exe LAVFilters.sln /nologo /consoleloggerparameters:Verbosity=minimal /nodeReuse:true /m /t:%BUILDTYPE% /property:Configuration=%RELEASETYPE%;Platform=%ARCHVS%
IF %ERRORLEVEL% NEQ 0 (
  CALL "%COMMON%" :SubMsg "ERROR" "'MSBuild.exe LAVFilters.sln /nologo /consoleloggerparameters:Verbosity=minimal /nodeReuse:true /m /t:%BUILDTYPE% /property:Configuration=%RELEASETYPE%;Platform=%ARCHVS%' failed!"
  EXIT /B
)

POPD

IF /I "%RELEASETYPE%" == "Debug" (
  SET "SRCFOLDER=src\bin_%ARCHVS%d"
) ELSE (
  SET "SRCFOLDER=src\bin_%ARCHVS%"
)

IF /I "%RELEASETYPE%" == "Debug" (
  SET "DESTFOLDER=%BIN_DIR%\mpc-hc_%ARCH%_Debug"
) ELSE (
  SET "DESTFOLDER=%BIN_DIR%\mpc-hc_%ARCH%"
)

IF /I "%ARCH%" == "x64" (
  SET "DESTFOLDER=%DESTFOLDER%\LAVFilters64"
) ELSE (
  SET "DESTFOLDER=%DESTFOLDER%\LAVFilters"
)

IF /I "%BUILDTYPE%" == "Build" (
  REM Copy LAVFilters files to MPC-HC output directory
  IF NOT EXIST %DESTFOLDER% MD %DESTFOLDER%

  COPY /Y /V %SRCFOLDER%\*.dll %DESTFOLDER%
  COPY /Y /V %SRCFOLDER%\*.ax %DESTFOLDER%
  COPY /Y /V %SRCFOLDER%\*.manifest %DESTFOLDER%
  IF /I "%RELEASETYPE%" == "Release" (
    COPY /Y /V %SRCFOLDER%\IntelQuickSyncDecoder\IntelQuickSyncDecoder.pdb %DESTFOLDER%
    COPY /Y /V %SRCFOLDER%\LAVAudio\LAVAudio.pdb %DESTFOLDER%
    COPY /Y /V %SRCFOLDER%\LAVSplitter\LAVSplitter.pdb %DESTFOLDER%
    COPY /Y /V %SRCFOLDER%\LAVVideo\LAVVideo.pdb %DESTFOLDER%
    COPY /Y /V %SRCFOLDER%\libbluray\libbluray.pdb %DESTFOLDER%
    IF EXIST %SRCFOLDER%\avcodec-lav-*.pdb COPY /Y /V %SRCFOLDER%\*-lav-*.pdb %DESTFOLDER%
  ) ELSE (
    COPY /Y /V %SRCFOLDER%\*.pdb %DESTFOLDER%
  )
) ELSE IF /I "%BUILDTYPE%" == "Clean" (
  REM Remove LAVFilters files in MPC-HC output directory
  IF EXIST %DESTFOLDER% RD /Q /S %DESTFOLDER%
)

EXIT /B


:MissingVar
ECHO Not all build dependencies were found.
ECHO.
ECHO See "%ROOT_DIR%\docs\Compilation.md" for more information.
CALL "%COMMON%" :SubMsg "ERROR" "LAV Filters compilation failed!" & EXIT /B 1


:UnsupportedSwitch
ECHO.
ECHO Unsupported commandline switch!
ECHO.
ECHO "%~nx0 %*"
ECHO.
ECHO Run "%~nx0 help" for details about the commandline switches.
CALL "%COMMON%" :SubMsg "ERROR" "LAV Filters compilation failed!" & EXIT /B 1


:ShowHelp
TITLE %~nx0 Help
ECHO.
ECHO Usage:
ECHO %~nx0 [Clean^|Build^|Rebuild] [x86^|x64^|Both] [Debug^|Release] [MSVC^|GCC] [VS2017^|VS2019]
ECHO.
ECHO Notes: You can also prefix the commands with "-", "--" or "/".
ECHO        The arguments are not case sensitive and can be ommitted.
ECHO        GCC builds ffmpeg with MinGW-w64/MSYS2 through build_ffmpeg.sh; MSVC builds it
ECHO        with the MSBuild projects in msvc\ and needs only Visual Studio and nasm.exe.
ECHO        Without the switch, MPCHC_LAV_TOOLCHAIN from build.user.bat decides, and without
ECHO        that GCC is used when a MinGW-w64 gcc is configured and MSVC otherwise.
ECHO. & ECHO.
ECHO Executing %~nx0 without any arguments will use the default ones:
ECHO "%~nx0 Build Both Release VS2019"
ECHO.
POPD
ENDLOCAL
EXIT /B
