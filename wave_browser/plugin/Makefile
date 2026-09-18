TARGET  := WaveBrowserRemote
BUILD   := build_plugin

DEVKITPRO  ?= /opt/devkitpro
DEVKITPPC  := $(DEVKITPRO)/devkitPPC
WUPS_ROOT  := $(DEVKITPRO)/wups
WUT_ROOT   := $(DEVKITPRO)/wut

# Use the official WUPS build rules so the plugin linker script and WPS
# packaging format are applied correctly.
include $(WUPS_ROOT)/share/wups_rules

CXX := $(DEVKITPPC)/bin/powerpc-eabi-g++

CXXFLAGS := \
    -O2 -Wall -mcpu=750 -meabi -mhard-float \
    -ffunction-sections -fdata-sections \
    -std=c++17 -fno-exceptions -fno-rtti \
    -I$(WUPS_ROOT)/include \
    -I$(WUT_ROOT)/include \
    -I$(DEVKITPRO)/portlibs/wiiu/include \
    -I.. \
    -DWUPS_PLUGIN

LDFLAGS := \
    -T$(WUPS_ROOT)/share/wups.ld \
    -specs=$(WUPS_ROOT)/share/wups.specs \
    -Wl,--gc-sections \
    -L$(WUPS_ROOT)/lib \
    -L$(WUT_ROOT)/lib \
    -L$(DEVKITPRO)/portlibs/wiiu/lib

# ACP is loaded at runtime from acp.rpl via OSDynLoad.
# There is no static libacp in the current devkitPro Wii U environment.
LIBS := -lwups -lwut

CXXFILES := WaveBrowserRemote.cpp
OFILES   := $(patsubst %.cpp,$(BUILD)/%.o,$(CXXFILES))

.PHONY: all clean

all: $(TARGET).wps

$(BUILD)/%.o: %.cpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET).elf: $(OFILES)
	$(CXX) $(OFILES) $(LDFLAGS) $(LIBS) -o $@

# The %.wps rule is supplied by wups_rules and uses elf2rpl to create
# a valid WUPS plugin file instead of simply stripping the ELF.

clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).wps
