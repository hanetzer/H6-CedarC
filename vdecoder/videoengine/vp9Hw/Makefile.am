
include $(top_srcdir)/Makefile.inc

pkglib_LTLIBRARIES = libcdc_vd_vp9Hw.la

libcdc_vd_vp9Hw_la_SOURCES = vp9Hw.c  \
                             vp9Hw_dec.c \
                             vp9Sha.c \
                             vp9Hw_core/vp9hwd_decapi.c \
                             vp9Hw_core/vp9hwd_entropymode.c \
                             vp9Hw_core/vp9hwd_entropymv.c \
                             vp9Hw_core/vp9hwd_modecont.c \
                             vp9Hw_core/vp9hwd_modecontext.c \
                             vp9Hw_core/vp9hwd_treecoder.c \
                             vp9Hw_core/vp9hwd_asic.c \
                             vp9Hw_core/vp9hwd_bool.c \
                             vp9Hw_core/vp9hwd_buffer_queue.c \
                             vp9Hw_core/vp9hwd_decoder.c \
                             vp9Hw_core/vp9hwd_headers.c \
                             vp9Hw_core/vp9hwd_output.c \
                             vp9Hw_core/vp9hwd_probs.c \
                             vp9Hw_core/vp9hwd_stream.c \
                             vp9Hw_core/vp9hwd_regdrv.c \
                             vp9Hw_core/vp9hwd_fifo.c \
                             vp9Hw_core/vp9hwd_dwl_linux.c \
                             vp9Hw_core/vp9hwd_dwl_activity_trace.c \
                             vp9Hw_core/vp9hwd_dwl_linux_hw.c \
                             vp9Hw_core/vp9hwd_commonconfig.c

libcdc_vd_vp9Hw_la_CFLAGS = $(CFLAGS_CDCG)
LOCAL_INCLUDE = \
				-I$(top_srcdir)/include \
				-I$(top_srcdir)/vdecoder/include \
                -I$(top_srcdir)/vdecoder/videoengine/vp9Hw/vp9Hw_include/ \
                -I$(top_srcdir)/vdecoder/videoengine/vp9Hw/ \
				-I$(top_srcdir)/ve/include \
				-I$(top_srcdir)/base/include \
				-I$(top_srcdir)/vdecoder


libcdc_vd_vp9Hw_la_CFLAGS += $(LOCAL_INCLUDE)


libcdc_vd_vp9Hw_la_LDFLAGS = $(LDFLAGS_CDCG)

libcdc_vd_vp9Hw_la_LIBADD = \
				$(top_srcdir)/vdecoder/libcdc_vdecoder.la \
				$(top_srcdir)/vdecoder/videoengine/libcdc_videoengine.la \
				$(top_srcdir)/ve/libcdc_ve.la

libcdc_vd_vp9Hw_la_LDFLAGS += $(LOCAL_LIB)

