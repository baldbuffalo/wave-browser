# Makefile
RPX=build/wavebrowser/wave_browser.rpx
WUHB=build/wavebrowser/wave_browser.wuhb
ICON=icon.png
META=meta.xml

all: $(RPX) $(WUHB)

# Existing RPX build
$(RPX): wave_browser/main.c
	@echo "Building Wave Browser RPX..."
	$(DEVKITPPC)/bin/powerpc-eabi-gcc -O2 -Wall -I$(WUT)/include -I$(PORTLIBS)/wiiu/include $< -o $@ -L$(WUT)/lib -L$(PORTLIBS)/wiiu/lib -lwut -lcurl -lm -specs=$(WUT)/share/wut.specs

# New WUHB build
$(WUHB): $(RPX) $(ICON) $(META)
	@echo "Packaging WUHB..."
	mkdir -p $(dir $@)/temp
	cp $(RPX) $(ICON) $(META) $(dir $@)/temp
	cd $(dir $@)/temp && zip -r ../$(notdir $@) *
	rm -rf $(dir $@)/temp

clean:
	@echo "Cleaning build..."
	rm -rf build
