#---------------------------------------------------------------------------------
# Project name
#---------------------------------------------------------------------------------
TARGET      := WaveBrowser
BUILD       := build
SOURCES     := wave_browser
INCLUDES    :=

#---------------------------------------------------------------------------------
# Devkit paths
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
# Flags
#---------------------------------------------------------------------------------
CFLAGS  := -O2 -Wall -mcpu=750 -meabi -mhard-float \
           -ffunction-sections -fdata-sections \
           -I$(WUT_ROOT)/include \
           -I$(DEVKITPRO)/portlibs/wiiu/include \
           -I$(DEVKITPRO)/libogc/include

LDFLAGS := -specs=$(WUT_ROOT)/share/wut.specs \
           -Wl,--gc-sections \
           -L$(WUT_ROOT)/lib \
           -L$(DEVKITPRO)/portlibs/wiiu/lib

# IMPORTANT: these fix your missing symbols
LIBS    := -lwut -lnsysnet -lm

#---------------------------------------------------------------------------------
# Source listing
#---------------------------------------------------------------------------------
CFILES      := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.c))
OFILES      := $(patsubst %.c,$(BUILD)/%.o,$(CFILES))

#---------------------------------------------------------------------------------
# Rules
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
