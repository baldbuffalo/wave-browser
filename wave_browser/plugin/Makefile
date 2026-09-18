TARGET  := WaveBrowserRemote
BUILD   := build_plugin

DEVKITPRO  ?= /opt/devkitpro
DEVKITPPC  := $(DEVKITPRO)/devkitPPC
WUPS_ROOT  := $(DEVKITPRO)/wups
WUT_ROOT   := $(DEVKITPRO)/wut

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

# The WUPS linker script only defines the WUPS-specific sections and relies
# on the WUT linker environment for loadmem/datamem. WUPSSPECS provides both
# the WUPS linker script and the WUT specs needed to define those regions.
LDFLAGS := \
    -Wl,--gc-sections \
    -L$(WUPS_ROOT)/lib \
    -L$(WUT_ROOT)/lib \
    -L$(DEVKITPRO)/portlibs/wiiu/lib

WUPSSPECS := -T$(WUPS_ROOT)/share/wups.ld -specs=$(WUPS_ROOT)/share/wups.specs

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
	$(CXX) $(OFILES) $(WUPSSPECS) $(LDFLAGS) $(LIBS) -o $@

$(TARGET).wps: $(TARGET).elf
	@cp $< $*.strip.elf
	@$(DEVKITPPC)/bin/powerpc-eabi-strip -g $*.strip.elf
	@elf2rpl $*.strip.elf $@
	@printf 'PL' | dd of=$@ bs=1 seek=9 count=2 conv=notrunc status=none
	@rm $*.strip.elf
	@echo "built ... $@"

clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).wps $(TARGET).strip.elf

