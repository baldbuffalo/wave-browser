#include <proc_ui/procui.h>
#include <coreinit/foreground.h>
#include <coreinit/screen.h>
#include <coreinit/cache.h>
#include <vpad/input.h>
#include <curl/curl.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "font_data.h"
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define CURRENT_VERSION "v1.0.0"
#define GITHUB_API_URL  "https://api.github.com/repos/baldbuffalo/wave-browser/releases/latest"
#define DOWNLOAD_PATH   "fs:/vol/external01/wave-browser-update.rpx"

// Screen dimensions
#define TV_W  1280
#define TV_H   720
#define DRC_W  854
#define DRC_H  480

// Nintendo-style colors (RGBA)
#define COL_BG_TOP    0x0096C7FF  // Wave blue
#define COL_BG_BOT    0x023E8AFF  // Deep ocean blue
#define COL_WHITE     0xFFFFFFFF
#define COL_WHITE_DIM 0xCCCCCCFF
#define COL_GREEN     0x00E676FF  // Bright Nintendo green
#define COL_GREEN_DRK 0x00B248FF  // Darker green for track bg
#define COL_OVERLAY   0x00000033  // Subtle dark overlay

// ----------------------------------------------------------------
// ProcUI
// ----------------------------------------------------------------
static void SaveCallback(void) { OSSavesDone_ReadyToRelease(); }

// ----------------------------------------------------------------
// Framebuffers
// ----------------------------------------------------------------
static void *s_tv_buf  = NULL;
static void *s_drc_buf = NULL;
static int   s_inFg    = 0;

static void acquireForeground(void) {
    OSScreenInit();
    size_t tvSize  = OSScreenGetBufferSizeEx(SCREEN_TV);
    size_t drcSize = OSScreenGetBufferSizeEx(SCREEN_DRC);
    s_tv_buf  = memalign(0x100, tvSize);
    s_drc_buf = memalign(0x100, drcSize);
    memset(s_tv_buf,  0, tvSize);
    memset(s_drc_buf, 0, drcSize);
    OSScreenSetBufferEx(SCREEN_TV,  s_tv_buf);
    OSScreenSetBufferEx(SCREEN_DRC, s_drc_buf);
    OSScreenEnableEx(SCREEN_TV,  1);
    OSScreenEnableEx(SCREEN_DRC, 1);
    s_inFg = 1;
}

static void releaseForeground(void) {
    free(s_tv_buf);
    free(s_drc_buf);
    s_tv_buf = s_drc_buf = NULL;
    s_inFg = 0;
    ProcUIDrawDoneRelease();
}

static void screen_flip(void) {
    DCFlushRange(s_tv_buf,  OSScreenGetBufferSizeEx(SCREEN_TV));
    DCFlushRange(s_drc_buf, OSScreenGetBufferSizeEx(SCREEN_DRC));
    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);
}

// ----------------------------------------------------------------
// Direct framebuffer drawing (fast, bypasses OSScreen per-pixel overhead)
// ----------------------------------------------------------------
static inline void fb_put(void *buf, int w, int x, int y, uint32_t rgba) {
    if ((unsigned)x >= (unsigned)w) return;
    ((uint32_t*)buf)[y * w + x] = rgba;
}

static void fb_fill(void *buf, int fb_w, int fb_h, int x, int y, int w, int h, uint32_t rgba) {
    for (int r = y; r < y+h && r < fb_h; r++)
        for (int c = x; c < x+w && c < fb_w; c++)
            fb_put(buf, fb_w, c, r, rgba);
}

// Vertical gradient fill
static void fb_gradient(void *buf, int fb_w, int fb_h, uint32_t top, uint32_t bot) {
    uint8_t tr = (top>>24)&0xFF, tg = (top>>16)&0xFF, tb = (top>>8)&0xFF;
    uint8_t br = (bot>>24)&0xFF, bg = (bot>>16)&0xFF, bb = (bot>>8)&0xFF;
    for (int y = 0; y < fb_h; y++) {
        uint8_t r = tr + (br - tr) * y / fb_h;
        uint8_t g = tg + (bg - tg) * y / fb_h;
        uint8_t b = tb + (bb - tb) * y / fb_h;
        uint32_t color = ((uint32_t)r<<24)|((uint32_t)g<<16)|((uint32_t)b<<8)|0xFF;
        for (int x = 0; x < fb_w; x++)
            fb_put(buf, fb_w, x, y, color);
    }
}

// Rounded progress bar
static void fb_progress_bar(void *buf, int fb_w, int fb_h,
                             int x, int y, int w, int h, double pct) {
    int radius = h / 2;

    // Track (dark green)
    fb_fill(buf, fb_w, fb_h, x + radius, y, w - radius*2, h, COL_GREEN_DRK);
    // Track end caps (circles approximated as filled squares for simplicity)
    fb_fill(buf, fb_w, fb_h, x, y + radius/2, radius, h - radius, COL_GREEN_DRK);
    fb_fill(buf, fb_w, fb_h, x + w - radius, y + radius/2, radius, h - radius, COL_GREEN_DRK);

    // Fill
    int fill_w = (int)((w - radius*2) * pct / 100.0);
    if (fill_w > 0) {
        fb_fill(buf, fb_w, fb_h, x + radius, y, fill_w, h, COL_GREEN);
        // Left cap of fill
        fb_fill(buf, fb_w, fb_h, x, y + radius/2, radius, h - radius, COL_GREEN);
    }

    // White border outline
    for (int i = x; i < x+w; i++) {
        fb_put(buf, fb_w, i, y,     COL_WHITE);
        fb_put(buf, fb_w, i, y+h-1, COL_WHITE);
    }
    for (int i = y; i < y+h; i++) {
        fb_put(buf, fb_w, x,   i, COL_WHITE);
        fb_put(buf, fb_w, x+w-1, i, COL_WHITE);
    }
}

// ----------------------------------------------------------------
// FreeType text rendering
// ----------------------------------------------------------------
static FT_Library s_ft  = NULL;
static FT_Face    s_face = NULL;

static void ft_init(void) {
    FT_Init_FreeType(&s_ft);
    FT_New_Memory_Face(s_ft, (const FT_Byte*)font_data, (FT_Long)font_data_len, 0, &s_face);
}

static void ft_done(void) {
    if (s_face) FT_Done_Face(s_face);
    if (s_ft)   FT_Done_FreeType(s_ft);
}

static uint32_t blend_pixel(uint32_t bg, uint32_t fg, uint8_t a) {
    if (a == 0)   return bg;
    if (a == 255) return fg;
    uint8_t r = ((fg>>24&0xFF)*a + (bg>>24&0xFF)*(255-a)) / 255;
    uint8_t g = ((fg>>16&0xFF)*a + (bg>>16&0xFF)*(255-a)) / 255;
    uint8_t b = ((fg>> 8&0xFF)*a + (bg>> 8&0xFF)*(255-a)) / 255;
    return (r<<24)|(g<<16)|(b<<8)|0xFF;
}

static int ft_text_width(const char *text, int size) {
    FT_Set_Pixel_Sizes(s_face, 0, size);
    int w = 0;
    for (const char *p = text; *p; p++) {
        if (!FT_Load_Char(s_face, (unsigned char)*p, FT_LOAD_ADVANCE_ONLY))
            w += s_face->glyph->advance.x >> 6;
    }
    return w;
}

static void ft_draw_text(void *buf, int fb_w, int fb_h,
                          const char *text, int cx, int y,
                          int size, uint32_t color) {
    FT_Set_Pixel_Sizes(s_face, 0, size);
    int tw = ft_text_width(text, size);
    int pen_x = cx - tw / 2;
    int pen_y = y;

    for (const char *p = text; *p; p++) {
        if (FT_Load_Char(s_face, (unsigned char)*p, FT_LOAD_RENDER)) continue;
        FT_GlyphSlot slot = s_face->glyph;
        FT_Bitmap *bm = &slot->bitmap;
        int bx = pen_x + slot->bitmap_left;
        int by = pen_y - slot->bitmap_top;
        for (int row = 0; row < (int)bm->rows; row++) {
            for (int col = 0; col < (int)bm->width; col++) {
                uint8_t alpha = bm->buffer[row * bm->pitch + col];
                if (!alpha) continue;
                int px = bx + col, py = by + row;
                if (px < 0 || py < 0 || px >= fb_w || py >= fb_h) continue;
                uint32_t bg = ((uint32_t*)buf)[py * fb_w + px];
                ((uint32_t*)buf)[py * fb_w + px] = blend_pixel(bg, color, alpha);
            }
        }
        pen_x += slot->advance.x >> 6;
    }
}

// ----------------------------------------------------------------
// Splash drawing
// ----------------------------------------------------------------
static void draw_splash(const char *status, double pct) {
    // Gradient background on both screens
    fb_gradient(s_tv_buf,  TV_W,  TV_H,  COL_BG_TOP, COL_BG_BOT);
    fb_gradient(s_drc_buf, DRC_W, DRC_H, COL_BG_TOP, COL_BG_BOT);

    // Subtle white divider line across middle area
    fb_fill(s_tv_buf,  TV_W,  TV_H,  TV_W/2  - 200, TV_H/2  - 60, 400, 2, 0xFFFFFF55);
    fb_fill(s_drc_buf, DRC_W, DRC_H, DRC_W/2 - 150, DRC_H/2 - 40, 300, 2, 0xFFFFFF55);

    // Title - large bold
    ft_draw_text(s_tv_buf,  TV_W,  TV_H,  "Wave Browser", TV_W/2,  TV_H/2  - 60, 72, COL_WHITE);
    ft_draw_text(s_drc_buf, DRC_W, DRC_H, "Wave Browser", DRC_W/2, DRC_H/2 - 40, 48, COL_WHITE);

    // Status text
    if (status && status[0]) {
        ft_draw_text(s_tv_buf,  TV_W,  TV_H,  status, TV_W/2,  TV_H/2  + 20, 30, COL_WHITE_DIM);
        ft_draw_text(s_drc_buf, DRC_W, DRC_H, status, DRC_W/2, DRC_H/2 + 20, 22, COL_WHITE_DIM);
    }

    // Progress bar (only when pct >= 0)
    if (pct >= 0.0) {
        // TV bar
        int tv_bw = 800, tv_bh = 22;
        int tv_bx = (TV_W - tv_bw) / 2, tv_by = TV_H - 130;
        fb_progress_bar(s_tv_buf, TV_W, TV_H, tv_bx, tv_by, tv_bw, tv_bh, pct);

        // DRC bar
        int drc_bw = 600, drc_bh = 16;
        int drc_bx = (DRC_W - drc_bw) / 2, drc_by = DRC_H - 90;
        fb_progress_bar(s_drc_buf, DRC_W, DRC_H, drc_bx, drc_by, drc_bw, drc_bh, pct);

        // Percentage text
        char pct_str[16];
        snprintf(pct_str, sizeof(pct_str), "%d%%", (int)pct);
        ft_draw_text(s_tv_buf,  TV_W,  TV_H,  pct_str, TV_W/2,  TV_H - 90,  26, COL_WHITE);
        ft_draw_text(s_drc_buf, DRC_W, DRC_H, pct_str, DRC_W/2, DRC_H - 60, 20, COL_WHITE);
    }

    screen_flip();
}

// ----------------------------------------------------------------
// curl helpers
// ----------------------------------------------------------------
typedef struct { char *data; size_t size; } Buffer;

static size_t write_cb(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total = size * nmemb;
    Buffer *buf  = (Buffer *)userp;
    char *p      = realloc(buf->data, buf->size + total + 1);
    if (!p) return 0;
    buf->data = p;
    memcpy(buf->data + buf->size, contents, total);
    buf->size += total;
    buf->data[buf->size] = '\0';
    return total;
}

static size_t file_write_cb(void *contents, size_t size, size_t nmemb, void *userp) {
    return fwrite(contents, size, nmemb, (FILE *)userp);
}

static int progress_cb(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
                       curl_off_t ultotal, curl_off_t ulnow) {
    (void)clientp; (void)ultotal; (void)ulnow;
    if (dltotal <= 0) return 0;
    draw_splash("Downloading update...", (double)dlnow / (double)dltotal * 100.0);
    return 0;
}

static int check_for_update(char *out_tag, size_t tag_size) {
    CURL *curl = curl_easy_init();
    if (!curl) return 0;
    Buffer buf = { malloc(1), 0 };
    if (!buf.data) { curl_easy_cleanup(curl); return 0; }
    buf.data[0] = '\0';
    curl_easy_setopt(curl, CURLOPT_URL,            GITHUB_API_URL);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,      "wave-browser/1.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &buf);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        10L);
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    int update_available = 0;
    if (res == CURLE_OK) {
        char *tag_ptr = strstr(buf.data, "\"tag_name\":\"");
        if (tag_ptr) {
            tag_ptr += strlen("\"tag_name\":\"");
            char *end = strchr(tag_ptr, '"');
            if (end) {
                size_t len = (size_t)(end - tag_ptr);
                if (len >= tag_size) len = tag_size - 1;
                strncpy(out_tag, tag_ptr, len);
                out_tag[len] = '\0';
                if (strcmp(out_tag, CURRENT_VERSION) != 0)
                    update_available = 1;
            }
        }
    }
    free(buf.data);
    return update_available;
}

static int download_update(const char *url) {
    FILE *f = fopen(DOWNLOAD_PATH, "wb");
    if (!f) return 1;
    CURL *curl = curl_easy_init();
    if (!curl) { fclose(f); return 1; }
    curl_easy_setopt(curl, CURLOPT_URL,              url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,        "wave-browser/1.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,    file_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,        f);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION,   1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS,       0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_cb);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,          120L);
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    fclose(f);
    return (res == CURLE_OK) ? 0 : 1;
}

// ----------------------------------------------------------------
// Splash sequence
// ----------------------------------------------------------------
static void run_splash(void) {
    ft_init();
    draw_splash("Loading...", -1.0);
    curl_global_init(CURL_GLOBAL_ALL);

    draw_splash("Checking for updates...", -1.0);
    char latest_tag[64] = {0};
    int has_update = check_for_update(latest_tag, sizeof(latest_tag));

    if (!has_update) {
        draw_splash("You're up to date!", -1.0);
        for (int i = 0; i < 90; i++) usleep(16000);
        ft_done();
        return;
    }

    char msg[64];
    snprintf(msg, sizeof(msg), "Update found: %s", latest_tag);
    draw_splash(msg, 0.0);
    usleep(1000000);

    char dl_url[256];
    snprintf(dl_url, sizeof(dl_url),
        "https://github.com/baldbuffalo/wave-browser/releases/download/%s/wave-browser.rpx",
        latest_tag);

    int ok = download_update(dl_url);
    if (ok == 0)
        draw_splash("Update downloaded! Restart to apply.", 100.0);
    else
        draw_splash("Download failed. Starting current version...", -1.0);

    for (int i = 0; i < 180; i++) usleep(16000);
    ft_done();
}

// ----------------------------------------------------------------
// MAIN
// ----------------------------------------------------------------
int main(void) {
    ProcUIInit(&SaveCallback);
    VPADInit();

    int splashDone = 0;

    while (1) {
        ProcUIStatus status = ProcUIProcessMessages(TRUE);
        if (status == PROCUI_STATUS_EXITING) break;
        else if (status == PROCUI_STATUS_RELEASE_FOREGROUND) {
            if (s_inFg) releaseForeground();
        } else if (status == PROCUI_STATUS_IN_FOREGROUND) {
            if (!s_inFg) acquireForeground();
            if (!splashDone) {
                run_splash();
                splashDone = 1;
            }
            VPADStatus vpad;
            VPADReadError error;
            VPADRead(VPAD_CHAN_0, &vpad, 1, &error);
            usleep(16000);
        }
    }

    if (s_inFg) {
        OSScreenEnableEx(SCREEN_TV,  0);
        OSScreenEnableEx(SCREEN_DRC, 0);
        free(s_tv_buf);
        free(s_drc_buf);
    }
    curl_global_cleanup();
    ProcUIShutdown();
    return 0;
}
