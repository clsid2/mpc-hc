# Compilation instructions

MPC-HC, including the LAV Filters it uses as its internal codecs, builds with Visual Studio
alone. Parts A to E are everything a normal build needs. Parts F and G are optional extras
(translations and the installer), and Part H describes the alternative of building ffmpeg
with MinGW-w64 GCC, which needs MSYS2 and is only for those who want it.

## Part A: Visual Studio

1. Install Visual Studio 2019, 2022 or 2026 (any edition will work fine). Select at minimum the following components:
    - C++ core features
    - Windows Universal C Runtime
    - Windows Universal CRT SDK
    - C++ build tools (x86 & x64)
    - C++ ATL
    - C++ MFC
    - Windows 10 SDK (10.0.17763.0 or any other version)
2. Install the Windows 8.1 SDK → <https://go.microsoft.com/fwlink/p/?LinkId=323507>
    - When choosing which features to install you only need to select "Windows Software Development Kit".
    - Alternatively you can use Windows 10 SDK, but then resulting binaries will require at least Windows 7 SP1, so you lose compatibility with Windows 7 RTM.
3. Optional but recommended: also select **C++ Clang Compiler for Windows** and **MSBuild support
   for LLVM (clang-cl) toolset**. When they are present, LAV Filters' ffmpeg is compiled with clang,
   which can compile ffmpeg's inline assembly (cl cannot) and decodes about 10% faster as a result.
   Without them ffmpeg is compiled with cl and everything still works.

## Part B: Git

Install **Git for Windows** from <https://git-for-windows.github.io/>. Any install options are fine.
Besides fetching the source, the build uses `git.exe` to stamp the version number. It looks for it
in `MPCHC_GIT` (see Part D), on `%PATH%`, in the default Git for Windows locations, and finally in
the copy Visual Studio installs with its C++ workload. Git Bash is not used.

Clone the repository to **C:\mpc-hc** (or anywhere else you like):

```text
git clone --recursive https://github.com/clsid2/mpc-hc.git
```

or

```text
git clone https://github.com/clsid2/mpc-hc.git
git submodule update --init --recursive
```

If a submodule update fails, try `git submodule foreach --recursive git fetch --tags` and run the
update again. Add `-b master` to the clone command if you want the latest stable version instead of
the development version.

## Part C: NASM

1. Download NASM from <https://www.nasm.us/pub/nasm/releasebuilds/2.16.03/win64/nasm-2.16.03-win64.zip>
2. Put nasm.exe in a folder that is included in %PATH%. For example **`C:\Windows`**.

## Part D: Config file with paths

Create a file named **build.user.bat** in the source code folder of MPC-HC. For a normal build it
only needs to exist; everything in it is optional:

```bat
@ECHO OFF
REM [Optional] Git location if it is not already in %PATH%
SET "MPCHC_GIT=C:\Program Files\Git"
REM [Optional] Visual Studio location if automatic detection fails
SET "MPCHC_VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community"
REM [Optional] Python, only for building the translations (Part F)
SET "MPCHC_PYTHON=C:\Program Files\Python38"
REM [Optional] Windows SDK version to use
SET "MPCHC_WINSDK_VER=8.1"
```

If you don't have Git installed the build still works, but the revision number will be zero.

## Part E: Compiling

1. Open the solution file **C:\mpc-hc\mpc-hc.sln**.
   Change the solution's configuration to **Release** (in the toolbar).
2. Press **F7** to build the solution. This also builds LAV Filters.
3. You now have **mpc-hc.exe** under **C:\mpc-hc\bin\mpc-hc_x86** (or **mpc-hc64.exe** under **mpc-hc_x64**).
4. Open the solution file **C:\mpc-hc\mpciconlib.sln**
5. Press **F7** to build the solution.
6. You now have **mpciconlib.dll** under **C:\mpc-hc\bin\mpc-hc_x86**

Alternatively, **build.bat** can build everything for you (run `build.bat help` for more info).

To build the player without LAV Filters, for quick builds during development, build only the
**mpc-hc** project (right-click it in Solution Explorer) instead of the whole solution. The
"Release Lite"/"Debug Lite" configurations do the same when using build.bat. The resulting
binary will be missing the internal filter functionality, so don't use it for actual releases.

## Part F: Translations (optional)

Building the translation DLL files requires Python 3.

1. Install Python version 3.8.7 from <https://www.python.org/downloads/release/python-387/> (You can use Python 3.6 or later version)
2. Run this command to install a required library:
    `C:\Program Files\Python38\Scripts\pip install --upgrade polib`
3. Set `MPCHC_PYTHON` in **build.user.bat** (Part D).
4. Open **C:\mpc-hc\mpcresources.sln** and build the **BuildAll** project.
   You now have **mpcresources.XX.dll** under **C:\mpc-hc\bin\mpc-hc_x86\Lang**

## Part G: Building the installer (optional)

Download Inno Setup Unicode v5.5.9 or newer from <http://www.jrsoftware.org/isdl.php>.
Install everything and then go to **C:\mpc-hc\distrib**, open **mpc-hc_setup.iss** with Inno Setup,
read the first comments in the script and compile it.

* **build.bat** can build the installer by using the **installer** or the **packages** switch.
* Use Inno Setup's built-in IDE if you want to edit the iss file and don't change its encoding since it can break easily.

## Part H: Building ffmpeg with MinGW-w64 GCC instead (optional)

By default LAV Filters' ffmpeg and the libraries it depends on are built with the MSBuild projects
under **`src\thirdparty\LAVFilters\msvc`** (see the README there), which is what Parts A to E cover.
This part is only for building ffmpeg with MinGW-w64 GCC the way LAV Filters upstream does.

When a MinGW-w64 gcc is configured in **build.user.bat**, or found at the default location
**`C:\msys64\mingw64`**, the build uses it. `SET "MPCHC_LAV_TOOLCHAIN=MSVC"` (or `GCC`) in
**build.user.bat**, or the `MSVC`/`GCC` switch of `build_lavfilters.bat`, overrides that.
Maintainers also need this environment when regenerating the MSVC projects after a LAV Filters
update; the msvc README explains that.

1. Download MSYS2 from <http://www.msys2.org/>.
   If you are using a 64-bit Operating System, which you should be, get the 64-bit version.
2. Install it to for example **`C:\MSYS64\`**. The installation path should be specified in your **build.user.bat** configuration script.
3. Run `msys2_shell.bat`
4. Install some additional required tools by running this command:
   ```text
   pacman -S make pkg-config diffutils
   ```
5. Then update all packages by running this command:
   ```text
   pacman -Syu
   ```
   When you are asked to restart MSYS, say yes. Start MSYS again and repeat the above command. Once everything is updated, you can close MSYS.
6. Download the latest mingw-w64-gcc package from <http://files.1f0.de/mingw/> and extract it to folder **`C:\MSYS64\mingw64`** (overwriting any existing files).
7. It is recommended to add **`C:\MSYS64\mingw64\bin`** and **`C:\MSYS64\usr\bin`** to the %PATH% environment variable.
   This allows you to run GCC and all other MSYS tools from the Windows command line.  
   Windows Control Panel > System > Advanced System Settings > Environment variables.  
   On Windows 10 you can access the legacy control panel by clicking on the Windows Start menu and typing `control.exe`.
8. Add the paths, and optionally the number of make jobs (default 4), to **build.user.bat**:
   ```bat
   SET "MPCHC_MSYS=C:\MSYS64"
   SET "MPCHC_MINGW32=C:\MSYS64\mingw64"
   SET "MPCHC_MINGW64=C:\MSYS64\mingw64"
   SET "MSYSTEM=MINGW32"
   SET "MSYS2_PATH_TYPE=inherit"
   SET "MPCHC_LAV_JOBS=4"
   ```
