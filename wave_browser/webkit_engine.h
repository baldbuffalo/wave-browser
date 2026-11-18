#pragma once
// webkit_engine.h — WKC (NetFront/WebKit) integration for Wave Browser
//
// The WebKit-WiiU engine is intended to be integrated from source, not a static .a archive.

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

// Per-tab page lifecycle. The index order must match the browser's tab array.
bool webkit_engine_create_tab(int tab_index);
void webkit_engine_select_tab(int tab_index);
void webkit_engine_close_tab(int tab_index);

// Copy the active page's committed URL into out_url. Returns false when the
// page has not committed a URL yet or the output buffer is invalid.
bool webkit_engine_get_url(char* out_url, unsigned int out_size);

// Load a URL in the currently selected page.
void webkit_engine_navigate(const char* url);

// Browser toolbar actions backed by the active WKCWebView.
bool webkit_engine_can_go_back(void);
bool webkit_engine_can_go_forward(void);
void webkit_engine_go_back(void);
void webkit_engine_go_forward(void);
void webkit_engine_reload(void);

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

// Synthetic key events (keyCode = standard browser keyCode value)
void webkit_engine_key_down(int keycode);
void webkit_engine_key_up(int keycode);

// True if WKC was successfully initialised and can render pages.
bool webkit_engine_available(void);

// Clean shutdown.
void webkit_engine_shutdown(void);

#ifdef __cplusplus
}
#endif
