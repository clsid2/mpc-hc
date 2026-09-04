LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
 	




LOCAL_CFLAGS :=   $(PV_CFLAGS)


LOCAL_STATIC_LIBRARIES := 

LOCAL_SHARED_LIBRARIES := 

LOCAL_C_INCLUDES := \
	$(PV_TOP)/codecs_v2/audio/gsm_amr/common/dec/build/make \
 	$(PV_TOP)/codecs_v2/audio/gsm_amr/common/dec/include \
 	$(PV_INCLUDES)

LOCAL_COPY_HEADERS_TO := $(PV_COPY_HEADERS_TO)

LOCAL_COPY_HEADERS := \
 	include/pvgsmamrdecoderinterface.h

include $(BUILD_COPY_HEADERS)
