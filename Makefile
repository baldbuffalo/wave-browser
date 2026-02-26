#-------------------------------------------------------------------------------
# Wave Browser (Wii U) Makefile
#-------------------------------------------------------------------------------

# Paths
DEVKITPRO ?= /opt/devkitpro
DEVKITPPC := $(DEVKITPRO)/devkitPPC
PORTLIBS := $(DEVKITPRO)/portlibs/wiiu

# Compiler
CC := $(DEVKITPPC)/bin/powerpc-eabi-gcc

# Directories
SRC_DIR := wave_browser
BUILD_DIR := build

# Files
TARGET := $(BUILD_DIR)/wave_browser.rpx
SRCS := $(SRC_DIR)/main.c

# Flags
CFLAGS  := -O2 -Wall \
           -I$(PORTLIBS)/include \
           -I$(DEVKITPRO)/wut/include

LDFLAGS := -L$(PORTLIBS)/lib \
           -L$(DEVKITPRO)/wut/lib

LIBS := -lwut -lproc_ui -lvpad -lcoreinit -lcurl -lm

#-------------------------------------------------------------------------------
# Rules
#-------------------------------------------------------------------------------

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(BUILD_DIR) $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS) $(LIBS)

clean:
	rm -rf $(BUILD_DIR)
