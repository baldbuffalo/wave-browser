#pragma once
// gamepad_keyboard.h — Analog stick → WASD/Arrow keys for browser games
//
// TOGGLE:  Hold ZL + ZR together for 1 second
//
// When ACTIVE:
//   Left stick  UP    → W          Left stick  DOWN  → S
//   Left stick  LEFT  → A          Left stick  RIGHT → D
//   Right stick UP    → Up arrow   Right stick DOWN  → Down arrow
//   Right stick LEFT  → Left arrow Right stick RIGHT → Right arrow
//
// D-pad, face buttons, and all browser navigation continue working normally.

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void gamepad_keyboard_init(void);

// Feed raw stick values each frame. zl_held / zr_held for toggle detection.
// Returns true if a key event was generated this frame.
bool gamepad_keyboard_update(float lx, float ly,
                             float rx, float ry,
                             bool zl_held, bool zr_held,
                             unsigned int tick_ms);

bool gamepad_keyboard_active(void);
void gamepad_keyboard_set_active(bool on);

// Draw a small "GAME KEYS" pill indicator when active.
// renderer = SDL_Renderer*, font = TTF_Font*
void gamepad_keyboard_draw_hud(void* renderer, void* font, int screen_w, int content_y);

#ifdef __cplusplus
}
#endif
