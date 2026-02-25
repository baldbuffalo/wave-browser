# -----------------------------------------------------------------------------
# devkitPro / WUT Makefile for Wave Browser (WiiU)
# -----------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment (e.g., /opt/devkitpro).")
endif

TOPDIR ?= $(CURDIR)
BUILD   ?= build
TARGET  ?= wave-browser

# Source folder
SOURCES := wave_browser

# -----------------------------------------------------------------------------
# Compiler / Toolchain
# -----------------------------------------------------------------------------
CC      := $(DEVKITPPC)/bin/powerpc-eabi-gcc
CXX     := $(DEVKITPPC)/bin/powerpc-eabi-g++
LD      := $(CXX)
AR      := $(DEVKITPPC)/bin/powerpc-eabi-ar

# -----------------------------------------------------------------------------
# Compiler flags
# -----------------------------------------------------------------------------
CFLAGS   := -Wall -Wextra -ffunction-sections -fdata-sections -D__WIIU__ -I$(DEVKITPRO)/wut/include
CXXFLAGS := $(CFLAGS) -std=gnu++20
ASFLAGS  := 
LDFLAGS  := -Wl,--gc-sections -L$(DEVKITPRO)/wut/lib -L$(DEVKITPRO)/portlibs/wiiu/lib

# Libraries
LIBS := -lwut -lwhb -lcurl

# -----------------------------------------------------------------------------
# Files
# -----------------------------------------------------------------------------
CFILES   := $(wildcard $(foreach dir,$(SOURCES),$(dir)/*.c))
CPPFILES := $(wildcard $(foreach dir,$(SOURCES),$(dir)/*.cpp))
OFILES   := $(CFILES:%.c=$(BUILD)/%.o) $(CPPFILES:%.cpp=$(BUILD)/%.o)

# -----------------------------------------------------------------------------
# Targets
# -----------------------------------------------------------------------------
.PHONY: all clean rpx elf

all: rpx elf

rpx: $(BUILD)/$(TARGET).rpx

elf: $(BUILD)/$(TARGET).elf

# -----------------------------------------------------------------------------
# Build rules
# -----------------------------------------------------------------------------
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OFILES)
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

# Convert ELF to RPX
$(BUILD)/$(TARGET).rpx: $(BUILD)/$(TARGET).elf
	@echo "Creating RPX..."
	wut-make-rpx $< $@

# -----------------------------------------------------------------------------
# Clean
# -----------------------------------------------------------------------------
clean:
	rm -rf $(BUILD)
