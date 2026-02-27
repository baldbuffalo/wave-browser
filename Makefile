# Paths
DEVKITPPC ?= /opt/devkitpro/devkitPPC
WUT ?= /opt/devkitpro/wut
PORTLIBS ?= /opt/devkitpro/portlibs/wiiu

# Compiler flags
CC := $(DEVKITPPC)/bin/powerpc-eabi-gcc
CFLAGS := -O2 -Wall -I$(WUT)/include -I$(PORTLIBS)/include
LDFLAGS := -L$(WUT)/lib -L$(PORTLIBS)/lib -lwut -lcurl -lm -specs=$(WUT)/share/wut.specs

# Source & output
SRC := wave_browser/main.c
BUILD_DIR := build/wavebrowser
OUT := $(BUILD_DIR)/wave_browser.wuhb

.PHONY: all clean

all: $(OUT)

$(OUT): $(SRC)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	@echo "Cleaning build folder..."
	@rm -rf $(BUILD_DIR)
