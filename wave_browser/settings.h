#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include <vpad/input.h>

// ─── Persistence path ────────────────────────────────────────────────────────

#define SETTINGS_PATH \
    "fs:/vol/external01/wiiu/apps/WaveBrowser/wave-browser-settings.cfg"

// ─── Settings data ───────────────────────────────────────────────────────────

struct WaveSettings {
    bool improved_multitasking = false;
    bool improved_remote       = false;
    int  tv_brand_index        = 0;     // index into TV_BRANDS[]
};

extern WaveSettings g_settings;

void settings_load();
void settings_save();

// ─── Settings UI ─────────────────────────────────────────────────────────────
// Fully self-contained: call from the main loop, pass renderer + fonts.

void settings_draw(SDL_Renderer* renderer,
                   TTF_Font* font_sm,
                   TTF_Font* font_md,
                   TTF_Font* font_lg);

// Returns true while settings should stay open, false when the user exits.
bool settings_handle_input(VPADStatus* vpad);
