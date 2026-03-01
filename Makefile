# Makefile for Wave Browser (Wii U)

# DevkitPro paths inside the Docker image
DEVKITPRO := /opt/devkitpro
DEVKITPPC := $(DEVKITPRO)/devkitPPC
WUT_ROOT  := $(DEVKITPRO)/wut
PORTLIBS  := $(DEVKITPRO)/portlibs/wiiu

# Compiler & flags
CC := $(DEVKITPPC)/bin/powerpc-eabi-gcc
CFLAGS := -O2 -Wall -I$(WUT_ROOT)/include -I$(PORTLIBS)/include
LDFLAGS := -specs=$(WUT_ROOT)/share/wut.specs -L$(PORTLIBS)/lib/wiiu

# Files
SRC := wave_browser/main.c
OBJ := build/main.o
RPX := build/wave_browser.rpx

# Targets
all: $(RPX)

build:
	mkdir -p build

$(OBJ): $(SRC) | build
	$(CC) $(CFLAGS) -c $< -o $@

$(RPX): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

clean:
	rm -rf build
