.SUFFIXES:

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment.")
endif

TOPDIR ?= $(CURDIR)

include $(DEVKITPRO)/wut/share/wut_rules

# -----------------------------------------------------------------------------
# Project Settings
# -----------------------------------------------------------------------------

TARGET      := wave-browser
BUILD       := build
SOURCES     := .
INCLUDES    := include

PORTLIBS    := $(DEVKITPRO)/portlibs/wiiu

# -----------------------------------------------------------------------------
# Compiler Flags
# -----------------------------------------------------------------------------

CFLAGS   += -Wall -Wextra -ffunction-sections -fdata-sections $(MACHDEP)
CFLAGS   += -I$(CURDIR)/$(INCLUDES)
CFLAGS   += -I$(PORTLIBS)/include
CFLAGS   += -D__WIIU__

CXXFLAGS += -Wall -Wextra -ffunction-sections -fdata-sections $(MACHDEP) -std=gnu++20
CXXFLAGS += -I$(CURDIR)/$(INCLUDES)
CXXFLAGS += -I$(PORTLIBS)/include

ASFLAGS  += $(MACHDEP)

LDFLAGS  += -Wl,--gc-sections
LDFLAGS  += -L$(PORTLIBS)/lib

# -----------------------------------------------------------------------------
# Libraries
# -----------------------------------------------------------------------------

LIBS := -lcurl -lwut

.DEFAULT_GOAL := all

# -----------------------------------------------------------------------------
# Recursive Build Setup
# -----------------------------------------------------------------------------

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT := $(CURDIR)/$(TARGET)
export TOPDIR := $(CURDIR)

export VPATH := $(CURDIR)

CFILES   := $(notdir $(wildcard *.c))
CPPFILES := $(notdir $(wildcard *.cpp))
SFILES   := $(notdir $(wildcard *.s))

ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif

export OFILES := $(SFILES:.s=.o) \
                 $(CFILES:.c=.o) \
                 $(CPPFILES:.cpp=.o)

.PHONY: all clean

all: $(BUILD)
	@$(MAKE) -C $(BUILD) -f $(CURDIR)/Makefile

$(BUILD):
	@mkdir -p $@

clean:
	@rm -rf $(BUILD) $(TARGET).elf $(TARGET).rpx

else

DEPENDS := $(OFILES:.o=.d)

all: $(OUTPUT).rpx $(OUTPUT).elf

$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

endif
