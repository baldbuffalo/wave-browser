#-------------------------------------------------------------------------------
.SUFFIXES:
#-------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)

# Use the official WUPS rules. This includes WUT's build rules, which define
# RPXSPECS and the loadmem/datamem linker regions required by wups.ld.
include $(DEVKITPRO)/wups/share/wups_rules

TARGET := WaveBrowserRemote
BUILD := build_plugin
SOURCES := .
DATA :=
INCLUDES := .

CFLAGS := -O2 -Wall -ffunction-sections $(MACHDEP)
CFLAGS += $(INCLUDE) -D__WIIU__ -D__WUT__ -D__WUPS__
CXXFLAGS := $(CFLAGS) -std=c++17 -fno-exceptions -fno-rtti
ASFLAGS := $(ARCH)

# RPXSPECS supplies the WUT memory layout; WUPSSPECS adds the WUPS sections.
LDFLAGS := $(ARCH) $(RPXSPECS) -Wl,--gc-sections $(WUPSSPECS)

LIBS := -lwups -lwut

LIBDIRS := $(PORTLIBS) $(WUPS_ROOT) $(WUT_ROOT)

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT := $(CURDIR)/$(TARGET)
export TOPDIR := $(CURDIR)
export VPATH := $(CURDIR)/$(SOURCES)
export DEPSDIR := $(CURDIR)/$(BUILD)

CPPFILES := $(notdir $(wildcard $(SOURCES)/*.cpp))
export OFILES_SRC := $(CPPFILES:.cpp=.o)
export OFILES := $(OFILES_SRC)

export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
-I$(CURDIR)/$(BUILD)

export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean all

all: $(BUILD)

$(BUILD):
	@mkdir -p $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).wps $(TARGET).elf

else

.PHONY: all

DEPENDS := $(OFILES:.o=.d)

all: $(OUTPUT).wps

$(OUTPUT).wps: $(OUTPUT).elf

$(OUTPUT).elf: $(OFILES)

%.o: %.cpp
	@echo $(notdir $<)
	@$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(DEPENDS)

endif
