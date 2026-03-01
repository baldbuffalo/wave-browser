DEVKITPRO ?= /opt/devkitpro
DEVKITPPC := $(DEVKITPRO)/devkitPPC
WUT_ROOT := $(DEVKITPRO)/wut
PORTLIBS := $(DEVKITPRO)/portlibs/wiiu

CC := $(DEVKITPPC)/bin/powerpc-eabi-gcc
CFLAGS := -O2 -Wall -I$(WUT_ROOT)/include -I$(PORTLIBS)/include
LDFLAGS := -specs=$(WUT_ROOT)/share/wut.specs

SRC := $(wildcard wave_browser/*.c)
OBJ := $(patsubst wave_browser/%.c,build/%.o,$(SRC))

all: build/wave_browser.rpx

build/%.o: wave_browser/%.c
	mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/wave_browser.rpx: $(OBJ)
	# The specs file will automatically pull in wut, portlibs, VPAD, ProcUI, curl
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

clean:
	rm -rf build wave_browser.rpx
