#---------------------------------------------------------------------------------
# Wave Browser - Wii U (WUT build system)
#---------------------------------------------------------------------------------

TARGET      := wave-browser
BUILD       := build
SOURCES     := wave_browser
DATA        :=
INCLUDES    :=

CFLAGS      := -Wall -Wextra -O2
CXXFLAGS    := $(CFLAGS)

LIBS        := -lcurl

#---------------------------------------------------------------------------------
# DO NOT EDIT BELOW THIS LINE
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error Please set DEVKITPRO in your environment.)
endif

include $(DEVKITPRO)/wut/share/wut_rules
