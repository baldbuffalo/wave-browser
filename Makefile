DEVKITPRO ?= /opt/devkitpro
DEVKITPPC := $(DEVKITPRO)/devkitPPC
WUT := $(DEVKITPRO)/wut

CC := powerpc-eabi-gcc

CFLAGS := -O2 -Wall \
-I$(WUT)/include \
-I$(DEVKITPRO)/portlibs/wiiu/include

LDFLAGS := \
-specs=$(WUT)/share/wut.specs \
-L$(WUT)/lib \
-lwut -lm

BUILD := build
TARGET := wave_browser

SOURCES := wave_browser/main.c
OBJECTS := $(BUILD)/main.o

all: $(BUILD)/$(TARGET).rpx

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/main.o: wave_browser/main.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).rpx: $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILD)

.PHONY: all clean
