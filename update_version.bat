@ECHO OFF
REM (C) 2010-2020 see Authors.txt
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

REM Writes build\version_rev.h and src\mpc-hc\res\mpc-hc.exe.manifest from the
REM git checkout. Needs only git.exe: it is looked for in MPCHC_GIT, on %PATH%,
REM in the default Git for Windows locations and finally in the MinGit that every
REM Visual Studio edition installs with the "Desktop development with C++"
REM workload. Without any git.exe the fallback files are used instead.

SETLOCAL
SET "FILE_DIR=%~dp0"
PUSHD "%FILE_DIR%"

SET "COMMON=%FILE_DIR%\common.bat"
SET "VERSIONFILE_FIXED=include\version.h"
SET "VERSIONFILE=build\version_rev.h"
SET "MANIFESTFILE=src\mpc-hc\res\mpc-hc.exe.manifest"

IF /I "%~1" == "--quiet" (SET "QUIET=1") ELSE (SET "QUIET=")

IF EXIST "build.user.bat" CALL "build.user.bat"

IF NOT DEFINED MPCHC_GIT IF DEFINED GIT SET "MPCHC_GIT=%GIT%"
CALL :SubFindGit
IF NOT DEFINED GIT_EXE GOTO MissingVar
SET "PATH=%GIT_EXE%\..;%PATH%"

REM Read major, minor and patch version numbers from the static version.h file
FOR /F "usebackq tokens=2,3" %%A IN (`findstr /R /C:"^#define MPC_VERSION_MAJOR " /C:"^#define MPC_VERSION_MINOR " /C:"^#define MPC_VERSION_PATCH " "%VERSIONFILE_FIXED%"`) DO SET "%%A=%%B"
SET "VER_FIXED=%MPC_VERSION_MAJOR%.%MPC_VERSION_MINOR%.%MPC_VERSION_PATCH%"
IF NOT DEFINED QUIET ECHO Version:   %VER_FIXED%

SET "HASH="
SET "VER="
SET "BRANCH="
SET "VER_ADDITIONAL="
SET "BRANCH_DEFINE="

git rev-parse --git-dir >NUL 2>&1
IF %ERRORLEVEL% NEQ 0 (
  ECHO Warning: Git not available or not a git repo. Using dummy values for hash and version number.
  GOTO NoRepo
)

REM The abbreviated hash of the current changeset and the number of changesets
REM since the last (annotated) tag, which is what "git describe --long" reports
FOR /F "usebackq delims=" %%A IN (`git rev-parse --short HEAD`) DO SET "HASH=%%A"
FOR /F "usebackq delims=" %%A IN (`git describe --abbrev^=0 2^>NUL`) DO SET "TAG=%%A"
IF DEFINED TAG FOR /F "usebackq delims=" %%A IN (`git rev-list --count %TAG%..HEAD`) DO SET "VER=%%A"
IF NOT DEFINED HASH SET "HASH=0000000"
IF NOT DEFINED VER SET "VER=0"
SET "VER_ADDITIONAL= (%HASH%)"

REM The current branch name
FOR /F "usebackq delims=" %%A IN (`git symbolic-ref -q --short HEAD 2^>NUL`) DO SET "BRANCH=%%A"
IF NOT DEFINED BRANCH SET "BRANCH=no branch"

IF NOT DEFINED QUIET (
  ECHO On branch: %BRANCH%
  ECHO Hash:      %HASH%
  ECHO Revision:  %VER%
)

REM On a branch other than develop or master, also record the branch and, if a
REM local develop exists, the commit on develop it is based on
IF "%BRANCH%" == "develop" GOTO NoRepo
IF "%BRANCH%" == "master" GOTO NoRepo
SET "BRANCH_DEFINE=#define MPCHC_BRANCH _T("%BRANCH%")"
SET "VER_ADDITIONAL=%VER_ADDITIONAL% (%BRANCH%)"
git show-ref --verify --quiet refs/heads/develop
IF %ERRORLEVEL% NEQ 0 GOTO NoRepo
FOR /F "usebackq delims=" %%A IN (`git merge-base develop HEAD`) DO SET "BASE=%%A"
SET "BASE=%BASE:~0,7%"
SET "VER_ADDITIONAL=%VER_ADDITIONAL% (develop@%BASE%)"
IF NOT DEFINED QUIET ECHO Mergebase: develop@%BASE%

:NoRepo
IF NOT DEFINED HASH SET "HASH=0000000"
IF NOT DEFINED VER SET "VER=0"

REM Versions of the MinGW-w64 compilers, when present (used by the GCC build of LAV Filters)
SET "GCC32_VERSION="
SET "GCC64_VERSION="
CALL :SubGccVersion gcc GCC32_VERSION
CALL :SubGccVersion x86_64-w64-mingw32-gcc GCC64_VERSION

REM Write version_rev.h, but only touch the file when its content changes
SET "TMPFILE=%VERSIONFILE%.tmp"
>"%TMPFILE%" TYPE NUL
IF DEFINED BRANCH_DEFINE >>"%TMPFILE%" ECHO %BRANCH_DEFINE%
>>"%TMPFILE%" ECHO #define MPCHC_HASH _T("%HASH%")
>>"%TMPFILE%" ECHO #define MPC_VERSION_REV %VER%
>>"%TMPFILE%" ECHO #define MPC_VERSION_ADDITIONAL _T("%VER_ADDITIONAL%")
>>"%TMPFILE%" ECHO #define GCC32_VERSION _T("%GCC32_VERSION%")
>>"%TMPFILE%" ECHO #define GCC64_VERSION _T("%GCC64_VERSION%")
CALL :SubReplaceIfChanged "%TMPFILE%" "%VERSIONFILE%"

REM Write the manifest with the version number filled in, same rule
SET "TMPFILE=%MANIFESTFILE%.tmp"
>"%TMPFILE%" TYPE NUL
SETLOCAL ENABLEDELAYEDEXPANSION
FOR /F "usebackq tokens=1,* delims=:" %%A IN (`findstr /N "^" "%MANIFESTFILE%.conf"`) DO (
  SET "LINE=%%B"
  >>"%TMPFILE%" ECHO(!LINE:$VERSION$=%VER_FIXED%.%VER%!
)
ENDLOCAL
CALL :SubReplaceIfChanged "%TMPFILE%" "%MANIFESTFILE%"


:END
POPD
ENDLOCAL
EXIT /B


:MissingVar
copy /Y "build\version_rev_fallback.h" "build\version_rev.h"
copy /Y "src\mpc-hc\res\mpc-hc.exe.manifest.fallback" "src\mpc-hc\res\mpc-hc.exe.manifest"
ECHO Not all build dependencies were found: Missing git.exe
ECHO.
ECHO See "docs\Compilation.md" for more information.
ENDLOCAL
EXIT /B 1


:SubFindGit
REM Sets GIT_EXE to the first git.exe found, or leaves it undefined
SET "GIT_EXE="
IF DEFINED MPCHC_GIT (
  CALL :SubTryGit "%MPCHC_GIT%\cmd\git.exe"
  CALL :SubTryGit "%MPCHC_GIT%\bin\git.exe"
)
IF NOT DEFINED GIT_EXE FOR %%G IN (git.exe) DO IF NOT "%%~$PATH:G" == "" SET "GIT_EXE=%%~$PATH:G"
CALL :SubTryGit "%ProgramFiles%\Git\cmd\git.exe"
CALL :SubTryGit "%ProgramFiles(x86)%\Git\cmd\git.exe"
CALL :SubTryGit "%LOCALAPPDATA%\Programs\Git\cmd\git.exe"
REM Visual Studio's own copy: under the IDE when building from it, under the
REM install root in a developer command prompt, else wherever vswhere finds VS
SET "VS_GIT=CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe"
IF DEFINED VSAPPIDDIR CALL :SubTryGit "%VSAPPIDDIR%%VS_GIT%"
IF DEFINED VSINSTALLDIR CALL :SubTryGit "%VSINSTALLDIR%Common7\IDE\%VS_GIT%"
IF DEFINED MPCHC_VS_PATH CALL :SubTryGit "%MPCHC_VS_PATH%\Common7\IDE\%VS_GIT%"
IF NOT DEFINED GIT_EXE IF NOT DEFINED MPCHC_VS_PATH CALL "%COMMON%" :SubVSPath
IF DEFINED MPCHC_VS_PATH CALL :SubTryGit "%MPCHC_VS_PATH%\Common7\IDE\%VS_GIT%"
EXIT /B

:SubTryGit
IF NOT DEFINED GIT_EXE IF EXIST "%~1" SET "GIT_EXE=%~1"
EXIT /B

:SubGccVersion
REM %1 = compiler, %2 = variable to set to "<MinGW flavour> GCC <version>"
SET "MACHINE="
FOR /F "usebackq delims=" %%A IN (`%1 -dumpmachine 2^>NUL`) DO SET "MACHINE=%%A"
IF NOT DEFINED MACHINE EXIT /B
SET "MINGW_NAME="
IF NOT "%MACHINE:w64-mingw32=%" == "%MACHINE%" (SET "MINGW_NAME=MinGW-w64") ELSE IF NOT "%MACHINE:-mingw32=%" == "%MACHINE%" SET "MINGW_NAME=MinGW"
FOR /F "usebackq delims=" %%A IN (`%1 -dumpversion 2^>NUL`) DO SET "%2=%MINGW_NAME% GCC %%A"
EXIT /B

:SubReplaceIfChanged
REM %1 = new file, %2 = destination; the destination is only rewritten when different
IF NOT EXIST "%~2" (MOVE /Y "%~1" "%~2" >NUL & EXIT /B)
FC "%~1" "%~2" >NUL 2>&1
IF %ERRORLEVEL% EQU 0 (DEL "%~1") ELSE (MOVE /Y "%~1" "%~2" >NUL)
EXIT /B
