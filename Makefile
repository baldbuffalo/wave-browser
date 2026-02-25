# -----------------------------------------------------------------------------
# Wave Browser Makefile for Wii U (devkitPro Docker)
# -----------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=/opt/devkitpro")
endif

TOPDIR ?= $(CURDIR)

TARGET    := wave-browser
BUILD     := build
SOURCES   := wave_browser
INCLUDES  := $(DEVKITPRO)/wut/include $(DEVKITPRO)/portlibs/wiiu/include

# -----------------------------------------------------------------------------
# Compiler and Linker Flags
# -----------------------------------------------------------------------------
CFLAGS   := -Wall -Wextra -ffunction-sections -fdata-sections -D__WIIU__
CFLAGS   += $(foreach dir,$(INCLUDES),-I$(dir))

CXXFLAGS := $(CFLAGS) -std=gnu++20
ASFLAGS  := $(CFLAGS)
LDFLAGS  := -Wl,--gc-sections -L$(DEVKITPRO)/wut/lib -L$(DEVKITPRO)/portlibs/wiiu/lib
LIBS     := -lwut -lwhb -lcurl

CC  := $(DEVKITPPC)/bin/powerpc-eabi-gcc
CXX := $(DEVKITPPC)/bin/powerpc-eabi-g++
AS  := $(DEVKITPPC)/bin/powerpc-eabi-as
LD  := $(CXX)

# -----------------------------------------------------------------------------
# Files
# -----------------------------------------------------------------------------
CFILES   := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.c))
CPPFILES := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.cpp))
SFILES   := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.s))

OFILES := $(CFILES:%.c=$(BUILD)/%.o) $(CPPFILES:%.cpp=$(BUILD)/%.o) $(SFILES:%.s=$(BUILD)/%.o)

# -----------------------------------------------------------------------------
# Default target
# -----------------------------------------------------------------------------
.PHONY: all clean

all: $(BUILD)/$(TARGET).rpx $(BUILD)/$(TARGET).elf

# -----------------------------------------------------------------------------
# Build object files
# -----------------------------------------------------------------------------
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%.o: %.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

# -----------------------------------------------------------------------------
# Link targets
# -----------------------------------------------------------------------------
$(BUILD)/$(TARGET).elf: $(OFILES)
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD)/$(TARGET).rpx: $(BUILD)/$(TARGET).elf
	@echo "Generating RPX..."
	$(DEVKITPRO)/wut/bin/make_rpx $< $@

# -----------------------------------------------------------------------------
# Clean
# -----------------------------------------------------------------------------
clean:
	@rm -rf $(BUILD)
