# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := pvdecoder_gsmamr


OPTIMIZE_FOR_PERFORMANCE_OVER_SIZE := true

XINCDIRS := ../../../common/include

SRCDIR := ../../src
INCSRCDIR := ../../include

SRCS := decoder_gsm_amr.cpp \
	a_refl.cpp \
	agc.cpp \
	amrdecode.cpp \
	b_cn_cod.cpp \
	bgnscd.cpp \
	c_g_aver.cpp \
	d1035pf.cpp \
	d2_11pf.cpp \
	d2_9pf.cpp \
	d3_14pf.cpp \
	d4_17pf.cpp \
	d8_31pf.cpp \
	d_gain_c.cpp \
	d_gain_p.cpp \
	d_plsf.cpp \
	d_plsf_3.cpp \
	d_plsf_5.cpp \
	dec_amr.cpp \
	dec_gain.cpp \
	dec_input_format_tab.cpp \
	dec_lag3.cpp \
	dec_lag6.cpp \
	dtx_dec.cpp \
	ec_gains.cpp \
	ex_ctrl.cpp \
	if2_to_ets.cpp \
	int_lsf.cpp \
	lsp_avg.cpp \
	ph_disp.cpp \
	post_pro.cpp \
	preemph.cpp \
	pstfilt.cpp \
	qgain475_tab.cpp \
	sp_dec.cpp \
	wmf_to_ets.cpp

HDRS := decoder_gsm_amr.h pvamrnbdecoder_api.h

include $(MK)/library.mk


