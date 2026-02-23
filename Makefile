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
SOURCES     := wave_browser   # app source folder
INCLUDES    := include        # your headers folder

# -----------------------------------------------------------------------------
# Compiler Flags
# -----------------------------------------------------------------------------

CFLAGS   += -Wall -Wextra -ffunction-sections -fdata-sections $(MACHDEP)
CFLAGS   += $(INCLUDE) -D__WIIU__

CXXFLAGS += -Wall -Wextra -ffunction-sections -fdata-sections $(MACHDEP) -std=gnu++20
ASFLAGS  += $(MACHDEP)
LDFLAGS  += -Wl,--gc-sections

# -----------------------------------------------------------------------------
# Libraries (static linking)
# -----------------------------------------------------------------------------

LIBS := -lcurl -lwhb -lwut

.DEFAULT_GOAL := all

# -----------------------------------------------------------------------------
# Recursive Build Setup
# -----------------------------------------------------------------------------

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT := $(CURDIR)/$(TARGET)
export TOPDIR := $(CURDIR)

# Source search paths
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))

# Source file detection
CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

# Choose linker
ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif

export OFILES_SRC := $(SFILES:.s=.o) $(CFILES:.c=.o) $(CPPFILES:.cpp=.o)
export OFILES := $(OFILES_SRC)

# Include paths
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                  -I$(CURDIR)

.PHONY: all rpx elf clean

all: rpx elf

rpx: $(BUILD)
	@$(MAKE) -C $(BUILD) -f $(CURDIR)/Makefile $(OUTPUT).rpx

elf: $(BUILD)
	@$(MAKE) -C $(BUILD) -f $(CURDIR)/Makefile $(OUTPUT).elf

$(BUILD):
	@mkdir -p $@

clean:
	@rm -rf $(BUILD) $(TARGET).elf $(TARGET).rpx

# -----------------------------------------------------------------------------
# Subdirectory build rules
# -----------------------------------------------------------------------------

else

DEPENDS := $(OFILES:.o=.d)

all: $(OUTPUT).rpx $(OUTPUT).elf

$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

endif
