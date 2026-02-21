#-------------------------------------------------------------------------------
.SUFFIXES:
#-------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)

include $(DEVKITPRO)/wut/share/wut_rules

#-------------------------------------------------------------------------------
# Project settings
#-------------------------------------------------------------------------------
TARGET      := wave-browser
BUILD       := build
SOURCES     := wave-browser
DATA        := data
INCLUDES    := wave-browser

#-------------------------------------------------------------------------------
# Build flags
#-------------------------------------------------------------------------------
CFLAGS  := -Wall -Wextra -ffunction-sections -fdata-sections $(MACHDEP)
CFLAGS  += $(INCLUDE) -D__WIIU__

CXXFLAGS := $(CFLAGS) -std=gnu++20
ASFLAGS  := $(MACHDEP)
LDFLAGS  := $(ARCH) -Wl,--gc-sections

LIBS    := -lcurl -lwhb -lwut
LIBDIRS := $(PORTLIBS) $(WUT_ROOT)

.DEFAULT_GOAL := all

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT := $(CURDIR)/$(TARGET)
export TOPDIR := $(CURDIR)

export VPATH  := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                 $(foreach dir,$(DATA),$(CURDIR)/$(dir))

CFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES    := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES    := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif

export OFILES_BIN := $(addsuffix .o,$(BINFILES))
export OFILES_SRC := $(SFILES:.s=.o) $(CFILES:.c=.o) $(CPPFILES:.cpp=.o)
export OFILES     := $(OFILES_BIN) $(OFILES_SRC)
export HFILES     := $(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                  $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                  -I$(CURDIR)

.PHONY: all modern legacy rpx elf clean

# Build both formats by default:
# - .rpx for modern Wii U homebrew environments
# - .elf for older environments/tooling
all: rpx elf

modern: rpx
legacy: elf

rpx: $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile $(OUTPUT).rpx

elf: $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile $(OUTPUT).elf

$(BUILD):
	@mkdir -p $@

clean:
	@echo clean ...
	@rm -rf $(BUILD) $(TARGET).elf $(TARGET).rpx $(TARGET).wuhb

else

DEPENDS := $(OFILES:.o=.d)

.PHONY: all
all: $(OUTPUT).rpx $(OUTPUT).elf

$(OUTPUT).elf: $(OFILES)

$(OFILES_SRC): $(HFILES)

%_bin.h %.bin.o: %.bin
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

endif
#-------------------------------------------------------------------------------
