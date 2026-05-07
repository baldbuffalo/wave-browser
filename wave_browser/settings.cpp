#include "settings.h"
#include "ui_common.h"
#include "tv_remote.h"
#include "tv_remotes/brand_registry.h"

#include <vpad/input.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ─── Global settings instance ────────────────────────────────────────────────

WaveSettings g_settings;

// ─── Persistence ─────────────────────────────────────────────────────────────

void settings_load()
{
    FILE* f = fopen(SETTINGS_PATH, "r");
    if (!f) return;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        // Strip trailing newline
        size_t ln = strlen(line);
        if (ln && line[ln - 1] == '\n') line[--ln] = '\0';

        int n = 0;
        if      (sscanf(line, "improved_multitasking=%d", &n) == 1)
            g_settings.improved_multitasking = (n != 0);
        else if (sscanf(line, "improved_remote=%d", &n) == 1)
            g_settings.improved_remote = (n != 0);
        else if (sscanf(line, "tv_brand_index=%d", &n) == 1)
            g_settings.tv_brand_index = (n >= 0 && n < TV_BRAND_COUNT) ? n : 0;
    }
    fclose(f);

    // Keep tv_remote in sync with what was loaded
    tv_remote_set_brand(g_settings.tv_brand_index);
}

void settings_save()
{
    FILE* f = fopen(SETTINGS_PATH, "w");
    if (!f) return;
    fprintf(f, "improved_multitasking=%d\n", g_settings.improved_multitasking ? 1 : 0);
    fprintf(f, "improved_remote=%d\n",       g_settings.improved_remote       ? 1 : 0);
    fprintf(f, "tv_brand_index=%d\n",        g_settings.tv_brand_index);
    fclose(f);
}

// ─── Settings UI state ───────────────────────────────────────────────────────

static int  s_sel  = 0;       // currently focused row (0..2)
static bool s_open = false;   // used internally for enter/exit tracking

// Row indices
#define ROW_MULTITASK  0
#define ROW_REMOTE     1
#define ROW_BRAND      2
#define ROW_COUNT      3

// Panel geometry
#define PANEL_X   280
#define PANEL_Y    80
#define PANEL_W   720
#define PANEL_H   560

#define HEADER_H   56
#define ROW_H      72
#define ROW_START  (PANEL_Y + HEADER_H + 24)
#define COL_LABEL  (PANEL_X + 32)
#define COL_VALUE  (PANEL_X + PANEL_W - 32)

// ─── Draw ────────────────────────────────────────────────────────────────────

void settings_draw(SDL_Renderer* ren,
                   TTF_Font* font_sm,
                   TTF_Font* font_md,
                   TTF_Font* font_lg)
{
    // Dim background
    ui_dim_overlay(ren);

    // Panel shadow
    ui_rect(ren, PANEL_X + 6, PANEL_Y + 6, PANEL_W, PANEL_H,
            {0x00,0x00,0x00,0x80});

    // Panel body
    ui_rect(ren, PANEL_X, PANEL_Y, PANEL_W, PANEL_H, {0xF8,0xF8,0xF8,0xFF});
    ui_outline(ren, PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COL_BLUE, 2);

    // Header bar
    ui_rect(ren, PANEL_X, PANEL_Y, PANEL_W, HEADER_H, COL_BG_TOP);
    ui_text(ren, font_lg, "Settings",
            PANEL_X + PANEL_W / 2, PANEL_Y + HEADER_H - 10, COL_WHITE, 1);

    // ── Draw each row ─────────────────────────────────────────────────────────
    const char* labels[ROW_COUNT] = {
        "Improved Multitasking",
        "Improved Remote Control",
        "TV Remote Brand",
    };

    for (int i = 0; i < ROW_COUNT; i++) {
        int ry = ROW_START + i * ROW_H;
        bool focused = (i == s_sel);

        // Row highlight
        SDL_Color row_bg = focused
            ? SDL_Color{0xE8,0xF0,0xFE,0xFF}
            : SDL_Color{0xF8,0xF8,0xF8,0xFF};
        ui_rect(ren, PANEL_X + 4, ry - 4, PANEL_W - 8, ROW_H - 8, row_bg);

        if (focused)
            ui_outline(ren, PANEL_X + 4, ry - 4, PANEL_W - 8, ROW_H - 8,
                       COL_FOCUS_RING, 2);

        // Label
        SDL_Color label_col = focused ? COL_BLUE : SDL_Color{0x1A,0x1A,0x1A,0xFF};
        ui_text(ren, font_md, labels[i],
                COL_LABEL, ry + ROW_H / 2 + 6, label_col, 0);

        // Value
        char val_str[64] = {};
        if (i == ROW_MULTITASK) {
            snprintf(val_str, sizeof(val_str), "%s",
                     g_settings.improved_multitasking ? "ON" : "OFF");
        } else if (i == ROW_REMOTE) {
            snprintf(val_str, sizeof(val_str), "%s",
                     g_settings.improved_remote ? "ON" : "OFF");
        } else {
            // Brand row: show arrows when focused
            const char* brand = (g_settings.tv_brand_index >= 0 &&
                                  g_settings.tv_brand_index < TV_BRAND_COUNT)
                ? TV_BRANDS[g_settings.tv_brand_index]->name
                : "Unknown";
            if (focused)
                snprintf(val_str, sizeof(val_str), "< %s >", brand);
            else
                snprintf(val_str, sizeof(val_str), "%s", brand);
        }

        SDL_Color val_col = (i == ROW_MULTITASK || i == ROW_REMOTE)
            ? (((i == ROW_MULTITASK && g_settings.improved_multitasking) ||
                (i == ROW_REMOTE    && g_settings.improved_remote))
               ? SDL_Color{0x00,0x99,0x00,0xFF}
               : SDL_Color{0x99,0x00,0x00,0xFF})
            : label_col;

        ui_text(ren, font_md, val_str, COL_VALUE,
                ry + ROW_H / 2 + 6, val_col, 2);

        // Separator
        if (i < ROW_COUNT - 1)
            ui_rect(ren, PANEL_X + 16, ry + ROW_H - 10,
                    PANEL_W - 32, 1, COL_TOOLBAR_LINE);
    }

    // Hint bar
    ui_text(ren, font_sm,
            "\xe2\x96\xb2\xe2\x96\xbc: select  A / \xe2\x97\x84\xe2\x96\xba: toggle  B: save & close",
            PANEL_X + PANEL_W / 2,
            PANEL_Y + PANEL_H - 10,
            COL_GRAY, 1);

    SDL_RenderPresent(ren);
}

// ─── Input ───────────────────────────────────────────────────────────────────

bool settings_handle_input(VPADStatus* vpad)
{
    uint32_t btn = vpad->trigger;

    // Navigate rows
    if (btn & VPAD_BUTTON_UP) {
        s_sel = (s_sel > 0) ? s_sel - 1 : ROW_COUNT - 1;
    }
    if (btn & VPAD_BUTTON_DOWN) {
        s_sel = (s_sel < ROW_COUNT - 1) ? s_sel + 1 : 0;
    }

    // Toggle / change value
    auto toggle = [&]() {
        if (s_sel == ROW_MULTITASK) {
            g_settings.improved_multitasking = !g_settings.improved_multitasking;
        } else if (s_sel == ROW_REMOTE) {
            g_settings.improved_remote = !g_settings.improved_remote;
        }
    };

    if (btn & VPAD_BUTTON_A) toggle();

    if (s_sel == ROW_BRAND) {
        if ((btn & VPAD_BUTTON_RIGHT) || (btn & VPAD_BUTTON_A)) {
            g_settings.tv_brand_index =
                (g_settings.tv_brand_index + 1) % TV_BRAND_COUNT;
            tv_remote_set_brand(g_settings.tv_brand_index);
        }
        if (btn & VPAD_BUTTON_LEFT) {
            g_settings.tv_brand_index =
                (g_settings.tv_brand_index + TV_BRAND_COUNT - 1) % TV_BRAND_COUNT;
            tv_remote_set_brand(g_settings.tv_brand_index);
        }
    } else {
        if (btn & VPAD_BUTTON_LEFT)  toggle();
        if (btn & VPAD_BUTTON_RIGHT) toggle();
    }

    // B = save and close
    if (btn & VPAD_BUTTON_B) {
        settings_save();
        s_sel = 0;
        return false;
    }

    return true;   // stay open
}
