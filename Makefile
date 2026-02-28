DEVKITPRO ?= /opt/devkitpro
DEVKITPPC ?= $(DEVKITPRO)/devkitPPC
WUT_ROOT  := $(DEVKITPRO)/wut
PORTLIBS  := $(DEVKITPRO)/portlibs/wiiu

CC := $(DEVKITPPC)/bin/powerpc-eabi-gcc

CFLAGS := -O2 -Wall \
	-I$(WUT_ROOT)/include \
	-I$(PORTLIBS)/include

LDFLAGS := -specs=$(WUT_ROOT)/share/wut.specs \
	-L$(PORTLIBS)/lib/wiiu

SRC := wave_browser/main.c
OBJ := build/main.o
OUT := build/wave_browser.rpx

all: $(OUT)

build:
	mkdir -p build

$(OBJ): $(SRC) | build
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

clean:
	rm -rf build
