# -----------------------------
# Wave Browser Makefile (Wii U)
# -----------------------------

APP_NAME = WaveBrowser
SRC = wave_browser/main.c
BUILD_DIR = build
OUTPUT_ELF = $(APP_NAME).elf
OUTPUT_WUHB = $(APP_NAME).wuhb

DEVKITPRO ?= /opt/devkitpro
DEVKITPPC ?= $(DEVKITPRO)/devkitPPC
WUT_ROOT ?= $(DEVKITPRO)/wut
PORTLIBS ?= $(DEVKITPRO)/portlibs/wiiu
LIBOGC ?= $(DEVKITPRO)/libogc

CC = $(DEVKITPPC)/bin/powerpc-eabi-gcc

# Added libogc include path for network.h/socket.h
CFLAGS = -O2 -Wall -mcpu=750 -meabi -mhard-float -ffunction-sections -fdata-sections \
         -I$(WUT_ROOT)/include -I$(PORTLIBS)/include -I$(LIBOGC)/include

# Removed curl/zlib/Brotli libs; only link wut + math
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

$(OUTPUT_ELF): $(BUILD_DIR)/main.o
	$(CC) $^ $(LDFLAGS) -o $@

$(OUTPUT_WUHB): $(OUTPUT_ELF)
	# Use wut-tool to convert ELF -> .wuhb
	$(WUT_ROOT)/bin/wut-tool pack $< $@ --icon=icon.png

clean:
	rm -rf $(BUILD_DIR) *.elf *.wuhb
