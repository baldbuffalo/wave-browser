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

    // TV remote model selection
    // Stored as "brand/year/model" string, e.g. "Samsung/2014/UE40H6400"
    // Empty = no model selected / use generic remote
    char tv_model_key[128] = {};

    // Auto-detect: 0=not started, 1=in progress, 2=done-found, 3=done-not found
    int  detect_state = 0;
};

extern WaveSettings g_settings;

void settings_load();
void settings_open();   // call whenever settings screen is entered
void settings_save();

// Build the key string from brand/year/model fields
void settings_make_key(const char* brand, int year, const char* model,
                        char* out, int out_len);

// ─── Settings UI ─────────────────────────────────────────────────────────────

void settings_draw(SDL_Renderer* renderer,
                   TTF_Font* font_sm,
                   TTF_Font* font_md,
                   TTF_Font* font_lg);

// Returns true while settings should stay open.
// tp_pressed/tp_x/tp_y: touch-down event from this frame (main.cpp s_tp_* vars).
bool settings_handle_input(VPADStatus* vpad,
                            bool tp_pressed = false,
                            int  tp_x = 0,
                            int  tp_y = 0);
