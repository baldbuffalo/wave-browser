#-------------------------------------------------------------------------------
# Paths
#-------------------------------------------------------------------------------
DEVKITPRO ?= /opt/devkitpro
DEVKITPPC  ?= $(DEVKITPRO)/devkitPPC
WUT        ?= $(DEVKITPRO)/wut

#-------------------------------------------------------------------------------
# Compiler / Linker
#-------------------------------------------------------------------------------
CC      := $(DEVKITPPC)/bin/powerpc-eabi-gcc
CXX     := $(DEVKITPPC)/bin/powerpc-eabi-g++
AR      := $(DEVKITPPC)/bin/powerpc-eabi-ar
OBJCOPY := $(DEVKITPPC)/bin/powerpc-eabi-objcopy

#-------------------------------------------------------------------------------
# Flags
#-------------------------------------------------------------------------------
CFLAGS   := -m32 -Wall -Wextra -ffunction-sections -fdata-sections -D__WIIU__ \
            -I$(WUT)/include -I$(DEVKITPRO)/portlibs/wiiu/include
CXXFLAGS := $(CFLAGS) -std=gnu++20
LDFLAGS  := -m32 -Wl,-O2 -L$(WUT)/lib -lwut

#-------------------------------------------------------------------------------
# Sources / Build
#-------------------------------------------------------------------------------
SRC_DIR := wave_browser
BUILD   := build
SRC     := $(wildcard $(SRC_DIR)/*.c)
OBJ     := $(SRC:$(SRC_DIR)/%.c=$(BUILD)/%.o)
TARGET  := $(BUILD)/wave_browser.elf

#-------------------------------------------------------------------------------
# Rules
#-------------------------------------------------------------------------------
all: $(TARGET)

$(BUILD)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) -o $@

clean:
	rm -rf $(BUILD)

.PHONY: all clean
