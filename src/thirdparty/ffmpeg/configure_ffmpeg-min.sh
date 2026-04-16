#!/bin/bash

echo "$(pwd)" | grep -q '[[:blank:]]' &&
  echo "Out of tree builds are impossible with whitespace in source path." && exit 1

mkdir -p tmp/32
mkdir -p tmp/64

COMPILER=MSVC
LAVSRCDIR=../../../LAVFilters-min/src/

configure() {
  arch=$1
  OPTIONS="
    --enable-gpl \
    --enable-version3 \
    --enable-w32threads \
    --enable-static \
    --enable-libsoxr \
    --disable-shared \
    --disable-everything \
    --disable-mediafoundation \
    --disable-schannel \
    --disable-dxva2 \
    --disable-d3d11va \
    --disable-programs \
    --disable-doc \
    --disable-avformat \
    --disable-avdevice \
    --disable-devices \
    --disable-swscale \
    --enable-filter=abuffer,abuffersink,atempo,aresample \
    --arch=${arch}"
  
  TOOLCHAIN="--toolchain=msvc"
  if [ "$arch" == "x86" ]; then
	EXTRA_CFLAGS="-I${LAVSRCDIR}thirdparty/32/include"
	EXTRA_LDFLAGS="-LIBPATH:${LAVSRCDIR}thirdparty/32/lib -LIBPATH:../../../../../bin/lib/Debug_Win32 "
  else
	EXTRA_CFLAGS="-I${LAVSRCDIR}thirdparty/64/include"
	EXTRA_LDFLAGS="-LIBPATH:${LAVSRCDIR}thirdparty/64/lib -LIBPATH:../../../../../bin/lib/Debug_x64 "
  fi
  EXTRA_CFLAGS="${EXTRA_CFLAGS} -I../../../zlib"
  EXTRA_CFLAGS="${EXTRA_CFLAGS} -I../../../soxr/libsoxr/src"

  sh ${LAVSRCDIR}ffmpeg/configure ${TOOLCHAIN} --x86asmexe=nasm ${OPTIONS} --extra-cflags="${EXTRA_CFLAGS}" --extra-ldflags="${EXTRA_LDFLAGS}"
}

pushd tmp/64
cmd /c "VsDevCmd.bat -no_logo -arch=amd64 && bash -c 'export' > vcvars.tmp"
source vcvars.tmp
rm vcvars.tmp
configure "x86_64" 2>&1 | tee config.out
popd

pushd tmp/32
cmd /c "VsDevCmd.bat -no_logo -arch=x86 && bash -c 'export' > vcvars.tmp"
source vcvars.tmp
rm vcvars.tmp
configure "x86" 2>&1 | tee config.out
popd

fgrep -x -f tmp/32/config.h tmp/64/config.h | grep -v "#endif" > tmp/config.h
echo "#ifdef WIN64" >> tmp/config.h
comm -13 <(sort tmp/32/config.h) <(sort tmp/64/config.h) >> tmp/config.h
echo "#else" >> tmp/config.h
comm -23 <(sort tmp/32/config.h) <(sort tmp/64/config.h) >> tmp/config.h
echo "#endif" >> tmp/config.h
echo "#endif /* FFMPEG_CONFIG_H */" >> tmp/config.h
cp -f config.h config.h.old
cp -f tmp/config.h ./config.h

fgrep -x -f tmp/32/config_components.h tmp/64/config_components.h | grep -v "#endif" > tmp/config_components.h
echo "#ifdef WIN64" >> tmp/config_components.h
comm -13 <(sort tmp/32/config_components.h) <(sort tmp/64/config_components.h) >> tmp/config_components.h
echo "#else" >> tmp/config_components.h
comm -23 <(sort tmp/32/config_components.h) <(sort tmp/64/config_components.h) >> tmp/config_components.h
echo "#endif" >> tmp/config_components.h
echo "#endif /* FFMPEG_CONFIG_COMPONENTS_H */" >> tmp/config_components.h
cp -f config_components.h config_components.h.old
cp -f tmp/config_components.h ./config_components.h

fgrep -x -f tmp/32/config.asm tmp/64/config.asm | grep -v "#endif" > tmp/config.asm
echo "%ifdef WIN64" >> tmp/config.asm
comm -13 <(sort tmp/32/config.asm) <(sort tmp/64/config.asm) >> tmp/config.asm
echo "%else" >> tmp/config.asm
comm -23 <(sort tmp/32/config.asm) <(sort tmp/64/config.asm) >> tmp/config.asm
echo "%endif" >> tmp/config.asm
cp -f config.asm config.asm.old
cp -f tmp/config.asm ./config.asm
