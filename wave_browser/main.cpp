#include <proc_ui/procui.h>
#include <coreinit/foreground.h>
#include <coreinit/title.h>
#include <coreinit/memdefaultheap.h>
#include <sysapp/launch.h>
#include <vpad/input.h>
#include <nn/swkbd.h>
#include <curl/curl.h>
#include <sndcore2/core.h>
#include "font_data.h"
#include "settings.h"
#include "tv_remote.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <zlib.h>
#include "unzip.h"
#include <sys/stat.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <math.h>

// ─── Constants ───────────────────────────────────────────────────────────────

#define INSTALL_DIR     "fs:/vol/external01/wiiu/apps/WaveBrowser"
#define INSTALL_WUHB    "fs:/vol/external01/wiiu/apps/WaveBrowser/WaveBrowser.wuhb"
#define INSTALL_META    "fs:/vol/external01/wiiu/apps/WaveBrowser/meta.xml"
#define RUN_ID_PATH     "fs:/vol/external01/wiiu/apps/WaveBrowser/wave-browser-run-id.txt"
#define SESSION_PATH    "fs:/vol/external01/wiiu/apps/WaveBrowser/wave-browser-session.cfg"
#define ZIP_TMP_PATH    "fs:/vol/external01/wave-browser-update.zip"
#define ZIP_FOLDER_PFX  "WaveBrowser/"

#define MAX_UPDATE_ATTEMPTS 3

#define ACTIONS_API_URL \
    "https://api.github.com/repos/baldbuffalo/wave-browser/actions/runs" \
    "?branch=main&status=success&per_page=1"

#define ARTIFACT_ZIP_URL \
    "https://nightly.link/baldbuffalo/wave-browser/workflows/build.yml/main/WaveBrowser.zip"

// ─── Screen / UI dimensions ──────────────────────────────────────────────────

#define TV_W        1280
#define TV_H         720
#define DRC_W        854
#define DRC_H        480

#define MAX_TABS      8
#define MAX_URL       512

#define TOOLBAR_H     48
#define TAB_BAR_H     36
#define TAB_W        180
#define TAB_H         34
#define ADDR_BAR_X   120
#define ADDR_BAR_W   860
#define ADDR_BAR_H    32
#define ADDR_BAR_Y     8
#define BTN_SIZE      36
#define BTN_BACK_X     8
#define BTN_FWD_X     52
#define BTN_RELOAD_X  96
#define CONTENT_Y    (TOOLBAR_H + TAB_BAR_H)
#define CONTENT_H    (TV_H - CONTENT_Y)

#define GEAR_BTN_X   (TV_W - 44)
#define GEAR_BTN_Y    6
#define GEAR_BTN_SIZE 36

#define SWKBD_WORK_SIZE 0x20000

// ─── Colours ─────────────────────────────────────────────────────────────────

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
static constexpr SDL_Color COL_BLUE         = {0x42,0x85,0xF4,0xFF};
static constexpr SDL_Color COL_FOCUS_RING   = {0x42,0x85,0xF4,0xFF};

// ─── Tab state ───────────────────────────────────────────────────────────────

typedef struct {
    char url[MAX_URL];
    char title[64];
} Tab;

static Tab  s_tabs[MAX_TABS];
static int  s_tab_count  = 1;
static int  s_active_tab = 0;

// ─── UI state ────────────────────────────────────────────────────────────────

static bool s_in_settings       = false;
static bool s_show_tab_switcher = false;
static bool s_was_backgrounded  = false;

// ─── Focus system ────────────────────────────────────────────────────────────

static int s_focus_row = 0;
static int s_focus_col = 3;

static int  s_stick_held   = 0;
static bool s_stick_active = false;

#define STICK_DEAD    0.45f
#define STICK_REPEAT  18

// ─── Global touch state ──────────────────────────────────────────────────────

static bool s_tp_prev   = false;
static int  s_tp_last_x = 0;
static int  s_tp_last_y = 0;
static bool s_tp_tapped = false;

static void poll_touch(VPADStatus* vpad)
{
    VPADTouchData tp;
    VPADGetTPCalibratedPointEx(VPAD_CHAN_0, VPAD_TP_854X480, &tp, &vpad->tpNormal);

    bool valid_touch = (tp.touched != 0) && (tp.validity == VPAD_VALID);

    if (valid_touch) {
        s_tp_last_x = (int)((float)tp.x / DRC_W * TV_W);
        s_tp_last_y = (int)((float)tp.y / DRC_H * TV_H);
    }

    s_tp_tapped = s_tp_prev && !valid_touch;
    s_tp_prev   = valid_touch;
}

// ─── SDL globals ─────────────────────────────────────────────────────────────

static SDL_Window*   s_window   = nullptr;
static SDL_Renderer* s_renderer = nullptr;
static TTF_Font*     s_font_sm  = nullptr;
static TTF_Font*     s_font_md  = nullptr;
static TTF_Font*     s_font_lg  = nullptr;
static TTF_Font*     s_font_xl  = nullptr;

// ─── ProcUI ──────────────────────────────────────────────────────────────────

static bool s_running = true;

static uint32_t SaveCallback(void* /*context*/)
{
    OSSavesDone_ReadyToRelease();
    return 0;
}

// ─── Session persistence ─────────────────────────────────────────────────────

static void save_session()
{
    FILE* f = fopen(SESSION_PATH, "w");
    if (!f) return;
    fprintf(f, "tab_count=%d\n",  s_tab_count);
    fprintf(f, "active_tab=%d\n", s_active_tab);
    for (int i = 0; i < s_tab_count; i++) {
        fprintf(f, "url_%d=%s\n",   i, s_tabs[i].url);
        fprintf(f, "title_%d=%s\n", i, s_tabs[i].title);
    }
    fclose(f);
}

static void load_session()
{
    FILE* f = fopen(SESSION_PATH, "r");
    if (!f) return;

    char line[MAX_URL + 32];
    while (fgets(line, sizeof(line), f)) {
        size_t ln = strlen(line);
        if (ln && line[ln - 1] == '\n') line[--ln] = '\0';

        int n;
        if      (sscanf(line, "tab_count=%d",  &n) == 1) {
            s_tab_count  = (n > 0 && n <= MAX_TABS) ? n : 1;
        }
        else if (sscanf(line, "active_tab=%d", &n) == 1) {
            s_active_tab = n;
        }
        else if (sscanf(line, "url_%d=",   &n) == 1) {
            char* eq = strchr(line, '=');
            if (eq && n >= 0 && n < MAX_TABS)
                strncpy(s_tabs[n].url,   eq + 1, MAX_URL - 1);
        }
        else if (sscanf(line, "title_%d=", &n) == 1) {
            char* eq = strchr(line, '=');
            if (eq && n >= 0 && n < MAX_TABS)
                strncpy(s_tabs[n].title, eq + 1, 63);
        }
    }
    fclose(f);
    if (s_active_tab >= s_tab_count) s_active_tab = s_tab_count - 1;
}

// ─── Drawing helpers ─────────────────────────────────────────────────────────

static void sdl_rect(int x, int y, int w, int h, SDL_Color c)
{
    SDL_SetRenderDrawColor(s_renderer, c.r, c.g, c.b, c.a);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderFillRect(s_renderer, &r);
}

static void sdl_outline(int x, int y, int w, int h, SDL_Color c, int thickness = 1)
{
    SDL_SetRenderDrawColor(s_renderer, c.r, c.g, c.b, c.a);
    for (int d = 0; d < thickness; d++) {
        SDL_Rect r = {x + d, y + d, w - d * 2, h - d * 2};
        SDL_RenderDrawRect(s_renderer, &r);
    }
}

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

static void sdl_text(TTF_Font* font, const char* text,
                     int x, int y, SDL_Color c, int align = 0)
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
    SDL_Rect dst = {x, y - th, tw, th};
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

static void draw_gear_icon(int x, int y, int size, SDL_Color c)
{
    int bar_h = size / 6;
    if (bar_h < 2) bar_h = 2;
    int gap = (size - bar_h * 3) / 2;
    for (int i = 0; i < 3; i++)
        sdl_rect(x, y + i * (bar_h + gap), size, bar_h, c);
}

// ─── Splash ──────────────────────────────────────────────────────────────────

static void draw_splash(const char* status, double pct)
{
    sdl_gradient(COL_BG_TOP, COL_BG_BOT);
    sdl_text(s_font_xl, "Wave Browser", TV_W/2, TV_H/2 - 10, COL_WHITE, 1);

    if (status && status[0])
        sdl_text(s_font_lg, status, TV_W/2, TV_H/2 + 60, COL_WHITE_DIM, 1);

    if (pct >= 0.0) {
        int bx = (TV_W - 800) / 2, by = TV_H - 110;
        sdl_progress_bar(bx, by, 800, 20, pct);
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", (int)pct);
        sdl_text(s_font_lg, buf, TV_W/2, TV_H - 68, COL_WHITE, 1);
    }

    SDL_RenderPresent(s_renderer);
}

// ─── Tab switcher overlay ────────────────────────────────────────────────────

static void draw_tab_switcher()
{
    SDL_SetRenderDrawBlendMode(s_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(s_renderer, 0x10, 0x10, 0x20, 200);
    SDL_Rect full = {0, 0, TV_W, TV_H};
    SDL_RenderFillRect(s_renderer, &full);
    SDL_SetRenderDrawBlendMode(s_renderer, SDL_BLENDMODE_NONE);

    sdl_text(s_font_lg, "Open Tabs", TV_W / 2, 54, COL_WHITE, 1);

    const int CARD_W = 210, CARD_H = 130, CARD_GAP = 24;
    int total_w = s_tab_count * CARD_W + (s_tab_count - 1) * CARD_GAP;
    int start_x = (TV_W - total_w) / 2;
    int card_y  = TV_H / 2 - CARD_H / 2;

    for (int i = 0; i < s_tab_count; i++) {
        int cx      = start_x + i * (CARD_W + CARD_GAP);
        bool active = (i == s_active_tab);

        sdl_rect(cx + 3, card_y + 3, CARD_W, CARD_H, {0x00,0x00,0x00,0x60});
        sdl_rect(cx, card_y, CARD_W, CARD_H, COL_WHITE);

        if (active)
            for (int d = 0; d < 3; d++)
                sdl_outline(cx - d, card_y - d, CARD_W + d*2, CARD_H + d*2, COL_BLUE);
        else
            sdl_outline(cx, card_y, CARD_W, CARD_H, COL_WHITE_DIM);

        sdl_rect(cx, card_y, CARD_W, 28, COL_FAVICON_BG);
        const char* title = s_tabs[i].title[0] ? s_tabs[i].title : "New Tab";
        char short_title[20];
        strncpy(short_title, title, 18);
        short_title[18] = '\0';
        sdl_text(s_font_sm, short_title, cx + CARD_W / 2, card_y + 20, COL_WHITE, 1);

        if (s_tabs[i].url[0]) {
            char short_url[28];
            strncpy(short_url, s_tabs[i].url, 26);
            short_url[26] = '\0';
            sdl_text(s_font_sm, short_url, cx + 8, card_y + 58, COL_GRAY, 0);
        } else {
            sdl_text(s_font_sm, "New Tab", cx + CARD_W/2, card_y + CARD_H/2 + 10,
                     COL_GRAY, 1);
        }

        char num[4];
        snprintf(num, sizeof(num), "%d", i + 1);
        sdl_rect(cx + CARD_W - 22, card_y + 32, 18, 18, active ? COL_BLUE : COL_GRAY);
        sdl_text(s_font_sm, num, cx + CARD_W - 13, card_y + 48, COL_WHITE, 1);
    }

    sdl_text(s_font_sm,
        "ZL/ZR: switch  |  A: open  |  B or SELECT: close",
        TV_W / 2, TV_H - 16, COL_WHITE_DIM, 1);

    SDL_RenderPresent(s_renderer);
}

static void handle_switcher_input(VPADStatus* vpad)
{
    uint32_t btn = vpad->trigger;

    if ((btn & VPAD_BUTTON_MINUS) || (btn & VPAD_BUTTON_B)) {
        s_show_tab_switcher = false;
        return;
    }
    if ((btn & VPAD_BUTTON_ZL) && s_active_tab > 0)               s_active_tab--;
    if ((btn & VPAD_BUTTON_ZR) && s_active_tab < s_tab_count - 1) s_active_tab++;
    if (btn & VPAD_BUTTON_A) {
        s_show_tab_switcher = false;
        return;
    }

    if (s_tp_tapped) {
        const int CARD_W = 210, CARD_H = 130, CARD_GAP = 24;
        int total_w = s_tab_count * CARD_W + (s_tab_count - 1) * CARD_GAP;
        int start_x = (TV_W - total_w) / 2;
        int card_y  = TV_H / 2 - CARD_H / 2;

        for (int i = 0; i < s_tab_count; i++) {
            int cx = start_x + i * (CARD_W + CARD_GAP);
            if (s_tp_last_x >= cx && s_tp_last_x < cx + CARD_W &&
                s_tp_last_y >= card_y && s_tp_last_y < card_y + CARD_H) {
                s_active_tab        = i;
                s_show_tab_switcher = false;
                break;
            }
        }
    }
}

// ─── Browser UI ──────────────────────────────────────────────────────────────

static void draw_focus_ring(int x, int y, int w, int h)
{
    sdl_outline(x - 2, y - 2, w + 4, h + 4, COL_FOCUS_RING, 2);
}

static void draw_tab(int idx, bool active)
{
    int tx = idx * TAB_W, ty = TOOLBAR_H;
    sdl_rect(tx, ty + 2, TAB_W - 2, TAB_H, active ? COL_TAB_ACTIVE : COL_TAB_INACTIVE);
    sdl_rect(tx + 8, ty + 10, 14, 14, COL_FAVICON_BG);

    if (s_focus_row == 1 && s_focus_col == idx)
        sdl_outline(tx + 1, ty + 2, TAB_W - 3, TAB_H, COL_FOCUS_RING, 2);

    const char* title = s_tabs[idx].title[0] ? s_tabs[idx].title : "New Tab";
    char trunc[24];
    strncpy(trunc, title, 20);
    trunc[20] = '\0';
    sdl_text(s_font_sm, trunc, tx + 28,         ty + TAB_H - 4, COL_TAB_TEXT,  0);
    sdl_text(s_font_sm, "x",   tx + TAB_W - 18, ty + TAB_H - 4, COL_CLOSE_BTN, 0);

    if (active)
        sdl_rect(tx, ty + TAB_H, TAB_W - 2, 2, COL_TAB_ACTIVE);
}

static void draw_browser_ui(void)
{
    sdl_rect(0, 0, TV_W, TV_H,      COL_CONTENT_BG);
    sdl_rect(0, 0, TV_W, TOOLBAR_H, COL_CHROME_BG);

    bool f_back   = (s_focus_row == 0 && s_focus_col == 0);
    bool f_fwd    = (s_focus_row == 0 && s_focus_col == 1);
    bool f_reload = (s_focus_row == 0 && s_focus_col == 2);
    bool f_addr   = (s_focus_row == 0 && s_focus_col == 3);
    bool f_gear   = (s_focus_row == 0 && s_focus_col == 4);

    sdl_rect(BTN_BACK_X, 6, BTN_SIZE, BTN_SIZE, COL_CHROME_BG);
    if (f_back) draw_focus_ring(BTN_BACK_X, 6, BTN_SIZE, BTN_SIZE);
    sdl_text(s_font_md, "<", BTN_BACK_X + BTN_SIZE/2, TOOLBAR_H - 8, COL_GRAY, 1);

    sdl_rect(BTN_FWD_X, 6, BTN_SIZE, BTN_SIZE, COL_CHROME_BG);
    if (f_fwd) draw_focus_ring(BTN_FWD_X, 6, BTN_SIZE, BTN_SIZE);
    sdl_text(s_font_md, ">", BTN_FWD_X + BTN_SIZE/2, TOOLBAR_H - 8, COL_GRAY, 1);

    sdl_outline(BTN_RELOAD_X + 4, 10, BTN_SIZE - 8, BTN_SIZE - 8, COL_GRAY);
    if (f_reload) draw_focus_ring(BTN_RELOAD_X, 6, BTN_SIZE, BTN_SIZE);

    sdl_rect(ADDR_BAR_X, ADDR_BAR_Y, ADDR_BAR_W, ADDR_BAR_H, COL_ADDR_BG);
    SDL_Color addr_border = f_addr ? COL_FOCUS_RING : COL_ADDR_BORDER;
    sdl_outline(ADDR_BAR_X, ADDR_BAR_Y, ADDR_BAR_W, ADDR_BAR_H, addr_border,
                f_addr ? 2 : 1);

    const char* url = s_tabs[s_active_tab].url[0]
        ? s_tabs[s_active_tab].url
        : "Select me and press A to enter URL";
    SDL_Color url_col = s_tabs[s_active_tab].url[0] ? COL_ADDR_TEXT : COL_GRAY;
    sdl_text(s_font_md, url, ADDR_BAR_X + 10,
             ADDR_BAR_Y + ADDR_BAR_H - 4, url_col, 0);

    sdl_rect(GEAR_BTN_X, GEAR_BTN_Y, GEAR_BTN_SIZE, GEAR_BTN_SIZE, COL_CHROME_BG);
    if (f_gear) draw_focus_ring(GEAR_BTN_X, GEAR_BTN_Y, GEAR_BTN_SIZE, GEAR_BTN_SIZE);
    draw_gear_icon(GEAR_BTN_X + 6, GEAR_BTN_Y + 8, GEAR_BTN_SIZE - 12, COL_GRAY);

    sdl_rect(0, TOOLBAR_H - 1, TV_W, 1, COL_TOOLBAR_LINE);
    sdl_rect(0, TOOLBAR_H,     TV_W, TAB_BAR_H, COL_CHROME_BG);

    for (int i = 0; i < s_tab_count; i++)
        draw_tab(i, i == s_active_tab);

    int new_tab_x = s_tab_count * TAB_W;
    bool f_newtab = (s_focus_row == 1 && s_focus_col == s_tab_count);
    SDL_Color ntcol = f_newtab ? COL_FOCUS_RING : COL_NEW_TAB_BTN;
    sdl_text(s_font_md, "+", new_tab_x + 8, TOOLBAR_H + TAB_BAR_H - 6, ntcol, 0);
    if (f_newtab)
        sdl_outline(new_tab_x + 2, TOOLBAR_H + 2, 28, TAB_H, COL_FOCUS_RING, 2);

    sdl_rect(0, TOOLBAR_H + TAB_BAR_H - 1, TV_W, 1, COL_TOOLBAR_LINE);

    sdl_text(s_font_xl, "New Tab", TV_W/2, CONTENT_Y + CONTENT_H/2 + 24, COL_GRAY, 1);

    const char* hint = g_settings.improved_multitasking
        ? "\xe2\x96\xb2\xe2\x96\xbc: rows  \xe2\x97\x84\xe2\x96\xba: move  A: select  B: back  X: new tab  Y: close tab  SELECT: tab view"
        : "\xe2\x96\xb2\xe2\x96\xbc: rows  \xe2\x97\x84\xe2\x96\xba: move  A: select  B: back  X: new tab  Y: close tab";
    sdl_text(s_font_sm, hint, TV_W/2, TV_H - 8, COL_GRAY, 1);

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

        nn::swkbd::ControllerInfo ci = {};
        ci.vpad = &vpad;
        nn::swkbd::Calc(ci);

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
        memcpy(s_tabs[s_active_tab].url,   result, MAX_URL - 1);
        s_tabs[s_active_tab].url[MAX_URL - 1] = '\0';
        memcpy(s_tabs[s_active_tab].title, result, 63);
        s_tabs[s_active_tab].title[63] = '\0';
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

static int progress_cb(void* /*cp*/, curl_off_t dltotal, curl_off_t dlnow,
                       curl_off_t /*ul*/, curl_off_t /*uln*/)
{
    if (dltotal <= 0) return 0;
    draw_splash("Downloading update...", (double)dlnow / (double)dltotal * 100.0);
    return 0;
}

static void curl_common(CURL* curl)
{
    curl_easy_setopt(curl, CURLOPT_USERAGENT,      "wave-browser/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
}

static char* fetch_string(const char* url)
{
    CURL* curl = curl_easy_init();
    if (!curl) return nullptr;

    Buffer buf = {(char*)malloc(1), 0};
    if (!buf.data) { curl_easy_cleanup(curl); return nullptr; }
    buf.data[0] = '\0';

    curl_easy_setopt(curl, CURLOPT_URL,           url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,     &buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,       15L);
    curl_common(curl);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) { free(buf.data); return nullptr; }
    return buf.data;
}

static int fetch_file(const char* url, const char* path)
{
    FILE* f = fopen(path, "wb");
    if (!f) return 1;

    CURL* curl = curl_easy_init();
    if (!curl) { fclose(f); return 1; }

    curl_easy_setopt(curl, CURLOPT_URL,              url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,    file_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,        f);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS,       0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_cb);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA,     nullptr);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,          180L);
    curl_common(curl);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    fclose(f);
    return (res == CURLE_OK) ? 0 : 1;
}

// ─── Run-ID persistence ──────────────────────────────────────────────────────

static long long read_stored_run_id(void)
{
    FILE* f = fopen(RUN_ID_PATH, "r");
    if (!f) return 0;
    long long id = 0;
    fscanf(f, "%lld", &id);
    fclose(f);
    return id;
}

static void write_run_id(long long id)
{
    FILE* f = fopen(RUN_ID_PATH, "w");
    if (!f) return;
    fprintf(f, "%lld", id);
    fclose(f);
}

// ─── File backup / restore helpers ───────────────────────────────────────────

static char* file_backup(const char* path, size_t* out_len)
{
    *out_len = 0;
    FILE* f = fopen(path, "rb");
    if (!f) return nullptr;

    fseek(f, 0, SEEK_END);
    size_t len = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buf = (char*)malloc(len + 1);
    if (!buf) { fclose(f); return nullptr; }

    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);

    *out_len = len;
    return buf;
}

static void file_restore(const char* path, const char* buf, size_t len)
{
    if (!buf || len == 0) return;
    FILE* f = fopen(path, "wb");
    if (!f) return;
    fwrite(buf, 1, len, f);
    fclose(f);
}

// ─── Install directory helpers ───────────────────────────────────────────────

static void mkdir_p(const char* path)
{
    char tmp[512];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0777);
            *p = '/';
        }
    }
    mkdir(tmp, 0777);
}

static void remove_old_install(void)
{
    remove(INSTALL_WUHB);
    remove(INSTALL_META);
    remove(RUN_ID_PATH);
    remove(SESSION_PATH);
    rmdir(INSTALL_DIR);
}

// ─── ZIP extraction ──────────────────────────────────────────────────────────

static int extract_zip_to_dir(const char* zip_path, const char* out_dir)
{
    unzFile zf = unzOpen(zip_path);
    if (!zf) return 1;

    mkdir_p(out_dir);

    const size_t pfx_len = strlen(ZIP_FOLDER_PFX);
    int result = 0;

    if (unzGoToFirstFile(zf) == UNZ_OK) {
        do {
            char fname[256] = {0};
            unz_file_info fi;
            unzGetCurrentFileInfo(zf, &fi, fname, sizeof(fname) - 1,
                                  nullptr, 0, nullptr, 0);

            size_t nlen = strlen(fname);
            if (nlen > 0 && fname[nlen - 1] == '/') continue;

            const char* rel = fname;
            if (strncmp(fname, ZIP_FOLDER_PFX, pfx_len) == 0)
                rel = fname + pfx_len;

            if (rel[0] == '\0') continue;

            char out_path[512];
            snprintf(out_path, sizeof(out_path), "%s/%s", out_dir, rel);

            if (unzOpenCurrentFile(zf) != UNZ_OK) { result = 1; break; }

            FILE* out = fopen(out_path, "wb");
            if (!out) { unzCloseCurrentFile(zf); result = 1; break; }

            uint8_t buf[8192];
            int bytes_read;
            bool file_ok = true;
            while ((bytes_read = unzReadCurrentFile(zf, buf, sizeof(buf))) > 0) {
                if (fwrite(buf, 1, (size_t)bytes_read, out) != (size_t)bytes_read) {
                    file_ok = false;
                    break;
                }
            }

            fclose(out);
            unzCloseCurrentFile(zf);

            if (!file_ok || bytes_read < 0) { result = 1; break; }

        } while (unzGoToNextFile(zf) == UNZ_OK);
    }

    unzClose(zf);
    return result;
}

// ─── Update check ────────────────────────────────────────────────────────────

static void run_splash_and_update(void)
{
    draw_splash("Loading...", -1.0);
    curl_global_init(CURL_GLOBAL_ALL);

    draw_splash("Checking for updates...", -1.0);

    char* json = fetch_string(ACTIONS_API_URL);
    if (!json) {
        draw_splash("No network. Starting...", -1.0);
        usleep(16000 * 90);
        return;
    }

    long long latest_run_id = 0;
    const char* id_ptr = strstr(json, "\"id\":");
    if (id_ptr) {
        id_ptr += 5;
        while (*id_ptr == ' ') id_ptr++;
        latest_run_id = atoll(id_ptr);
    }
    free(json);

    if (latest_run_id == 0) {
        draw_splash("Couldn't read run ID. Starting...", -1.0);
        usleep(16000 * 90);
        return;
    }

    long long stored_run_id = read_stored_run_id();
    if (latest_run_id == stored_run_id) {
        draw_splash("You're up to date!", -1.0);
        usleep(16000 * 60);
        return;
    }

    char msg[80];
    snprintf(msg, sizeof(msg), "New build found (run #%lld). Downloading...",
             latest_run_id);
    draw_splash(msg, 0.0);
    usleep(800000);

    // Back up session before any destructive work.
    size_t session_len = 0;
    char*  session_buf = file_backup(SESSION_PATH, &session_len);

    // ── Download once ─────────────────────────────────────────────────────────
    remove(ZIP_TMP_PATH);
    if (fetch_file(ARTIFACT_ZIP_URL, ZIP_TMP_PATH) != 0) {
        draw_splash("Download failed. Starting anyway...", -1.0);
        usleep(16000 * 120);
        free(session_buf);
        return;
    }

    // ── Retry only the extraction (zip is already on disk) ────────────────────
    bool success = false;

    for (int attempt = 1; attempt <= MAX_UPDATE_ATTEMPTS && !success; attempt++) {

        if (attempt > 1) {
            char retry_msg[72];
            snprintf(retry_msg, sizeof(retry_msg),
                     "Retrying extraction... (attempt %d / %d)",
                     attempt, MAX_UPDATE_ATTEMPTS);
            draw_splash(retry_msg, 100.0);
            usleep(2000000);
        }

        draw_splash("Removing old version...", 100.0);
        remove_old_install();

        draw_splash("Installing update...", 100.0);
        int ex_ok = extract_zip_to_dir(ZIP_TMP_PATH, INSTALL_DIR);

        if (ex_ok != 0) {
            draw_splash("Extraction failed.", -1.0);
            usleep(16000 * 60);
            continue;
        }

        write_run_id(latest_run_id);
        file_restore(SESSION_PATH, session_buf, session_len);
        success = true;
    }

    // Always clean up the zip regardless of outcome.
    remove(ZIP_TMP_PATH);
    free(session_buf);

    if (!success) {
        draw_splash("Update failed after all attempts. Starting anyway...", -1.0);
        usleep(16000 * 180);
        return;
    }

    draw_splash("Update installed! Please restart Wave Browser.", 100.0);
    usleep(16000 * 300);
}

// ─── Touch hit test ──────────────────────────────────────────────────────────

static bool touch_hit(int tap_x, int tap_y,
                      int tv_x, int tv_y, int tv_w, int tv_h)
{
    return tap_x >= tv_x && tap_x < tv_x + tv_w &&
           tap_y >= tv_y && tap_y < tv_y + tv_h;
}

// ─── Focus navigation helpers ────────────────────────────────────────────────

static void focus_clamp()
{
    if (s_focus_row == 0) {
        if (s_focus_col < 0) s_focus_col = 0;
        if (s_focus_col > 4) s_focus_col = 4;
    } else {
        if (s_focus_col < 0) s_focus_col = 0;
        int max_col = s_tab_count;
        if (s_focus_col > max_col) s_focus_col = max_col;
    }
}

static void focus_activate()
{
    if (s_focus_row == 0) {
        switch (s_focus_col) {
            case 0: /* Back   — stub */  break;
            case 1: /* Fwd    — stub */  break;
            case 2: /* Reload — stub */  break;
            case 3: open_url_keyboard(); break;
            case 4: s_in_settings = true; break;
        }
    } else {
        if (s_focus_col < s_tab_count) {
            s_active_tab = s_focus_col;
        } else {
            if (s_tab_count < MAX_TABS) {
                memset(&s_tabs[s_tab_count], 0, sizeof(Tab));
                strcpy(s_tabs[s_tab_count].title, "New Tab");
                s_active_tab = s_tab_count++;
                s_focus_col  = s_active_tab;
            }
        }
    }
}

// ─── Browser input ───────────────────────────────────────────────────────────

static void handle_input(VPADStatus* vpad)
{
    uint32_t btn = vpad->trigger;

    if (btn & VPAD_BUTTON_UP) {
        if (s_focus_row == 1) { s_focus_row = 0; s_focus_col = 3; }
    }
    if (btn & VPAD_BUTTON_DOWN) {
        if (s_focus_row == 0) { s_focus_row = 1; s_focus_col = s_active_tab; }
    }
    if (btn & VPAD_BUTTON_LEFT) {
        s_focus_col--;
        focus_clamp();
        if (s_focus_row == 1 && s_focus_col < s_tab_count)
            s_active_tab = s_focus_col;
    }
    if (btn & VPAD_BUTTON_RIGHT) {
        s_focus_col++;
        focus_clamp();
        if (s_focus_row == 1 && s_focus_col < s_tab_count)
            s_active_tab = s_focus_col;
    }

    float sx = vpad->leftStick.x;
    float sy = vpad->leftStick.y;

    bool stick_any = (sx < -STICK_DEAD || sx > STICK_DEAD ||
                      sy >  STICK_DEAD || sy < -STICK_DEAD);
    if (stick_any) {
        s_stick_held++;
        bool fire = (s_stick_held == 1 || s_stick_held >= STICK_REPEAT);
        if (s_stick_held >= STICK_REPEAT) s_stick_held = STICK_REPEAT;

        if (fire) {
            if (sy >  STICK_DEAD && s_focus_row == 1) {
                s_focus_row = 0; s_focus_col = 3;
            }
            if (sy < -STICK_DEAD && s_focus_row == 0) {
                s_focus_row = 1; s_focus_col = s_active_tab;
            }
            if (sx < -STICK_DEAD) {
                s_focus_col--;
                focus_clamp();
                if (s_focus_row == 1 && s_focus_col < s_tab_count)
                    s_active_tab = s_focus_col;
            }
            if (sx >  STICK_DEAD) {
                s_focus_col++;
                focus_clamp();
                if (s_focus_row == 1 && s_focus_col < s_tab_count)
                    s_active_tab = s_focus_col;
            }
        }
    } else {
        s_stick_held = 0;
    }

    if ((btn & VPAD_BUTTON_ZL) && s_active_tab > 0) {
        s_active_tab--;
        s_focus_row = 1;
        s_focus_col = s_active_tab;
    }
    if ((btn & VPAD_BUTTON_ZR) && s_active_tab < s_tab_count - 1) {
        s_active_tab++;
        s_focus_row = 1;
        s_focus_col = s_active_tab;
    }

    if (btn & VPAD_BUTTON_A) focus_activate();

    if ((btn & VPAD_BUTTON_X) && s_tab_count < MAX_TABS) {
        memset(&s_tabs[s_tab_count], 0, sizeof(Tab));
        strcpy(s_tabs[s_tab_count].title, "New Tab");
        s_active_tab = s_tab_count++;
        s_focus_row  = 1;
        s_focus_col  = s_active_tab;
    }

    if ((btn & VPAD_BUTTON_Y) && s_tab_count > 1) {
        int close_idx = s_active_tab;
        if (s_focus_row == 1 && s_focus_col < s_tab_count)
            close_idx = s_focus_col;

        for (int i = close_idx; i < s_tab_count - 1; i++)
            s_tabs[i] = s_tabs[i + 1];
        s_tab_count--;

        if (s_active_tab >= s_tab_count) s_active_tab = s_tab_count - 1;
        if (s_focus_col >= s_tab_count)  s_focus_col  = s_tab_count - 1;
        s_active_tab = s_focus_col < s_tab_count ? s_focus_col : s_tab_count - 1;
    }

    if ((btn & VPAD_BUTTON_MINUS) && g_settings.improved_multitasking)
        s_show_tab_switcher = true;

    // ── Touch ─────────────────────────────────────────────────────────────────
    if (s_tp_tapped) {
        int tx = s_tp_last_x, ty = s_tp_last_y;

        if (touch_hit(tx, ty, GEAR_BTN_X, GEAR_BTN_Y, GEAR_BTN_SIZE, GEAR_BTN_SIZE)) {
            s_in_settings = true;
            return;
        }
        if (touch_hit(tx, ty, ADDR_BAR_X, ADDR_BAR_Y, ADDR_BAR_W, ADDR_BAR_H)) {
            s_focus_row = 0; s_focus_col = 3;
            open_url_keyboard();
            return;
        }
        if (touch_hit(tx, ty, BTN_BACK_X,   6, BTN_SIZE, BTN_SIZE)) {
            s_focus_row = 0; s_focus_col = 0; return;
        }
        if (touch_hit(tx, ty, BTN_FWD_X,    6, BTN_SIZE, BTN_SIZE)) {
            s_focus_row = 0; s_focus_col = 1; return;
        }
        if (touch_hit(tx, ty, BTN_RELOAD_X, 6, BTN_SIZE, BTN_SIZE)) {
            s_focus_row = 0; s_focus_col = 2; return;
        }

        for (int i = 0; i < s_tab_count; i++) {
            if (s_tab_count > 1 &&
                touch_hit(tx, ty, i * TAB_W + TAB_W - 20, TOOLBAR_H, 20, TAB_H)) {
                for (int j = i; j < s_tab_count - 1; j++) s_tabs[j] = s_tabs[j + 1];
                s_tab_count--;
                if (s_active_tab >= s_tab_count) s_active_tab = s_tab_count - 1;
                s_focus_row = 1;
                s_focus_col = s_active_tab;
                return;
            }
            if (touch_hit(tx, ty, i * TAB_W, TOOLBAR_H, TAB_W - 20, TAB_H)) {
                s_active_tab = i;
                s_focus_row  = 1;
                s_focus_col  = i;
                return;
            }
        }

        if (s_tab_count < MAX_TABS &&
            touch_hit(tx, ty, s_tab_count * TAB_W, TOOLBAR_H, 36, TAB_BAR_H)) {
            memset(&s_tabs[s_tab_count], 0, sizeof(Tab));
            strcpy(s_tabs[s_tab_count].title, "New Tab");
            s_active_tab = s_tab_count++;
            s_focus_row  = 1;
            s_focus_col  = s_active_tab;
        }
    }
}

// ─── Entry point ─────────────────────────────────────────────────────────────

int main(int /*argc*/, char** /*argv*/)
{
    ProcUIInitEx(SaveCallback, nullptr);
    AXInit();

    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    s_window = SDL_CreateWindow("Wave Browser",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, TV_W, TV_H, 0);
    s_renderer = SDL_CreateRenderer(s_window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    s_font_sm = TTF_OpenFontRW(SDL_RWFromConstMem(font_data, font_data_len), 0, 13);
    s_font_md = TTF_OpenFontRW(SDL_RWFromConstMem(font_data, font_data_len), 0, 15);
    s_font_lg = TTF_OpenFontRW(SDL_RWFromConstMem(font_data, font_data_len), 0, 28);
    s_font_xl = TTF_OpenFontRW(SDL_RWFromConstMem(font_data, font_data_len), 0, 48);

    VPADInit();
    memset(s_tabs, 0, sizeof(s_tabs));
    strncpy(s_tabs[0].title, "New Tab", 63);

    settings_load();
    if (g_settings.improved_multitasking)
        load_session();

    run_splash_and_update();

    while (s_running) {
        ProcUIStatus status = ProcUIProcessMessages(TRUE);

        if (status == PROCUI_STATUS_EXITING) {
            s_running = false;

        } else if (status == PROCUI_STATUS_RELEASE_FOREGROUND) {
            if (g_settings.improved_multitasking) {
                save_session();
                s_was_backgrounded = true;
            }
            ProcUIDrawDoneRelease();

        } else if (status == PROCUI_STATUS_IN_FOREGROUND) {
            s_was_backgrounded = false;

            VPADStatus vpad;
            VPADReadError error;
            VPADRead(VPAD_CHAN_0, &vpad, 1, &error);

            // Poll touch once per frame so all handlers share the same result.
            poll_touch(&vpad);

            // ── TV button interception ──────────────────────────────────────
            // When "Improved Remote Control" is enabled, catch the GamePad's
            // TV button (VPAD_BUTTON_TV) before the system sees it and show
            // our custom remote overlay instead.
            if (g_settings.improved_remote &&
                (vpad.trigger & VPAD_BUTTON_TV) &&
                !s_in_settings &&
                !s_show_tab_switcher) {
                tv_remote_open();
            }

            // ── Route to active overlay ─────────────────────────────────────
            if (tv_remote_is_open()) {
                // TV remote is top-most — nothing else renders
                tv_remote_handle_input(&vpad, s_tp_tapped,
                                        s_tp_last_x, s_tp_last_y);
                tv_remote_draw(s_renderer,
                               s_font_sm, s_font_md, s_font_lg);

            } else if (s_in_settings) {
                if (!settings_handle_input(&vpad))
                    s_in_settings = false;
                else
                    settings_draw(s_renderer, s_font_sm, s_font_md, s_font_lg);

            } else if (s_show_tab_switcher) {
                handle_switcher_input(&vpad);
                draw_tab_switcher();

            } else {
                handle_input(&vpad);
                draw_browser_ui();
            }
        }
    }

    if (g_settings.improved_multitasking)
        save_session();

    curl_global_cleanup();

    TTF_CloseFont(s_font_sm);
    TTF_CloseFont(s_font_md);
    TTF_CloseFont(s_font_lg);
    TTF_CloseFont(s_font_xl);
    TTF_Quit();

    SDL_DestroyRenderer(s_renderer);
    SDL_DestroyWindow(s_window);
    SDL_Quit();

    AXQuit();
    ProcUIShutdown();
    return 0;
}
