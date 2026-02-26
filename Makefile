#-------------------------------------------------------------------------------
# Makefile for Wave Browser (WiiU)
#-------------------------------------------------------------------------------

# Toolchain paths (preinstalled in devkitpro/devkitppc container)
DEVKITPRO ?= /opt/devkitpro
DEVKITPPC := $(DEVKITPRO)/devkitPPC
WUT := $(DEVKITPRO)/wut
WIIU_CURL := $(DEVKITPRO)/wiiu-curl

# Compiler and flags
CC := $(DEVKITPPC)/bin/powerpc-eabi-gcc
CFLAGS := -O2 -Wall \
           -I$(WUT)/include \
           -I$(WIIU_CURL)/include
LDFLAGS := -L$(WUT)/lib -L$(WIIU_CURL)/lib \
           -lwut -lproc_ui -lvpad -lcurl -lcoreinit -lm

# Source and build directories
SRC_DIR := wave_browser
BUILD_DIR := build
TARGET := $(BUILD_DIR)/wave_browser.rpx

# Sources
SOURCES := $(SRC_DIR)/main.c

#-------------------------------------------------------------------------------
# Build rules
#-------------------------------------------------------------------------------

all: $(TARGET)

$(TARGET): $(SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)
