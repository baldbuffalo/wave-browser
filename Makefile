# -------------------------------
# Wave Browser Makefile (WiiU)
# -------------------------------

# Toolchain
DEVKITPRO ?= /opt/devkitpro
DEVKITPPC ?= $(DEVKITPRO)/devkitPPC
WUT_ROOT ?= $(DEVKITPRO)/wut
PORTLIBS ?= $(DEVKITPRO)/portlibs/wiiu
PORTLIBS_PPC ?= $(DEVKITPRO)/portlibs/ppc

CC := $(DEVKITPPC)/bin/powerpc-eabi-gcc
CFLAGS := -O2 -Wall -mcpu=750 -meabi -mhard-float -ffunction-sections -fdata-sections \
           -I$(WUT_ROOT)/include -I$(PORTLIBS)/include
LDFLAGS := -specs=$(WUT_ROOT)/share/wut.specs -Wl,--gc-sections \
           -L$(WUT_ROOT)/lib \
           -L$(PORTLIBS)/lib

LIBS := $(PORTLIBS)/lib/libcurl.a \
        $(PORTLIBS)/lib/libmbedtls.a \
        $(PORTLIBS)/lib/libmbedx509.a \
        $(PORTLIBS)/lib/libmbedcrypto.a \
        -lbrotlidec -lbrotlicommon -lz -lm -lwut

SRC := wave_browser/main.c
OBJ := build/main.o
TARGET := wave_browser.elf

# -------------------------------
# Rules
# -------------------------------
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) $(LIBS) -o $@

$(OBJ): $(SRC)
	mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build *.elf *.rpx *.wuhb

.PHONY: all clean
