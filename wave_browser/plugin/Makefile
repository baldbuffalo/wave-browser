#---------------------------------------------------------------------------------
# WaveBrowserRemote Aroma plugin
# Output: WaveBrowserRemote.wps
# Requires: wups package from devkitPro (dkp-pacman -S wiiu-wups)
#---------------------------------------------------------------------------------

TARGET  := WaveBrowserRemote
BUILD   := build_plugin

DEVKITPRO  ?= /opt/devkitpro
DEVKITPPC  := $(DEVKITPRO)/devkitPPC
WUPS_ROOT  := $(DEVKITPRO)/wups

CC   := $(DEVKITPPC)/bin/powerpc-eabi-gcc
CXX  := $(DEVKITPPC)/bin/powerpc-eabi-g++
LD   := $(CXX)

CXXFLAGS := \
    -O2 -Wall -mcpu=750 -meabi -mhard-float \
    -ffunction-sections -fdata-sections \
    -std=c++17 -fno-exceptions -fno-rtti \
    -I$(WUPS_ROOT)/include \
    -I$(DEVKITPRO)/wut/include \
    -I.. \
    -DWUPS_PLUGIN

LDFLAGS := \
    -specs=$(WUPS_ROOT)/share/wups.specs \
    -Wl,--gc-sections \
    -L$(WUPS_ROOT)/lib \
    -L$(DEVKITPRO)/wut/lib

LIBS := -lwups -lwut

CXXFILES := WaveBrowserRemote.cpp
OFILES   := $(patsubst %.cpp,$(BUILD)/%.o,$(CXXFILES))

all: $(TARGET).wps

$(BUILD)/%.o: %.cpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET).elf: $(OFILES)
	$(LD) $(OFILES) $(LDFLAGS) $(LIBS) -o $@

$(TARGET).wps: $(TARGET).elf
	$(DEVKITPRO)/tools/bin/wpsstrip $< $@

clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).wps

.PHONY: all clean
