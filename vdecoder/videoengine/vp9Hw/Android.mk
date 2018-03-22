
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

SCLIB_TOP=${LOCAL_PATH}/../../..
include ${SCLIB_TOP}/config.mk

vp9Hw_srcs_c   := vp9Hw.c
vp9Hw_srcs_c   += vp9Hw_dec.c
vp9Hw_srcs_c   += vp9Sha.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_decapi.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_entropymode.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_entropymv.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_modecont.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_modecontext.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_treecoder.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_asic.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_bool.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_buffer_queue.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_decoder.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_headers.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_output.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_probs.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_stream.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_regdrv.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_fifo.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_dwl_linux.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_dwl_activity_trace.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_dwl_linux_hw.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_commonconfig.c


vp9Hw_srcs_inc := 	$(LOCAL_PATH) \
	            $(LOCAL_PATH)/vp9Hw_include/ \
				$(SCLIB_TOP)/include/ \
				$(SCLIB_TOP)/ve/include \
				$(SCLIB_TOP)/base/include/ \
				$(SCLIB_TOP)/vdecoder/include \
				$(SCLIB_TOP)/vdecoder \

LOCAL_SRC_FILES := $(vp9Hw_srcs_c)
LOCAL_C_INCLUDES := $(vp9Hw_srcs_inc)
LOCAL_CFLAGS :=
LOCAL_LDFLAGS :=

LOCAL_MODULE_TAGS := optional

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libutils \
	libbinder \
	libui       \
	libdl       \
	libVE       \
	libvdecoder \
	libvideoengine \
	liblog

#libawavs
LOCAL_MODULE:= libawvp9Hw

include $(BUILD_SHARED_LIBRARY)

#===========================================
#========== build vendor lib ==============

ifeq ($(COMPILE_SO_IN_VENDOR), yes)

include $(CLEAR_VARS)

SCLIB_TOP=${LOCAL_PATH}/../../..
include ${SCLIB_TOP}/config.mk

vp9Hw_srcs_c   := vp9Hw.c
vp9Hw_srcs_c   += vp9Hw_dec.c
vp9Hw_srcs_c   += vp9Sha.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_decapi.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_entropymode.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_entropymv.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_modecont.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_modecontext.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_treecoder.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_asic.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_bool.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_buffer_queue.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_decoder.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_headers.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_output.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_probs.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_stream.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_regdrv.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_fifo.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_dwl_linux.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_dwl_activity_trace.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_dwl_linux_hw.c
vp9Hw_srcs_c   += vp9Hw_core/vp9hwd_commonconfig.c


vp9Hw_srcs_inc := 	$(LOCAL_PATH) \
	            $(LOCAL_PATH)/vp9Hw_include/ \
				$(SCLIB_TOP)/include/ \
				$(SCLIB_TOP)/ve/include \
				$(SCLIB_TOP)/base/include/ \
				$(SCLIB_TOP)/vdecoder/include \
				$(SCLIB_TOP)/vdecoder \

LOCAL_SRC_FILES := $(vp9Hw_srcs_c)
LOCAL_C_INCLUDES := $(vp9Hw_srcs_inc)
LOCAL_CFLAGS :=
LOCAL_LDFLAGS :=

LOCAL_MODULE_TAGS := optional

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libutils \
	libbinder \
	libui       \
	libdl       \
	libomx_VE       \
	libomx_vdecoder \
	libomx_videoengine \
	liblog

#libawavs
LOCAL_MODULE:= libomx_vp9Hw
LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_SHARED_LIBRARY)

endif
