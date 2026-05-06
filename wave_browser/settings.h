#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include <vpad/input.h>

// ─── Settings data ────────────────────────────────────────────────────────────

struct WaveSettings {
    bool improved_multitasking = false;
};

extern WaveSettings g_settings;

void settings_load();
void settings_save();

// ─── Settings UI ─────────────────────────────────────────────────────────────
// Fully self-contained: pass SDL renderer + fonts from main.

void settings_draw(SDL_Renderer* renderer,
                   TTF_Font* font_sm,
                   TTF_Font* font_md,
                   TTF_Font* font_lg);

// Returns true while settings should stay open, false when user exits.
bool settings_handle_input(VPADStatus* vpad);
