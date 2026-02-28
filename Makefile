DEVKITPRO ?= /opt/devkitpro
DEVKITPPC ?= $(DEVKITPRO)/devkitPPC
WUT_ROOT := $(DEVKITPRO)/wut

CC := powerpc-eabi-gcc

# Use wut-config to auto-detect include and link flags
CFLAGS := $(shell $(WUT_ROOT)/bin/wut-config --cflags)
LDFLAGS := -specs=$(WUT_ROOT)/share/wut.specs $(shell $(WUT_ROOT)/bin/wut-config --libs)

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
