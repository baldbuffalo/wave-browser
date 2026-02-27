#---------------------------------------------------------------------------------
# Wave Browser - WiiU (WUT + curl)
#---------------------------------------------------------------------------------

DEVKITPRO ?= /opt/devkitpro
TARGET      := wave_browser
BUILD       := build
SOURCE_DIR  := wave_browser

CC := powerpc-eabi-gcc

CFLAGS := -O2 -Wall \
	-I$(DEVKITPRO)/wut/include \
	-I$(DEVKITPRO)/portlibs/wiiu/include

LDFLAGS := \
	-specs=$(DEVKITPRO)/wut/share/wut.specs \
	-L$(DEVKITPRO)/wut/lib \
	-L$(DEVKITPRO)/portlibs/wiiu/lib \
	-lwut \
	-lcurl \
	-lsocket \
	-lssl \
	-lcrypto \
	-lz \
	-lbrotlidec \
	-lbrotlicommon \
	-lmbedtls \
	-lmbedx509 \
	-lmbedcrypto \
	-lm

SOURCES := $(wildcard $(SOURCE_DIR)/*.c)
OBJECTS := $(addprefix $(BUILD)/,$(notdir $(SOURCES:.c=.o)))

RPX := $(BUILD)/$(TARGET).rpx
WUHB := $(BUILD)/$(TARGET).wuhb

#---------------------------------------------------------------------------------

all: $(WUHB)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: $(SOURCE_DIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(RPX): $(OBJECTS)
	$(CC) $^ -o $@ $(LDFLAGS)

$(WUHB): $(RPX)
	@echo "Packaging WUHB..."
	mkdir -p $(BUILD)/temp
	cp $(RPX) $(BUILD)/temp/
	echo '<app><title>Wave Browser</title><author>Rishi</author><version>0.1.0</version></app>' > $(BUILD)/temp/meta.xml
	cd $(BUILD)/temp && zip -r ../$(TARGET).wuhb *
	rm -rf $(BUILD)/temp

clean:
	rm -rf $(BUILD)
