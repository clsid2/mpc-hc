@ECHO OFF
REM get-nasm.cmd <dir> - downloads the official NASM release zip with the curl and tar that ship
REM with Windows 10, checks its SHA-256 and puts nasm.exe into <dir>. Safe to run concurrently:
REM the first caller takes a lock directory and downloads, the others wait for the file.
SETLOCAL
SET "VER=2.16.03"
SET "URL=https://www.nasm.us/pub/nasm/releasebuilds/%VER%/win64/nasm-%VER%-win64.zip"
SET "SHA256=3ee4782247bcb874378d02f7eab4e294a84d3d15f3f6ee2de2f47a46aa7226e6"
SET "DEST=%~1"
IF EXIST "%DEST%\nasm.exe" EXIT /B 0
SET "LOCK=%DEST%.lock"
MKDIR "%DEST%" 2>NUL
MKDIR "%LOCK%" 2>NUL || GOTO Wait
ECHO nasm.exe not found, downloading NASM %VER% to "%DEST%"
SET "ZIP=%LOCK%\nasm.zip"
"%SystemRoot%\System32\curl.exe" -sSL --fail -o "%ZIP%" "%URL%" || GOTO Fail
SET "GOT="
FOR /F "skip=1 delims=" %%H IN ('certutil -hashfile "%ZIP%" SHA256') DO IF NOT DEFINED GOT SET "GOT=%%H"
IF /I NOT "%GOT%" == "%SHA256%" ECHO get-nasm: checksum mismatch, expected %SHA256% got %GOT% & GOTO Fail
"%SystemRoot%\System32\tar.exe" -xf "%ZIP%" -C "%LOCK%" --strip-components=1 nasm-%VER%/nasm.exe || GOTO Fail
MOVE /Y "%LOCK%\nasm.exe" "%DEST%\nasm.exe" >NUL || GOTO Fail
RMDIR /S /Q "%LOCK%"
EXIT /B 0

:Wait
REM another build is downloading it; give it two minutes
FOR /L %%I IN (1,1,120) DO (
  IF EXIST "%DEST%\nasm.exe" EXIT /B 0
  PING -n 2 127.0.0.1 >NUL
)
ECHO get-nasm: timed out waiting for another download to finish
EXIT /B 1

:Fail
ECHO get-nasm: could not fetch NASM %VER% from %URL%
ECHO get-nasm: install NASM and put nasm.exe on PATH, or copy it to "%DEST%"
RMDIR /S /Q "%LOCK%" 2>NUL
EXIT /B 1
