# Paths
DEVKITPRO ?= /opt/devkitpro
DEVKITPPC ?= $(DEVKITPRO)/devkitPPC
WUT_ROOT  := $(DEVKITPRO)/wut
PORTLIBS  := $(DEVKITPRO)/portlibs/wiiu

# Compiler
CC := powerpc-eabi-gcc

# Compiler flags
CFLAGS := -O2 -Wall \
	-I$(WUT_ROOT)/include \
	-I$(PORTLIBS)/include

# Linker flags (entry point __rpx_start)
LDFLAGS := -specs=$(WUT_ROOT)/share/wut.specs \
	-L$(PORTLIBS)/lib \
	-Wl,-e,__rpx_start

# Libraries
LIBS := -lcurl -lproc_ui -lvpad -los -lz -lmbedtls -lmbedx509 -lmbedcrypto -lws2 -lbrotlicommon -lbrotlidec

# Source and output
SRC := wave_browser/main.c
OBJ := build/main.o
OUT := build/wave_browser.rpx

# Default target
all: $(OUT)

# Create build folder
build:
	mkdir -p build

# Compile
$(OBJ): $(SRC) | build
	$(CC) $(CFLAGS) -c $< -o $@

# Link
$(OUT): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS) $(LIBS)

# Clean
clean:
	rm -rf build
