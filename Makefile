# -------------------------------------------------------------------
# Makefile for Wave Browser (Wii U) WUHB
# -------------------------------------------------------------------

# Compiler
CC := /opt/devkitpro/devkitPPC/bin/powerpc-eabi-gcc

# Flags
CFLAGS := -O2 -Wall \
          -I/opt/devkitpro/wut/include \
          -I/opt/devkitpro/portlibs/wiiu/include

LDFLAGS := -L/opt/devkitpro/wut/lib \
           -L/opt/devkitpro/portlibs/wiiu/lib \
           -lwut -lcurl -lm \
           -specs=/opt/devkitpro/wut/share/wut.specs \
           -Wl,-Map,build/wavebrowser/wave_browser.map

# Source
SRC := wave_browser/main.c

# Output folder & file
OUT_DIR := build/wavebrowser
OUT := $(OUT_DIR)/wave_browser.rpx

# -------------------------------------------------------------------
# Targets
# -------------------------------------------------------------------

all: $(OUT)

$(OUT): $(SRC)
	@echo "Building Wave Browser WUHB..."
	@mkdir -p $(OUT_DIR)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)

clean:
	@echo "Cleaning build folder..."
	@rm -rf build

.PHONY: all clean
