@echo off
setlocal
set "PATH=%~dp0work\nasm\nasm-2.16.03;%PATH%"
call C:\BuildTools\Common7\Tools\VsDevCmd.bat -arch=amd64
cd /d "%~dp0mpc-hc"
rem libass consumes these libraries without declaring project references.
for %%P in (zlib\zlib freetype2\freetype2 fribidi\libfribidi harfbuzz\libharfbuzz libiconv\libiconv libunibreak\libunibreak) do (
  msbuild src\thirdparty\%%P.vcxproj /nologo /m:4 /p:SolutionDir="%CD%/" /p:Configuration=Release /p:Platform=x64 /p:MPCHC_WINSDK_VER=10.0 /v:minimal
  if errorlevel 1 exit /b 1
)
msbuild src\mpc-hc\mpc-hc.vcxproj /nologo /m:4 /target:Build /p:SolutionDir="%CD%/" /p:Configuration=Release /p:Platform=x64 /p:MPCHC_WINSDK_VER=10.0 /v:minimal /fl /flp:logfile=..\work\player-build.log
exit /b %errorlevel%
