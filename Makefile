# -----------------------------
# Wave Browser Makefile (Wii U)
# -----------------------------

APP_NAME = WaveBrowser
SRC = wave_browser/main.c
BUILD_DIR = build
OUTPUT_RPX = $(APP_NAME).rpx
OUTPUT_WUHB = $(APP_NAME).wuhb

DEVKITPRO ?= /opt/devkitpro
DEVKITPPC ?= $(DEVKITPRO)/devkitPPC
WUT_ROOT ?= $(DEVKITPRO)/wut
PORTLIBS ?= $(DEVKITPRO)/portlibs/wiiu

CC = $(DEVKITPPC)/bin/powerpc-eabi-gcc
CFLAGS = -O2 -Wall -mcpu=750 -meabi -mhard-float -ffunction-sections -fdata-sections \
         -I$(WUT_ROOT)/include -I$(PORTLIBS)/include -I$(DEVKITPRO)/libogc/include
# Remove -lsysbase and -lc (WUT provides its own minimal C runtime)
LDFLAGS = -specs=$(WUT_ROOT)/share/wut.specs -Wl,--gc-sections \
          -L$(WUT_ROOT)/lib -L$(PORTLIBS)/lib \
          -lwut -lm

# -----------------------------
# Targets
# -----------------------------

all: $(OUTPUT_WUHB)

$(BUILD_DIR)/main.o: $(SRC)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTPUT_RPX): $(BUILD_DIR)/main.o
	$(CC) $^ $(LDFLAGS) -o $@

$(OUTPUT_WUHB): $(OUTPUT_RPX)
	$(WUT_ROOT)/bin/wut-tool pack $< $@

clean:
	rm -rf $(BUILD_DIR) *.rpx *.wuhb
