#include <whb/proc.h>
#include <coreinit/foreground.h>
#include <coreinit/screen.h>
#include <coreinit/cache.h>
#include <coreinit/time.h>
#include <coreinit/thread.h>
#include <coreinit/memdefaultheap.h>
#include <vpad/input.h>
#include <nn/swkbd.h>
#include <curl/curl.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "font_data.h"
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

// FIX: removed unused #include <padscore/kpad.h> and <coreinit/memorymap.h>

#define CURRENT_VERSION "v1.0.0"
#define GITHUB_API_URL  "https://api.github.com/repos/baldbuffalo/wave-browser/releases/latest"
#define DOWNLOAD_PATH   "fs:/vol/external01/wave-browser-update.rpx"
#define MAX_TABS        8
#define MAX_URL         512

#define TV_W  1280
#define TV_H   720
#define DRC_W  854
#define DRC_H  480

#define TOOLBAR_H       48
#define TAB_BAR_H       36
#define TAB_W          180
#define TAB_H           34
#define ADDR_BAR_X     120
#define ADDR_BAR_W     900
#define ADDR_BAR_H      32
#define ADDR_BAR_Y       8
#define BTN_SIZE        36
#define BTN_BACK_X       8
#define BTN_FWD_X       52
#define BTN_RELOAD_X    96
#define CONTENT_Y      (TOOLBAR_H + TAB_BAR_H)
#define CONTENT_H      (TV_H - CONTENT_Y)

#define COL_CHROME_BG    0xF2F2F2FF
#define COL_TAB_ACTIVE   0xFFFFFFFF
#define COL_TAB_INACTIVE 0xDEDEDEFF
#define COL_TAB_TEXT     0x3C3C3CFF
#define COL_ADDR_BG      0xFFFFFFFF
#define COL_ADDR_TEXT    0x1A1A1AFF
#define COL_ADDR_BORDER  0xCECECEFF
#define COL_CONTENT_BG   0xFFFFFFFF
#define COL_TOOLBAR_LINE 0xCECECEFF
#define COL_NEW_TAB_BTN  0x9E9E9EFF
#define COL_CLOSE_BTN    0x757575FF
#define COL_FAVICON_BG   0x4285F4FF
#define COL_WHITE        0xFFFFFFFF
#define COL_GRAY         0x9E9E9EFF
#define COL_BG_TOP       0x0096C7FF
#define COL_BG_BOT       0x023E8AFF
#define COL_WHITE_DIM    0xCCCCCCFF
#define COL_GREEN        0x00E676FF
#define COL_GREEN_DRK    0x00B248FF

#define SWKBD_WORK_SIZE  0x20000

typedef struct {
    char url[MAX_URL];
    char title[64];
    int  active;
} Tab;

static Tab  s_tabs[MAX_TABS];
static int  s_tab_count  = 1;
static int  s_active_tab = 0;

static void    *s_tv_buf  = NULL;
static void    *s_drc_buf = NULL;
static size_t   s_tv_size  = 0;
static size_t   s_drc_size = 0;
static int      s_inFg     = 0;
static int      s_buf_idx  = 0;

static inline void *tv_back(void) {
    return (uint8_t *)s_tv_buf  + (size_t)s_buf_idx * s_tv_size;
}
static inline void *drc_back(void) {
    return (uint8_t *)s_drc_buf + (size_t)s_buf_idx * s_drc_size;
}

static void acquireForeground(void) {
    OSScreenInit();
    s_tv_size  = OSScreenGetBufferSizeEx(SCREEN_TV);
    s_drc_size = OSScreenGetBufferSizeEx(SCREEN_DRC);

    s_tv_buf  = memalign(0x100, s_tv_size  * 2);
    s_drc_buf = memalign(0x100, s_drc_size * 2);

    // FIX: null-check memalign results before use; crash is guaranteed otherwise
    if (!s_tv_buf || !s_drc_buf) {
        free(s_tv_buf);
        free(s_drc_buf);
        s_tv_buf  = NULL;
        s_drc_buf = NULL;
        return;
    }

    memset(s_tv_buf,  0, s_tv_size  * 2);
    memset(s_drc_buf, 0, s_drc_size * 2);

    OSScreenSetBufferEx(SCREEN_TV,  s_tv_buf);
    OSScreenSetBufferEx(SCREEN_DRC, s_drc_buf);
    OSScreenEnableEx(SCREEN_TV,  1);
    OSScreenEnableEx(SCREEN_DRC, 1);

    s_buf_idx = 0;
    s_inFg = 1;
}

static void releaseForeground(void) {
    free(s_tv_buf);
    free(s_drc_buf);

    s_tv_buf  = NULL;
    s_drc_buf = NULL;
    s_tv_size  = 0;
    s_drc_size = 0;
    s_buf_idx  = 0;
    s_inFg = 0;
}

static void screen_flip(void) {
    DCFlushRange(tv_back(),  s_tv_size);
    DCFlushRange(drc_back(), s_drc_size);

    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);

    s_buf_idx ^= 1;
}

// ─── Pixel helpers ───────────────────────────────────────────────────────────

static inline uint32_t *fb_pixel(void *buf, int w, int x, int y) {
    return ((uint32_t *)buf) + (y * w + x);
}

static void fb_fill(void *buf, int fb_w, int fb_h, int x, int y, int w, int h, uint32_t col) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > fb_w) w = fb_w - x;
    if (y + h > fb_h) h = fb_h - y;
    if (w <= 0 || h <= 0) return;
    for (int r = y; r < y + h; r++)
        for (int c = x; c < x + w; c++)
            *fb_pixel(buf, fb_w, c, r) = col;
}

static void fb_rect_outline(void *buf, int fb_w, int fb_h, int x, int y, int w, int h, uint32_t col) {
    fb_fill(buf, fb_w, fb_h, x,     y,     w,  1, col);
    fb_fill(buf, fb_w, fb_h, x,     y+h-1, w,  1, col);
    fb_fill(buf, fb_w, fb_h, x,     y,     1,  h, col);
    fb_fill(buf, fb_w, fb_h, x+w-1, y,     1,  h, col);
}

static void fb_gradient(void *buf, int fb_w, int fb_h, uint32_t top, uint32_t bot) {
    int tr = (int)((top >> 24) & 0xFF);
    int tg = (int)((top >> 16) & 0xFF);
    int tb = (int)((top >>  8) & 0xFF);
    int br = (int)((bot >> 24) & 0xFF);
    int bg = (int)((bot >> 16) & 0xFF);
    int bb = (int)((bot >>  8) & 0xFF);
    for (int y = 0; y < fb_h; y++) {
        uint8_t r = (uint8_t)(tr + (br - tr) * y / fb_h);
        uint8_t g = (uint8_t)(tg + (bg - tg) * y / fb_h);
        uint8_t b = (uint8_t)(tb + (bb - tb) * y / fb_h);
        uint32_t color = ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | 0xFF;
        for (int x = 0; x < fb_w; x++)
            *fb_pixel(buf, fb_w, x, y) = color;
    }
}

static void fb_progress_bar(void *buf, int fb_w, int fb_h,
                             int x, int y, int w, int h, double pct) {
    fb_fill(buf, fb_w, fb_h, x, y, w, h, COL_GREEN_DRK);
    int fill_w = (int)(w * pct / 100.0);
    if (fill_w > 0) fb_fill(buf, fb_w, fb_h, x, y, fill_w, h, COL_GREEN);
    fb_rect_outline(buf, fb_w, fb_h, x, y, w, h, COL_WHITE);
}

static void fb_rounded_rect(void *buf, int fb_w, int fb_h,
                              int x, int y, int w, int h, int r, uint32_t col) {
    fb_fill(buf, fb_w, fb_h, x+r, y,   w-r*2, h,     col);
    fb_fill(buf, fb_w, fb_h, x,   y+r, w,     h-r*2, col);
}

static void fb_arrow(void *buf, int fb_w, int fb_h,
                      int cx, int cy, int size, int dir, uint32_t col) {
    for (int i = 0; i < size; i++) {
        int sx = cx + dir * (size/2 - i);
        fb_fill(buf, fb_w, fb_h, sx, cy - i/2, 1, i+1, col);
    }
}

// ─── FreeType ────────────────────────────────────────────────────────────────

static FT_Library s_ft   = NULL;
static FT_Face    s_face = NULL;

static void ft_init(void) {
    // FIX: check FT_Init_FreeType return value; FT_New_Memory_Face on a bad
    // library handle is undefined behaviour and will typically crash.
    if (FT_Init_FreeType(&s_ft) != 0) {
        s_ft = NULL;
        return;
    }
    if (FT_New_Memory_Face(s_ft, (const FT_Byte *)font_data, (FT_Long)font_data_len, 0, &s_face) != 0) {
        s_face = NULL;
    }
}

static void ft_done(void) {
    if (s_face) { FT_Done_Face(s_face);    s_face = NULL; }
    if (s_ft)   { FT_Done_FreeType(s_ft);  s_ft   = NULL; }
}

static uint32_t blend_pixel(uint32_t bg, uint32_t fg, uint8_t a) {
    if (!a) return bg;
    uint8_t r = (uint8_t)(((fg >> 24 & 0xFF) * a + (bg >> 24 & 0xFF) * (255 - a)) / 255);
    uint8_t g = (uint8_t)(((fg >> 16 & 0xFF) * a + (bg >> 16 & 0xFF) * (255 - a)) / 255);
    uint8_t b = (uint8_t)(((fg >>  8 & 0xFF) * a + (bg >>  8 & 0xFF) * (255 - a)) / 255);
    return ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | 0xFF;
}

static int ft_text_width(const char *text, int size) {
    if (!s_face) return 0;
    FT_Set_Pixel_Sizes(s_face, 0, size);
    int w = 0;
    for (const char *p = text; *p; p++)
        if (!FT_Load_Char(s_face, (unsigned char)*p, FT_LOAD_ADVANCE_ONLY))
            w += s_face->glyph->advance.x >> 6;
    return w;
}

static void ft_draw(void *buf, int fb_w, int fb_h,
                     const char *text, int x, int y, int size,
                     uint32_t color, int align) {
    if (!s_face || !text || !text[0]) return;
    FT_Set_Pixel_Sizes(s_face, 0, size);
    int tw = ft_text_width(text, size);
    int pen_x = x;
    if      (align == 1) pen_x = x - tw / 2;
    else if (align == 2) pen_x = x - tw;
    int pen_y = y;
    for (const char *p = text; *p; p++) {
        if (FT_Load_Char(s_face, (unsigned char)*p, FT_LOAD_RENDER)) continue;
        FT_GlyphSlot slot = s_face->glyph;
        FT_Bitmap   *bm   = &slot->bitmap;
        int bx = pen_x + slot->bitmap_left;
        int by = pen_y - slot->bitmap_top;
        for (int row = 0; row < (int)bm->rows; row++) {
            for (int col = 0; col < (int)bm->width; col++) {
                uint8_t alpha = bm->buffer[row * bm->pitch + col];
                if (!alpha) continue;
                int px = bx + col, py = by + row;
                if (px < 0 || py < 0 || px >= fb_w || py >= fb_h) continue;
                uint32_t *dst = fb_pixel(buf, fb_w, px, py);
                *dst = blend_pixel(*dst, color, alpha);
            }
        }
        pen_x += slot->advance.x >> 6;
    }
}

// ─── Splash screen ───────────────────────────────────────────────────────────

static void draw_splash(const char *status, double pct) {
    void *tv  = tv_back();
    void *drc = drc_back();

    fb_gradient(tv,  TV_W,  TV_H,  COL_BG_TOP, COL_BG_BOT);
    fb_gradient(drc, DRC_W, DRC_H, COL_BG_TOP, COL_BG_BOT);

    ft_draw(tv,  TV_W,  TV_H,  "Wave Browser", TV_W/2,  TV_H/2-40,  72, COL_WHITE, 1);
    ft_draw(drc, DRC_W, DRC_H, "Wave Browser", DRC_W/2, DRC_H/2-30, 48, COL_WHITE, 1);

    if (status && status[0]) {
        ft_draw(tv,  TV_W,  TV_H,  status, TV_W/2,  TV_H/2+30,  28, COL_WHITE_DIM, 1);
        ft_draw(drc, DRC_W, DRC_H, status, DRC_W/2, DRC_H/2+20, 22, COL_WHITE_DIM, 1);
    }

    if (pct >= 0.0) {
        int tv_bw=800, tv_bh=20, tv_bx=(TV_W-800)/2, tv_by=TV_H-110;
        fb_progress_bar(tv,  TV_W,  TV_H,  tv_bx,  tv_by,  tv_bw,  tv_bh,  pct);
        int drc_bw=600, drc_bh=14, drc_bx=(DRC_W-600)/2, drc_by=DRC_H-80;
        fb_progress_bar(drc, DRC_W, DRC_H, drc_bx, drc_by, drc_bw, drc_bh, pct);
        char pct_str[16];
        snprintf(pct_str, sizeof(pct_str), "%d%%", (int)pct);
        ft_draw(tv,  TV_W,  TV_H,  pct_str, TV_W/2,  TV_H-75,  24, COL_WHITE, 1);
        ft_draw(drc, DRC_W, DRC_H, pct_str, DRC_W/2, DRC_H-55, 18, COL_WHITE, 1);
    }

    screen_flip();
}

// ─── Browser UI ──────────────────────────────────────────────────────────────

static void draw_tab(void *buf, int fb_w, int fb_h, int idx, int active) {
    int tx = idx * TAB_W;
    int ty = TOOLBAR_H;
    uint32_t bg = active ? COL_TAB_ACTIVE : COL_TAB_INACTIVE;
    fb_rounded_rect(buf, fb_w, fb_h, tx, ty+2, TAB_W-2, TAB_H, 4, bg);
    fb_fill(buf, fb_w, fb_h, tx+8, ty+10, 14, 14, COL_FAVICON_BG);
    const char *title = s_tabs[idx].title[0] ? s_tabs[idx].title : "New Tab";
    char truncated[24];
    strncpy(truncated, title, 20);
    truncated[20] = '\0';
    ft_draw(buf, fb_w, fb_h, truncated,   tx+28,       ty+22, 13, COL_TAB_TEXT,  0);
    ft_draw(buf, fb_w, fb_h, "x",         tx+TAB_W-18, ty+22, 13, COL_CLOSE_BTN, 0);
    if (active)
        fb_fill(buf, fb_w, fb_h, tx, ty+TAB_H, TAB_W-2, 2, COL_TAB_ACTIVE);
}

static void draw_browser_ui(void) {
    void *tv  = tv_back();
    void *drc = drc_back();

    fb_fill(tv, TV_W, TV_H, 0, 0, TV_W, TV_H, COL_CONTENT_BG);
    fb_fill(tv, TV_W, TV_H, 0, 0, TV_W, TOOLBAR_H, COL_CHROME_BG);

    fb_rounded_rect(tv, TV_W, TV_H, BTN_BACK_X,   6, BTN_SIZE, BTN_SIZE, 4, COL_CHROME_BG);
    fb_arrow(tv, TV_W, TV_H, BTN_BACK_X+BTN_SIZE/2, TOOLBAR_H/2, 14, -1, COL_GRAY);
    fb_rounded_rect(tv, TV_W, TV_H, BTN_FWD_X,    6, BTN_SIZE, BTN_SIZE, 4, COL_CHROME_BG);
    fb_arrow(tv, TV_W, TV_H, BTN_FWD_X+BTN_SIZE/2,  TOOLBAR_H/2, 14,  1, COL_GRAY);
    fb_rect_outline(tv, TV_W, TV_H, BTN_RELOAD_X+4, 10, BTN_SIZE-8, BTN_SIZE-8, COL_GRAY);

    fb_rounded_rect(tv, TV_W, TV_H, ADDR_BAR_X, ADDR_BAR_Y, ADDR_BAR_W, ADDR_BAR_H, 16, COL_ADDR_BG);
    fb_rect_outline(tv, TV_W, TV_H, ADDR_BAR_X, ADDR_BAR_Y, ADDR_BAR_W, ADDR_BAR_H, COL_ADDR_BORDER);
    fb_fill(tv, TV_W, TV_H, ADDR_BAR_X+10, ADDR_BAR_Y+8, 10, 14, COL_GRAY);

    const char *url = s_tabs[s_active_tab].url[0] ?
        s_tabs[s_active_tab].url : "Search or type a URL";
    uint32_t url_col = s_tabs[s_active_tab].url[0] ? COL_ADDR_TEXT : COL_GRAY;
    ft_draw(tv, TV_W, TV_H, url, ADDR_BAR_X+28, ADDR_BAR_Y+22, 15, url_col, 0);
    fb_fill(tv, TV_W, TV_H, 0, TOOLBAR_H-1, TV_W, 1, COL_TOOLBAR_LINE);

    fb_fill(tv, TV_W, TV_H, 0, TOOLBAR_H, TV_W, TAB_BAR_H, COL_CHROME_BG);
    for (int i = 0; i < s_tab_count; i++)
        draw_tab(tv, TV_W, TV_H, i, i == s_active_tab);
    int plus_x = s_tab_count * TAB_W + 8;
    ft_draw(tv, TV_W, TV_H, "+", plus_x, TOOLBAR_H+24, 20, COL_NEW_TAB_BTN, 0);
    fb_fill(tv, TV_W, TV_H, 0, TOOLBAR_H+TAB_BAR_H-1, TV_W, 1, COL_TOOLBAR_LINE);

    ft_draw(tv, TV_W, TV_H, "New Tab", TV_W/2, CONTENT_Y + CONTENT_H/2, 32, COL_GRAY, 1);

    fb_fill(drc, DRC_W, DRC_H, 0, 0, DRC_W, DRC_H, COL_CONTENT_BG);
    fb_fill(drc, DRC_W, DRC_H, 0, 0, DRC_W, 48, COL_CHROME_BG);
    fb_rounded_rect(drc, DRC_W, DRC_H, 60, 8, 734, 32, 16, COL_ADDR_BG);
    fb_rect_outline(drc, DRC_W, DRC_H, 60, 8, 734, 32, COL_ADDR_BORDER);
    const char *drc_url = s_tabs[s_active_tab].url[0] ?
        s_tabs[s_active_tab].url : "Search or type a URL";
    ft_draw(drc, DRC_W, DRC_H, drc_url, 76, 28, 14, url_col, 0);
    fb_fill(drc, DRC_W, DRC_H, 0, 47, DRC_W, 1, COL_TOOLBAR_LINE);
    ft_draw(drc, DRC_W, DRC_H, "Press A to type URL   ZL/ZR to switch tabs",
            DRC_W/2, DRC_H-20, 13, COL_GRAY, 1);

    screen_flip();
}

// ─── On-screen keyboard ──────────────────────────────────────────────────────

static void open_url_keyboard(void) {
    void *workMem = MEMAllocFromDefaultHeapEx(SWKBD_WORK_SIZE, 0x1000);
    if (!workMem) return;

    nn::swkbd::CreateArg createArg = {};
    createArg.workMemory = workMem;
    createArg.regionType = nn::swkbd::RegionType::Europe;
    createArg.fsClient   = nullptr;

    if (!nn::swkbd::Create(createArg)) {
        MEMFreeToDefaultHeap(workMem);
        return;
    }

    nn::swkbd::AppearArg appearArg = {};

    if (!nn::swkbd::AppearInputForm(appearArg)) {
        nn::swkbd::Destroy();
        MEMFreeToDefaultHeap(workMem);
        return;
    }

    char result[MAX_URL] = {0};
    bool done = false;

    while (!done) {
        VPADStatus vpad;
        VPADReadError vpadErr;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &vpadErr);

        nn::swkbd::ControllerInfo ctrlInfo = {};
        ctrlInfo.vpad = &vpad;

        nn::swkbd::Calc(ctrlInfo);

        bool isFirst = false;
        if (nn::swkbd::IsDecideOkButton(&isFirst)) {
            const char16_t *str = nn::swkbd::GetInputFormString();
            if (str) {
                int i = 0;
                while (str[i] && i < MAX_URL - 1) {
                    result[i] = (char)(str[i] & 0x7F);
                    i++;
                }
                result[i] = '\0';
            }
            done = true;
        } else if (nn::swkbd::IsDecideCancelButton(&isFirst)) {
            done = true;
        }

        if (!done) {
            nn::swkbd::DrawTV();
            nn::swkbd::DrawDRC();
            screen_flip();
        }

        usleep(16000);
    }

    nn::swkbd::DisappearInputForm();
    nn::swkbd::Destroy();
    MEMFreeToDefaultHeap(workMem);

    if (result[0]) {
        strncpy(s_tabs[s_active_tab].url,   result, MAX_URL - 1);
        strncpy(s_tabs[s_active_tab].title, result, 63);
    }
}

// ─── Network helpers ─────────────────────────────────────────────────────────

typedef struct { char *data; size_t size; } Buffer;

static size_t write_cb(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total = size * nmemb;
    Buffer *buf  = (Buffer *)userp;
    char *p      = (char *)realloc(buf->data, buf->size + total + 1);
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

    Buffer buf = { (char *)malloc(1), 0 };
    if (!buf.data) {
        curl_easy_cleanup(curl);
        return 0;
    }
    buf.data[0] = '\0';

    curl_easy_setopt(curl, CURLOPT_URL,            GITHUB_API_URL);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,      "wave-browser/1.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &buf);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        10L);
    // FIX: WiiU has no system CA certificate store; without these options
    // every HTTPS request fails with a certificate-verification error.
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

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
    // FIX: provide the clientp for the progress callback (NULL is fine here
    // since progress_cb ignores it, but it must be explicitly set)
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA,     NULL);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,          120L);
    // FIX: same SSL verification disable required for download
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER,   0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST,   0L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    fclose(f);
    return (res == CURLE_OK) ? 0 : 1;
}

// ─── Splash / update flow ────────────────────────────────────────────────────

static void run_splash(void) {
    draw_splash("Loading...", -1.0);
    curl_global_init(CURL_GLOBAL_ALL);

    draw_splash("Checking for updates...", -1.0);
    char latest_tag[64] = {0};
    int has_update = check_for_update(latest_tag, sizeof(latest_tag));

    if (!has_update) {
        draw_splash("You're up to date!", -1.0);
        for (int i = 0; i < 60; i++) usleep(16000);
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "Update found: %s", latest_tag);
        draw_splash(msg, 0.0);
        usleep(1000000);

        char dl_url[256];
        snprintf(dl_url, sizeof(dl_url),
            "https://github.com/baldbuffalo/wave-browser/releases/download/%s/wave-browser.rpx",
            latest_tag);

        int ok = download_update(dl_url);
        draw_splash(ok == 0 ? "Update downloaded! Restart to apply." : "Download failed. Starting...",
                    ok == 0 ? 100.0 : -1.0);
        for (int i = 0; i < 180; i++) usleep(16000);
    }
}

// ─── Input handling ──────────────────────────────────────────────────────────

static void handle_input(VPADStatus *vpad) {
    uint32_t btn = vpad->trigger;

    if (btn & VPAD_BUTTON_A)  open_url_keyboard();

    if ((btn & VPAD_BUTTON_ZL) && s_active_tab > 0)
        s_active_tab--;

    if ((btn & VPAD_BUTTON_ZR) && s_active_tab < s_tab_count - 1)
        s_active_tab++;

    if ((btn & VPAD_BUTTON_X) && s_tab_count < MAX_TABS) {
        memset(&s_tabs[s_tab_count], 0, sizeof(Tab));
        strcpy(s_tabs[s_tab_count].title, "New Tab");
        s_tab_count++;
        s_active_tab = s_tab_count - 1;
    }

    if ((btn & VPAD_BUTTON_Y) && s_tab_count > 1) {
        for (int i = s_active_tab; i < s_tab_count - 1; i++)
            s_tabs[i] = s_tabs[i + 1];
        s_tab_count--;
        // FIX: removed the unreachable (s_tab_count <= 0) branch; the outer
        // guard (s_tab_count > 1) ensures the minimum after decrement is 1.
        if (s_active_tab >= s_tab_count)
            s_active_tab = s_tab_count - 1;
    }
}

// ─── Entry point ─────────────────────────────────────────────────────────────

int main(void) {
    WHBProcInit();
    VPADInit();

    memset(s_tabs, 0, sizeof(s_tabs));
    strncpy(s_tabs[0].title, "New Tab", 63);

    // Give ProcUI enough cycles to grant foreground before touching the screen
    for (int i = 0; i < 5 && WHBProcIsRunning(); i++)
        OSSleepTicks(OSMillisecondsToTicks(16));

    acquireForeground();
    ft_init();

    run_splash();

    while (WHBProcIsRunning()) {
        VPADStatus vpad;
        VPADReadError error;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &error);

        handle_input(&vpad);
        draw_browser_ui();

        OSSleepTicks(OSMillisecondsToTicks(16));
    }

    ft_done();

    if (s_inFg) {
        releaseForeground();
    }

    curl_global_cleanup();

    WHBProcShutdown();
    return 0;
}
