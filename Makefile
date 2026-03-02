# ===== DevkitPro Paths =====
DEVKITPRO ?= /opt/devkitpro
DEVKITPPC := $(DEVKITPRO)/devkitPPC
WUT_ROOT  := $(DEVKITPRO)/wut

# ===== Project Files =====
SRC     := $(wildcard wave_browser/*.c)
OBJ     := $(SRC:wave_browser/%.c=build/%.o)
TARGET  := build/wave_browser.rpx

# ===== Default Target =====
all: $(TARGET)

# ===== Link RPX =====
# IMPORTANT: libwut.a is inside $(WUT_ROOT)/lib
$(TARGET): $(OBJ)
	$(DEVKITPPC)/bin/powerpc-eabi-gcc $^ -o $@ \
		-specs=$(WUT_ROOT)/share/wut.specs \
		-L$(WUT_ROOT)/lib -lwut

# ===== Compile C Files =====
build/%.o: wave_browser/%.c
	mkdir -p build
	$(DEVKITPPC)/bin/powerpc-eabi-gcc -O2 -Wall \
		-I$(WUT_ROOT)/include \
		-c $< -o $@

# ===== Clean =====
.PHONY: clean
clean:
	rm -rf build
