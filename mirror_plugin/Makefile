# MirrorScreen WUPS plugin. Built with devkitPPC + WUT + WUPS + libjpeg-turbo.
.SUFFIXES:
ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment")
endif
TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/wups/share/wups_rules

TARGET := MirrorScreen
BUILD := build_plugin
SOURCES := src
DATA :=
INCLUDES := src .

CFLAGS := -O2 -Wall -ffunction-sections $(MACHDEP)
CFLAGS += $(INCLUDE) -D__WIIU__ -D__WUT__ -D__WUPS__
CXXFLAGS := $(CFLAGS) -std=c++17 -fno-exceptions -fno-rtti
ASFLAGS := $(ARCH)
LDFLAGS := $(ARCH) $(RPXSPECS) -Wl,--gc-sections $(WUPSSPECS)
LIBS := -ljpeg -lwups -lwut
LIBDIRS := $(PORTLIBS) $(WUPS_ROOT) $(WUT_ROOT)

ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(CURDIR)/$(TARGET)
export TOPDIR := $(CURDIR)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)
export LD := $(CXX)
CPPFILES := $(notdir $(wildcard $(SOURCES)/*.cpp))
export OFILES_SRC := $(CPPFILES:.cpp=.o)
export OFILES := $(OFILES_SRC)
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                  $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                  -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
.PHONY: all clean
all: $(BUILD)
$(BUILD):
	@mkdir -p $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
clean:
	rm -rf $(BUILD) $(TARGET).wps $(TARGET).elf
else
.PHONY: all
DEPENDS := $(OFILES:.o=.d)
all: $(OUTPUT).wps
$(OUTPUT).wps: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)
%.o: %.cpp
	@$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@
-include $(DEPENDS)
endif
