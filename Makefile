#---------------------------------------------------------------------------------
# Project
#---------------------------------------------------------------------------------
TARGET      := WaveBrowser
BUILD       := build
SOURCES     := wave_browser

#---------------------------------------------------------------------------------
# Devkit Paths
#---------------------------------------------------------------------------------
DEVKITPRO   ?= /opt/devkitpro
DEVKITPPC   := $(DEVKITPRO)/devkitPPC
WUT_ROOT    := $(DEVKITPRO)/wut

#---------------------------------------------------------------------------------
# Tools
#---------------------------------------------------------------------------------
CC          := $(DEVKITPPC)/bin/powerpc-eabi-gcc
CXX         := $(DEVKITPPC)/bin/powerpc-eabi-g++
LD          := $(CXX)

#---------------------------------------------------------------------------------
# Compiler Flags
#---------------------------------------------------------------------------------
CFLAGS  := -O2 -Wall -mcpu=750 -meabi -mhard-float \
           -ffunction-sections -fdata-sections \
           -I$(WUT_ROOT)/include \
           -I$(DEVKITPRO)/portlibs/wiiu/include \
           -I$(DEVKITPRO)/libogc/include

#---------------------------------------------------------------------------------
# Linker Flags
#---------------------------------------------------------------------------------
LDFLAGS := -specs=$(WUT_ROOT)/share/wut.specs \
           -Wl,--gc-sections \
           -L$(WUT_ROOT)/lib \
           -L$(DEVKITPRO)/portlibs/wiiu/lib

# IMPORTANT: only these libraries
LIBS    := -lwut -lm

#---------------------------------------------------------------------------------
# Source Files
#---------------------------------------------------------------------------------
CFILES  := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.c))
OFILES  := $(patsubst %.c,$(BUILD)/%.o,$(CFILES))

#---------------------------------------------------------------------------------
# Build Rules
#---------------------------------------------------------------------------------
all: $(TARGET).wuhb

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).rpx: $(OFILES)
	$(LD) $^ $(LDFLAGS) $(LIBS) -o $@

$(TARGET).wuhb: $(TARGET).rpx
	wuhbtool $(TARGET).rpx $(TARGET).wuhb

clean:
	rm -rf $(BUILD) *.rpx *.wuhb

.PHONY: all clean
