TARGET      := WaveBrowser
BUILD       := build
SOURCES     := wave_browser
RELEASE_DIR := release/$(TARGET)

DEVKITPRO   ?= /opt/devkitpro
DEVKITPPC   := $(DEVKITPRO)/devkitPPC
WUT_ROOT    := $(DEVKITPRO)/wut

CC          := $(DEVKITPPC)/bin/powerpc-eabi-gcc
LD          := $(CC)

CFLAGS  := -O2 -Wall -mcpu=750 -meabi -mhard-float \
           -ffunction-sections -fdata-sections \
           -I$(WUT_ROOT)/include \
           -I$(DEVKITPRO)/portlibs/wiiu/include \
           -I$(DEVKITPRO)/libogc/include

LDFLAGS := -specs=$(WUT_ROOT)/share/wut.specs \
           -Wl,--gc-sections \
           -Wl,--defsym=__end__=0x02000000 \
           -L$(WUT_ROOT)/lib \
           -L$(DEVKITPRO)/portlibs/wiiu/lib

LIBS    := -lcurl -lnn_ac -lwut -lm

CFILES  := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.c))
OFILES  := $(patsubst %.c,$(BUILD)/%.o,$(CFILES))

all: $(TARGET).wuhb

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).rpx: $(OFILES)
	$(LD) $^ $(LDFLAGS) $(LIBS) -o $@

$(TARGET).wuhb: $(TARGET).rpx
	wuhbtool $(TARGET).rpx $(TARGET).wuhb

release: $(TARGET).wuhb
	mkdir -p $(RELEASE_DIR)
	cp $(TARGET).wuhb $(RELEASE_DIR)/
	cp meta.xml $(RELEASE_DIR)/
	zip -r release/$(TARGET).zip $(RELEASE_DIR)

clean:
	rm -rf $(BUILD) *.rpx *.wuhb release WaveBrowser WaveBrowser.zip

.PHONY: all clean release
