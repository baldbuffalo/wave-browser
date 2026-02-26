# Paths
BUILD_DIR := build
WUHB_DIR := $(BUILD_DIR)/wavebrowser

# Default target
all: $(WUHB_DIR)

# Build and create WUHB directly
$(WUHB_DIR):
	@echo "Building Wave Browser WUHB..."
	@mkdir -p $(WUHB_DIR)
	# Build RPX directly inside the WUHB folder
	/opt/devkitpro/devkitPPC/bin/powerpc-eabi-gcc -O2 -Wall \
	-I/opt/devkitpro/wut/include \
	-I/opt/devkitpro/portlibs/wiiu/include \
	wave_browser/main.c \
	-o $(WUHB_DIR)/wave_browser.rpx \
	-L/opt/devkitpro/wut/lib \
	-L/opt/devkitpro/portlibs/wiiu/lib \
	-lwut -lcoreinit -lm -lcurl \
	-specs=/opt/devkitpro/wut/share/wut.specs \
	-Wl,-Map,$(WUHB_DIR)/wave_browser.map

	# Create meta.xml for Aroma
	@echo '<homebrew>' > $(WUHB_DIR)/meta.xml
	@echo '  <name>Wave Browser</name>' >> $(WUHB_DIR)/meta.xml
	@echo '  <author>baldbuffalo</author>' >> $(WUHB_DIR)/meta.xml
	@echo '  <version>v0.1.0</version>' >> $(WUHB_DIR)/meta.xml
	@echo '</homebrew>' >> $(WUHB_DIR)/meta.xml

# Clean
clean:
	@rm -rf $(BUILD_DIR)
