# Just set OC_BASE to the opencore root, or set AMR_BASE directly to
# a detached gsm_amr directory
OC_BASE = $(top_srcdir)/opencore
AMR_BASE = $(OC_BASE)/codecs_v2/audio/gsm_amr

DEC_DIR = $(AMR_BASE)/amr_wb/dec
DEC_SRC_DIR = $(DEC_DIR)/src
OSCL = $(top_srcdir)/oscl

AM_CFLAGS = -I$(OSCL) -I$(DEC_SRC_DIR) -I$(DEC_DIR)/include \
    -I$(AMR_BASE)/common/dec/include

if GCC_ARMV5
    AM_CFLAGS += -DPV_CPU_ARCH_VERSION=5 -DPV_COMPILER=1
endif

if COMPILE_AS_C
    AM_CFLAGS += -x c -std=c99
    libopencore_amrwb_la_LINK = $(LINK) $(libopencore_amrwb_la_LDFLAGS)
    # Mention a dummy pure C file to trigger generation of the $(LINK) variable
    nodist_EXTRA_libopencore_amrwb_la_SOURCES = dummy.c
else
    libopencore_amrwb_la_LINK = $(CXXLINK) $(libopencore_amrwb_la_LDFLAGS)
endif

AM_CXXFLAGS = $(AM_CFLAGS)

amrwbincludedir = $(includedir)/opencore-amrwb
amrwbinclude_HEADERS = dec_if.h if_rom.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = opencore-amrwb.pc

lib_LTLIBRARIES = libopencore-amrwb.la

libopencore_amrwb_la_LDFLAGS = -version-info @OPENCORE_AMRWB_VERSION@ -no-undefined -export-symbols $(top_srcdir)/amrwb/opencore-amrwb.sym
EXTRA_DIST = $(top_srcdir)/amrwb/opencore-amrwb.sym

# Our sources to include. There are certain sources we exclude and they are
# $(DEC_SRC_DIR)/decoder_amr_wb.cpp
libopencore_amrwb_la_SOURCES = \
    wrapper.cpp \
    $(DEC_SRC_DIR)/agc2_amr_wb.cpp \
        $(DEC_SRC_DIR)/band_pass_6k_7k.cpp \
        $(DEC_SRC_DIR)/dec_acelp_2p_in_64.cpp \
        $(DEC_SRC_DIR)/dec_acelp_4p_in_64.cpp \
        $(DEC_SRC_DIR)/dec_alg_codebook.cpp \
        $(DEC_SRC_DIR)/dec_gain2_amr_wb.cpp \
        $(DEC_SRC_DIR)/deemphasis_32.cpp \
        $(DEC_SRC_DIR)/dtx_decoder_amr_wb.cpp \
        $(DEC_SRC_DIR)/get_amr_wb_bits.cpp \
        $(DEC_SRC_DIR)/highpass_400hz_at_12k8.cpp \
        $(DEC_SRC_DIR)/highpass_50hz_at_12k8.cpp \
        $(DEC_SRC_DIR)/homing_amr_wb_dec.cpp \
        $(DEC_SRC_DIR)/interpolate_isp.cpp \
        $(DEC_SRC_DIR)/isf_extrapolation.cpp \
        $(DEC_SRC_DIR)/isp_az.cpp \
        $(DEC_SRC_DIR)/isp_isf.cpp \
        $(DEC_SRC_DIR)/lagconceal.cpp \
        $(DEC_SRC_DIR)/low_pass_filt_7k.cpp \
        $(DEC_SRC_DIR)/median5.cpp \
        $(DEC_SRC_DIR)/mime_io.cpp \
        $(DEC_SRC_DIR)/noise_gen_amrwb.cpp \
        $(DEC_SRC_DIR)/normalize_amr_wb.cpp \
        $(DEC_SRC_DIR)/oversamp_12k8_to_16k.cpp \
        $(DEC_SRC_DIR)/phase_dispersion.cpp \
        $(DEC_SRC_DIR)/pit_shrp.cpp \
        $(DEC_SRC_DIR)/pred_lt4.cpp \
        $(DEC_SRC_DIR)/preemph_amrwb_dec.cpp \
        $(DEC_SRC_DIR)/pvamrwbdecoder.cpp \
        $(DEC_SRC_DIR)/pvamrwb_math_op.cpp \
        $(DEC_SRC_DIR)/q_gain2_tab.cpp \
        $(DEC_SRC_DIR)/qisf_ns.cpp \
        $(DEC_SRC_DIR)/qisf_ns_tab.cpp \
        $(DEC_SRC_DIR)/qpisf_2s.cpp \
        $(DEC_SRC_DIR)/qpisf_2s_tab.cpp \
        $(DEC_SRC_DIR)/scale_signal.cpp \
        $(DEC_SRC_DIR)/synthesis_amr_wb.cpp \
        $(DEC_SRC_DIR)/voice_factor.cpp \
        $(DEC_SRC_DIR)/wb_syn_filt.cpp \
        $(DEC_SRC_DIR)/weight_amrwb_lpc.cpp

