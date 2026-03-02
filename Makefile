#---------------------------------------
# Wave Browser Makefile (WiiU)
#---------------------------------------

#---------------------------------------
# Toolchain
#---------------------------------------
DEVKITPRO ?= /opt/devkitpro
DEVKITPPC ?= $(DEVKITPRO)/devkitPPC
WUT_ROOT ?= $(DEVKITPRO)/wut
PORTLIBS_WIIU ?= $(DEVKITPRO)/portlibs/wiiu
PORTLIBS_PPC ?= $(DEVKITPRO)/portlibs/ppc
PATH := $(DEVKITPPC)/bin:$(DEVKITPRO)/tools/bin:$(PATH)

CC := $(DEVKITPPC)/bin/powerpc-eabi-gcc
CFLAGS := -O2 -Wall -mcpu=750 -meabi -mhard-float -ffunction-sections -fdata-sections
INCLUDES := -I$(WUT_ROOT)/include -I$(PORTLIBS_WIIU)/include

LDFLAGS := -specs=$(WUT_ROOT)/share/wut.specs \
           -Wl,--gc-sections \
           -L$(WUT_ROOT)/lib \
           -L$(PORTLIBS_WIIU)/lib/wiiu \
           -L$(PORTLIBS_PPC)/lib

LIBS := -lwut \
        -lcurl \
        -lbrotlidec -lbrotlicommon \
        -lmbedtls -lmbedx509 -lmbedcrypto \
        -lz \
        -lnsysnet \
        -lm

SRC := $(wildcard wave_browser/*.c)
OBJ := $(patsubst wave_browser/%.c, build/%.o, $(SRC))
TARGET := wave_browser.elf
WUH := wave_browser.wuhb

#---------------------------------------
# Default target
#---------------------------------------
all: $(TARGET) $(WUH)

#---------------------------------------
# Build object files
#---------------------------------------
build/%.o: wave_browser/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

#---------------------------------------
# Link ELF
#---------------------------------------
$(TARGET): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) $(LIBS) -o $@

#---------------------------------------
# Convert ELF to WUHB
#---------------------------------------
$(WUH): $(TARGET)
	wut-make-wuhb $< -o $@

#---------------------------------------
# Clean
#---------------------------------------
clean:
	rm -rf build *.elf *.rpx *.wuhb

#---------------------------------------
# Phony targets
#---------------------------------------
.PHONY: all clean
