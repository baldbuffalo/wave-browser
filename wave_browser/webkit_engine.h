#pragma once
// webkit_engine.h — WKC (NetFront/WebKit) integration for Wave Browser
//
// The compiled WKC library lives at vendor/webkit-wiiu/lib/libWebKitWKC.a
// Drop a new build of the .a there to update the engine.
// If the .a is absent the build still works; browsing shows a placeholder.

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Call once at startup. Returns false if the WKC lib is not available/init failed.
bool webkit_engine_init(int screen_w, int screen_h);

// Load a URL into the engine. No-op if not initialised.
void webkit_engine_navigate(const char* url);

// Per-frame tick (timers, JS, layout). Call every frame.
void webkit_engine_tick(void);

// Returns true when a new frame is ready to be read out via webkit_engine_pixels().
bool webkit_engine_needs_redraw(void);

// Raw ARGB pixel buffer, screen_w * screen_h * 4 bytes. Valid after init.
// Returns nullptr if WKC is not available.
const unsigned char* webkit_engine_pixels(void);

// Feed keyboard text input.
void webkit_engine_input_text(const char* utf8);

// Scroll: delta in pixels (positive = scroll down).
void webkit_engine_scroll(int dx, int dy);

// Touch/click at screen coordinates.
void webkit_engine_touch_down(int x, int y);
void webkit_engine_touch_up(int x, int y);

// True if WKC was successfully initialised.
bool webkit_engine_available(void);

// Clean shutdown.
void webkit_engine_shutdown(void);

#ifdef __cplusplus
}
#endif
