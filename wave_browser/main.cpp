#include <proc_ui/procui.h>
#include <coreinit/foreground.h>
#include <coreinit/title.h>
#include <coreinit/memdefaultheap.h>
#include <sysapp/launch.h>
#include <vpad/input.h>
#include <nn/swkbd.h>
#include <curl/curl.h>
#include "font_data.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

// ─── Version / URLs ──────────────────────────────────────────────────────────

#define CURRENT_VERSION "v1.0.0"
#define GITHUB_API_URL  "https://api.github.com/repos/baldbuffalo/wave-browser/releases/latest"
#define DOWNLOAD_PATH   "fs:/vol/external01/wave-browser-update.rpx"

// ─── Screen dimensions ───────────────────────────────────────────────────────

#define TV_W   1280
#define TV_H    720

// ─── UI layout ───────────────────────────────────────────────────────────────

#define MAX_TABS    8
#define MAX_URL     512

#define TOOLBAR_H   48
#define TAB_BAR_H   36
#define TAB_W       180
#define TAB_H       34
#define ADDR_BAR_X  120
#define ADDR_BAR_W  900
#define ADDR_BAR_H  32
#define ADDR_BAR_Y  8
#define BTN_SIZE    36
#define BTN_BACK_X  8
#define BTN_FWD_X   52
#define BTN_RELOAD_X 96
#define CONTENT_Y   (TOOLBAR_H + TAB_BAR_H)
#define CONTENT_H   (TV_H - CONTENT_Y)

#define SWKBD_WORK_SIZE 0x20000

// ─── Colours (SDL) ───────────────────────────────────────────────────────────

static constexpr SDL_Color COL_CHROME_BG    = {0xF2,0xF2,0xF2,0xFF};
static constexpr SDL_Color COL_TAB_ACTIVE   = {0xFF,0xFF,0xFF,0xFF};
static constexpr SDL_Color COL_TAB_INACTIVE = {0xDE,0xDE,0xDE,0xFF};
static constexpr SDL_Color COL_TAB_TEXT     = {0x3C,0x3C,0x3C,0xFF};
static constexpr SDL_Color COL_ADDR_BG      = {0xFF,0xFF,0xFF,0xFF};
static constexpr SDL_Color COL_ADDR_TEXT    = {0x1A,0x1A,0x1A,0xFF};
static constexpr SDL_Color COL_ADDR_BORDER  = {0xCE,0xCE,0xCE,0xFF};
static constexpr SDL_Color COL_CONTENT_BG   = {0xFF,0xFF,0xFF,0xFF};
static constexpr SDL_Color COL_TOOLBAR_LINE = {0xCE,0xCE,0xCE,0xFF};
static constexpr SDL_Color COL_NEW_TAB_BTN  = {0x9E,0x9E,0x9E,0xFF};
static constexpr SDL_Color COL_CLOSE_BTN    = {0x75,0x75,0x75,0xFF};
static constexpr SDL_Color COL_FAVICON_BG   = {0x42,0x85,0xF4,0xFF};
static constexpr SDL_Color COL_WHITE        = {0xFF,0xFF,0xFF,0xFF};
static constexpr SDL_Color COL_GRAY         = {0x9E,0x9E,0x9E,0xFF};
static constexpr SDL_Color COL_WHITE_DIM    = {0xCC,0xCC,0xCC,0xFF};
static constexpr SDL_Color COL_GREEN        = {0x00,0xE6,0x76,0xFF};
static constexpr SDL_Color COL_GREEN_DRK    = {0x00,0xB2,0x48,0xFF};
static constexpr SDL_Color COL_BG_TOP       = {0x00,0x96,0xC7,0xFF};
static constexpr SDL_Color COL_BG_BOT       = {0x02,0x3E,0x8A,0xFF};

// ─── Tab state ───────────────────────────────────────────────────────────────

typedef struct {
    char url[MAX_URL];
    char title[64];
} Tab;

static Tab  s_tabs[MAX_TABS];
static int  s_tab_count  = 1;
static int  s_active_tab = 0;

// ─── SDL globals ─────────────────────────────────────────────────────────────

static SDL_Window*   s_window   = nullptr;
static SDL_Renderer* s_renderer = nullptr;
static TTF_Font*     s_font_sm  = nullptr; // 13px  – tab labels, small UI
static TTF_Font*     s_font_md  = nullptr; // 15px  – address bar
static TTF_Font*     s_font_lg  = nullptr; // 28px  – splash status
static TTF_Font*     s_font_xl  = nullptr; // 48px  – splash title

// ─── ProcUI state ────────────────────────────────────────────────────────────

static bool s_running = true;

static uint32_t SaveCallback(void* /*context*/)
{
    OSSavesDone_ReadyToRelease();
    return 0;
}

// ─── SDL drawing helpers ─────────────────────────────────────────────────────

static void sdl_rect(int x, int y, int w, int h, SDL_Color c)
{
    SDL_SetRenderDrawColor(s_renderer, c.r, c.g, c.b, c.a);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderFillRect(s_renderer, &r);
}

static void sdl_outline(int x, int y, int w, int h, SDL_Color c)
{
    SDL_SetRenderDrawColor(s_renderer, c.r, c.g, c.b, c.a);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderDrawRect(s_renderer, &r);
}

// Gradient: fills the entire renderer with a vertical gradient top→bot
static void sdl_gradient(SDL_Color top, SDL_Color bot)
{
    for (int y = 0; y < TV_H; y++) {
        uint8_t r = (uint8_t)(top.r + (bot.r - top.r) * y / TV_H);
        uint8_t g = (uint8_t)(top.g + (bot.g - top.g) * y / TV_H);
        uint8_t b = (uint8_t)(top.b + (bot.b - top.b) * y / TV_H);
        SDL_SetRenderDrawColor(s_renderer, r, g, b, 0xFF);
        SDL_RenderDrawLine(s_renderer, 0, y, TV_W, y);
    }
}

// Draw text with the given font; align: 0=left, 1=centre, 2=right
static void sdl_text(TTF_Font* font, const char* text, int x, int y, SDL_Color c, int align = 0)
{
    if (!font || !text || !text[0]) return;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text, c);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(s_renderer, surf);
    int tw = surf->w, th = surf->h;
    SDL_FreeSurface(surf);
    if (!tex) return;
    if      (align == 1) x -= tw / 2;
    else if (align == 2) x -= tw;
    SDL_Rect dst = {x, y - th, tw, th}; // y is baseline
    SDL_RenderCopy(s_renderer, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
}

static void sdl_progress_bar(int x, int y, int w, int h, double pct)
{
    sdl_rect(x, y, w, h, COL_GREEN_DRK);
    int fill = (int)(w * pct / 100.0);
    if (fill > 0) sdl_rect(x, y, fill, h, COL_GREEN);
    sdl_outline(x, y, w, h, COL_WHITE);
}

// ─── Splash screen ───────────────────────────────────────────────────────────

static void draw_splash(const char* status, double pct)
{
    sdl_gradient(COL_BG_TOP, COL_BG_BOT);

    sdl_text(s_font_xl, "Wave Browser", TV_W/2, TV_H/2 - 10, COL_WHITE, 1);

    if (status && status[0])
        sdl_text(s_font_lg, status, TV_W/2, TV_H/2 + 60, COL_WHITE_DIM, 1);

    if (pct >= 0.0) {
        int bw = 800, bh = 20, bx = (TV_W - 800) / 2, by = TV_H - 110;
        sdl_progress_bar(bx, by, bw, bh, pct);
        char pct_str[16];
        snprintf(pct_str, sizeof(pct_str), "%d%%", (int)pct);
        sdl_text(s_font_lg, pct_str, TV_W/2, TV_H - 68, COL_WHITE, 1);
    }

    SDL_RenderPresent(s_renderer);
}

// ─── Browser UI ──────────────────────────────────────────────────────────────

static void draw_tab(int idx, int active)
{
    int tx = idx * TAB_W;
    int ty = TOOLBAR_H;
    SDL_Color bg = active ? COL_TAB_ACTIVE : COL_TAB_INACTIVE;
    sdl_rect(tx, ty + 2, TAB_W - 2, TAB_H, bg);
    sdl_rect(tx + 8, ty + 10, 14, 14, COL_FAVICON_BG);

    const char* title = s_tabs[idx].title[0] ? s_tabs[idx].title : "New Tab";
    char trunc[24];
    strncpy(trunc, title, 20);
    trunc[20] = '\0';
    sdl_text(s_font_sm, trunc,  tx + 28,       ty + TAB_H - 4, COL_TAB_TEXT,  0);
    sdl_text(s_font_sm, "x",    tx + TAB_W - 18, ty + TAB_H - 4, COL_CLOSE_BTN, 0);

    if (active)
        sdl_rect(tx, ty + TAB_H, TAB_W - 2, 2, COL_TAB_ACTIVE);
}

static void draw_browser_ui(void)
{
    // Background
    sdl_rect(0, 0, TV_W, TV_H, COL_CONTENT_BG);

    // Toolbar
    sdl_rect(0, 0, TV_W, TOOLBAR_H, COL_CHROME_BG);

    // Back / forward / reload buttons (simple shapes)
    sdl_rect(BTN_BACK_X,    6, BTN_SIZE, BTN_SIZE, COL_CHROME_BG);
    sdl_text(s_font_md, "<", BTN_BACK_X + BTN_SIZE/2, TOOLBAR_H - 8, COL_GRAY, 1);
    sdl_rect(BTN_FWD_X,     6, BTN_SIZE, BTN_SIZE, COL_CHROME_BG);
    sdl_text(s_font_md, ">", BTN_FWD_X  + BTN_SIZE/2, TOOLBAR_H - 8, COL_GRAY, 1);
    sdl_outline(BTN_RELOAD_X + 4, 10, BTN_SIZE - 8, BTN_SIZE - 8, COL_GRAY);

    // Address bar
    sdl_rect(ADDR_BAR_X, ADDR_BAR_Y, ADDR_BAR_W, ADDR_BAR_H, COL_ADDR_BG);
    sdl_outline(ADDR_BAR_X, ADDR_BAR_Y, ADDR_BAR_W, ADDR_BAR_H, COL_ADDR_BORDER);

    const char* url = s_tabs[s_active_tab].url[0] ?
        s_tabs[s_active_tab].url : "Search or type a URL";
    SDL_Color url_col = s_tabs[s_active_tab].url[0] ? COL_ADDR_TEXT : COL_GRAY;
    sdl_text(s_font_md, url, ADDR_BAR_X + 10, ADDR_BAR_Y + ADDR_BAR_H - 4, url_col, 0);

    // Toolbar bottom line
    sdl_rect(0, TOOLBAR_H - 1, TV_W, 1, COL_TOOLBAR_LINE);

    // Tab bar
    sdl_rect(0, TOOLBAR_H, TV_W, TAB_BAR_H, COL_CHROME_BG);
    for (int i = 0; i < s_tab_count; i++)
        draw_tab(i, i == s_active_tab);

    int plus_x = s_tab_count * TAB_W + 8;
    sdl_text(s_font_md, "+", plus_x, TOOLBAR_H + TAB_BAR_H - 6, COL_NEW_TAB_BTN, 0);
    sdl_rect(0, TOOLBAR_H + TAB_BAR_H - 1, TV_W, 1, COL_TOOLBAR_LINE);

    // Content area placeholder
    sdl_text(s_font_xl, "New Tab", TV_W/2, CONTENT_Y + CONTENT_H/2 + 24, COL_GRAY, 1);

    SDL_RenderPresent(s_renderer);
}

// ─── On-screen keyboard ──────────────────────────────────────────────────────

static void open_url_keyboard(void)
{
    void* workMem = MEMAllocFromDefaultHeapEx(SWKBD_WORK_SIZE, 0x1000);
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
            const char16_t* str = nn::swkbd::GetInputFormString();
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
            SDL_RenderPresent(s_renderer);
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

typedef struct { char* data; size_t size; } Buffer;

static size_t write_cb(void* contents, size_t size, size_t nmemb, void* userp)
{
    size_t total = size * nmemb;
    Buffer* buf  = (Buffer*)userp;
    char* p      = (char*)realloc(buf->data, buf->size + total + 1);
    if (!p) return 0;
    buf->data = p;
    memcpy(buf->data + buf->size, contents, total);
    buf->size += total;
    buf->data[buf->size] = '\0';
    return total;
}

static size_t file_write_cb(void* contents, size_t size, size_t nmemb, void* userp)
{
    return fwrite(contents, size, nmemb, (FILE*)userp);
}

static int progress_cb(void* /*clientp*/, curl_off_t dltotal, curl_off_t dlnow,
                       curl_off_t /*ultotal*/, curl_off_t /*ulnow*/)
{
    if (dltotal <= 0) return 0;
    draw_splash("Downloading update...", (double)dlnow / (double)dltotal * 100.0);
    return 0;
}

static int check_for_update(char* out_tag, size_t tag_size)
{
    CURL* curl = curl_easy_init();
    if (!curl) return 0;

    Buffer buf = {(char*)malloc(1), 0};
    if (!buf.data) { curl_easy_cleanup(curl); return 0; }
    buf.data[0] = '\0';

    curl_easy_setopt(curl, CURLOPT_URL,            GITHUB_API_URL);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,      "wave-browser/1.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &buf);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        10L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    int update_available = 0;
    if (res == CURLE_OK) {
        char* tag_ptr = strstr(buf.data, "\"tag_name\":\"");
        if (tag_ptr) {
            tag_ptr += strlen("\"tag_name\":\"");
            char* end = strchr(tag_ptr, '"');
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

static int download_update(const char* url)
{
    FILE* f = fopen(DOWNLOAD_PATH, "wb");
    if (!f) return 1;

    CURL* curl = curl_easy_init();
    if (!curl) { fclose(f); return 1; }

    curl_easy_setopt(curl, CURLOPT_URL,              url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,        "wave-browser/1.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,    file_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,        f);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION,   1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS,       0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_cb);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA,     nullptr);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,          120L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER,   0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST,   0L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    fclose(f);
    return (res == CURLE_OK) ? 0 : 1;
}

// ─── Splash / update flow ────────────────────────────────────────────────────

static void run_splash(void)
{
    draw_splash("Loading...", -1.0);
    curl_global_init(CURL_GLOBAL_ALL);

    draw_splash("Checking for updates...", -1.0);
    char latest_tag[64] = {0};
    int has_update = check_for_update(latest_tag, sizeof(latest_tag));

    if (!has_update) {
        draw_splash("You're up to date!", -1.0);
        usleep(16000 * 60);
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
        usleep(16000 * 180);
    }
}

// ─── Input handling ──────────────────────────────────────────────────────────

static void handle_input(VPADStatus* vpad)
{
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
        if (s_active_tab >= s_tab_count)
            s_active_tab = s_tab_count - 1;
    }
}

// ─── Entry point ─────────────────────────────────────────────────────────────

int main(int /*argc*/, char** /*argv*/)
{
    // ── ProcUI init (Bloopair pattern) ────────────────────────────────────
    ProcUIInitEx(SaveCallback, nullptr);

    // ── SDL2 init ─────────────────────────────────────────────────────────
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    s_window = SDL_CreateWindow("Wave Browser",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        TV_W, TV_H, 0);

    s_renderer = SDL_CreateRenderer(s_window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    // Load fonts from the embedded font_data (Roboto Bold)
    s_font_sm = TTF_OpenFontRW(SDL_RWFromConstMem(font_data, font_data_len), 0, 13);
    s_font_md = TTF_OpenFontRW(SDL_RWFromConstMem(font_data, font_data_len), 0, 15);
    s_font_lg = TTF_OpenFontRW(SDL_RWFromConstMem(font_data, font_data_len), 0, 28);
    s_font_xl = TTF_OpenFontRW(SDL_RWFromConstMem(font_data, font_data_len), 0, 48);

    // ── App state init ────────────────────────────────────────────────────
    VPADInit();
    memset(s_tabs, 0, sizeof(s_tabs));
    strncpy(s_tabs[0].title, "New Tab", 63);

    // ── Splash / update check ─────────────────────────────────────────────
    run_splash();

    // ── Main loop (Bloopair ProcUI pattern) ───────────────────────────────
    while (s_running) {
        ProcUIStatus status = ProcUIProcessMessages(TRUE);

        if (status == PROCUI_STATUS_EXITING) {
            s_running = false;
        } else if (status == PROCUI_STATUS_RELEASE_FOREGROUND) {
            // Yield foreground back to the system (HOME menu etc.)
            ProcUIDrawDoneRelease();
        } else if (status == PROCUI_STATUS_IN_FOREGROUND) {
            VPADStatus vpad;
            VPADReadError error;
            VPADRead(VPAD_CHAN_0, &vpad, 1, &error);
            handle_input(&vpad);
            draw_browser_ui();
        }
    }

    // ── Cleanup ───────────────────────────────────────────────────────────
    curl_global_cleanup();

    TTF_CloseFont(s_font_sm);
    TTF_CloseFont(s_font_md);
    TTF_CloseFont(s_font_lg);
    TTF_CloseFont(s_font_xl);
    TTF_Quit();

    SDL_DestroyRenderer(s_renderer);
    SDL_DestroyWindow(s_window);
    SDL_Quit();

    ProcUIShutdown();
    return 0;
}
