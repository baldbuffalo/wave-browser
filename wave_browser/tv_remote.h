#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include <vpad/input.h>

// ─── TV Button constant ───────────────────────────────────────────────────────
// The WiiU GamePad's TV-control button.  Fallback to raw bit if wut omits it.
#ifndef VPAD_BUTTON_TV
#   define VPAD_BUTTON_TV 0x00040000u
#endif

// ─── Lifecycle ───────────────────────────────────────────────────────────────

// brand_index is an index into TV_BRANDS[] (see tv_remotes/brand_registry.h).
void tv_remote_set_brand(int brand_index);
int  tv_remote_get_brand();

bool tv_remote_is_open();
void tv_remote_open();
void tv_remote_close();

// Call once per frame when the remote overlay is visible.
// Returns false when the user dismisses the remote (B button or tap outside).
bool tv_remote_handle_input(VPADStatus* vpad,
                             bool tp_tapped, int tp_x, int tp_y);
void tv_remote_draw(SDL_Renderer* ren,
                    TTF_Font* font_sm, TTF_Font* font_md, TTF_Font* font_lg);
