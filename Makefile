#---------------------------------------------
# Wii U Wave Browser Makefile
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
WUT_ROOT ?= $(DEVKITPRO)/wut
PORTLIBS_WIIU ?= $(DEVKITPRO)/portlibs/wiiu
PORTLIBS_PPC ?= $(DEVKITPRO)/portlibs/ppc   # for libz.a

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
LDFLAGS += -L$(PORTLIBS_PPC)/lib  # for libz.a

#---------------------------------------------
# Libraries (order matters!)
#---------------------------------------------
LIBS := -lwut \
        -lcurl \
        -lbrotlidec -lbrotlicommon \
        -lmbedtls -lmbedx509 -lmbedcrypto \
        -lz \
        -lnsysnet \
        -lm

#---------------------------------------------
# Source files
#---------------------------------------------
CFILES := $(wildcard $(SRC_DIR)/*.c)
OFILES := $(patsubst $(SRC_DIR)/%.c,$(BUILD)/%.o,$(CFILES))

#---------------------------------------------
# Build Rules
#---------------------------------------------
all: $(BUILD) $(TARGET).wuhb

# Create build folder
$(BUILD):
	mkdir -p $(BUILD)

# Compile C files
$(BUILD)/%.o: $(SRC_DIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# Link ELF
$(TARGET).elf: $(OFILES)
	$(CC) $(OFILES) $(LDFLAGS) $(LIBS) -o $@

# Convert ELF → RPX
$(TARGET).rpx: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

# Convert RPX → WUHB with meta.xml
$(TARGET).wuhb: $(TARGET).rpx
	wuhbtool $< $@ --meta meta.xml

# Clean build
clean:
	rm -rf $(BUILD) *.elf *.rpx *.wuhb
