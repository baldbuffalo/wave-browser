#pragma once
// ─── ui_common.h ─────────────────────────────────────────────────────────────
// Generic SDL2 rendering helpers shared by all Wave Browser modules.
// No TV-remote or settings logic lives here.
// ─────────────────────────────────────────────────────────────────────────────
#include <SDL.h>
#include <SDL_ttf.h>

// ─── Screen dimensions ───────────────────────────────────────────────────────
#define TV_W   1280
#define TV_H    720
#define DRC_W   854
#define DRC_H   480

// ─── Shared colour palette ───────────────────────────────────────────────────
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
static constexpr SDL_Color COL_DARK_OVERLAY = {0x10,0x10,0x20,0xC8};

// ─── Primitives ──────────────────────────────────────────────────────────────
static inline void ui_rect(SDL_Renderer* ren, int x, int y, int w, int h, SDL_Color c)
{
    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderFillRect(ren, &r);
}

static inline void ui_outline(SDL_Renderer* ren, int x, int y, int w, int h,
                               SDL_Color c, int thickness = 1)
{
    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
    for (int d = 0; d < thickness; d++) {
        SDL_Rect r = {x + d, y + d, w - d * 2, h - d * 2};
        SDL_RenderDrawRect(ren, &r);
    }
}

// align: 0=left  1=center  2=right.  y is the baseline.
static inline void ui_text(SDL_Renderer* ren, TTF_Font* font,
                            const char* text, int x, int y, SDL_Color c, int align = 0)
{
    if (!ren || !font || !text || !text[0]) return;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text, c);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, surf);
    int tw = surf->w, th = surf->h;
    SDL_FreeSurface(surf);
    if (!tex) return;
    if      (align == 1) x -= tw / 2;
    else if (align == 2) x -= tw;
    SDL_Rect dst = {x, y - th, tw, th};
    SDL_RenderCopy(ren, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
}

static inline void ui_gradient(SDL_Renderer* ren, SDL_Color top, SDL_Color bot)
{
    for (int y = 0; y < TV_H; y++) {
        uint8_t r = (uint8_t)(top.r + (bot.r - top.r) * y / TV_H);
        uint8_t g = (uint8_t)(top.g + (bot.g - top.g) * y / TV_H);
        uint8_t b = (uint8_t)(top.b + (bot.b - top.b) * y / TV_H);
        SDL_SetRenderDrawColor(ren, r, g, b, 0xFF);
        SDL_RenderDrawLine(ren, 0, y, TV_W, y);
    }
}

static inline void ui_progress_bar(SDL_Renderer* ren,
                                    int x, int y, int w, int h, double pct)
{
    ui_rect(ren, x, y, w, h, COL_GREEN_DRK);
    int fill = (int)(w * pct / 100.0);
    if (fill > 0) ui_rect(ren, x, y, fill, h, COL_GREEN);
    ui_outline(ren, x, y, w, h, COL_WHITE);
}

static inline void ui_focus_ring(SDL_Renderer* ren, int x, int y, int w, int h)
{
    ui_outline(ren, x - 2, y - 2, w + 4, h + 4, COL_FOCUS_RING, 2);
}

static inline void ui_dim_overlay(SDL_Renderer* ren)
{
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, COL_DARK_OVERLAY.r, COL_DARK_OVERLAY.g,
                           COL_DARK_OVERLAY.b, COL_DARK_OVERLAY.a);
    SDL_Rect full = {0, 0, TV_W, TV_H};
    SDL_RenderFillRect(ren, &full);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
}

// Rounded-rect approximation using corner fills
static inline void ui_rect_rounded(SDL_Renderer* ren, int x, int y, int w, int h,
                                    int r, SDL_Color c)
{
    if (r < 1) { ui_rect(ren, x, y, w, h, c); return; }
    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
    // Centre cross
    SDL_Rect horiz = {x, y + r, w, h - r * 2};
    SDL_RenderFillRect(ren, &horiz);
    SDL_Rect vert  = {x + r, y, w - r * 2, h};
    SDL_RenderFillRect(ren, &vert);
    // Corner circles (approx)
    for (int dy = 0; dy < r; dy++) {
        int dx = (int)SDL_sqrtf((float)(r * r - (r - dy) * (r - dy)));
        SDL_RenderDrawLine(ren, x + r - dx,     y + dy,         x + w - r + dx - 1, y + dy);
        SDL_RenderDrawLine(ren, x + r - dx,     y + h - dy - 1, x + w - r + dx - 1, y + h - dy - 1);
    }
}
