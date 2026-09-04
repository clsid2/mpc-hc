# LAV Filters with MSVC only

This directory builds the ffmpeg that LAV Filters uses, and the external
libraries ffmpeg links against, with Visual Studio's compiler and MSBuild.
Together with `src\LAVFilters.sln` (already MSVC) that makes the whole of
MPC-HC's internal filters buildable without MinGW-w64, GCC or MSYS2. It is
the default path of `..\build_lavfilters.bat`; the historical GCC path is
still there behind the `GCC` switch (or `MPCHC_LAV_TOOLCHAIN=GCC` in
`build.user.bat`) and is untouched.

What a normal build needs: Visual Studio, and `nasm.exe` on `PATH` for
ffmpeg's and dav1d's x86 assembly. That is all. No shell, no make, nothing
generated at build time.

## Layout

| Path | What |
|---|---|
| `common.props` | shared settings: toolset (taken from LAV's own `platform.props`), output into LAV's `bin_<platform>[d]\` like the GCC build, CRT and optimisation flags |
| `libs\<lib>\` | one MSBuild project per external library, next to its source (a git submodule, except opencore-amr which has no upstream repository and is vendored). Config headers those libraries' own build systems would generate are committed under `libs\<lib>\include`. |
| `ffmpeg\<lib>.vcxproj` | one project per ffmpeg DLL (avutil, swresample, swscale, avcodec, avformat, avfilter) |
| `ffmpeg\ffmpeg.props` | compiler flags, exactly what ffmpeg's configure chooses for `--toolchain=msvc`; nasm and resource handling |
| `ffmpeg\items-<lib>-<platform>.props` | generated: the source list of each DLL, taken from the object list of a configured make tree |
| `ffmpeg\generated\<platform>\` | generated: `config.h`, `config_components.h`, `config.asm`, `avconfig.h`, `ffversion.h`, the `*_list.c` component tables and the `.def` export lists |
| `regen\` | maintainer tooling that produces the generated files (see below) |

The DLLs, their import libraries and the external static libraries land in
`src\bin_<platform>[d]\` and `...\lib\`, and the generated headers are copied
to `src\bin_<platform>[d]\thirdparty\ffmpeg\`, which is where LAV's own
projects have always looked for them. `build_lavfilters.bat` then builds
`LAVFilters.sln` and copies the results into MPC-HC's output directory as
before.

## Library configuration

The option set is that of `build_ffmpeg.sh` (the GCC path). The one
difference is TLS: gnutls with nettle and gmp does not build with MSVC and is
replaced by schannel, ffmpeg's native Windows TLS, which is also what LAV
upstream's own MSVC script uses. Everything else is at parity; the enabled
decoders, demuxers, parsers, hardware accelerators, filters and bitstream
filters are identical, and the only components lost with gnutls are the
legacy encrypted-RTMP protocols `rtmpe`, `rtmpte` and `ffrtmpcrypt`.

| Library | Version | Source of the file list |
|---|---|---|
| dav1d | 1.5.3 | `src/meson.build` (x86, 8 and 16 bit) |
| libxml2 | 2.11.5 | `win32/Makefile.msvc`, without ftp/http/iconv/zlib/python, as LAV's build |
| speex | 1.2.1 | `libspeex/Makefile.am`, floating point |
| opencore-amr | 0.1.6 | `amrnb/Makefile.am` (decoder only) and `amrwb/Makefile.am` |
| bzip2 | 1.0.8 | `Makefile`, library objects |

## Regenerating after a LAV Filters or library bump

ffmpeg's configure is a shell script and its Makefiles need GNU make, so the
generated files are produced once by a maintainer and committed, in the same
way `src\thirdparty\ffmpeg` (the player's own ffmpeg.lib) has always carried
its `config.h`. Run:

    msvc\regen\regen.cmd [x64] [Win32]

It builds MPC-HC's zlib and the external libraries for each platform,
configures and builds ffmpeg out of tree under `regen\build\<platform>`
(ignored by git), and rewrites `ffmpeg\generated\`, `ffmpeg\items-*.props`,
`ffmpeg\nasm-*.props` and `libs\*\*.vcxproj`. Review the diff and commit it.

It needs a POSIX shell with GNU make and pkg-config besides nasm. The shell
is Git for Windows' bash (`MPCHC_GIT`), deliberately: MSYS2's runtime hides
`INCLUDE` and `LIB` from nested msys processes, so under MSYS2's own bash
ffmpeg's configure never sees the compiler environment. GNU make and
pkg-config are copied out of an MSYS2 installation (`MPCHC_MSYS`, default
`C:\msys64`) into `regen\build\bin`, where they run on Git's runtime; without
MSYS2, put a `make.exe` and `pkg-config.exe` on `PATH` yourself. The compiler
flags in `ffmpeg.props` are not generated; if a new ffmpeg changes what
`--toolchain=msvc` emits, compare against `CFLAGS` in
`regen\build\<platform>\ffbuild\config.mak`.

## Known differences from the GCC build

* schannel instead of gnutls, see above.
* PDBs are native; no cv2pdb step.
* The `sanm` decoder, which LAV's Debug MSVC build used to exclude, compiles
  and is enabled.
