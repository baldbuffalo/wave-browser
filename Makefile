DEVKITPRO ?= /opt/devkitpro
DEVKITPPC := $(DEVKITPRO)/devkitPPC
WUT_ROOT := $(DEVKITPRO)/wut
PORTLIBS := $(DEVKITPRO)/portlibs/wiiu
PATH := $(DEVKITPPC)/bin:$(DEVKITPRO)/tools/bin:$(PATH)

SRC := $(wildcard wave_browser/*.c)
OBJ := $(SRC:wave_browser/%.c=build/%.o)
TARGET := build/wave_browser.rpx

all: $(TARGET)

$(TARGET): $(OBJ)
	$(DEVKITPPC)/bin/powerpc-eabi-gcc $^ -o $@ -specs=$(WUT_ROOT)/share/wut.specs

build/%.o: wave_browser/%.c
	mkdir -p build
	$(DEVKITPPC)/bin/powerpc-eabi-gcc -O2 -Wall \
		-I$(WUT_ROOT)/include \
		-I$(PORTLIBS)/include \
		-c $< -o $@

clean:
	rm -rf build
