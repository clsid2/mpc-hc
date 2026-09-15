LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	src/add.cpp \
 	src/az_lsp.cpp \
 	src/bitno_tab.cpp \
 	src/bitreorder_tab.cpp \
 	src/c2_9pf_tab.cpp \
 	src/div_s.cpp \
 	src/gains_tbl.cpp \
 	src/gc_pred.cpp \
 	src/get_const_tbls.cpp \
 	src/gmed_n.cpp \
 	src/grid_tbl.cpp \
 	src/gray_tbl.cpp \
 	src/int_lpc.cpp \
 	src/inv_sqrt.cpp \
 	src/inv_sqrt_tbl.cpp \
 	src/l_shr_r.cpp \
 	src/log2.cpp \
 	src/log2_norm.cpp \
 	src/log2_tbl.cpp \
 	src/lsfwt.cpp \
 	src/lsp.cpp \
 	src/lsp_az.cpp \
 	src/lsp_lsf.cpp \
 	src/lsp_lsf_tbl.cpp \
 	src/lsp_tab.cpp \
 	src/mult_r.cpp \
 	src/norm_l.cpp \
 	src/norm_s.cpp \
 	src/overflow_tbl.cpp \
 	src/ph_disp_tab.cpp \
 	src/pow2.cpp \
 	src/pow2_tbl.cpp \
 	src/pred_lt.cpp \
 	src/q_plsf.cpp \
 	src/q_plsf_3.cpp \
 	src/q_plsf_3_tbl.cpp \
 	src/q_plsf_5.cpp \
 	src/q_plsf_5_tbl.cpp \
 	src/qua_gain_tbl.cpp \
 	src/reorder.cpp \
 	src/residu.cpp \
 	src/round.cpp \
 	src/shr.cpp \
 	src/shr_r.cpp \
 	src/sqrt_l.cpp \
 	src/sqrt_l_tbl.cpp \
 	src/sub.cpp \
 	src/syn_filt.cpp \
 	src/weight_a.cpp \
 	src/window_tab.cpp


LOCAL_MODULE := libpv_amr_nb_common_lib

LOCAL_CFLAGS :=   $(PV_CFLAGS)
LOCAL_ARM_MODE := arm

LOCAL_STATIC_LIBRARIES := 

LOCAL_SHARED_LIBRARIES := 

LOCAL_C_INCLUDES := \
	$(PV_TOP)/codecs_v2/audio/gsm_amr/amr_nb/common/src \
 	$(PV_TOP)/codecs_v2/audio/gsm_amr/amr_nb/common/include \
 	$(PV_INCLUDES)

LOCAL_COPY_HEADERS_TO := $(PV_COPY_HEADERS_TO)

LOCAL_COPY_HEADERS := \
 	

include $(BUILD_STATIC_LIBRARY)
