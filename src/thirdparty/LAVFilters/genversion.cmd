@ECHO OFF
REM genversion.cmd - writes LAV Filters' common\includes\version_rev.h
REM
REM A plain-cmd equivalent of the submodule's common\version.sh (which needs bash),
REM invoked instead of common\genversion.bat through Directory.Build.targets.
REM Same result: "#define LAV_VERSION_BUILD <n>", where <n> is the number of commits
REM between the last tag and the commit preceding "[MPC-HC] Use our own ffmpeg clone
REM repository." (the point where clsid2's fork starts carrying MPC-HC-only patches),
REM or 0 when git is unavailable. The file is only rewritten when the value changes.

SETLOCAL EnableDelayedExpansion
SET "LAVCOMMON=%~dp0src\common"
PUSHD "%LAVCOMMON%" || (ECHO genversion: cannot find "%LAVCOMMON%" & EXIT /B 1)

REM git: from PATH, else from MPCHC_GIT (build.user.bat), else the default Git for Windows location.
REM Its directory goes on PATH so that it can be called unquoted inside FOR /F.
SET "GITEXE="
FOR %%G IN (git.exe) DO SET "GITEXE=%%~$PATH:G"
IF NOT DEFINED GITEXE IF DEFINED MPCHC_GIT IF EXIST "%MPCHC_GIT%\cmd\git.exe" SET "PATH=%MPCHC_GIT%\cmd;%PATH%" & SET "GITEXE=%MPCHC_GIT%\cmd\git.exe"
IF NOT DEFINED GITEXE IF EXIST "C:\Program Files\Git\cmd\git.exe" SET "PATH=C:\Program Files\Git\cmd;%PATH%" & SET "GITEXE=C:\Program Files\Git\cmd\git.exe"

SET "VER=0"
IF NOT DEFINED GITEXE (
  ECHO genversion: git not found, using build number 0
  GOTO Write
)
git rev-parse --git-dir >NUL 2>&1 || (ECHO genversion: not a git repository, using build number 0 & GOTO Write)

SET "BASE="
REM (the "=" of --format must be inside quotes: FOR /F would otherwise split the command there)
FOR /F "usebackq delims=" %%H IN (`git log --grep="\[MPC-HC\] Use our own ffmpeg clone repository\." "--format=%%H"`) DO IF NOT DEFINED BASE SET "BASE=%%H"
IF NOT DEFINED BASE (
  ECHO genversion: base commit not found, using build number 0
  GOTO Write
)
SET "DESCRIBE="
FOR /F "usebackq delims=" %%D IN (`git describe --long %BASE%~1`) DO SET "DESCRIBE=%%D"
IF NOT DEFINED DESCRIBE (
  ECHO genversion: git describe failed, using build number 0
  GOTO Write
)
REM DESCRIBE is <tag>-<n>-g<hash>; LAV's tags carry no dash, so <n> is the second dash-separated field
FOR /F "tokens=2 delims=-" %%N IN ("%DESCRIBE%") DO SET "VER=%%N"
SET "HASH=%DESCRIBE:*-g=%"
ECHO genversion: describe %DESCRIBE%, build %VER%, hash %HASH%
git diff-index --quiet HEAD || ECHO genversion: local modifications found

:Write
SET "LINE=#define LAV_VERSION_BUILD %VER%"
SET "CURRENT="
IF EXIST includes\version_rev.h FOR /F "usebackq delims=" %%L IN ("includes\version_rev.h") DO IF NOT DEFINED CURRENT SET "CURRENT=%%L"
IF "!CURRENT!" == "%LINE%" GOTO Done
>includes\version_rev.h ECHO %LINE%
ECHO genversion: wrote includes\version_rev.h

:Done
POPD
ENDLOCAL
EXIT /B 0
