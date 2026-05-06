#include "settings.h"
#include <stdio.h>
#include <string.h>

#define SETTINGS_PATH "fs:/vol/external01/wave-browser-settings.cfg"

// ─── Global settings object ───────────────────────────────────────────────────

WaveSettings g_settings;

// ─── Persistence ─────────────────────────────────────────────────────────────

void settings_load() {
    FILE* f = fopen(SETTINGS_PATH, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        int val;
        if (sscanf(line, "improved_multitasking=%d", &val) == 1)
            g_settings.improved_multitasking = (val != 0);
    }
    fclose(f);
}

void settings_save() {
    FILE* f = fopen(SETTINGS_PATH, "w");
    if (!f) return;
    fprintf(f, "improved_multitasking=%d\n", g_settings.improved_multitasking ? 1 : 0);
    fclose(f);
}

// ─── UI layout constants ─────────────────────────────────────────────────────

#define S_TV_W            1280
#define S_TV_H             720
#define S_DRC_W            854
#define S_DRC_H            480
#define S_HEADER_H          56
#define S_ITEM_H            88
#define S_CB_SIZE           22
#define S_CB_MARGIN         28

// ─── Colours ─────────────────────────────────────────────────────────────────

static constexpr SDL_Color SC_BG          = {0xF6, 0xF6, 0xF6, 0xFF};
static constexpr SDL_Color SC_CHROME      = {0xF2, 0xF2, 0xF2, 0xFF};
static constexpr SDL_Color SC_LINE        = {0xCE, 0xCE, 0xCE, 0xFF};
static constexpr SDL_Color SC_BACK_BTN    = {0x42, 0x85, 0xF4, 0xFF};
static constexpr SDL_Color SC_TITLE       = {0x3C, 0x3C, 0x3C, 0xFF};
static constexpr SDL_Color SC_WHITE       = {0xFF, 0xFF, 0xFF, 0xFF};
static constexpr SDL_Color SC_GRAY        = {0x9E, 0x9E, 0x9E, 0xFF};
static constexpr SDL_Color SC_BORDER      = {0xCE, 0xCE, 0xCE, 0xFF};
static constexpr SDL_Color SC_BLUE        = {0x42, 0x85, 0xF4, 0xFF};
static constexpr SDL_Color SC_ITEM_BORDER = {0xE8, 0xE8, 0xE8, 0xFF};
static constexpr SDL_Color SC_FOCUS_RING  = {0x42, 0x85, 0xF4, 0xFF};

// ─── Module-local renderer / font handles ────────────────────────────────────
// Set each frame by settings_draw() before drawing.

static SDL_Renderer* s_ren = nullptr;
static TTF_Font*     s_fsm = nullptr;
static TTF_Font*     s_fmd = nullptr;
static TTF_Font*     s_flg = nullptr;

// ─── Local drawing helpers ────────────────────────────────────────────────────

static void s_fill(int x, int y, int w, int h, SDL_Color c)
{
    SDL_SetRenderDrawColor(s_ren, c.r, c.g, c.b, c.a);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderFillRect(s_ren, &r);
}

static void s_border(int x, int y, int w, int h, SDL_Color c, int thickness = 1)
{
    SDL_SetRenderDrawColor(s_ren, c.r, c.g, c.b, c.a);
    for (int d = 0; d < thickness; d++) {
        SDL_Rect r = {x + d, y + d, w - d * 2, h - d * 2};
        SDL_RenderDrawRect(s_ren, &r);
    }
}

static void s_text(TTF_Font* font, const char* text,
                   int x, int y, SDL_Color c, int align = 0)
{
    if (!font || !text || !text[0]) return;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text, c);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(s_ren, surf);
    int tw = surf->w, th = surf->h;
    SDL_FreeSurface(surf);
    if (!tex) return;
    if      (align == 1) x -= tw / 2;
    else if (align == 2) x -= tw;
    SDL_Rect dst = {x, y - th, tw, th};
    SDL_RenderCopy(s_ren, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
}

static void s_checkbox(int x, int y, bool checked, bool focused)
{
    s_fill(x, y, S_CB_SIZE, S_CB_SIZE, SC_WHITE);
    s_border(x, y, S_CB_SIZE, S_CB_SIZE, focused ? SC_FOCUS_RING : SC_BORDER,
             focused ? 2 : 1);

    if (checked) {
        s_fill(x + 3, y + 3, S_CB_SIZE - 6, S_CB_SIZE - 6, SC_BLUE);
        // White checkmark
        SDL_SetRenderDrawColor(s_ren, 0xFF, 0xFF, 0xFF, 0xFF);
        int ax  = x + 4,             ay  = y + S_CB_SIZE / 2;
        int bx  = x + S_CB_SIZE / 2, by  = y + S_CB_SIZE - 5;
        int cx2 = x + S_CB_SIZE - 4, cy2 = y + 4;
        for (int t = -1; t <= 1; t++) {
            SDL_RenderDrawLine(s_ren, ax, ay + t, bx, by + t);
            SDL_RenderDrawLine(s_ren, bx, by + t, cx2, cy2 + t);
        }
    }
}

// ─── Setting items ────────────────────────────────────────────────────────────

struct SettingItem {
    const char* label;
    const char* desc1;
    const char* desc2;
    bool*       value;
};

static SettingItem s_items[] = {
    {
        "Improved Multi Tasking",
        "Keeps your session when Home is pressed — resume instantly",
        "like iPad. Press \xe2\x8a\x9f (SELECT) in browser to view open tabs.",
        &g_settings.improved_multitasking
    },
};
static const int s_item_count =
    (int)(sizeof(s_items) / sizeof(s_items[0]));

// D-pad selection within settings
static int s_sel = 0;

// ─── Touch state ─────────────────────────────────────────────────────────────

static bool s_tp_prev     = false;
static int  s_tp_last_x   = 0;
static int  s_tp_last_y   = 0;

// ─── Public: draw ─────────────────────────────────────────────────────────────

void settings_draw(SDL_Renderer* renderer,
                   TTF_Font* font_sm,
                   TTF_Font* font_md,
                   TTF_Font* font_lg)
{
    s_ren = renderer;
    s_fsm = font_sm;
    s_fmd = font_md;
    s_flg = font_lg;

    // ── Background ────────────────────────────────────────────────────────────
    s_fill(0, 0, S_TV_W, S_TV_H, SC_BG);

    // ── Header bar ───────────────────────────────────────────────────────────
    s_fill(0, 0, S_TV_W, S_HEADER_H, SC_CHROME);
    s_fill(0, S_HEADER_H - 1, S_TV_W, 1, SC_LINE);

    s_text(s_fmd, "< Back",      20,         S_HEADER_H - 14, SC_BACK_BTN, 0);
    s_text(s_flg, "WiiU Settings", S_TV_W / 2, S_HEADER_H - 10, SC_TITLE,   1);

    // ── Setting rows ─────────────────────────────────────────────────────────
    int iy = S_HEADER_H + 12;
    for (int i = 0; i < s_item_count; i++) {
        bool focused = (i == s_sel);

        // Card
        s_fill(0, iy, S_TV_W, S_ITEM_H, SC_WHITE);
        s_fill(0, iy + S_ITEM_H - 1, S_TV_W, 1, SC_ITEM_BORDER);

        // Blue focus ring around the whole row
        if (focused)
            s_border(4, iy + 3, S_TV_W - 8, S_ITEM_H - 6, SC_FOCUS_RING, 2);

        // Checkbox
        int cb_y = iy + (S_ITEM_H - S_CB_SIZE) / 2;
        s_checkbox(S_CB_MARGIN, cb_y, *s_items[i].value, focused);

        // Label + description
        int tx = S_CB_MARGIN + S_CB_SIZE + 20;
        s_text(s_fmd, s_items[i].label, tx, iy + 28, SC_TITLE, 0);
        s_text(s_fsm, s_items[i].desc1, tx, iy + 52, SC_GRAY,  0);
        s_text(s_fsm, s_items[i].desc2, tx, iy + 68, SC_GRAY,  0);

        iy += S_ITEM_H + 2;
    }

    // ── Hint ─────────────────────────────────────────────────────────────────
    s_text(s_fsm, "A: toggle  |  B / \xe2\x97\x80 Back: return",
           S_TV_W / 2, S_TV_H - 14, SC_GRAY, 1);

    SDL_RenderPresent(renderer);
}

// ─── Public: input ────────────────────────────────────────────────────────────

bool settings_handle_input(VPADStatus* vpad)
{
    uint32_t btn = vpad->trigger;

    // B = exit settings
    if (btn & VPAD_BUTTON_B)
        return false;

    // D-pad / left-stick up-down to navigate items
    float sy = vpad->leftStick.y;

    if ((btn & VPAD_BUTTON_UP) || sy > 0.6f)
        if (s_sel > 0) s_sel--;

    if ((btn & VPAD_BUTTON_DOWN) || sy < -0.6f)
        if (s_sel < s_item_count - 1) s_sel++;

    // A = toggle focused setting
    if (btn & VPAD_BUTTON_A) {
        *s_items[s_sel].value = !(*s_items[s_sel].value);
        settings_save();
    }

    // ── Touch (detect tap-release for reliability) ────────────────────────────
    VPADTouchData tp;
    VPADGetTPCalibratedPointEx(VPAD_CHAN_0, VPAD_TP_854X480, &tp, &vpad->tpNormal);

    // Track last known finger position while touching
    if (tp.touched) {
        s_tp_last_x = (int)((float)tp.x / S_DRC_W * S_TV_W);
        s_tp_last_y = (int)((float)tp.y / S_DRC_H * S_TV_H);
    }

    bool tapped  = s_tp_prev && !tp.touched; // finger just lifted
    s_tp_prev    = (bool)tp.touched;

    if (tapped) {
        int tx = s_tp_last_x, ty = s_tp_last_y;

        // Back button strip
        if (tx < 200 && ty < S_HEADER_H)
            return false;

        // Item rows
        int iy = S_HEADER_H + 12;
        for (int i = 0; i < s_item_count; i++) {
            if (ty >= iy && ty < iy + S_ITEM_H) {
                s_sel = i;
                *s_items[i].value = !(*s_items[i].value);
                settings_save();
                break;
            }
            iy += S_ITEM_H + 2;
        }
    }

    return true; // stay open
}
