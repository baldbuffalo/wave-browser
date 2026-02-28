# DevkitPro / DevkitPPC setup
DEVKITPRO ?= /opt/devkitpro
DEVKITPPC ?= $(DEVKITPRO)/devkitPPC
WUT_ROOT  := $(DEVKITPRO)/wut
PORTLIBS  := $(DEVKITPRO)/portlibs/wiiu

CC := powerpc-eabi-gcc

# Include headers
CFLAGS := -O2 -Wall \
	-I$(WUT_ROOT)/include \
	-I$(PORTLIBS)/include

# Linker flags
LDFLAGS := -specs=$(WUT_ROOT)/share/wut.specs \
	-L$(PORTLIBS)/lib/wiiu \
	-Wl,-e,__rpx_start

# Automatically gather all .a static libraries in portlibs
STATIC_LIBS := $(wildcard $(PORTLIBS)/lib/wiiu/*.a)

SRC := wave_browser/main.c
OBJ := build/main.o
OUT := build/wave_browser.rpx

# Default target
all: $(OUT)

# Create build folder
build:
	mkdir -p build

# Compile source
$(OBJ): $(SRC) | build
	$(CC) $(CFLAGS) -c $< -o $@

# Link with all static libraries automatically
$(OUT): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS) $(STATIC_LIBS)

# Clean build artifacts
clean:
	rm -rf build
