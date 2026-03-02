#---------------------------------------------
# Wii U WUT Makefile - builds RPX → WUHB
#---------------------------------------------

# Devkit paths
DEVKITPRO ?= /opt/devkitpro
DEVKITPPC ?= $(DEVKITPRO)/devkitPPC
PREFIX := $(DEVKITPPC)/bin/powerpc-eabi-
CC := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy

# Project
TARGET := wave_browser
BUILD := build
SRC_DIR := wave_browser

# WUT + portlibs
WUT_ROOT := $(DEVKITPRO)/wut
PORTLIBS_WIIU := $(DEVKITPRO)/portlibs/wiiu
PORTLIBS_PPC := $(DEVKITPRO)/portlibs/ppc   # has libz.a

#---------------------------------------------
# Compiler Flags
#---------------------------------------------
CFLAGS := -O2 -Wall -mcpu=750 -meabi -mhard-float
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -I$(WUT_ROOT)/include
CFLAGS += -I$(PORTLIBS_WIIU)/include

#---------------------------------------------
# Linker Flags
#---------------------------------------------
LDFLAGS := -specs=$(WUT_ROOT)/share/wut.specs
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -L$(WUT_ROOT)/lib
LDFLAGS += -L$(PORTLIBS_WIIU)/lib
LDFLAGS += -L$(PORTLIBS_PPC)/lib   # needed for libz.a

LIBS := -lwut -lcurl -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer -lSDL2_net \
        -lmbedtls -lmbedx509 -lmbedcrypto -lz -lm

#---------------------------------------------
# Files
#---------------------------------------------
CFILES := $(wildcard $(SRC_DIR)/*.c)
OFILES := $(patsubst $(SRC_DIR)/%.c,$(BUILD)/%.o,$(CFILES))

#---------------------------------------------
# Build Rules
#---------------------------------------------
all: $(BUILD) $(TARGET).wuhb

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: $(SRC_DIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OFILES)
	$(CC) $(OFILES) $(LDFLAGS) $(LIBS) -o $@

$(TARGET).rpx: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(TARGET).wuhb: $(TARGET).rpx
	wuhbtool $< $@ --meta meta.xml

clean:
	rm -rf $(BUILD) *.elf *.rpx *.wuhb
