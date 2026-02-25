# -----------------------------------------------------------------------------
# Basic WiiU Makefile using devkitPro Docker and wut.mk
# -----------------------------------------------------------------------------

# Ensure DEVKITPRO is set
ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment.")
endif

# Project name
TARGET      := wave-browser
# Build directory
BUILD       := build
# Source folder
SOURCES     := wave_browser
# Optional local headers
INCLUDES    := include

# Compiler flags
CFLAGS   += -Wall -Wextra -ffunction-sections -fdata-sections
CXXFLAGS += -Wall -Wextra -ffunction-sections -fdata-sections -std=gnu++20
ASFLAGS  += $(MACHDEP)
LDFLAGS  += -Wl,--gc-sections

# Libraries (libcurl only; whb & wut are linked automatically by wut.mk)
LIBS := -lcurl

# -----------------------------------------------------------------------------
# Include official wut Makefile rules
# -----------------------------------------------------------------------------
include $(DEVKITPRO)/wut/share/wut.mk

# -----------------------------------------------------------------------------
# Clean target
# -----------------------------------------------------------------------------
.PHONY: clean
clean:
	@echo "Cleaning build..."
	@rm -rf $(BUILD) $(TARGET).rpx $(TARGET).elf
