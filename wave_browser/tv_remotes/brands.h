#pragma once

#include <SDL.h>

// ─── Per-brand colour theme ───────────────────────────────────────────────────

struct BrandTheme {
    SDL_Color body;      // remote body fill
    SDL_Color primary;   // brand colour (header bar, power button)
    SDL_Color accent;    // OK / nav ring highlight colour
    SDL_Color btn;       // regular button face
    SDL_Color btn_text;  // regular button label colour
};

// ─── Brand entry (name + theme) ──────────────────────────────────────────────

struct BrandEntry {
    const char* name;
    BrandTheme  theme;
};
