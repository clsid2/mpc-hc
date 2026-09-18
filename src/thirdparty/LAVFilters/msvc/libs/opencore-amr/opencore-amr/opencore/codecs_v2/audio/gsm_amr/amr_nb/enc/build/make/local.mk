# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk



TARGET := pvencoder_gsmamr


OPTIMIZE_FOR_PERFORMANCE_OVER_SIZE := true

XINCDIRS +=  ../../../common/include

SRCDIR := ../../src
INCSRCDIR := ../../include

SRCS := amrencode.cpp \
	autocorr.cpp \
	c1035pf.cpp \
	c2_11pf.cpp \
	c2_9pf.cpp \
	c3_14pf.cpp \
	c4_17pf.cpp \
	c8_31pf.cpp \
	calc_cor.cpp \
	calc_en.cpp \
	cbsearch.cpp \
	cl_ltp.cpp \
	cod_amr.cpp \
	convolve.cpp \
	cor_h.cpp \
	cor_h_x.cpp \
	cor_h_x2.cpp \
	corrwght_tab.cpp \
	div_32.cpp \
	dtx_enc.cpp \
	enc_lag3.cpp \
	enc_lag6.cpp \
	enc_output_format_tab.cpp \
	ets_to_if2.cpp \
	ets_to_wmf.cpp \
	g_adapt.cpp \
	g_code.cpp \
	g_pitch.cpp \
	gain_q.cpp \
	gsmamr_encoder_wrapper.cpp \
	hp_max.cpp \
	inter_36.cpp \
	inter_36_tab.cpp \
	l_abs.cpp \
	l_comp.cpp \
	l_extract.cpp \
	l_negate.cpp \
	lag_wind.cpp \
	lag_wind_tab.cpp \
	levinson.cpp \
	lpc.cpp \
	ol_ltp.cpp \
	p_ol_wgh.cpp \
	pitch_fr.cpp \
	pitch_ol.cpp \
	pre_big.cpp \
	pre_proc.cpp \
	prm2bits.cpp \
	q_gain_c.cpp \
	q_gain_p.cpp \
	qgain475.cpp \
	qgain795.cpp \
	qua_gain.cpp \
	s10_8pf.cpp \
	set_sign.cpp \
	sid_sync.cpp \
	sp_enc.cpp \
	spreproc.cpp \
	spstproc.cpp \
	ton_stab.cpp \
	vad1.cpp 

HDRS := gsmamr_encoder_wrapper.h

include $(MK)/library.mk

