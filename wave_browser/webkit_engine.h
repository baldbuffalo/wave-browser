#pragma once
// webkit_engine.h — WKC (NetFront/WebKit) integration for Wave Browser
//
// The compiled WKC library lives at vendor/webkit-wiiu/lib/libWebKitWKC.a
// Drop a new build of the .a there to update the engine.
// If the .a is absent the build still works in stub mode.

#include <stdbool.h>

#ifdef __cplusplus
#include <SDL.h>
extern "C" {
#endif

// Call once at startup after SDL_CreateRenderer().
bool webkit_engine_init(int screen_w, int screen_h);

// Give the engine access to the SDL renderer so it can upload its
// framebuffer as a texture and blit it into the content area.
void webkit_engine_set_renderer(SDL_Renderer* ren);

// Load a URL.
void webkit_engine_navigate(const char* url);

// Per-frame tick (timers, JS, layout). Call every frame.
void webkit_engine_tick(void);

// Draw the current page into the content area of the screen.
// content_x/y/w/h define where to blit (pixels).
void webkit_engine_draw(int content_x, int content_y, int content_w, int content_h);

// Feed keyboard text input.
void webkit_engine_input_text(const char* utf8);

// Scroll delta in pixels.
void webkit_engine_scroll(int dx, int dy);

// Touch/click at screen coordinates.
void webkit_engine_touch_down(int x, int y);
void webkit_engine_touch_up(int x, int y);

// True if WKC was successfully initialised and can render pages.
bool webkit_engine_available(void);

// Clean shutdown.
void webkit_engine_shutdown(void);

#ifdef __cplusplus
}
#endif
