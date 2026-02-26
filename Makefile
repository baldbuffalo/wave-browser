#-------------------------------------------------------------------------------
# Wave Browser (Wii U)
#-------------------------------------------------------------------------------

DEVKITPRO ?= /opt/devkitpro
DEVKITPPC := $(DEVKITPRO)/devkitPPC
PORTLIBS := $(DEVKITPRO)/portlibs/wiiu
WUT := $(DEVKITPRO)/wut

CC := $(DEVKITPPC)/bin/powerpc-eabi-gcc

SRC_DIR := wave_browser
BUILD_DIR := build
TARGET := $(BUILD_DIR)/wave_browser.rpx
SOURCES := $(SRC_DIR)/main.c

CFLAGS := -O2 -Wall \
          -I$(WUT)/include \
          -I$(PORTLIBS)/include

LDFLAGS := -L$(WUT)/lib \
           -L$(PORTLIBS)/lib

LIBS := -lwut -lproc_ui -lvpad -lcoreinit -lcurl -lm

#-------------------------------------------------------------------------------

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS) $(LIBS)

clean:
	rm -rf $(BUILD_DIR)
