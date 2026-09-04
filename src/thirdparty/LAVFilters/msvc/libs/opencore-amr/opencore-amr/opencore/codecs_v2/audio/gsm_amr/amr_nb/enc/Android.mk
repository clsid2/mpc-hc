LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	src/amrencode.cpp \
 	src/autocorr.cpp \
 	src/c1035pf.cpp \
 	src/c2_11pf.cpp \
 	src/c2_9pf.cpp \
 	src/c3_14pf.cpp \
 	src/c4_17pf.cpp \
 	src/c8_31pf.cpp \
 	src/calc_cor.cpp \
 	src/calc_en.cpp \
 	src/cbsearch.cpp \
 	src/cl_ltp.cpp \
 	src/cod_amr.cpp \
 	src/convolve.cpp \
 	src/cor_h.cpp \
 	src/cor_h_x.cpp \
 	src/cor_h_x2.cpp \
 	src/corrwght_tab.cpp \
 	src/div_32.cpp \
 	src/dtx_enc.cpp \
 	src/enc_lag3.cpp \
 	src/enc_lag6.cpp \
 	src/enc_output_format_tab.cpp \
 	src/ets_to_if2.cpp \
 	src/ets_to_wmf.cpp \
 	src/g_adapt.cpp \
 	src/g_code.cpp \
 	src/g_pitch.cpp \
 	src/gain_q.cpp \
 	src/gsmamr_encoder_wrapper.cpp \
 	src/hp_max.cpp \
 	src/inter_36.cpp \
 	src/inter_36_tab.cpp \
 	src/l_abs.cpp \
 	src/l_comp.cpp \
 	src/l_extract.cpp \
 	src/l_negate.cpp \
 	src/lag_wind.cpp \
 	src/lag_wind_tab.cpp \
 	src/levinson.cpp \
 	src/lpc.cpp \
 	src/ol_ltp.cpp \
 	src/p_ol_wgh.cpp \
 	src/pitch_fr.cpp \
 	src/pitch_ol.cpp \
 	src/pre_big.cpp \
 	src/pre_proc.cpp \
 	src/prm2bits.cpp \
 	src/q_gain_c.cpp \
 	src/q_gain_p.cpp \
 	src/qgain475.cpp \
 	src/qgain795.cpp \
 	src/qua_gain.cpp \
 	src/s10_8pf.cpp \
 	src/set_sign.cpp \
 	src/sid_sync.cpp \
 	src/sp_enc.cpp \
 	src/spreproc.cpp \
 	src/spstproc.cpp \
 	src/ton_stab.cpp \
 	src/vad1.cpp


LOCAL_MODULE := libpvencoder_gsmamr

LOCAL_CFLAGS :=   $(PV_CFLAGS)
LOCAL_ARM_MODE := arm

LOCAL_STATIC_LIBRARIES := 

LOCAL_SHARED_LIBRARIES := 

LOCAL_C_INCLUDES := \
	$(PV_TOP)/codecs_v2/audio/gsm_amr/amr_nb/enc/src \
 	$(PV_TOP)/codecs_v2/audio/gsm_amr/amr_nb/enc/include \
 	$(PV_TOP)/codecs_v2/audio/gsm_amr/amr_nb/common/include \
 	$(PV_INCLUDES)

LOCAL_COPY_HEADERS_TO := $(PV_COPY_HEADERS_TO)

LOCAL_COPY_HEADERS := \
 	include/gsmamr_encoder_wrapper.h

include $(BUILD_STATIC_LIBRARY)
