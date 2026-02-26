#---------------------------------------------------------------------------------
# Wave Browser - Wii U (WUT official)
#---------------------------------------------------------------------------------

TARGET      := wave-browser
BUILD       := build
SOURCES     := .
INCLUDES    :=

CFILES      := wave_browser/main.c

CFLAGS      := -Wall -Wextra -O2
CXXFLAGS    := $(CFLAGS)

LIBS        := -lcurl

ifeq ($(strip $(DEVKITPRO)),)
$(error Please set DEVKITPRO in your environment.)
endif

include $(DEVKITPRO)/wut/share/wut_rules
