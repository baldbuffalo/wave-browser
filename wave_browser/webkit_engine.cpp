// webkit_engine.cpp — WKC integration
//
// When libWebKitWKC.a is present (linked via Makefile), WKC_AVAILABLE is
// defined by the build system and the real WKC paths are compiled in.
// Without the .a the stub paths compile and browsing shows a placeholder frame.
//
// Architecture:
//   webkit_engine_init()      → wkcTimerInitializePeer + WKCWebKitInitialize +
//                               WKCWebView::create() + framebuffer alloc
//   webkit_engine_navigate()  → WKCWebView::loadURL()
//   webkit_engine_tick()      → WKCWebView::notifyInternalTimerFired() etc.
//   webkit_engine_pixels()    → pixel buffer rendered by WKC into our alloc

#include "webkit_engine.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ── WKC headers (always included; guarded by WKC_AVAILABLE for actual calls) ──

#ifdef WKC_AVAILABLE
// The peers we must implement for WKC to function
#include <wkc/wkcbase.h>
#include <wkc/wkcpeer.h>
#include <wkc/wkcgpeer.h>
#include <wkc/wkcclib.h>
// WKCWebView C++ API lives in WKC namespace
// Include the browser C++ API when available
// #include <wkc/WKCWebView.h>   // uncomment when header is added to the lib
#endif

// ── Module state ──────────────────────────────────────────────────────────────

static bool    s_available  = false;
static int     s_w          = 1280;
static int     s_h          = 720;
static unsigned char* s_pixels = nullptr;  // ARGB framebuffer
static bool    s_dirty      = false;

// Current URL (for display even when WKC isn't running)
static char    s_current_url[512] = {};

// ── Stub framebuffer ──────────────────────────────────────────────────────────
// Draws a simple placeholder so the browser UI shows *something*

static void stub_draw_placeholder(const char* url)
{
    if (!s_pixels) return;
    // White background
    memset(s_pixels, 0xFF, (size_t)s_w * s_h * 4);

    // Draw a thin blue bar at top (simulates a loading indicator)
    for (int x = 0; x < s_w / 2; x++) {
        unsigned char* p = s_pixels + x * 4;
        p[0] = 0x42; p[1] = 0x85; p[2] = 0xF4; p[3] = 0xFF;  // blue
    }

    s_dirty = true;
}

// ── Peer stubs (minimal set required for WKC to initialise) ──────────────────
//
// WKC requires the application to implement "peer" functions for platform
// services (timers, heap, fonts, networking, GL …). These stubs satisfy
// the linker; replace them with real WiiU implementations as the engine matures.

#ifdef WKC_AVAILABLE

extern "C" {

// ── Timer peer ────────────────────────────────────────────────────────────────
bool wkcTimerInitializePeer(bool(*)(unsigned int, void*))        { return true; }
void wkcTimerFinalizePeer(void)                                  {}
unsigned int wkcGetTickCountPeer(void)                           { return 0; }

// ── Thread peer ───────────────────────────────────────────────────────────────
bool wkcThreadInitializePeer(void)   { return true; }
void wkcThreadFinalizePeer(void)     {}
void* wkcThreadCreatePeer(void*(*fn)(void*), void* data)         { return nullptr; }
void* wkcThreadCreateForMultiCorePeer(void*(*fn)(void*),void*d)  { return nullptr; }
void  wkcThreadDetachPeer(void*)     {}
void  wkcThreadJoinPeer(void*)       {}
bool  wkcThreadIsCurrentPeer(void*)  { return true; }
bool  wkcThreadIsSamePeer(void*,void*)   { return true; }
void  wkcThreadSleepPeer(int)        {}
void  wkcThreadYieldPeer(void)       {}

// ── Heap peer ─────────────────────────────────────────────────────────────────
bool wkcHeapInitializePeer(void*, int, void*, int)   { return true; }
void wkcHeapFinalizePeer(void)                        {}

// ── System peer ───────────────────────────────────────────────────────────────
bool wkcSystemInitializePeer(void)  { return true; }
void wkcSystemFinalizePeer(void)    {}

// ── File peer ─────────────────────────────────────────────────────────────────
bool wkcFileInitializePeer(void)    { return true; }
void wkcFileFinalizePeer(void)      {}

// ── Debug peer ────────────────────────────────────────────────────────────────
bool wkcDebugPrintInitializePeer(void)  { return true; }
void wkcDebugPrintFinalizePeer(void)    {}

// ── SSL peer ─────────────────────────────────────────────────────────────────
bool wkcSSLInitializePeer(void)     { return true; }
void wkcSSLFinalizePeer(void)       {}

// ── Font engine peer (returns false → WKC falls back to internal font) ────────
bool wkcFontEngineInitializePeer(void*,int,fontPeerMalloc,fontPeerFree,bool)
{ return false; }
void wkcFontEngineFinalizePeer(void) {}

} // extern "C"

#endif // WKC_AVAILABLE

// ── Public API ────────────────────────────────────────────────────────────────

bool webkit_engine_init(int screen_w, int screen_h)
{
    s_w = screen_w;
    s_h = screen_h;

    // Allocate ARGB framebuffer
    s_pixels = (unsigned char*)malloc((size_t)s_w * s_h * 4);
    if (!s_pixels) return false;
    memset(s_pixels, 0xFF, (size_t)s_w * s_h * 4);

#ifdef WKC_AVAILABLE
    // TODO: call WKCWebKitInitialize() and WKCWebView::create() here once
    // the full WKC browser API headers are added to the lib.
    // For now just mark as available so page content will render when
    // the API is wired up.
    s_available = false;   // flip to true once WKCWebKitInitialize succeeds
#else
    s_available = false;
#endif

    stub_draw_placeholder("about:blank");
    return true;   // engine layer always succeeds; s_available tracks WKC
}

void webkit_engine_navigate(const char* url)
{
    if (!url) return;
    strncpy(s_current_url, url, sizeof(s_current_url) - 1);
    s_current_url[sizeof(s_current_url) - 1] = '\0';

#ifdef WKC_AVAILABLE
    if (s_available) {
        // TODO: s_webview->loadURL(url);
        return;
    }
#endif

    // Stub: redraw placeholder with the URL visible
    stub_draw_placeholder(url);
}

void webkit_engine_tick(void)
{
#ifdef WKC_AVAILABLE
    if (s_available) {
        // TODO: tick WKC timers
    }
#endif
}

bool webkit_engine_needs_redraw(void)
{
    if (s_dirty) { s_dirty = false; return true; }
    return false;
}

const unsigned char* webkit_engine_pixels(void) { return s_pixels; }

void webkit_engine_input_text(const char* utf8)
{
#ifdef WKC_AVAILABLE
    if (s_available && utf8) {
        // TODO: feed keyboard events into WKC
    }
#endif
    (void)utf8;
}

void webkit_engine_scroll(int dx, int dy)
{
#ifdef WKC_AVAILABLE
    if (s_available) {
        // TODO: WKCWebView::scrollBy(dx, dy);
    }
#endif
    (void)dx; (void)dy;
}

void webkit_engine_touch_down(int x, int y)
{
#ifdef WKC_AVAILABLE
    if (s_available) {
        // TODO: WKCWebView::notifyMousePressed(x, y, 0 /*left*/);
    }
#endif
    (void)x; (void)y;
}

void webkit_engine_touch_up(int x, int y)
{
#ifdef WKC_AVAILABLE
    if (s_available) {
        // TODO: WKCWebView::notifyMouseReleased(x, y, 0);
    }
#endif
    (void)x; (void)y;
}

bool webkit_engine_available(void) { return s_available; }

void webkit_engine_shutdown(void)
{
#ifdef WKC_AVAILABLE
    if (s_available) {
        // TODO: WKCWebView::destroy + WKCWebKitFinalize
        s_available = false;
    }
#endif
    free(s_pixels);
    s_pixels = nullptr;
}
