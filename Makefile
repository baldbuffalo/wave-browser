# Environment paths
DEVKITPRO ?= /opt/devkitpro
DEVKITPPC := $(DEVKITPRO)/devkitPPC
WUT_ROOT := $(DEVKITPRO)/wut
PORTLIBS := $(DEVKITPRO)/portlibs/wiiu

# Source & build
SRC := $(wildcard wave_browser/*.c)
OBJ := $(SRC:wave_browser/%.c=build/%.o)
TARGET := build/wave_browser.rpx

# Default target
all: $(TARGET)

# Link RPX with correct libwut.a path
$(TARGET): $(OBJ)
	$(DEVKITPPC)/bin/powerpc-eabi-gcc $^ -o $@ \
		-specs=$(WUT_ROOT)/share/wut.specs \
		-L$(PORTLIBS)/lib/wiiu -lwut

# Compile object files
build/%.o: wave_browser/%.c
	mkdir -p build
	$(DEVKITPPC)/bin/powerpc-eabi-gcc -O2 -Wall \
		-I$(WUT_ROOT)/include \
		-I$(PORTLIBS)/include \
		-c $< -o $@

# Clean build folder
.PHONY: clean
clean:
	rm -rf build $(TARGET)
