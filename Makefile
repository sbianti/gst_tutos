EXE = basic-1_hello basic-2_concepts basic-3_dynamic_pipelines \
	basic-3b_dynamic_pipelines basic-4_time_management basic-5_gui_toolkit \
	basic-6_media-format_and_pad-caps playback-1_playbin \
	basic-6b_media-format_and_pad-caps playback-2_sub-management \
	basic-7_multithreading_and_pad_availability playback-5_color-balance \
	basic-8_short-cutting_pipeline playback-3_short-cutting_pipeline \
	basic-9_media-info-gathering basic-12_streaming basic-15_clutter \
	playback-4_progressive_streaming basic-13_playback-speed \
	playback-6_audio-visualization playback-7_custom-playbin-sinks \
	playback-7b_custom-playbin-sinks

CC = gcc

GST_VERSION ?= 1.0
GTK_VERSION ?= 3.0

CFLAGS += `pkg-config --cflags gstreamer-$(GST_VERSION)`
CFLAGS_GTK = `pkg-config --cflags gtk+-$(GTK_VERSION)`
CFLAGS_CLUTTER_GST = `pkg-config --cflags clutter-gst-1.0`

PKG_CONFIG_PATH ?= $(LOCAL_PKG_CONFIG_PATH)
export PKG_CONFIG_PATH

LDFLAGS ?= $(LOCAL_LDFLAGS)
LDFLAGS += `pkg-config --libs gstreamer-$(GST_VERSION)`
LDFLAGS_GTK = `pkg-config --libs gtk+-$(GTK_VERSION)`
LDFLAGS_GST_INTERFACES_0.10 = `pkg-config --libs gstreamer-interfaces-$(GST_VERSION)`
LDFLAGS_GST_INTERFACES_1.0 =`pkg-config --libs gstreamer-video-1.0`
LDFLAGS_GST_PBUTILS = `pkg-config --libs gstreamer-pbutils-$(GST_VERSION)`
LDFLAGS_CLUTTER_GST = `pkg-config --libs clutter-gst-1.0`

ifeq ($(origin debug), undefined)
	CFLAGS += -O2
else
	CFLAGS += -g -O0
endif

all:
	@echo entrez un nom de programme à compiler \(sans l\'extension\)

basic-5_gui_toolkit : CFLAGS += $(CFLAGS_GTK)
basic-5_gui_toolkit : LDFLAGS += $(LDFLAGS_GTK) $(LDFLAGS_GST_INTERFACES_$(GST_VERSION))

basic-9_media-info-gathering : LDFLAGS += $(LDFLAGS_GST_PBUTILS)

basic-15_clutter : CFLAGS += $(CFLAGS_CLUTTER_GST)
basic-15_clutter : LDFLAGS += $(LDFLAGS_CLUTTER_GST)

playback-5_color-balance : LDFLAGS += $(LDFLAGS_GST_INTERFACES_$(GST_VERSION))

.PHONY : clean
clean:
	-rm $(EXE) 2> /dev/null
