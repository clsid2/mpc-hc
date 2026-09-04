@ECHO OFF
REM regen.cmd [x64] [Win32]
REM
REM Maintainer-time only: regenerates msvc\ffmpeg\ (generated headers, source lists,
REM export files) and msvc\libs\*.vcxproj after the LAV Filters submodule or one of
REM the library submodules was bumped. Normal builds never run this; they only use
REM its committed output.
REM
REM Requirements beyond a normal build: a POSIX shell with GNU make and pkg-config,
REM because ffmpeg's configure is a shell script. An MSYS2 installation (MPCHC_MSYS)
REM is used when present; otherwise Git for Windows' bash is used and make.exe plus
REM pkg-config.exe must be found on PATH. See README.md.

SETLOCAL EnableDelayedExpansion
SET "REGEN_DIR=%~dp0"
SET "ROOT_DIR=%REGEN_DIR%..\..\..\..\.."
PUSHD "%ROOT_DIR%"
SET "ROOT_DIR=%CD%"
POPD
IF EXIST "%ROOT_DIR%\build.user.bat" CALL "%ROOT_DIR%\build.user.bat"

REM The shell is always Git for Windows' bash: MSYS2's runtime hides INCLUDE and LIB from
REM nested msys processes, so under MSYS2's bash ffmpeg's configure cannot see the compiler
REM environment at all. GNU make and pkg-config are not part of Git for Windows; they are
REM copied out of an MSYS2 installation (MPCHC_MSYS) into build\bin, where they run on
REM Git's runtime. Without MSYS2, make.exe and pkg-config.exe must already be on PATH.
IF NOT DEFINED MPCHC_GIT  IF DEFINED GIT  SET "MPCHC_GIT=%GIT%"
IF NOT DEFINED MPCHC_GIT  SET "MPCHC_GIT=C:\Program Files\Git"
SET "POSIX_BIN=%MPCHC_GIT%\usr\bin"
IF NOT EXIST "%POSIX_BIN%\bash.exe" (
  ECHO ERROR: no bash.exe in "%POSIX_BIN%". Set MPCHC_GIT in build.user.bat.
  EXIT /B 1
)
IF NOT DEFINED MPCHC_MSYS IF DEFINED MSYS SET "MPCHC_MSYS=%MSYS%"
IF NOT DEFINED MPCHC_MSYS IF EXIST "C:\msys64\usr\bin\make.exe" SET "MPCHC_MSYS=C:\msys64"
SET "TOOLS_BIN=%REGEN_DIR%build\bin"
IF NOT EXIST "%TOOLS_BIN%" MD "%TOOLS_BIN%"
IF EXIST "%MPCHC_MSYS%\usr\bin\make.exe" (
  COPY /Y "%MPCHC_MSYS%\usr\bin\make.exe" "%TOOLS_BIN%\" >NUL
  IF EXIST "%MPCHC_MSYS%\usr\bin\pkgconf.exe" (
    COPY /Y "%MPCHC_MSYS%\usr\bin\pkgconf.exe" "%TOOLS_BIN%\pkg-config.exe" >NUL
    COPY /Y "%MPCHC_MSYS%\usr\bin\msys-pkgconf-*.dll" "%TOOLS_BIN%\" >NUL
  ) ELSE (
    COPY /Y "%MPCHC_MSYS%\usr\bin\pkg-config.exe" "%TOOLS_BIN%\" >NUL
  )
)

IF NOT EXIST "%MPCHC_VS_PATH%" CALL "%ROOT_DIR%\common.bat" :SubVSPath
IF NOT EXIST "%MPCHC_VS_PATH%\Common7\Tools\vsdevcmd.bat" (
  ECHO ERROR: Visual Studio not found. Set MPCHC_VS_PATH in build.user.bat.
  EXIT /B 1
)

SET "PLATFORMS=%*"
IF "%PLATFORMS%"=="" SET "PLATFORMS=x64 Win32"

PUSHD "%REGEN_DIR%"
"%POSIX_BIN%\bash.exe" gen-libs.sh || GOTO Failed
POPD

FOR %%P IN (%PLATFORMS%) DO (
  IF /I "%%P"=="x64" (SET "VSARCH=amd64") ELSE (SET "VSARCH=x86")
  SETLOCAL
  CALL "%MPCHC_VS_PATH%\Common7\Tools\vsdevcmd.bat" -no_logo -arch=!VSARCH!
  REM Our tools first so that nothing else on the machine's PATH (an old GNU make, Windows'
  REM sort.exe, a stray sed) gets picked up; ffmpeg's configure locates link.exe next to
  REM cl.exe itself, so coreutils' link.exe being first does not matter. git is needed by
  REM ffmpeg's version.sh to stamp the real revision into ffversion.h.
  SET "PATH=%TOOLS_BIN%;%POSIX_BIN%;%MPCHC_GIT%\cmd;!PATH!"
  SET "MSYS2_ARG_CONV_EXCL="
  ECHO ===== %%P: MPC-HC zlib and the external libraries
  MSBuild.exe "%ROOT_DIR%\src\thirdparty\zlib\zlib.vcxproj" /nologo /v:m /m /p:Configuration=Release;Platform=%%P "/p:SolutionDir=%ROOT_DIR%\\" || GOTO Failed
  FOR %%L IN (bzip2\bzip2 speex\speex opencore-amr\opencore-amrnb opencore-amr\opencore-amrwb libxml2\libxml2 dav1d\dav1d) DO (
    MSBuild.exe "%REGEN_DIR%..\libs\%%L.vcxproj" /nologo /v:m /m /p:Configuration=Release;Platform=%%P || GOTO Failed
  )
  ECHO ===== %%P: ffmpeg configure, make, project generation
  "%POSIX_BIN%\bash.exe" "%REGEN_DIR%regen-ffmpeg.sh" %%P || GOTO Failed
  ENDLOCAL
)
ECHO.
ECHO Regeneration finished. Review "git status" under src\thirdparty\LAVFilters\msvc and commit.
EXIT /B 0

:Failed
ECHO.
ECHO Regeneration FAILED.
EXIT /B 1
