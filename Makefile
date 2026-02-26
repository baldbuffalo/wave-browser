#-------------------------------------------------------------------------------
# Makefile for Wave Browser (WiiU) - builds .rpx
#-------------------------------------------------------------------------------

ifndef DEVKITPRO
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path>")
endif

include $(DEVKITPRO)/wut/share/wut/make/wut_rules

TARGET = wave_browser
BUILD = build
SRC = $(wildcard wave_browser/*.c)
OBJ = $(SRC:%.c=$(BUILD)/%.o)

CFLAGS = -O2 -Wall -I$(DEVKITPRO)/wut/include -I$(DEVKITPRO)/wut/include/libcurl
LDFLAGS = -lcurl -lwut -lm

all: $(TARGET).rpx

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) -o $@

# Convert ELF to RPX
$(TARGET).rpx: $(TARGET).elf
	wut-make-rpx $< $@

clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).rpx
