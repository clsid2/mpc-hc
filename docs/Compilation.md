# Compilation instructions

## Part A: Preparing the Visual Studio environment

### Visual Studio 2019

1. Install Visual Studio (any edition will work fine). Select at minimum the following components:
    - C++ core features
    - IntelliCode
    - Windows Universal C Runtime
    - Windows Universal CRT SDK
    - C++ build tools (x86 & x64)
    - C++ ATL
    - C++ MFC
    - Windows 10 SDK (10.0.17763.0 or any other version)
2. Install the Windows 8.1 SDK → <https://go.microsoft.com/fwlink/p/?LinkId=323507>
    - When choosing which features to install you only need to select "Windows Software Development Kit".
    - Alternatively you can use Windows 10 SDK, but then resulting binaries will require at least Windows 7 SP1, so you lose compatibility with Windows 7 RTM.


## Part B: Install Python 3 (optional)

This is required for building the translation DLL files.

1. Install Python version 3.8.7 from <https://www.python.org/downloads/release/python-387/> (You can use Python 3.6 or later version)
2. Run this command to install a required library:
    `C:\Program Files\Python38\Scripts\pip install --upgrade polib`


## Part C: Preparing the MSYS and GCC environment (optional, GCC toolchain only)

LAV Filters, which MPC-HC uses as its internal codecs, are built with Visual Studio alone:
their ffmpeg and the libraries it depends on have MSBuild projects under
**`src\thirdparty\LAVFilters\msvc`** (see the README there). Besides Visual Studio you only
need NASM (Part E). Nothing in this part is required for that. Optionally, install the Visual
Studio components "C++ Clang Compiler for Windows" and "MSBuild support for LLVM (clang-cl)
toolset": when present, ffmpeg is compiled with clang, which is about 10% faster at software
H.264 decoding than the cl build (it can compile ffmpeg's inline assembly, cl cannot).

This part is only needed if you want to build ffmpeg with MinGW-w64 GCC instead, the way
LAV Filters upstream does. When a MinGW-w64 gcc is configured (Part F) the build uses it
by default; without one it uses the MSVC projects. `SET "MPCHC_LAV_TOOLCHAIN=MSVC"` (or
`GCC`) in **build.user.bat**, or the `MSVC`/`GCC` switch of `build_lavfilters.bat`,
overrides that. Maintainers also need a POSIX shell with GNU make when regenerating the
MSVC projects after a LAV Filters update; the msvc README explains that.

You can skip compilation of LAV Filters altogether by selecting the "Release Lite"/"Debug Lite"
build configuration in the MPC-HC project file. This can be useful for making quick builds during
development. The resulting binary will be missing the internal filter functionality. So don't use
this configuration for actual releases.

1. Download MSYS2 from <http://www.msys2.org/>.
   If you are using a 64-bit Operating System, which you should be, get the 64-bit version.
2. Install it to for example **`C:\MSYS64\`**. The installation path should be specified in your **build.user.bat** configuration script that you will create later.
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

## Part D: Yasm

1. Download Microsoft Visual C++ 2010 Redistributable Package from <https://www.microsoft.com/en-us/download/details.aspx?id=26999> and install it.
2. Download YASM from <http://www.tortall.net/projects/yasm/releases/yasm-1.3.0-win64.exe>
3. Rename it to **yasm.exe** and put it in a folder that is included in %PATH%. For example **`C:\Windows`** or **`C:\MSYS64\usr\bin`** (see part C).

## PART E: NASM

1. Download NASM from <https://www.nasm.us/pub/nasm/releasebuilds/2.16.03/win64/nasm-2.16.03-win64.zip>
2. Put nasm.exe in a folder that is included in %PATH%. For example **`C:\Windows`**.

## Part F: Config file with paths

Create a file named **build.user.bat** in the source code folder of MPC-HC (see part F). It should have the following contents: (with paths adapted for your system!)

```bat
@ECHO OFF
REM [Optional, GCC toolchain for LAVFilters only] MSYS2/MinGW paths:
SET "MPCHC_MSYS=C:\MSYS64"
SET "MPCHC_MINGW32=C:\MSYS64\mingw64"
SET "MPCHC_MINGW64=C:\MSYS64\mingw64"
SET "MSYSTEM=MINGW32"
SET "MSYS2_PATH_TYPE=inherit"
REM [Optional] Specify GIT location if it is not already set in %PATH%
SET "MPCHC_GIT=C:\Program Files\Git"
REM [Optional] If you plan to modify the translations, install Python 3.8 and set the variable to its path
SET "MPCHC_PYTHON=C:\Program Files\Python38"
REM [Optional] If you want to customize the Windows SDK version used, set this variable
SET "MPCHC_WINSDK_VER=8.1"
```

Notes:

* For Visual Studio, we will try to detect the VS installation path automatically. If that fails you need to specify the installation path yourself. For example:
  ```
  SET "MPCHC_VS_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\"
  ```
* If you installed the MSYS package in another directory then make sure that you have set the correct paths in your **build.user.bat** file.
* If you don't have Git installed then the build version will be inaccurate, the revision number will be a hard-coded as zero.


## Part G: Downloading the MPC-HC source

You need Git for downloading the source code.

Install **Git for Windows** from <https://git-for-windows.github.io/> and also **Git Extensions** from <http://gitextensions.github.io/>.
The build uses Git only to stamp the version number. It looks for `git.exe` in `MPCHC_GIT`
(see **build.user.bat** in Part F), on `%PATH%`, in the default Git for Windows install
locations, and finally in the copy that Visual Studio installs with its C++ workload, so no
particular install option is required and Git Bash is not used.

Use Git to clone MPC-HC's repository to **C:\mpc-hc** (or anywhere else you like).

1. Install Git
2. Run these commands:

    ```text
    git clone --recursive https://github.com/clsid2/mpc-hc.git
    ```

    or

    ```text
    git clone https://github.com/clsid2/mpc-hc.git
    git submodule update --init --recursive
    ```

    If a submodule update fails, try running:

    ```text
    git submodule foreach --recursive git fetch --tags
    ```

    then run the update again

    ```text
    git submodule update --init --recursive
    ```

Note that you can add `-b master` to the `git clone` command if you want to get the latest
stable version instead of the latest development version.

## Part H: Compiling the MPC-HC source

1. Open the solution file **C:\mpc-hc\mpc-hc.sln**.
   Change the solution's configuration to **Release** (in the toolbar).
2. Press **F7** to build the solution.
3. You now have **mpc-hc.exe** under **C:\mpc-hc\bin\mpc-hc_x86**
4. Open the solution file **C:\mpc-hc\mpciconlib.sln**
5. Press **F7** to build the solution.
6. You now have **mpciconlib.dll** under **C:\mpc-hc\bin\mpc-hc_x86**
7. Open the solution file **C:\mpc-hc\mpcresources.sln**
8. Build **BuildAll** project.
9. You now have **mpcresources.XX.dll** under **C:\mpc-hc\bin\mpc-hc_x86\Lang**

Alternatively, you can use **build.bat** that can build everything for you (run: `build.bat help` for more info).


## Part I: Building the installer

Download Inno Setup Unicode v5.5.9 or newer from <http://www.jrsoftware.org/isdl.php>.
Install everything and then go to **C:\mpc-hc\distrib**, open **mpc-hc_setup.iss** with Inno Setup,
read the first comments in the script and compile it.

### NOTES

* **build.bat** can build the installer by using the **installer** or the **packages** switch.
* Use Inno Setup's built-in IDE if you want to edit the iss file and don't change its encoding since it can break easily.
