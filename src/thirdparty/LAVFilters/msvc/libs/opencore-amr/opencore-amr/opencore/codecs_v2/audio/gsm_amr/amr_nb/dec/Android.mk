LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	src/decoder_gsm_amr.cpp \
 	src/a_refl.cpp \
 	src/agc.cpp \
 	src/amrdecode.cpp \
 	src/b_cn_cod.cpp \
 	src/bgnscd.cpp \
 	src/c_g_aver.cpp \
 	src/d1035pf.cpp \
 	src/d2_11pf.cpp \
 	src/d2_9pf.cpp \
 	src/d3_14pf.cpp \
 	src/d4_17pf.cpp \
 	src/d8_31pf.cpp \
 	src/d_gain_c.cpp \
 	src/d_gain_p.cpp \
 	src/d_plsf.cpp \
 	src/d_plsf_3.cpp \
 	src/d_plsf_5.cpp \
 	src/dec_amr.cpp \
 	src/dec_gain.cpp \
 	src/dec_input_format_tab.cpp \
 	src/dec_lag3.cpp \
 	src/dec_lag6.cpp \
 	src/dtx_dec.cpp \
 	src/ec_gains.cpp \
 	src/ex_ctrl.cpp \
 	src/if2_to_ets.cpp \
 	src/int_lsf.cpp \
 	src/lsp_avg.cpp \
 	src/ph_disp.cpp \
 	src/post_pro.cpp \
 	src/preemph.cpp \
 	src/pstfilt.cpp \
 	src/qgain475_tab.cpp \
 	src/sp_dec.cpp \
 	src/wmf_to_ets.cpp


LOCAL_MODULE := libpvdecoder_gsmamr

LOCAL_CFLAGS :=   $(PV_CFLAGS)
LOCAL_ARM_MODE := arm

LOCAL_STATIC_LIBRARIES := 

LOCAL_SHARED_LIBRARIES := 

LOCAL_C_INCLUDES := \
	$(PV_TOP)/codecs_v2/audio/gsm_amr/amr_nb/dec/src \
 	$(PV_TOP)/codecs_v2/audio/gsm_amr/amr_nb/dec/include \
 	$(PV_TOP)/codecs_v2/audio/gsm_amr/amr_nb/common/include \
 	$(PV_INCLUDES)

LOCAL_COPY_HEADERS_TO := $(PV_COPY_HEADERS_TO)

LOCAL_COPY_HEADERS := \
	include/decoder_gsm_amr.h \
 	include/pvamrnbdecoder_api.h

include $(BUILD_STATIC_LIBRARY)
