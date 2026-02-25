# -----------------------------------------------------------------------------
# Wave Browser – Wii U (WUT + curl, no WHB)
# -----------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error DEVKITPRO is not set. Use /opt/devkitpro)
endif

TARGET  := wave-browser
BUILD   := build
SOURCES := wave_browser

INCLUDES := -I$(DEVKITPRO)/wut/include \
            -I$(DEVKITPRO)/portlibs/wiiu/include

CFLAGS   := -Wall -Wextra -ffunction-sections -fdata-sections -D__WIIU__ $(INCLUDES)
CXXFLAGS := $(CFLAGS) -std=gnu++20

LDFLAGS  := -Wl,--gc-sections \
            -L$(DEVKITPRO)/wut/lib \
            -L$(DEVKITPRO)/portlibs/wiiu/lib

LIBS := -lwut -lcurl

CC  := $(DEVKITPPC)/bin/powerpc-eabi-gcc
CXX := $(DEVKITPPC)/bin/powerpc-eabi-g++
LD  := $(CXX)

CFILES   := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.c))
CPPFILES := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.cpp))

OFILES := $(CFILES:%.c=$(BUILD)/%.o) \
          $(CPPFILES:%.cpp=$(BUILD)/%.o)

.PHONY: all clean

all: $(BUILD)/$(TARGET).rpx

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OFILES)
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD)/$(TARGET).rpx: $(BUILD)/$(TARGET).elf
	$(DEVKITPRO)/wut/bin/make_rpx $< $@

clean:
	rm -rf $(BUILD)
