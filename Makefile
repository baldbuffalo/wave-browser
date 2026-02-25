# -----------------------------------------------------------------------------
# Wave Browser – Wii U (devkitPro Docker)
# -----------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error DEVKITPRO is not set. Use /opt/devkitpro)
endif

TARGET  := wave-browser
BUILD   := build
SOURCES := wave_browser

# -----------------------------------------------------------------------------
# Include Paths
# -----------------------------------------------------------------------------

INCLUDES := -I$(DEVKITPRO)/wut/include \
            -I$(DEVKITPRO)/portlibs/wiiu/include

# -----------------------------------------------------------------------------
# Compiler & Linker Flags
# -----------------------------------------------------------------------------

CFLAGS   := -Wall -Wextra -ffunction-sections -fdata-sections -D__WIIU__ $(INCLUDES)
CXXFLAGS := $(CFLAGS) -std=gnu++20

LDFLAGS  := -Wl,--gc-sections \
            -L$(DEVKITPRO)/wut/lib \
            -L$(DEVKITPRO)/portlibs/wiiu/lib

LIBS := -lwut -lwhb -lcurl

CC  := $(DEVKITPPC)/bin/powerpc-eabi-gcc
CXX := $(DEVKITPPC)/bin/powerpc-eabi-g++
LD  := $(CXX)

# -----------------------------------------------------------------------------
# Source Files
# -----------------------------------------------------------------------------

CFILES   := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.c))
CPPFILES := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.cpp))

OFILES := $(CFILES:%.c=$(BUILD)/%.o) \
          $(CPPFILES:%.cpp=$(BUILD)/%.o)

# -----------------------------------------------------------------------------
# Targets
# -----------------------------------------------------------------------------

.PHONY: all clean

all: $(BUILD)/$(TARGET).rpx

# Compile C
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile C++
$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link ELF
$(BUILD)/$(TARGET).elf: $(OFILES)
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

# Convert ELF → RPX
$(BUILD)/$(TARGET).rpx: $(BUILD)/$(TARGET).elf
	$(DEVKITPRO)/wut/bin/make_rpx $< $@

# Clean
clean:
	rm -rf $(BUILD)
