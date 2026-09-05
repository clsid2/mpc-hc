#!/bin/bash
# regen-ffmpeg.sh <platform: x64|Win32> [compiler: cl|clang]
#
# Maintainer-time only: configure LAV's ffmpeg with the MSVC toolchain and the given
# compiler, build it once with make so that every generated file exists, then hand the
# build directory to gen-ffmpeg.sh. Run through regen.cmd, which sets up the Visual
# Studio environment and builds the external libraries first (configure link-tests
# against them).
#
# Needs on PATH: cl/link (from vsdevcmd), nasm, GNU make >= 3.81, pkg-config (or pkgconf),
# a POSIX shell - Git for Windows' bash is enough - and, for clang, clang.exe (the GNU-style
# driver; Visual Studio's is under VC\Tools\Llvm\x64\bin).
set -e
PLAT=$1; CC=${2:-cl}
case "$PLAT" in
  x64)   arch=x86_64; lavarch=64; triple=x86_64-pc-windows-msvc;;
  Win32) arch=x86;    lavarch=32; triple=i686-pc-windows-msvc;;
  *) echo "usage: regen-ffmpeg.sh x64|Win32 [cl|clang]"; exit 1;;
esac
[ "$CC" = cl ] || [ "$CC" = clang ] || { echo "compiler must be cl or clang"; exit 1; }
R=$(cd "$(dirname "$0")" && pwd)         # .../LAVFilters/msvc/regen
M=$(cd "$R/.." && pwd)                   # .../LAVFilters/msvc
LAV=$(cd "$M/../src" && pwd)             # the LAVFilters submodule
MPC=$(cd "$M/../../../.." && pwd)        # MPC-HC root
B="$R/build/$PLAT-$CC"
LAVBIN="$LAV/bin_$PLAT"
[ -f "$LAVBIN/lib/dav1d.lib" ] || { echo "external libraries not built for $PLAT (expected $LAVBIN/lib/dav1d.lib) - run regen.cmd"; exit 1; }
[ -f "$MPC/bin/lib/Release_$PLAT/zlib.lib" ] || { echo "MPC-HC's zlib.lib not built for $PLAT (bin/lib/Release_$PLAT/zlib.lib) - build the zlib project first"; exit 1; }
for t in make nasm pkg-config cl $CC; do command -v $t >/dev/null || { echo "$t not found on PATH"; exit 1; }; done

rm -rf "$B"; mkdir -p "$B/pkgconfig" "$B/ffbuild"
w() { cygpath -m "$1"; }   # C:/... form: understood by cl, clang, link and pkgconf alike

# pkg-config descriptions of the MSVC-built libraries, pointing at the tree
pc() { printf 'Name: %s\nDescription: %s\nVersion: %s\nLibs: -L%s %s\nCflags: %s\n' "$1" "$1" "$2" "$(w "$LAVBIN/lib")" "$3" "$4" > "$B/pkgconfig/$5.pc"; }
ver_dav1d=$(sed -n "s/^ *version: '\([0-9.]*\)'.*/\1/p" "$M/libs/dav1d/dav1d/meson.build" | head -1)
ver_xml=$(sed -n 's/^#define LIBXML_DOTTED_VERSION "\(.*\)"/\1/p' "$M/libs/libxml2/include/libxml/xmlversion.h")
ver_amr=$(sed -n 's/^AC_INIT(\[opencore-amr\], *\[\([0-9.]*\)\].*/\1/p' "$M/libs/opencore-amr/opencore-amr/configure.ac")
pc libdav1d "$ver_dav1d" "-ldav1d" "-I$(w "$M/libs/dav1d/dav1d/include")" dav1d
pc libxml-2.0 "$ver_xml" "-lxml2" "-I$(w "$M/libs/libxml2/include") -I$(w "$M/libs/libxml2/libxml2/include") -DLIBXML_STATIC" libxml-2.0
pc opencore-amrnb "$ver_amr" "-lopencore-amrnb" "-I$(w "$M/libs/opencore-amr/include")" opencore-amrnb
pc opencore-amrwb "$ver_amr" "-lopencore-amrwb" "-I$(w "$M/libs/opencore-amr/include")" opencore-amrwb
export PKG_CONFIG_PATH="$B/pkgconfig"

# Keep in sync with build_ffmpeg.sh (the GCC path). gnutls/gmp are replaced by schannel,
# ffmpeg's native Windows TLS, which is what LAV upstream's own MSVC script uses too.
OPTIONS="
    --enable-shared                 \
    --disable-static                \
    --enable-gpl                    \
    --enable-version3               \
    --disable-autodetect            \
    --enable-w32threads             \
    --disable-demuxer=matroska      \
    --disable-filters               \
    --enable-filter=scale,yadif,w3fdif,bwdif \
    --disable-protocol=async,cache,concat,httpproxy,icecast,md5,subfile \
    --disable-muxers                \
    --enable-muxer=spdif            \
    --disable-bsfs                  \
    --enable-bsf=extract_extradata,dovi_split  \
    --disable-avdevice              \
    --disable-encoders              \
    --disable-devices               \
    --disable-programs              \
    --disable-doc                   \
    --enable-avisynth               \
    --enable-d3d11va                \
    --enable-dxva2                  \
    --enable-zlib                   \
    --build-suffix=-lav             \
    --disable-stripping             \
    --disable-debug                 \
    --enable-schannel               \
    --enable-bzlib                  \
    --enable-libdav1d               \
    --enable-libspeex               \
    --enable-libopencore-amrnb      \
    --enable-libopencore-amrwb      \
    --enable-libxml2                \
    --arch=$arch"

EXTRA_CFLAGS="-D_WIN32_WINNT=0x0601 -DWINVER=0x0601"
if [ "$CC" = cl ]; then
  CCOPT=()
  EXTRA_CFLAGS="$EXTRA_CFLAGS -Zo -GS- -MD"
else
  # clang's GNU-style driver targeting the MSVC ABI; -fms-runtime-lib=dll is clang's spelling of -MD
  CCOPT=("--cc=clang --target=$triple")
  EXTRA_CFLAGS="$EXTRA_CFLAGS -fms-runtime-lib=dll"
fi
EXTRA_CFLAGS="$EXTRA_CFLAGS -I$(w "$M/libs/speex/include") -I$(w "$M/libs/speex/speex/include") -I$(w "$M/libs/bzip2/bzip2")"
EXTRA_CFLAGS="$EXTRA_CFLAGS -I$(w "$LAV/thirdparty/$lavarch/include") -I$(w "$MPC/src/thirdparty/zlib") -I$(w "$M/../msvcInclude")"
EXTRA_LDFLAGS="-LIBPATH:$(w "$LAVBIN/lib") -LIBPATH:$(w "$MPC/bin/lib/Release_$PLAT") -NODEFAULTLIB:libcmt"

cd "$B"
echo "== $PLAT/$CC: configure"
sh "$LAV/ffmpeg/configure" --toolchain=msvc "${CCOPT[@]}" --x86asmexe=nasm --pkg-config-flags="--static" \
   --extra-cflags="$EXTRA_CFLAGS" --extra-ldflags="$EXTRA_LDFLAGS" $OPTIONS > ffbuild/config.out 2>&1 || { tail -20 ffbuild/config.out; echo "configure failed - see $B/ffbuild/config.log"; exit 1; }
grep -A3 "^External libraries" ffbuild/config.out
grep -E "^#define HAVE_INLINE_ASM " config.h
if [ "$CC" = clang ]; then
  # LAV's two HEVC intrinsic files use "#pragma GCC target", which clang ignores; give them
  # the same target features explicitly (ffmpeg.props does the same for the MSBuild build)
  for f in libavcodec/x86/hevc/intra_intrinsic.o libavcodec/x86/hevc/idct_intrinsic.o; do
    mkdir -p "$(dirname "$f")"
    cmd=$(make -n V=1 "$f" | grep '^clang ' | tail -1)
    [ -n "$cmd" ] || { echo "no compile command for $f"; exit 1; }
    eval "${cmd/ -c / -mssse3 -msse4.1 -c }"
  done
fi
echo "== $PLAT/$CC: make"
make -j"${NUMBER_OF_PROCESSORS:-4}" > make.log 2>&1 || { tail -20 make.log; echo "make failed - see $B/make.log"; exit 1; }
echo "== $PLAT/$CC: generating projects"
bash "$R/gen-ffmpeg.sh" "$B" "$PLAT" "$CC"
