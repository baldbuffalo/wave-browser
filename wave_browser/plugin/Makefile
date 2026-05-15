TARGET  := WaveBrowserRemote
BUILD   := build_plugin

DEVKITPRO  ?= /opt/devkitpro
DEVKITPPC  := $(DEVKITPRO)/devkitPPC
WUPS_ROOT  := $(DEVKITPRO)/wups

CXX := $(DEVKITPPC)/bin/powerpc-eabi-g++
STRIP := $(DEVKITPPC)/bin/powerpc-eabi-strip

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
	$(CXX) $(OFILES) $(LDFLAGS) $(LIBS) -o $@

# Strip the ELF and rename to .wps — Aroma accepts stripped ELF as .wps
$(TARGET).wps: $(TARGET).elf
	$(STRIP) $< -o $@

clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).wps

.PHONY: all clean
