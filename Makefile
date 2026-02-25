# Wave Browser Wii U Makefile (Docker devkitppc image)

TARGET      := wave-browser
BUILD       := build
SOURCES     := main.c

# -----------------------------------------------------------------------------
# Compiler & Linker Flags
# -----------------------------------------------------------------------------

CFLAGS      += -Wall -Wextra -D__WIIU__
CXXFLAGS    += -Wall -Wextra -D__WIIU__
LDFLAGS     +=

# Libraries (link libcurl from portlibs)
LIBS        := -lcurl

# -----------------------------------------------------------------------------
# Include WUT build system (handles .elf -> .rpx)
# -----------------------------------------------------------------------------
include $(DEVKITPRO)/wut/share/wut_rules
