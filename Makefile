#---------------------------------------
# Paths
#---------------------------------------
DEVKITPRO ?= /opt/devkitpro
DEVKITPPC ?= $(DEVKITPRO)/devkitPPC
WUT_ROOT ?= $(DEVKITPRO)/wut
PORTLIBS ?= $(DEVKITPRO)/portlibs/wiiu
PORTLIBS_PPC ?= $(DEVKITPRO)/portlibs/ppc

CC := $(DEVKITPPC)/bin/powerpc-eabi-gcc
CFLAGS := -O2 -Wall -mcpu=750 -meabi -mhard-float -ffunction-sections -fdata-sections \
          -I$(WUT_ROOT)/include -I$(PORTLIBS)/include
LDFLAGS := -specs=$(WUT_ROOT)/share/wut.specs -Wl,--gc-sections \
           -L$(WUT_ROOT)/lib -L$(PORTLIBS)/lib -L$(PORTLIBS_PPC)/lib

LIBS := $(PORTLIBS)/lib/libcurl.a \
        $(PORTLIBS)/lib/libnsysnet.a \
        $(PORTLIBS)/lib/libmbedtls.a \
        $(PORTLIBS)/lib/libmbedx509.a \
        $(PORTLIBS)/lib/libmbedcrypto.a \
        -lbrotlidec -lbrotlicommon -lz -lm -lwut

SRC := $(wildcard wave_browser/*.c)
OBJ := $(patsubst wave_browser/%.c, build/%.o, $(SRC))
TARGET := wave_browser.elf
WUHB := wave_browser.wuhb

#---------------------------------------
# Default target
#---------------------------------------
all: $(TARGET) $(WUHB)

#---------------------------------------
# Build object files
#---------------------------------------
build/%.o: wave_browser/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

#---------------------------------------
# Link ELF
#---------------------------------------
$(TARGET): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) $(LIBS) -o $@

#---------------------------------------
# Make WUHB package
#---------------------------------------
$(WUHB): $(TARGET)
	$(WUT_ROOT)/bin/make_wuhb $(TARGET) $(WUHB)

#---------------------------------------
# Clean
#---------------------------------------
clean:
	rm -rf build *.elf *.rpx *.wuhb

.PHONY: all clean
