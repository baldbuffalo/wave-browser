# Makefile for Wave Browser (Wii U)

# Toolchain paths (from devkitPro pacman install)
export DEVKITPRO ?= /opt/devkitpro
export DEVKITPPC ?= $(DEVKITPRO)/devkitPPC
export PATH := $(DEVKITPPC)/bin:$(PATH)

# Project
TARGET = wave_browser
SRC = wave_browser/main.c
BUILD_DIR = build
OUTPUT = $(BUILD_DIR)/$(TARGET).rpx

# Compiler flags
CFLAGS = -O2 -Wall -I$(DEVKITPRO)/wut/include
LDFLAGS = -L$(DEVKITPRO)/wut/lib -lwut -lproc_ui -lvpad -lcurl -lcoreinit -lm

all: $(OUTPUT)

$(OUTPUT): $(SRC) | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)
	$(DEVKITPPC)/bin/powerpc-eabi-gcc $(CFLAGS) $(SRC) -o $(OUTPUT) $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
