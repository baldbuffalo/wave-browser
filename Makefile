# Wave Browser (WiiU) Makefile
DEVKITPRO ?= /opt/devkitpro
DEVKITPPC ?= $(DEVKITPRO)/devkitPPC
WUT_ROOT ?= $(DEVKITPRO)/wut
PORTLIBS ?= $(DEVKITPRO)/portlibs/wiiu

CC := $(DEVKITPPC)/bin/powerpc-eabi-gcc
CFLAGS := -O2 -Wall -I$(WUT_ROOT)/include -I$(PORTLIBS)/include
LDFLAGS := -specs=$(WUT_ROOT)/share/wut.specs -L$(PORTLIBS)/lib/wiiu -lwut -lportlibs -lcurl

SRC := wave_browser/main.c
OBJ := build/main.o
RPX := build/wave_browser.rpx

all: $(RPX)

build:
	mkdir -p build

$(OBJ): $(SRC) | build
	$(CC) $(CFLAGS) -c $< -o $@

$(RPX): $(OBJ)
	$(CC) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf build
