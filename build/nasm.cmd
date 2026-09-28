@ECHO OFF
REM nasm.cmd - runs nasm.exe with the given arguments.
REM Uses bin\tools\nasm\nasm.exe if present, else nasm.exe on PATH, else fetches NASM
REM into bin\tools\nasm first (see get-nasm.cmd). The build rules call this instead of nasm.exe.
SETLOCAL
SET "NASMDIR=%~dp0..\bin\tools\nasm"
IF EXIST "%NASMDIR%\nasm.exe" GOTO Local
FOR %%G IN (nasm.exe) DO IF NOT "%%~$PATH:G" == "" GOTO OnPath
CALL "%~dp0get-nasm.cmd" "%NASMDIR%" || EXIT /B 1
:Local
"%NASMDIR%\nasm.exe" %*
EXIT /B
:OnPath
nasm.exe %*
EXIT /B
