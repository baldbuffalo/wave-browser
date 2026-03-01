# Makefile for Wave Browser (Wii U)
# Uses WUT and Portlibs from /opt/devkitpro

DEVKITPRO ?= /opt/devkitpro
DEVKITPPC ?= $(DEVKITPRO)/devkitPPC
WUT_ROOT ?= $(DEVKITPRO)/wut
PORTLIBS ?= $(DEVKITPRO)/portlibs/wiiu

CC := $(DEVKITPPC)/bin/powerpc-eabi-gcc
CFLAGS := -O2 -Wall -I$(WUT_ROOT)/include -I$(PORTLIBS)/include
LDFLAGS := -specs=$(WUT_ROOT)/share/wut.specs -L$(PORTLIBS)/lib/wiiu

SRC := $(wildcard wave_browser/*.c)
OBJ := $(SRC:wave_browser/%.c=build/%.o)
TARGET := build/wave_browser.rpx

all: $(TARGET)

build/%.o: wave_browser/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

build:
	mkdir -p build

clean:
	rm -rf build
