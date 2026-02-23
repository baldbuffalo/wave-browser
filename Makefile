.SUFFIXES:

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment.")
endif

TOPDIR ?= $(CURDIR)

include $(DEVKITPRO)/wut/share/wut_rules

TARGET      := wave-browser
BUILD       := build
SOURCES     := wave-browser
INCLUDES    := wave-browser

CFLAGS   += -Wall -Wextra -ffunction-sections -fdata-sections $(MACHDEP)
CFLAGS   += $(INCLUDE) -D__WIIU__

CXXFLAGS += -Wall -Wextra -ffunction-sections -fdata-sections $(MACHDEP) -std=gnu++20
ASFLAGS  += $(MACHDEP)
LDFLAGS  += -Wl,--gc-sections

LIBS    := -lcurl -lwhb -lwut
LIBDIRS := $(PORTLIBS) $(WUT_ROOT)

.DEFAULT_GOAL := all

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT := $(CURDIR)/$(TARGET)
export TOPDIR := $(CURDIR)

export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif

export OFILES_SRC := $(SFILES:.s=.o) $(CFILES:.c=.o) $(CPPFILES:.cpp=.o)
export OFILES := $(OFILES_SRC)

export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                  $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
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

else

DEPENDS := $(OFILES:.o=.d)

all: $(OUTPUT).rpx $(OUTPUT).elf

$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

endif
