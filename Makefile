#---------------------------------------
# Wave Browser Makefile (Wii U)
#---------------------------------------

#---------------------------------------
# Environment
#---------------------------------------
DEVKITPRO ?= /opt/devkitpro
DEVKITPPC ?= $(DEVKITPRO)/devkitPPC
WUT_ROOT ?= $(DEVKITPRO)/wut
PORTLIBS ?= $(DEVKITPRO)/portlibs/wiiu
PORTLIBS_PPC ?= $(DEVKITPRO)/portlibs/ppc

PATH := $(DEVKITPPC)/bin:$(DEVKITPRO)/tools/bin:$(PATH)

#---------------------------------------
# Compiler flags
#---------------------------------------
CFLAGS := -O2 -Wall -mcpu=750 -meabi -mhard-float \
          -ffunction-sections -fdata-sections \
          -I$(WUT_ROOT)/include \
          -I$(PORTLIBS)/include

LDFLAGS := -specs=$(WUT_ROOT)/share/wut.specs \
           -Wl,--gc-sections \
           -L$(WUT_ROOT)/lib \
           -L$(PORTLIBS)/lib \
           -L$(PORTLIBS_PPC)/lib

#---------------------------------------
# Source and build directories
#---------------------------------------
SRC := wave_browser/main.c
BUILD_DIR := build
OBJ := $(BUILD_DIR)/main.o
TARGET := wave_browser.elf

#---------------------------------------
# Libraries (use full paths to static .a)
#---------------------------------------
LIBS := $(PORTLIBS)/lib/libcurl.a \
        $(PORTLIBS)/lib/libnsysnet.a \
        $(PORTLIBS)/lib/libmbedtls.a \
        $(PORTLIBS)/lib/libmbedx509.a \
        $(PORTLIBS)/lib/libmbedcrypto.a \
        -lbrotlidec -lbrotlicommon -lz -lm -lwut

#---------------------------------------
# Default target
#---------------------------------------
all: $(TARGET)

#---------------------------------------
# Build object
#---------------------------------------
$(OBJ): $(SRC) | $(BUILD_DIR)
	$(DEVKITPPC)/bin/powerpc-eabi-gcc $(CFLAGS) -c $< -o $@

#---------------------------------------
# Link
#---------------------------------------
$(TARGET): $(OBJ)
	$(DEVKITPPC)/bin/powerpc-eabi-gcc $(OBJ) $(LDFLAGS) $(LIBS) -o $@

#---------------------------------------
# Create build directory
#---------------------------------------
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

#---------------------------------------
# Clean
#---------------------------------------
clean:
	rm -rf $(BUILD_DIR) *.elf *.rpx *.wuhb

#---------------------------------------
# Phony
#---------------------------------------
.PHONY: all clean
