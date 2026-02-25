# Wave Browser Wii U Makefile (Docker devkitppc image)

TARGET := wave-browser
SOURCES := main.c
BUILD := build

# -----------------------------------------------------------------------------
# Compiler & Linker Flags
CFLAGS := -Wall -Wextra -D__WIIU__
LDFLAGS :=
LIBS := -lcurl

# -----------------------------------------------------------------------------
# Include WUT build rules
include $(DEVKITPRO)/wut/share/wut_rules

# -----------------------------------------------------------------------------
# Build targets
all: $(BUILD)/$(TARGET).rpx

# Build .elf first, then rpx
$(BUILD)/$(TARGET).elf: $(SOURCES)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

$(BUILD)/$(TARGET).rpx: $(BUILD)/$(TARGET).elf
	$(MAKE_RPX) $<
