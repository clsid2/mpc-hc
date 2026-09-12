# Just set OC_BASE to the opencore root, or set AMR_BASE directly to
# a detached gsm_amr directory
OC_BASE = $(top_srcdir)/opencore
AMR_BASE = $(OC_BASE)/codecs_v2/audio/gsm_amr

DEC_DIR = $(AMR_BASE)/amr_nb/dec
ENC_DIR = $(AMR_BASE)/amr_nb/enc
COMMON_DIR = $(AMR_BASE)/amr_nb/common
DEC_SRC_DIR = $(DEC_DIR)/src
ENC_SRC_DIR = $(ENC_DIR)/src
COMMON_SRC_DIR = $(COMMON_DIR)/src
OSCL = $(top_srcdir)/oscl

AM_CFLAGS = -I$(OSCL) -I$(DEC_SRC_DIR) -I$(COMMON_DIR)/include \
    -I$(DEC_DIR)/include -I$(AMR_BASE)/common/dec/include -I$(ENC_SRC_DIR)

if GCC_ARMV5
    AM_CFLAGS += -DPV_CPU_ARCH_VERSION=5 -DPV_COMPILER=1
endif

if COMPILE_AS_C
    AM_CFLAGS += -x c -std=c99
    libopencore_amrnb_la_LINK = $(LINK) $(libopencore_amrnb_la_LDFLAGS)
    # Mention a dummy pure C file to trigger generation of the $(LINK) variable
    nodist_EXTRA_libopencore_amrnb_la_SOURCES = dummy.c
else
    libopencore_amrnb_la_LINK = $(CXXLINK) $(libopencore_amrnb_la_LDFLAGS)
endif

AM_CXXFLAGS = $(AM_CFLAGS)

amrnbincludedir = $(includedir)/opencore-amrnb
amrnbinclude_HEADERS =

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = opencore-amrnb.pc

lib_LTLIBRARIES = libopencore-amrnb.la

libopencore_amrnb_la_LDFLAGS = -version-info @OPENCORE_AMRNB_VERSION@ -no-undefined -export-symbols $(top_srcdir)/amrnb/opencore-amrnb.sym
EXTRA_DIST = $(top_srcdir)/amrnb/opencore-amrnb.sym

# Our sources to include. There are certain sources we exclude and they are
# $(DEC_SRC_DIR)/decoder_gsm_amr.cpp
# $(DEC_SRC_DIR)/pvgsmamrdecoder.cpp
# $(ENC_SRC_DIR)/gsmamr_encoder_wrapper.cpp
# $(COMMON_SRC_DIR)/bits2prm.cpp
# $(COMMON_SRC_DIR)/copy.cpp
# $(COMMON_SRC_DIR)/div_32.cpp
# $(COMMON_SRC_DIR)/l_abs.cpp
# $(COMMON_SRC_DIR)/r_fft.cpp
# $(COMMON_SRC_DIR)/vad1.cpp
# $(COMMON_SRC_DIR)/vad2.cpp
libopencore_amrnb_la_SOURCES = \
    wrapper.cpp

if AMRNB_DECODER
    libopencore_amrnb_la_SOURCES += \
    $(DEC_SRC_DIR)/agc.cpp \
        $(DEC_SRC_DIR)/amrdecode.cpp \
        $(DEC_SRC_DIR)/a_refl.cpp \
        $(DEC_SRC_DIR)/b_cn_cod.cpp \
        $(DEC_SRC_DIR)/bgnscd.cpp \
        $(DEC_SRC_DIR)/c_g_aver.cpp \
        $(DEC_SRC_DIR)/d1035pf.cpp \
        $(DEC_SRC_DIR)/d2_11pf.cpp \
        $(DEC_SRC_DIR)/d2_9pf.cpp \
        $(DEC_SRC_DIR)/d3_14pf.cpp \
        $(DEC_SRC_DIR)/d4_17pf.cpp \
        $(DEC_SRC_DIR)/d8_31pf.cpp \
        $(DEC_SRC_DIR)/dec_amr.cpp \
        $(DEC_SRC_DIR)/dec_gain.cpp \
        $(DEC_SRC_DIR)/dec_input_format_tab.cpp \
        $(DEC_SRC_DIR)/dec_lag3.cpp \
        $(DEC_SRC_DIR)/dec_lag6.cpp \
        $(DEC_SRC_DIR)/d_gain_c.cpp \
        $(DEC_SRC_DIR)/d_gain_p.cpp \
        $(DEC_SRC_DIR)/d_plsf_3.cpp \
        $(DEC_SRC_DIR)/d_plsf_5.cpp \
        $(DEC_SRC_DIR)/d_plsf.cpp \
        $(DEC_SRC_DIR)/dtx_dec.cpp \
        $(DEC_SRC_DIR)/ec_gains.cpp \
        $(DEC_SRC_DIR)/ex_ctrl.cpp \
        $(DEC_SRC_DIR)/if2_to_ets.cpp \
        $(DEC_SRC_DIR)/int_lsf.cpp \
        $(DEC_SRC_DIR)/lsp_avg.cpp \
        $(DEC_SRC_DIR)/ph_disp.cpp \
        $(DEC_SRC_DIR)/post_pro.cpp \
        $(DEC_SRC_DIR)/preemph.cpp \
        $(DEC_SRC_DIR)/pstfilt.cpp \
        $(DEC_SRC_DIR)/qgain475_tab.cpp \
        $(DEC_SRC_DIR)/sp_dec.cpp \
        $(DEC_SRC_DIR)/wmf_to_ets.cpp
    amrnbinclude_HEADERS += interf_dec.h
else
    AM_CFLAGS += -DDISABLE_AMRNB_DECODER
endif

if AMRNB_ENCODER
    libopencore_amrnb_la_SOURCES += \
    $(ENC_SRC_DIR)/amrencode.cpp \
        $(ENC_SRC_DIR)/autocorr.cpp \
        $(ENC_SRC_DIR)/c1035pf.cpp \
        $(ENC_SRC_DIR)/c2_11pf.cpp \
        $(ENC_SRC_DIR)/c2_9pf.cpp \
        $(ENC_SRC_DIR)/c3_14pf.cpp \
        $(ENC_SRC_DIR)/c4_17pf.cpp \
        $(ENC_SRC_DIR)/c8_31pf.cpp \
        $(ENC_SRC_DIR)/calc_cor.cpp \
        $(ENC_SRC_DIR)/calc_en.cpp \
        $(ENC_SRC_DIR)/cbsearch.cpp \
        $(ENC_SRC_DIR)/cl_ltp.cpp \
        $(ENC_SRC_DIR)/cod_amr.cpp \
        $(ENC_SRC_DIR)/convolve.cpp \
        $(ENC_SRC_DIR)/cor_h.cpp \
        $(ENC_SRC_DIR)/cor_h_x2.cpp \
        $(ENC_SRC_DIR)/cor_h_x.cpp \
        $(ENC_SRC_DIR)/corrwght_tab.cpp \
        $(ENC_SRC_DIR)/div_32.cpp \
        $(ENC_SRC_DIR)/dtx_enc.cpp \
        $(ENC_SRC_DIR)/enc_lag3.cpp \
        $(ENC_SRC_DIR)/enc_lag6.cpp \
        $(ENC_SRC_DIR)/enc_output_format_tab.cpp \
        $(ENC_SRC_DIR)/ets_to_if2.cpp \
        $(ENC_SRC_DIR)/ets_to_wmf.cpp \
        $(ENC_SRC_DIR)/g_adapt.cpp \
        $(ENC_SRC_DIR)/gain_q.cpp \
        $(ENC_SRC_DIR)/g_code.cpp \
        $(ENC_SRC_DIR)/g_pitch.cpp \
        $(ENC_SRC_DIR)/hp_max.cpp \
        $(ENC_SRC_DIR)/inter_36.cpp \
        $(ENC_SRC_DIR)/inter_36_tab.cpp \
        $(ENC_SRC_DIR)/l_abs.cpp \
        $(ENC_SRC_DIR)/lag_wind.cpp \
        $(ENC_SRC_DIR)/lag_wind_tab.cpp \
        $(ENC_SRC_DIR)/l_comp.cpp \
        $(ENC_SRC_DIR)/levinson.cpp \
        $(ENC_SRC_DIR)/l_extract.cpp \
        $(ENC_SRC_DIR)/lflg_upd.cpp \
        $(ENC_SRC_DIR)/l_negate.cpp \
        $(ENC_SRC_DIR)/lpc.cpp \
        $(ENC_SRC_DIR)/ol_ltp.cpp \
        $(ENC_SRC_DIR)/pitch_fr.cpp \
        $(ENC_SRC_DIR)/pitch_ol.cpp \
        $(ENC_SRC_DIR)/p_ol_wgh.cpp \
        $(ENC_SRC_DIR)/pre_big.cpp \
        $(ENC_SRC_DIR)/pre_proc.cpp \
        $(ENC_SRC_DIR)/prm2bits.cpp \
        $(ENC_SRC_DIR)/qgain475.cpp \
        $(ENC_SRC_DIR)/qgain795.cpp \
        $(ENC_SRC_DIR)/q_gain_c.cpp \
        $(ENC_SRC_DIR)/q_gain_p.cpp \
        $(ENC_SRC_DIR)/qua_gain.cpp \
        $(ENC_SRC_DIR)/s10_8pf.cpp \
        $(ENC_SRC_DIR)/set_sign.cpp \
        $(ENC_SRC_DIR)/sid_sync.cpp \
        $(ENC_SRC_DIR)/sp_enc.cpp \
        $(ENC_SRC_DIR)/spreproc.cpp \
        $(ENC_SRC_DIR)/spstproc.cpp \
        $(ENC_SRC_DIR)/ton_stab.cpp \
        $(ENC_SRC_DIR)/vad1.cpp
    amrnbinclude_HEADERS += interf_enc.h
else
    AM_CFLAGS += -DDISABLE_AMRNB_ENCODER
endif

libopencore_amrnb_la_SOURCES += \
    $(COMMON_SRC_DIR)/add.cpp \
        $(COMMON_SRC_DIR)/az_lsp.cpp \
        $(COMMON_SRC_DIR)/bitno_tab.cpp \
        $(COMMON_SRC_DIR)/bitreorder_tab.cpp \
        $(COMMON_SRC_DIR)/c2_9pf_tab.cpp \
        $(COMMON_SRC_DIR)/div_s.cpp \
        $(COMMON_SRC_DIR)/extract_h.cpp \
        $(COMMON_SRC_DIR)/extract_l.cpp \
        $(COMMON_SRC_DIR)/gains_tbl.cpp \
        $(COMMON_SRC_DIR)/gc_pred.cpp \
        $(COMMON_SRC_DIR)/get_const_tbls.cpp \
        $(COMMON_SRC_DIR)/gmed_n.cpp \
        $(COMMON_SRC_DIR)/gray_tbl.cpp \
        $(COMMON_SRC_DIR)/grid_tbl.cpp \
        $(COMMON_SRC_DIR)/int_lpc.cpp \
        $(COMMON_SRC_DIR)/inv_sqrt.cpp \
        $(COMMON_SRC_DIR)/inv_sqrt_tbl.cpp \
        $(COMMON_SRC_DIR)/l_deposit_h.cpp \
        $(COMMON_SRC_DIR)/l_deposit_l.cpp \
        $(COMMON_SRC_DIR)/log2.cpp \
        $(COMMON_SRC_DIR)/log2_norm.cpp \
        $(COMMON_SRC_DIR)/log2_tbl.cpp \
        $(COMMON_SRC_DIR)/lsfwt.cpp \
        $(COMMON_SRC_DIR)/l_shr_r.cpp \
        $(COMMON_SRC_DIR)/lsp_az.cpp \
        $(COMMON_SRC_DIR)/lsp.cpp \
        $(COMMON_SRC_DIR)/lsp_lsf.cpp \
        $(COMMON_SRC_DIR)/lsp_lsf_tbl.cpp \
        $(COMMON_SRC_DIR)/lsp_tab.cpp \
        $(COMMON_SRC_DIR)/mult_r.cpp \
        $(COMMON_SRC_DIR)/negate.cpp \
        $(COMMON_SRC_DIR)/norm_l.cpp \
        $(COMMON_SRC_DIR)/norm_s.cpp \
        $(COMMON_SRC_DIR)/overflow_tbl.cpp \
        $(COMMON_SRC_DIR)/ph_disp_tab.cpp \
        $(COMMON_SRC_DIR)/pow2.cpp \
        $(COMMON_SRC_DIR)/pow2_tbl.cpp \
        $(COMMON_SRC_DIR)/pred_lt.cpp \
        $(COMMON_SRC_DIR)/q_plsf_3.cpp \
        $(COMMON_SRC_DIR)/q_plsf_3_tbl.cpp \
        $(COMMON_SRC_DIR)/q_plsf_5.cpp \
        $(COMMON_SRC_DIR)/q_plsf_5_tbl.cpp \
        $(COMMON_SRC_DIR)/q_plsf.cpp \
        $(COMMON_SRC_DIR)/qua_gain_tbl.cpp \
        $(COMMON_SRC_DIR)/reorder.cpp \
        $(COMMON_SRC_DIR)/residu.cpp \
        $(COMMON_SRC_DIR)/round.cpp \
        $(COMMON_SRC_DIR)/set_zero.cpp \
        $(COMMON_SRC_DIR)/shr.cpp \
        $(COMMON_SRC_DIR)/shr_r.cpp \
        $(COMMON_SRC_DIR)/sqrt_l.cpp \
        $(COMMON_SRC_DIR)/sqrt_l_tbl.cpp \
        $(COMMON_SRC_DIR)/sub.cpp \
        $(COMMON_SRC_DIR)/syn_filt.cpp \
        $(COMMON_SRC_DIR)/weight_a.cpp \
        $(COMMON_SRC_DIR)/window_tab.cpp

