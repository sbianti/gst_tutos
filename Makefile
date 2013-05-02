EXE = hello concepts dynamic_pipeline time_management playback_speed \
	dynamic_pipeline-2 playback_[1-9]

CC = gcc

GST_VERSION ?= 1.0
GTK_VERSION ?= 3.0

CFLAGS += `pkg-config --cflags gstreamer-$(GST_VERSION)`
CFLAGS_GTK = `pkg-config --cflags gtk+-$(GTK_VERSION)`

PKG_CONFIG_PATH ?= $(LOCAL_PKG_CONFIG_PATH)
export PKG_CONFIG_PATH

LDFLAGS ?= $(LOCAL_LDFLAGS)
LDFLAGS += `pkg-config --libs gstreamer-$(GST_VERSION)`
LDFLAGS_GTK = `pkg-config --libs gtk+-$(GTK_VERSION)`
LDFLAGS_GST_INTERFACES_0.10 = `pkg-config --libs gstreamer-interfaces-$(GST_VERSION)`
LDFLAGS_GST_INTERFACES_1.0 =`pkg-config --libs gstreamer-video-1.0`

ifeq ($(origin debug), undefined)
	CFLAGS += -O2
else
	CFLAGS += -g -O0
endif

all:
	@echo entrez un nom de programme à compiler \(sans l\'extension\)

gui_toolkit : CFLAGS += $(CFLAGS_GTK)
gui_toolkit : LDFLAGS += $(LDFLAGS_GTK) $(LDFLAGS_GST_INTERFACES_$(GST_VERSION))

.PHONY : clean
clean:
	-rm $(EXE) 2> /dev/null
