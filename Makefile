# Paths for Docker image
DEVKITPRO ?= /opt/devkitpro
DEVKITPPC ?= $(DEVKITPRO)/devkitPPC
WUT_ROOT  := $(DEVKITPRO)/wut
PORTLIBS  := $(DEVKITPRO)/portlibs/wiiu

# Compiler
CC := powerpc-eabi-gcc

# Compilation flags
CFLAGS := -O2 -Wall \
	-I$(WUT_ROOT)/include \
	-I$(PORTLIBS)/include

# Linking flags
LDFLAGS := -specs=$(WUT_ROOT)/share/wut.specs \
	-L$(PORTLIBS)/lib \
	-Wl,-e,__rpx_start

# Libraries (make sure they exist in $(PORTLIBS)/lib)
LIBS := -lcurl -lproc_ui -lvpad -los -lz -lws2 -lbrotlicommon -lbrotlidec

# Source & output
SRC := wave_browser/main.c
OBJ := build/main.o
OUT := build/wave_browser.rpx

# Build everything
all: $(OUT)

# Ensure build folder exists
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
