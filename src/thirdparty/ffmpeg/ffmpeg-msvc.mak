SOXR_DIR     = ../../../soxr/libsoxr/src
MAK_DIR      = ../../../ffmpeg/
BIN_DIR      = $(MAK_DIR)../../../bin

ifeq ($(64BIT),yes)
	PLATFORM = x64
else
	PLATFORM = Win32
endif

ifeq ($(DEBUG),yes)
	CONFIGURATION = Debug
else
	CONFIGURATION = Release
endif

OBJ_DIR	          = $(BIN_DIR)/obj/$(CONFIGURATION)_$(PLATFORM)/ffmpeg/
TARGET_LIB_DIR    = $(BIN_DIR)/lib/$(CONFIGURATION)_$(PLATFORM)
LIB_LIBAVCODEC    = $(OBJ_DIR)libavcodec.a
LIB_LIBAVFILTER   = $(OBJ_DIR)libavfilter.a
LIB_LIBAVUTIL     = $(OBJ_DIR)libavutil.a
LIB_LIBSWRESAMPLE = $(OBJ_DIR)libswresample.a
TARGET_LIB        = $(TARGET_LIB_DIR)/ffmpeg.lib
ARSCRIPT          = $(OBJ_DIR)script.ar

# Compiler and nasm flags

NASMFLAGS = -I. -I$(MAK_DIR)
AVCODECFLAGS= -DBUILDING_avcodec
AVFILTERFLAGS= -DBUILDING_avfilter
AVUTILFLAGS= -DBUILDING_avutil

CFLAGS= -D_ISOC99_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_USE_MATH_DEFINES -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS -DZLIB_CONST -DHAVE_AV_CONFIG_H -nologo -D_WIN32_WINNT=0x0600 -DWINVER=0x0600 -GS- -W3 -wd4018 -wd4146 -wd4244 -wd4305 -wd4554 -O2 -utf-8 -I $(MAK_DIR) -I . -I compat/atomics/win32 -I $(SOXR_DIR) 
CXXFLAGS=  -D__STDC_CONSTANT_MACROS 
CC=cl.exe
LIBFLAGS = -nologo -NODEFAULTLIB:libcmt 
LIB=lib.exe


ifeq ($(64BIT),yes)
	CFLAGS     += -I ../thirdparty/64/include -DWIN64=1 
	NASMFLAGS  += -f win64 -DWIN64=1 -DPIC 
else
	CFLAGS     += -I ../thirdparty/32/include -Oy- -DWIN32=1 
	NASMFLAGS  += -f win32 -DWIN32=1 -DPREFIX 
endif

#do after setting constants
NASMFLAGS += -Pconfig.asm


ifeq ($(DEBUG),yes)
	CFLAGS     += -Z7 -Zo 
else
	CFLAGS     += 
endif

# Object directories
OBJ_DIRS = $(OBJ_DIR) \
	$(OBJ_DIR)compat \
	$(OBJ_DIR)libavcodec \
	$(OBJ_DIR)libavcodec/x86 \
	$(OBJ_DIR)libavfilter \
	$(OBJ_DIR)libavfilter/x86 \
	$(OBJ_DIR)libavutil \
	$(OBJ_DIR)libavutil/x86 \
	$(OBJ_DIR)libswresample \
	$(OBJ_DIR)libswresample/x86 \
	$(TARGET_LIB_DIR)

# Targets
all: make_objdirs $(LIB_LIBAVCODEC) $(LIB_LIBAVCODEC_B) $(LIB_LIBAVFILTER) $(LIB_LIBAVUTIL) $(LIB_LIBSWRESAMPLE) $(TARGET_LIB)

make_objdirs: $(OBJ_DIRS)
$(OBJ_DIRS):
	$(shell test -d $(@) || mkdir -p $(@))

clean:
	@rm -f $(TARGET_LIB)
	@find $(OBJ_DIR) -type f ! -name "*.log" -delete
	@find $(OBJ_DIR) -type d -mindepth 1 -delete

SRCS_LC = \
	\
	libavcodec/ac3_parser.c \
	libavcodec/adts_parser.c \
	libavcodec/allcodecs.c \
	libavcodec/avcodec.c \
	libavcodec/avdct.c \
	libavcodec/bitstream.c \
	libavcodec/bitstream_filters.c \
	libavcodec/bsf.c \
	libavcodec/codec_desc.c \
	libavcodec/codec_par.c \
	libavcodec/decode.c \
	libavcodec/encode.c \
	libavcodec/faandct.c \
	libavcodec/faanidct.c \
	libavcodec/fdctdsp.c \
	libavcodec/file_open.c \
	libavcodec/get_buffer.c \
	libavcodec/idctdsp.c \
	libavcodec/imgconvert.c \
	libavcodec/jfdctfst.c \
	libavcodec/jfdctint.c \
	libavcodec/jni.c \
	libavcodec/jrevdct.c \
	libavcodec/mathtables.c \
	libavcodec/options.c \
	libavcodec/packet.c \
	libavcodec/parser.c \
	libavcodec/parsers.c \
	libavcodec/profiles.c \
	libavcodec/pthread.c \
	libavcodec/pthread_frame.c \
	libavcodec/pthread_slice.c \
	libavcodec/raw.c \
	libavcodec/simple_idct.c \
	libavcodec/utils.c \
	libavcodec/x86/constants.c \
	libavcodec/x86/fdctdsp_init.c \
	libavcodec/x86/idctdsp_init.c \

SRCS_LF = \
	libavfilter/af_aresample.c \
	libavfilter/af_atempo.c \
	libavfilter/af_biquads.c \
	libavfilter/allfilters.c \
	libavfilter/audio.c \
	libavfilter/avfilter.c \
	libavfilter/avfiltergraph.c \
	libavfilter/buffersink.c \
	libavfilter/buffersrc.c \
	libavfilter/formats.c \
	libavfilter/framepool.c \
	libavfilter/framequeue.c \
	libavfilter/graphparser.c \
	libavfilter/pthread.c \
	libavfilter/video.c

SRCS_LU = \
	libavutil/adler32.c \
	libavutil/aes.c \
	libavutil/aes_ctr.c \
	libavutil/audio_fifo.c \
	libavutil/avsscanf.c \
	libavutil/avstring.c \
	libavutil/base64.c \
	libavutil/blowfish.c \
	libavutil/bprint.c \
	libavutil/buffer.c \
	libavutil/camellia.c \
	libavutil/cast5.c \
	libavutil/channel_layout.c \
	libavutil/cpu.c \
	libavutil/crc.c \
	libavutil/des.c \
	libavutil/dict.c \
	libavutil/display.c \
	libavutil/dovi_meta.c \
	libavutil/downmix_info.c \
	libavutil/encryption_info.c \
	libavutil/error.c \
	libavutil/eval.c \
	libavutil/fifo.c \
	libavutil/file.c \
	libavutil/file_open.c \
	libavutil/film_grain_params.c \
	libavutil/fixed_dsp.c \
	libavutil/float_dsp.c \
	libavutil/frame.c \
	libavutil/hash.c \
	libavutil/hdr_dynamic_metadata.c \
	libavutil/hmac.c \
	libavutil/hwcontext.c \
	libavutil/imgutils.c \
	libavutil/integer.c \
	libavutil/intmath.c \
	libavutil/lfg.c \
	libavutil/lls.c \
	libavutil/log.c \
	libavutil/log2_tab.c \
	libavutil/mastering_display_metadata.c \
	libavutil/mathematics.c \
	libavutil/md5.c \
	libavutil/mem.c \
	libavutil/murmur3.c \
	libavutil/opt.c \
	libavutil/parseutils.c \
	libavutil/pixdesc.c \
	libavutil/pixelutils.c \
	libavutil/random_seed.c \
	libavutil/rational.c \
	libavutil/rc4.c \
	libavutil/refstruct.c \
	libavutil/reverse.c \
	libavutil/ripemd.c \
	libavutil/samplefmt.c \
	libavutil/sha.c \
	libavutil/sha512.c \
	libavutil/side_data.c \
	libavutil/slicethread.c \
	libavutil/spherical.c \
	libavutil/stereo3d.c \
	libavutil/tea.c \
	libavutil/threadmessage.c \
	libavutil/time.c \
	libavutil/timecode.c \
	libavutil/timestamp.c \
	libavutil/tree.c \
	libavutil/twofish.c \
	libavutil/tx.c \
	libavutil/tx_double.c \
	libavutil/tx_float.c \
	libavutil/tx_int32.c \
	libavutil/utils.c \
	libavutil/video_enc_params.c \
	libavutil/xga_font_data.c \
	libavutil/xtea.c \
	libavutil/x86/cpu.c \
	libavutil/x86/fixed_dsp_init.c \
	libavutil/x86/float_dsp_init.c \
	libavutil/x86/imgutils_init.c \
	libavutil/x86/lls_init.c \
	libavutil/x86/tx_float_init.c

SRCS_LR = \
	libswresample/audioconvert.c \
	libswresample/dither.c \
	libswresample/options.c \
	libswresample/rematrix.c \
	libswresample/resample.c \
	libswresample/resample_dsp.c \
	libswresample/soxr_resample.c \
	libswresample/swresample.c \
	libswresample/swresample_frame.c \
	libswresample/x86/audio_convert_init.c \
	libswresample/x86/rematrix_init.c \
	libswresample/x86/resample_init.c

# Nasm objects
SRCS_NASM_LC = \
	libavcodec/x86/idctdsp.asm \
	libavcodec/x86/fdct.asm \
	libavcodec/x86/simple_idct.asm \
	libavcodec/x86/simple_idct10.asm

SRCS_NASM_LF = 

SRCS_NASM_LU = \
	libavutil/x86/cpuid.asm \
	libavutil/x86/emms.asm \
	libavutil/x86/fixed_dsp.asm \
	libavutil/x86/float_dsp.asm \
	libavutil/x86/imgutils.asm \
	libavutil/x86/lls.asm \
	libavutil/x86/tx_float.asm

SRCS_NASM_LR = \
	libswresample/x86/audio_convert.asm \
	libswresample/x86/rematrix.asm \
	libswresample/x86/resample.asm

OBJS_LC = \
	$(SRCS_LC:%.c=$(OBJ_DIR)%.o) \
	$(SRCS_NASM_LC:%.asm=$(OBJ_DIR)%.o)

OBJS_LF = \
	$(SRCS_LF:%.c=$(OBJ_DIR)%.o) \
	$(SRCS_NASM_LF:%.asm=$(OBJ_DIR)%.o)

OBJS_LU = \
	$(SRCS_LU:%.c=$(OBJ_DIR)%.o) \
	$(SRCS_NASM_LU:%.asm=$(OBJ_DIR)%.o)

OBJS_LR = \
	$(SRCS_LR:%.c=$(OBJ_DIR)%.o) \
	$(SRCS_NASM_LR:%.asm=$(OBJ_DIR)%.o)

OBJS_LS = \
	$(SRCS_LS:%.c=$(OBJ_DIR)%.o) \
	$(SRCS_NASM_LS:%.asm=$(OBJ_DIR)%.o)

COMPILE = @$(CC) $(CFLAGS) $(OPTFLAGS) -c -Fo$@ $<
LIBAR = @$(LIB) $(LIBFLAGS) -out:$@ $^
NASMC = nasm $(NASMFLAGS) -I$(<D)/ -o $@ $<

VERSIONH=$(MAK_DIR)/libavutil/ffversion.h

# Commands
$(VERSIONH): Changelog
	sh ffbuild/version.sh . msvc_ffversion.h && mv msvc_ffversion.h $(VERSIONH)

$(OBJ_DIR)libavcodec/%.o: libavcodec/%.c
	@echo $<
	$(COMPILE) $(AVCODECFLAGS)

$(OBJ_DIR)libavutil/%.o: libavutil/%.c
	@echo $<
	$(COMPILE) $(AVUTILFLAGS)

$(OBJ_DIR)libavfilter/%.o: libavfilter/%.c
	@echo $<
	$(COMPILE) $(AVFILTERFLAGS)

$(OBJ_DIR)%.o: %.c
	@echo $<
	$(COMPILE)

$(OBJ_DIR)%.o: %.asm
	@echo $<
	$(NASMC)

$(LIB_LIBAVCODEC): $(OBJS_LC)
	@echo $@
	$(LIBAR)

$(LIB_LIBAVFILTER): $(OBJS_LF)
	@echo $@
	$(LIBAR)

$(LIB_LIBAVUTIL): $(OBJS_LU)
	@echo $@
	$(LIBAR)

$(LIB_LIBSWRESAMPLE): $(OBJS_LR)
	@echo $@
	$(LIBAR)

$(TARGET_LIB): $(LIB_LIBAVCODEC) $(LIB_LIBAVFILTER) $(LIB_LIBAVUTIL) $(LIB_LIBSWRESAMPLE)
	@echo $@
	$(LIBAR)

-include $(SRCS_LC:%.c=$(OBJ_DIR)%.d)
-include $(SRCS_LF:%.c=$(OBJ_DIR)%.d)
-include $(SRCS_LU:%.c=$(OBJ_DIR)%.d)
-include $(SRCS_LR:%.c=$(OBJ_DIR)%.d)
-include $(SRCS_LS:%.c=$(OBJ_DIR)%.d)

.PHONY: clean make_objdirs $(OBJ_DIRS)

.EXTRA_PREREQS+=$(VERSIONH) $(MAK_DIR)/config.asm