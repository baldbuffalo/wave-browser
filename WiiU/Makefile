#---------------------------------------------------------------------------------
# Project
#---------------------------------------------------------------------------------
TARGET      := WaveBrowser
BUILD       := build
RELEASE_DIR := release/$(TARGET)

#---------------------------------------------------------------------------------
# Devkit paths
#---------------------------------------------------------------------------------
DEVKITPRO   ?= /opt/devkitpro
DEVKITPPC   := $(DEVKITPRO)/devkitPPC
WUT_ROOT    := $(DEVKITPRO)/wut

#---------------------------------------------------------------------------------
# Tools
#---------------------------------------------------------------------------------
CC   := $(DEVKITPPC)/bin/powerpc-eabi-gcc
CXX  := $(DEVKITPPC)/bin/powerpc-eabi-g++
LD   := $(CXX)

#---------------------------------------------------------------------------------
# WebKit-WiiU full-engine release artifact
# The build workflow downloads the archive produced by baldbuffalo/webkit-wiiu.
# Keep the archive out of this repository; link the exact artifact staged by CI.
#---------------------------------------------------------------------------------
WEBKIT_ROOT  ?= ../build/webkit-wiiu-artifact
WEBKIT_INC   := $(WEBKIT_ROOT)/include
WEBKIT_LIB   := $(WEBKIT_ROOT)/lib/libWebKitWKC.a
WEBKIT_FLAGS := -I$(WEBKIT_INC) -DWKC_AVAILABLE
WEBKIT_DEPS  := $(filter-out $(WEBKIT_LIB),$(wildcard $(WEBKIT_ROOT)/lib/*.a))
WEBKIT_LIBS  := -Wl,--start-group $(WEBKIT_LIB) $(WEBKIT_DEPS) -Wl,--end-group

#---------------------------------------------------------------------------------
# Source files
#---------------------------------------------------------------------------------

# Core wave_browser sources (exclude plugin/ subdir)
WB_CXXFILES := wave_browser/main.cpp wave_browser/settings.cpp \
               wave_browser/webkit_engine.cpp wave_browser/gamepad_keyboard.cpp

# Vendored minizip
MINIZIP_CFILES := vendor/minizip/ioapi.c vendor/minizip/unzip.c

# TV Remotes engine files (flat)
TV_ENGINE_CPPFILES := \
    wave_browser/tv_remotes/tv_remote.cpp       \
    wave_browser/tv_remotes/model_registry.cpp  \
    wave_browser/tv_remotes/tv_detect.cpp

# All brand/year/model files (recursive, excludes plugin/)
TV_MODEL_CPPFILES := $(shell find wave_browser/tv_remotes -mindepth 3 -name '*.cpp' 2>/dev/null)

ALL_CFILES   := $(MINIZIP_CFILES)
ALL_CXXFILES := $(WB_CXXFILES) $(TV_ENGINE_CPPFILES) $(TV_MODEL_CPPFILES)

#---------------------------------------------------------------------------------
# Compiler flags
#---------------------------------------------------------------------------------
COMMON_FLAGS := \
    -O2 -Wall -mcpu=750 -meabi -mhard-float \
    -ffunction-sections -fdata-sections \
    -I$(WUT_ROOT)/include \
    -I$(DEVKITPRO)/portlibs/wiiu/include \
    -I$(DEVKITPRO)/portlibs/wiiu/include/SDL2 \
    -I$(DEVKITPRO)/portlibs/ppc/include/freetype2 \
    -I$(DEVKITPRO)/portlibs/ppc/include \
    -Ivendor/minizip \
    -Iwave_browser \
    -Iwave_browser/tv_remotes \
    $(WEBKIT_FLAGS)

CFLAGS   := $(COMMON_FLAGS) -DUSE_FILE32API
CXXFLAGS := $(COMMON_FLAGS) -std=c++17 -fno-exceptions -fno-rtti

#---------------------------------------------------------------------------------
# Linker flags
#---------------------------------------------------------------------------------
LDFLAGS := \
    -specs=$(WUT_ROOT)/share/wut.specs \
    -Wl,--gc-sections \
    -L$(WUT_ROOT)/lib \
    -L$(DEVKITPRO)/portlibs/wiiu/lib \
    -L$(DEVKITPRO)/portlibs/ppc/lib

LIBS := \
    $(WEBKIT_LIBS) \
    -lSDL2 -lSDL2_ttf -lharfbuzz \
    -lcurl -lmbedtls -lmbedx509 -lmbedcrypto \
    -lbrotlidec -lbrotlicommon \
    -lfreetype -lpng -lbz2 -lz \
    -lwut -lm

#---------------------------------------------------------------------------------
# Object files
#---------------------------------------------------------------------------------
OFILES := \
    $(patsubst %.c,  $(BUILD)/%.o, $(ALL_CFILES))   \
    $(patsubst %.cpp,$(BUILD)/%.o, $(ALL_CXXFILES))

#---------------------------------------------------------------------------------
# Targets
#---------------------------------------------------------------------------------
all: $(TARGET).wuhb

$(BUILD)/%.o: %.c
	@mkdir -p "$(dir $@)"
	$(CC) $(CFLAGS) -c "$<" -o "$@"

$(BUILD)/%.o: %.cpp
	@mkdir -p "$(dir $@)"
	$(CXX) $(CXXFLAGS) -c "$<" -o "$@"

$(TARGET).elf: $(OFILES) $(WEBKIT_LIB)
	$(LD) $(OFILES) $(LDFLAGS) $(LIBS) -o $@

# Fail early if the matching full-engine release was not staged.
$(WEBKIT_LIB):
	@test -s "$@" || { echo "Missing WebKit-WiiU engine archive: $@ (stage the matching release asset first)" >&2; exit 1; }

$(TARGET).rpx: $(TARGET).elf
	elf2rpl $< $@

$(TARGET).wuhb: $(TARGET).rpx
	wuhbtool $(TARGET).rpx $(TARGET).wuhb \
	    --name="Wave Browser" \
	    --short-name="WaveBrowser" \
	    --author="GameBuster"

release: $(TARGET).wuhb
	mkdir -p $(RELEASE_DIR)
	cp $(TARGET).wuhb $(RELEASE_DIR)/
	cp meta.xml $(RELEASE_DIR)/
	zip -r release/$(TARGET).zip $(RELEASE_DIR)

clean:
	rm -rf $(BUILD) *.elf *.rpx *.wuhb release WaveBrowser WaveBrowser.zip

.PHONY: all clean release
