# Wave Browser Makefile for Wii U (Docker)
DEVKITPRO ?= /opt/devkitpro
DEVKITPPC ?= $(DEVKITPRO)/devkitPPC
WUT_ROOT ?= $(DEVKITPRO)/wut
PORTLIBS ?= $(DEVKITPRO)/portlibs/wiiu

CC := $(DEVKITPPC)/bin/powerpc-eabi-gcc
CFLAGS := -O2 -Wall -I$(WUT_ROOT)/include -I$(PORTLIBS)/include
LDFLAGS := -specs=$(WUT_ROOT)/share/wut.specs -L$(PORTLIBS)/lib/wiiu

SRC_DIR := wave_browser
BUILD_DIR := build
TARGET := $(BUILD_DIR)/wave_browser.rpx

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

all: $(TARGET)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)
