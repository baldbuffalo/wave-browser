#-------------------------------------------------------------------------------
.SUFFIXES:
#-------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

CURDIR := $(shell pwd)
TARGET := wave-browser
BUILD  := build
SOURCES := wave_browser

#-------------------------------------------------------------------------------
# Compiler Flags
#-------------------------------------------------------------------------------
CFLAGS   := -Wall -Wextra -ffunction-sections -fdata-sections -D__WIIU__
CFLAGS   += -I$(DEVKITPRO)/wut/include -I$(DEVKITPRO)/portlibs/wiiu/include

CXXFLAGS := $(CFLAGS) -std=gnu++20
LDFLAGS  := -Wl,--gc-sections
LIBS     := -lwut -lcurl

#-------------------------------------------------------------------------------
# Source files
#-------------------------------------------------------------------------------
CFILES := $(CURDIR)/wave_browser/main.c
OFILES := $(CFILES:%.c=$(BUILD)/%.o)

#-------------------------------------------------------------------------------
# Rules
#-------------------------------------------------------------------------------
all: $(BUILD)/$(TARGET).rpx

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OFILES)
	$(CXX) $(OFILES) $(LDFLAGS) -L$(DEVKITPRO)/wut/lib -L$(DEVKITPRO)/portlibs/wiiu/lib $(LIBS) -o $@

$(BUILD)/$(TARGET).rpx: $(BUILD)/$(TARGET).elf
	$(DEVKITPRO)/wut/bin/wut-make-rpx $< -o $@

clean:
	rm -rf $(BUILD)

.PHONY: all clean
