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

# Include WUT headers and standard networking headers
CFLAGS = -O2 -Wall -mcpu=750 -meabi -mhard-float -ffunction-sections -fdata-sections \
         -I$(WUT_ROOT)/include -I$(PORTLIBS)/include

# Link WUT libs, math, and SSL if needed
LDFLAGS = -specs=$(WUT_ROOT)/share/wut.specs -Wl,--gc-sections \
          -L$(WUT_ROOT)/lib -L$(PORTLIBS)/lib \
          -lwut -lm

# -----------------------------
# Targets
# -----------------------------

all: $(OUTPUT_WUHB)

# Compile main.c
$(BUILD_DIR)/main.o: $(SRC)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Link ELF
$(OUTPUT_ELF): $(BUILD_DIR)/main.o
	$(CC) $^ $(LDFLAGS) -o $@

# Pack .elf -> .wuhb
$(OUTPUT_WUHB): $(OUTPUT_ELF)
	$(WUT_ROOT)/bin/wut-tool pack $< $@ --icon=icon.png

# Clean build files
clean:
	rm -rf $(BUILD_DIR) *.elf *.wuhb
