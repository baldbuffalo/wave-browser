#------------------------------------------------------------------------------
# Wii U Homebrew Makefile (WUT + curl)
#------------------------------------------------------------------------------

DEVKITPRO ?= /opt/devkitpro

TARGET      := wave_browser
BUILD       := build
SOURCES     := wave_browser
INCLUDES    :=

CC          := powerpc-eabi-gcc

CFLAGS  := -O2 -Wall \
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

SFILES      := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.c))
OFILES      := $(addprefix $(BUILD)/,$(notdir $(SFILES:.c=.o)))

#------------------------------------------------------------------------------
all: $(BUILD) $(BUILD)/$(TARGET).rpx

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: $(SOURCES)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).rpx: $(OFILES)
	$(CC) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILD)
