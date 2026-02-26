#---------------------------------------------------------------------------------
# Wave Browser - Wii U (WUT official build)
#---------------------------------------------------------------------------------

TARGET      := wave-browser
BUILD       := build

# Explicitly define source files (no auto-detection)
CFILES      := wave_browser/main.c

INCLUDES    :=

CFLAGS      := -Wall -Wextra -O2
CXXFLAGS    := $(CFLAGS)

LIBS        := -lcurl

ifeq ($(strip $(DEVKITPRO)),)
$(error Please set DEVKITPRO in your environment.)
endif

include $(DEVKITPRO)/wut/share/wut_rules
