# Paths from devkitpro image
DEVKITPRO ?= /opt/devkitpro
DEVKITPPC := $(DEVKITPRO)/devkitPPC
WUT_ROOT := $(DEVKITPRO)/wut
PORTLIBS := $(DEVKITPRO)/portlibs/wiiu

CC := $(DEVKITPPC)/bin/powerpc-eabi-gcc

# Include paths only, no manual -l flags
CFLAGS := -O2 -Wall -I$(WUT_ROOT)/include -I$(PORTLIBS)/include
LDFLAGS := -specs=$(WUT_ROOT)/share/wut.specs

SRC := $(wildcard wave_browser/*.c)
OBJ := $(patsubst wave_browser/%.c,build/%.o,$(SRC))

all: build/wave_browser.rpx

# Compile sources
build/%.o: wave_browser/%.c
	mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

# Link using only specs (auto-link)
build/wave_browser.rpx: $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

clean:
	rm -rf build wave_browser.rpx
