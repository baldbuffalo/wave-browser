#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include <vpad/input.h>

// ─── TV Button constant ───────────────────────────────────────────────────────
// The WiiU GamePad's TV-control button.  If wut doesn't define it yet, we
// fall back to the raw bit value found in hardware documentation.
#ifndef VPAD_BUTTON_TV
#   define VPAD_BUTTON_TV 0x00040000u
#endif

// ─── Brand list ──────────────────────────────────────────────────────────────

extern const char* const TV_BRAND_NAMES[];
extern const int         TV_BRAND_COUNT;

// ─── Lifecycle ───────────────────────────────────────────────────────────────

void tv_remote_set_brand(int brand_index);
int  tv_remote_get_brand();

bool tv_remote_is_open();
void tv_remote_open();
void tv_remote_close();

// Call once per frame when the remote is open.
// Returns false when the user closes the remote (B button pressed).
bool tv_remote_handle_input(VPADStatus* vpad, bool tp_tapped, int tp_x, int tp_y);
void tv_remote_draw(SDL_Renderer* ren, TTF_Font* fsm, TTF_Font* fmd, TTF_Font* flg);
