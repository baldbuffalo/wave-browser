#include "tv_remote.h"
#include "ui_common.h"
#include "tv_remotes/brand_registry.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ═══════════════════════════════════════════════════════════════════════════
//  Button definition
// ═══════════════════════════════════════════════════════════════════════════

enum RBtnID {
    RB_POWER=0, RB_MUTE,   RB_INPUT,
    RB_1,       RB_2,      RB_3,    RB_VOL_UP, RB_CH_UP,
    RB_4,       RB_5,      RB_6,    RB_VOL_DN, RB_CH_DN,
    RB_7,       RB_8,      RB_9,
    RB_STAR,    RB_0,      RB_HASH,
    RB_UP,      RB_LEFT,   RB_OK,   RB_RIGHT,  RB_DOWN,
    RB_BACK,    RB_HOME,   RB_MENU,
    RB_RED,     RB_GREEN,  RB_YELLOW, RB_BLUE,
    RB_INFO,    RB_GUIDE,
    RB_COUNT
};

struct BtnDef {
    int rx, ry, rw, rh;
    const char* label;
    int up, down, left, right;
    bool is_nav;
    bool is_color;
    SDL_Color color_face;
};

// Remote panel placed at (430, 30), 420 × 660
#define RP_X  430
#define RP_Y   30
#define RP_W  420
#define RP_H  660

#define BTN_SM   44
#define BTN_TALL 36

#define COL_A   24
#define COL_B   80
#define COL_C  136
#define COL_D  230
#define COL_E  290
#define COL_F  350

#define ROW_0   20
#define ROW_1   86
#define ROW_2  140
#define ROW_3  194
#define ROW_4  248
#define ROW_5  322
#define ROW_6  372
#define ROW_7  422
#define ROW_8  494
#define ROW_9  546

static const BtnDef BTNS[RB_COUNT] = {
    { COL_A, ROW_0, 70, BTN_TALL, "POWER", -1,RB_1,       -1,       RB_MUTE,   false,false,{} },
    { 154,   ROW_0, 70, BTN_TALL, "MUTE",  -1,RB_VOL_UP,  RB_POWER, RB_INPUT,  false,false,{} },
    { 310,   ROW_0, 86, BTN_TALL, "INPUT", -1,RB_CH_UP,   RB_MUTE,  -1,        false,false,{} },

    { COL_A,ROW_1,BTN_SM,BTN_SM,"1", RB_POWER,RB_4,  -1,   RB_2,      false,false,{} },
    { COL_B,ROW_1,BTN_SM,BTN_SM,"2", RB_MUTE, RB_5,  RB_1, RB_3,      false,false,{} },
    { COL_C,ROW_1,BTN_SM,BTN_SM,"3", RB_MUTE, RB_6,  RB_2, RB_VOL_UP, false,false,{} },
    { COL_D,ROW_1,BTN_SM,BTN_SM,"V+",RB_INPUT,RB_VOL_DN,RB_3,RB_CH_UP,false,false,{} },
    { COL_E,ROW_1,BTN_SM,BTN_SM,"C+",RB_INPUT,RB_CH_DN,RB_VOL_UP,-1,  false,false,{} },

    { COL_A,ROW_2,BTN_SM,BTN_SM,"4", RB_1,RB_7,  -1,   RB_5,      false,false,{} },
    { COL_B,ROW_2,BTN_SM,BTN_SM,"5", RB_2,RB_8,  RB_4, RB_6,      false,false,{} },
    { COL_C,ROW_2,BTN_SM,BTN_SM,"6", RB_3,RB_9,  RB_5, RB_VOL_DN, false,false,{} },
    { COL_D,ROW_2,BTN_SM,BTN_SM,"V-",RB_VOL_UP,RB_UP,RB_6, RB_CH_DN, false,false,{} },
    { COL_E,ROW_2,BTN_SM,BTN_SM,"C-",RB_CH_UP,RB_UP,RB_VOL_DN,-1,    false,false,{} },

    { COL_A,ROW_3,BTN_SM,BTN_SM,"7", RB_4,RB_STAR,-1,   RB_8, false,false,{} },
    { COL_B,ROW_3,BTN_SM,BTN_SM,"8", RB_5,RB_0,   RB_7, RB_9, false,false,{} },
    { COL_C,ROW_3,BTN_SM,BTN_SM,"9", RB_6,RB_HASH,RB_8, -1,   false,false,{} },

    { COL_A,ROW_4,BTN_SM,BTN_SM,"*", RB_7,RB_UP,  -1,    RB_0,   false,false,{} },
    { COL_B,ROW_4,BTN_SM,BTN_SM,"0", RB_8,RB_UP,  RB_STAR,RB_HASH,false,false,{} },
    { COL_C,ROW_4,BTN_SM,BTN_SM,"#", RB_9,RB_UP,  RB_0,  RB_BACK,false,false,{} },

    { 170,ROW_5,BTN_SM,BTN_SM,"\xe2\x96\xb2", RB_STAR,RB_OK,   RB_VOL_DN,RB_BACK,  true,false,{} },
    { 116,ROW_6,BTN_SM,BTN_SM,"\xe2\x97\x84", RB_UP,  RB_DOWN, -1,        RB_OK,    true,false,{} },
    { 170,ROW_6,BTN_SM,BTN_SM,"OK",            RB_UP,  RB_DOWN, RB_LEFT,   RB_RIGHT, true,false,{} },
    { 224,ROW_6,BTN_SM,BTN_SM,"\xe2\x96\xba", RB_UP,  RB_DOWN, RB_OK,     RB_BACK,  true,false,{} },
    { 170,ROW_7,BTN_SM,BTN_SM,"\xe2\x96\xbc", RB_OK,  RB_RED,  RB_VOL_DN, RB_MENU,  true,false,{} },

    { COL_F,ROW_5,60,BTN_TALL,"BACK", RB_HASH, RB_HOME,RB_UP,    -1, false,false,{} },
    { COL_F,ROW_6,60,BTN_TALL,"HOME", RB_BACK, RB_MENU,RB_RIGHT, -1, false,false,{} },
    { COL_F,ROW_7,60,BTN_TALL,"MENU", RB_HOME, RB_BLUE,RB_DOWN,  -1, false,false,{} },

    { 24, ROW_8,82,BTN_TALL,"RED",    RB_DOWN,RB_INFO, -1,       RB_GREEN, false,true,{0xE3,0x18,0x37,0xFF} },
    { 112,ROW_8,82,BTN_TALL,"GREEN",  RB_DOWN,RB_INFO, RB_RED,   RB_YELLOW,false,true,{0x00,0xB0,0x40,0xFF} },
    { 200,ROW_8,82,BTN_TALL,"YELLOW", RB_MENU,RB_GUIDE,RB_GREEN, RB_BLUE,  false,true,{0xE0,0xB0,0x00,0xFF} },
    { 288,ROW_8,82,BTN_TALL,"BLUE",   RB_MENU,RB_GUIDE,RB_YELLOW,-1,       false,true,{0x14,0x28,0xA0,0xFF} },

    { 24, ROW_9,170,BTN_TALL,"INFO",  RB_RED,   -1, -1,       RB_GUIDE, false,false,{} },
    { 206,ROW_9,170,BTN_TALL,"GUIDE", RB_YELLOW,-1, RB_INFO,  -1,       false,false,{} },
};

// ═══════════════════════════════════════════════════════════════════════════
//  Module state
// ═══════════════════════════════════════════════════════════════════════════

static bool s_open      = false;
static int  s_brand_idx = 0;
static int  s_focus_btn = RB_OK;

static SDL_Renderer* s_ren = nullptr;
static TTF_Font*     s_fsm = nullptr;
static TTF_Font*     s_fmd = nullptr;
static TTF_Font*     s_flg = nullptr;

// ═══════════════════════════════════════════════════════════════════════════
//  Public API
// ═══════════════════════════════════════════════════════════════════════════

void tv_remote_set_brand(int idx)
{
    s_brand_idx = (idx >= 0 && idx < TV_BRAND_COUNT) ? idx : 0;
}

int  tv_remote_get_brand() { return s_brand_idx; }
bool tv_remote_is_open()   { return s_open;      }
void tv_remote_open()      { s_open = true; s_focus_btn = RB_OK; }
void tv_remote_close()     { s_open = false; }

// ═══════════════════════════════════════════════════════════════════════════
//  Stub: send IR / HDMI-CEC command
// ═══════════════════════════════════════════════════════════════════════════

static void send_tv_command(int /*btn_id*/)
{
    // TODO: implement platform IR / CEC dispatch
}

// ═══════════════════════════════════════════════════════════════════════════
//  Draw
// ═══════════════════════════════════════════════════════════════════════════

void tv_remote_draw(SDL_Renderer* ren,
                    TTF_Font* fsm, TTF_Font* fmd, TTF_Font* /*flg*/)
{
    s_ren = ren; s_fsm = fsm; s_fmd = fmd;

    const BrandTheme& th = TV_BRANDS[s_brand_idx]->theme;
    SDL_Color white  = {0xFF,0xFF,0xFF,0xFF};
    SDL_Color gray   = {0x80,0x80,0x80,0xFF};
    SDL_Color shadow = {0x00,0x00,0x00,0x80};

    // Dim backdrop
    ui_dim_overlay(ren);

    // Remote body shadow + body
    ui_rect(ren, RP_X + 6, RP_Y + 6, RP_W, RP_H, shadow);
    ui_rect(ren, RP_X, RP_Y, RP_W, RP_H, th.body);
    ui_outline(ren, RP_X, RP_Y, RP_W, RP_H, th.primary, 3);

    // Header bar
    ui_rect(ren, RP_X, RP_Y, RP_W, 44, th.primary);
    ui_text(ren, fmd, TV_BRANDS[s_brand_idx]->name,
            RP_X + RP_W / 2, RP_Y + 30, white, 1);
    ui_text(ren, fsm, "B: close", RP_X + RP_W - 8, RP_Y + 30, white, 2);

    // Buttons
    for (int i = 0; i < RB_COUNT; i++) {
        const BtnDef& b = BTNS[i];
        int bx = RP_X + b.rx;
        int by = RP_Y + 44 + b.ry;   // 44 = header height
        bool focused = (i == s_focus_btn);

        SDL_Color face;
        if      (i == RB_POWER) face = th.primary;
        else if (b.is_nav)      face = th.accent;
        else if (b.is_color)    face = b.color_face;
        else                    face = th.btn;

        ui_rect(ren, bx, by, b.rw, b.rh, face);

        if (focused)
            ui_outline(ren, bx - 2, by - 2, b.rw + 4, b.rh + 4, th.accent, 2);
        else
            ui_outline(ren, bx, by, b.rw, b.rh, gray);

        SDL_Color lbl = (b.is_color || i == RB_POWER) ? white : th.btn_text;
        ui_text(ren, fmd, b.label, bx + b.rw / 2, by + b.rh / 2 + 8, lbl, 1);
    }

    // Hint
    ui_text(ren, fsm,
            "\xe2\x96\xb2\xe2\x96\xbc\xe2\x97\x84\xe2\x96\xba: navigate  A: press  B: close",
            RP_X + RP_W / 2, RP_Y + RP_H - 6, gray, 1);

    SDL_RenderPresent(ren);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Input
// ═══════════════════════════════════════════════════════════════════════════

bool tv_remote_handle_input(VPADStatus* vpad,
                             bool tp_tapped, int tp_x, int tp_y)
{
    if (!s_open) return false;

    uint32_t btn = vpad->trigger;

    if (btn & VPAD_BUTTON_B) { s_open = false; return false; }

    if (btn & VPAD_BUTTON_UP)    { int n = BTNS[s_focus_btn].up;    if (n >= 0) s_focus_btn = n; }
    if (btn & VPAD_BUTTON_DOWN)  { int n = BTNS[s_focus_btn].down;  if (n >= 0) s_focus_btn = n; }
    if (btn & VPAD_BUTTON_LEFT)  { int n = BTNS[s_focus_btn].left;  if (n >= 0) s_focus_btn = n; }
    if (btn & VPAD_BUTTON_RIGHT) { int n = BTNS[s_focus_btn].right; if (n >= 0) s_focus_btn = n; }

    if (btn & VPAD_BUTTON_A) send_tv_command(s_focus_btn);

    if (tp_tapped) {
        bool hit_panel = (tp_x >= RP_X && tp_x < RP_X + RP_W &&
                          tp_y >= RP_Y && tp_y < RP_Y + RP_H);
        if (!hit_panel) { s_open = false; return false; }

        for (int i = 0; i < RB_COUNT; i++) {
            const BtnDef& b = BTNS[i];
            int bx = RP_X + b.rx, by = RP_Y + 44 + b.ry;
            if (tp_x >= bx && tp_x < bx + b.rw &&
                tp_y >= by && tp_y < by + b.rh) {
                s_focus_btn = i;
                send_tv_command(i);
                break;
            }
        }
    }

    return true;
}
