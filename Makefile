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

CC = $(DEVKITPPC)/bin/powerpc-eabi-gcc
CFLAGS = -O2 -Wall -mcpu=750 -meabi -mhard-float -ffunction-sections -fdata-sections \
         -I$(WUT_ROOT)/include -I$(PORTLIBS)/include
LDFLAGS = -specs=$(WUT_ROOT)/share/wut.specs -Wl,--gc-sections \
          -L$(WUT_ROOT)/lib -L$(PORTLIBS)/lib \
          -lcurl -lmbedtls -lmbedx509 -lmbedcrypto -lwut -lm

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
	# Simply copy ELF to WUHB for simplicity
	cp $(OUTPUT_ELF) $(OUTPUT_WUHB)

clean:
	rm -rf $(BUILD_DIR) *.elf *.wuhb
