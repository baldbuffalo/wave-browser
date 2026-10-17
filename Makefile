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
# WebKit-WiiU integration
# WKC_AVAILABLE stays enabled in this Makefile. CI downloads the archive artifact
# from baldbuffalo/webkit-wiiu into this temporary build directory (not vendor/).
#---------------------------------------------------------------------------------
WEBKIT_ARTIFACT_DIR ?= $(BUILD)/webkit-wiiu-artifact
WEBKIT_INC  := $(WEBKIT_ARTIFACT_DIR)/include
WEBKIT_LIB  := $(WEBKIT_ARTIFACT_DIR)/lib/libWebKitWKC.a
WEBKIT_FLAGS := -I$(WEBKIT_INC) -DWKC_AVAILABLE
# The full-engine bundle may include separate static dependencies (for
# example the WebRTC backend). Link WebKit first and group the archives so
# cross-archive references resolve without relying on incidental ordering.
WEBKIT_DEPS := $(filter-out $(WEBKIT_LIB),$(wildcard $(WEBKIT_ARTIFACT_DIR)/lib/*.a))
WEBKIT_LIBS := -Wl,--start-group $(WEBKIT_LIB) $(WEBKIT_DEPS) -Wl,--end-group

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
# Fail early rather than producing a browser package with a missing, empty,
# or bootstrap-only WebKit library. CI downloads the matching full-engine bundle
# before invoking make; local builds must provide that same bundle explicitly.
.PHONY: check-webkit-engine
check-webkit-engine:
	@test -s "$(WEBKIT_LIB)" || { echo "ERROR: Full WebKit-WiiU archive missing: $(WEBKIT_LIB)"; echo "Download the full-engine bundle for the matching webkit-wiiu commit; do not use the bootstrap archive."; exit 1; }
	@test -s "$(WEBKIT_ARTIFACT_DIR)/include/wkc/WKCWebView.h" || { echo "ERROR: WKCWebView.h missing from the WebKit engine bundle."; exit 1; }
	@test -s "$(WEBKIT_ARTIFACT_DIR)/include/wkc/WKCClientBuilders.h" || { echo "ERROR: WKCClientBuilders.h missing from the WebKit engine bundle."; exit 1; }
	@test -s "$(WEBKIT_ARTIFACT_DIR)/BUILD_PROFILE.txt" || { echo "ERROR: BUILD_PROFILE.txt missing; refusing an unverified engine bundle."; exit 1; }
	@grep -q "full" "$(WEBKIT_ARTIFACT_DIR)/BUILD_PROFILE.txt" || { echo "ERROR: Engine profile does not identify itself as a full build."; exit 1; }

all: check-webkit-engine $(TARGET).wuhb

$(BUILD)/%.o: %.c
	@mkdir -p "$(dir $@)"
	$(CC) $(CFLAGS) -c "$<" -o "$@"

$(BUILD)/%.o: %.cpp
	@mkdir -p "$(dir $@)"
	$(CXX) $(CXXFLAGS) -c "$<" -o "$@"

$(TARGET).elf: check-webkit-engine $(OFILES)
	$(LD) $(OFILES) $(LDFLAGS) $(LIBS) -o $@

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
