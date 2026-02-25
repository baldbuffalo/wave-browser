# -----------------------------------------------------------------------------
# Project Settings
# -----------------------------------------------------------------------------

TARGET      := wave-browser
BUILD       := build
SOURCES     := .
INCLUDES    := include

PORTLIBS    := $(DEVKITPRO)/portlibs/wiiu

# -----------------------------------------------------------------------------
# Compiler & Linker Flags
# -----------------------------------------------------------------------------

CFLAGS      += -Wall -Wextra -I$(PORTLIBS)/include
CXXFLAGS    += -Wall -Wextra -I$(PORTLIBS)/include
LDFLAGS     += -L$(PORTLIBS)/lib

# Only link curl manually.
# WUT is linked automatically by wut_rules.
LIBS        := -lcurl

# -----------------------------------------------------------------------------
# Include WUT build system (must be last)
# -----------------------------------------------------------------------------

include $(DEVKITPRO)/wut/share/wut_rules
