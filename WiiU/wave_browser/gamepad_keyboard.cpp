// gamepad_keyboard.cpp — Analog stick → WASD / Arrow keys for browser games
//
// Inject synthetic keyboard events into the webkit engine so browser games
// that require WASD or arrow keys work with the WiiU analog sticks.
//
// HOW IT WORKS
// ─────────────
// Each frame we compare the stick position against a threshold (ACTIVATE)
// and a release deadzone (RELEASE).  When a key transitions pressed→released
// or vice versa, webkit_engine_key_down / webkit_engine_key_up is called.
//
// TOGGLE
// ──────
// Hold ZL + ZR simultaneously for TOGGLE_HOLD_MS to flip game-key mode.
// A brief on-screen HUD confirms the switch.

#include "gamepad_keyboard.h"
#include "webkit_engine.h"
#include "ui_common.h"       // sdl_rect / sdl_text helpers

#include <SDL.h>
#include <SDL_ttf.h>
#include <string.h>
#include <math.h>

// ── Tuning ────────────────────────────────────────────────────────────────────

static constexpr float    ACTIVATE_THRESH = 0.55f;  // stick must pass this to press key
static constexpr float    RELEASE_THRESH  = 0.30f;  // stick must drop below this to release
static constexpr unsigned TOGGLE_HOLD_MS  = 800;    // ms to hold ZL+ZR before toggling

// ── Key codes (standard USB HID / browser KeyboardEvent.keyCode) ──────────────

// WASD
static constexpr int KEY_W = 87;
static constexpr int KEY_A = 65;
static constexpr int KEY_S = 83;
static constexpr int KEY_D = 68;
// Arrows
static constexpr int KEY_UP    = 38;
static constexpr int KEY_DOWN  = 40;
static constexpr int KEY_LEFT  = 37;
static constexpr int KEY_RIGHT = 39;

// ── State ─────────────────────────────────────────────────────────────────────

static bool s_active = false;

// 8 virtual keys: left stick (W A S D), right stick (Up Down Left Right)
enum VKey {
    VK_W=0, VK_A, VK_S, VK_D,
    VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT,
    VK_COUNT
};
static bool     s_pressed[VK_COUNT]  = {};   // current logical state
static int      s_keycode[VK_COUNT]  = { KEY_W, KEY_A, KEY_S, KEY_D,
                                          KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT };

// Toggle tracking
static bool     s_zl_prev  = false;
static bool     s_zr_prev  = false;
static unsigned s_hold_start = 0;
static bool     s_hold_counting = false;
static bool     s_toggle_fired  = false;     // prevent double-fire

// HUD flash
static unsigned s_hud_flash_until = 0;       // show "GAME KEYS ON/OFF" until this tick

// ── Helpers ───────────────────────────────────────────────────────────────────

// Hysteresis check: if currently pressed, release only below RELEASE_THRESH
static bool axis_triggered(float val, bool currently_pressed) {
    if (!currently_pressed)
        return fabsf(val) >= ACTIVATE_THRESH;
    else
        return fabsf(val) >= RELEASE_THRESH;   // stays pressed until fully released
}

static void set_key(VKey vk, bool want_pressed, bool active_mode) {
    if (!active_mode) {
        // Make sure everything is released when mode turns off
        if (s_pressed[vk]) {
            webkit_engine_key_up(s_keycode[vk]);
            s_pressed[vk] = false;
        }
        return;
    }
    if (want_pressed == s_pressed[vk]) return;   // no change
    s_pressed[vk] = want_pressed;
    if (want_pressed)
        webkit_engine_key_down(s_keycode[vk]);
    else
        webkit_engine_key_up(s_keycode[vk]);
}

// ── Public API ────────────────────────────────────────────────────────────────

void gamepad_keyboard_init(void) {
    memset(s_pressed, 0, sizeof(s_pressed));
    s_active = false;
}

bool gamepad_keyboard_update(float lx, float ly,
                              float rx, float ry,
                              bool zl_held, bool zr_held,
                              unsigned int tick_ms)
{
    bool changed = false;

    // ── Toggle detection ─────────────────────────────────────────────────────
    bool both = zl_held && zr_held;

    if (both && !s_hold_counting) {
        s_hold_counting = true;
        s_hold_start    = tick_ms;
        s_toggle_fired  = false;
    }
    if (!both) {
        s_hold_counting = false;
        s_toggle_fired  = false;
    }
    if (s_hold_counting && !s_toggle_fired &&
        (tick_ms - s_hold_start) >= TOGGLE_HOLD_MS)
    {
        s_active         = !s_active;
        s_toggle_fired   = true;
        s_hud_flash_until = tick_ms + 2000;   // show HUD for 2 seconds
        changed = true;

        // Release all virtual keys when toggling off
        if (!s_active) {
            for (int i = 0; i < VK_COUNT; i++)
                set_key((VKey)i, false, false);
        }
    }

    s_zl_prev = zl_held;
    s_zr_prev = zr_held;

    if (!s_active) return changed;

    // ── Left stick → WASD ────────────────────────────────────────────────────
    //  ly > 0 = stick up = forward = W
    //  ly < 0 = stick down = backward = S
    //  lx < 0 = stick left = A
    //  lx > 0 = stick right = D

    bool w_want = ly >  RELEASE_THRESH  && axis_triggered( ly, s_pressed[VK_W]);
    bool s_want = ly < -RELEASE_THRESH  && axis_triggered(-ly, s_pressed[VK_S]);
    bool a_want = lx < -RELEASE_THRESH  && axis_triggered(-lx, s_pressed[VK_A]);
    bool d_want = lx >  RELEASE_THRESH  && axis_triggered( lx, s_pressed[VK_D]);

    // Only one of W/S at a time, one of A/D at a time
    if (w_want && s_want) { w_want = fabsf(ly) >= fabsf(lx); s_want = !w_want; }
    if (a_want && d_want) { a_want = fabsf(lx) >= fabsf(ly); d_want = !a_want; }

    bool prev_w = s_pressed[VK_W];
    bool prev_s = s_pressed[VK_S];
    bool prev_a = s_pressed[VK_A];
    bool prev_d = s_pressed[VK_D];

    set_key(VK_W, w_want, true);
    set_key(VK_S, s_want, true);
    set_key(VK_A, a_want, true);
    set_key(VK_D, d_want, true);

    if (s_pressed[VK_W]!=prev_w || s_pressed[VK_S]!=prev_s ||
        s_pressed[VK_A]!=prev_a || s_pressed[VK_D]!=prev_d) changed=true;

    // ── Right stick → Arrow keys ──────────────────────────────────────────────

    bool up_want    = ry >  RELEASE_THRESH && axis_triggered( ry, s_pressed[VK_UP]);
    bool down_want  = ry < -RELEASE_THRESH && axis_triggered(-ry, s_pressed[VK_DOWN]);
    bool left_want  = rx < -RELEASE_THRESH && axis_triggered(-rx, s_pressed[VK_LEFT]);
    bool right_want = rx >  RELEASE_THRESH && axis_triggered( rx, s_pressed[VK_RIGHT]);

    bool prev_up    = s_pressed[VK_UP];
    bool prev_down  = s_pressed[VK_DOWN];
    bool prev_left  = s_pressed[VK_LEFT];
    bool prev_right = s_pressed[VK_RIGHT];

    set_key(VK_UP,    up_want,    true);
    set_key(VK_DOWN,  down_want,  true);
    set_key(VK_LEFT,  left_want,  true);
    set_key(VK_RIGHT, right_want, true);

    if (s_pressed[VK_UP]!=prev_up || s_pressed[VK_DOWN]!=prev_down ||
        s_pressed[VK_LEFT]!=prev_left || s_pressed[VK_RIGHT]!=prev_right) changed=true;

    return changed;
}

bool gamepad_keyboard_active(void) { return s_active; }

void gamepad_keyboard_set_active(bool on) {
    if (on == s_active) return;
    s_active = on;
    if (!s_active)
        for (int i = 0; i < VK_COUNT; i++)
            set_key((VKey)i, false, false);
}

// ── HUD ───────────────────────────────────────────────────────────────────────

void gamepad_keyboard_draw_hud(void* renderer, void* font,
                                int screen_w, int content_y)
{
    SDL_Renderer* ren = (SDL_Renderer*)renderer;
    TTF_Font*     fnt = (TTF_Font*)font;
    if (!ren || !fnt) return;

    // Always draw a small dim indicator when active; flash brightly on toggle
    bool flashing = s_hud_flash_until > 0 &&
                    SDL_GetTicks() < s_hud_flash_until;

    if (!s_active && !flashing) return;

    const char* label = s_active ? "GAME KEYS  ON" : "GAME KEYS  OFF";

    // Pill dimensions
    int pw = 148, ph = 28;
    int px = screen_w - pw - 12;
    int py = content_y + 10;

    // Background pill
    Uint8 alpha = flashing ? 220 : 130;
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    if (s_active) {
        SDL_SetRenderDrawColor(ren, 0x00, 0xC8, 0x00, alpha);  // green
    } else {
        SDL_SetRenderDrawColor(ren, 0xCC, 0x00, 0x00, alpha);  // red
    }
    SDL_Rect pill = {px, py, pw, ph};
    SDL_RenderFillRect(ren, &pill);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

    // Label text centred in pill
    SDL_Color col = {0xFF, 0xFF, 0xFF, 0xFF};
    SDL_Surface* surf = TTF_RenderUTF8_Blended(fnt, label, col);
    if (surf) {
        SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, surf);
        if (tex) {
            SDL_Rect dst = {
                px + (pw - surf->w) / 2,
                py + (ph - surf->h) / 2,
                surf->w, surf->h
            };
            SDL_RenderCopy(ren, tex, nullptr, &dst);
            SDL_DestroyTexture(tex);
        }
        SDL_FreeSurface(surf);
    }
}
