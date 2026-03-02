# -----------------------------
# Wave Browser Makefile (WiiU)
# -----------------------------

# Compiler & tools
CC       := /opt/devkitpro/devkitPPC/bin/powerpc-eabi-gcc
WUT      := /opt/devkitpro/wut
PORTLIBS := /opt/devkitpro/portlibs/wiiu

# Output files
ELF      := wave_browser.elf
RPX      := wave_browser.rpx
WUHB     := wave_browser.wuhb

# Source & object files
SRC      := wave_browser/main.c
OBJ      := build/main.o

# Compiler flags
CFLAGS   := -O2 -Wall -mcpu=750 -meabi -mhard-float -ffunction-sections -fdata-sections
CFLAGS   += -I$(WUT)/include -I$(PORTLIBS)/include

# Linker flags
LDFLAGS  := -specs=$(WUT)/share/wut.specs -Wl,--gc-sections
LIBS     := $(PORTLIBS)/lib/libcurl.a \
            $(PORTLIBS)/lib/libmbedtls.a \
            $(PORTLIBS)/lib/libmbedx509.a \
            $(PORTLIBS)/lib/libmbedcrypto.a \
            -lm -lwut
LDFLAGS += -L$(WUT)/lib

# -----------------------------
# Targets
# -----------------------------

all: $(WUHB)

# Build WUHB from RPX
$(WUHB): $(RPX)
	@echo "Creating WUHB..."
	@$(WUT)/bin/make_wuhb $(RPX) $@

# Build RPX from ELF
$(RPX): $(ELF)
	@echo "Creating RPX..."
	@$(WUT)/bin/make_rpx $(ELF) $@

# Link ELF
$(ELF): $(OBJ)
	@echo "Linking ELF..."
	@$(CC) $(OBJ) $(LDFLAGS) $(LIBS) -o $@

# Compile C sources
$(OBJ): $(SRC)
	@mkdir -p build
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

# Clean build
clean:
	@echo "Cleaning..."
	@rm -rf build *.elf *.rpx *.wuhb

.PHONY: all clean
