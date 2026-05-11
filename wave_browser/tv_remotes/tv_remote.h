#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include <vpad/input.h>
#include <stdint.h>

#ifndef VPAD_BUTTON_TV
#   define VPAD_BUTTON_TV 0x00040000u
#endif

// ─── IR Protocol ─────────────────────────────────────────────────────────────

enum class IRProtocol : uint8_t {
    NONE = 0,
    NEC, SAMSUNG,
    SIRCS_12, SIRCS_15, SIRCS_20,
    RC5, RC6,
    PANASONIC, JVC, ROKU,
};

// ─── Button IDs ───────────────────────────────────────────────────────────────

enum TVBtn : int {
    TVBTN_POWER = 0,
    TVBTN_INPUT, TVBTN_MUTE,
    TVBTN_VOL_UP, TVBTN_VOL_DOWN,
    TVBTN_CH_UP,  TVBTN_CH_DOWN,
    TVBTN_0,TVBTN_1,TVBTN_2,TVBTN_3,TVBTN_4,
    TVBTN_5,TVBTN_6,TVBTN_7,TVBTN_8,TVBTN_9,
    TVBTN_UP,TVBTN_DOWN,TVBTN_LEFT,TVBTN_RIGHT,TVBTN_OK,
    TVBTN_BACK,TVBTN_HOME,TVBTN_MENU,TVBTN_GUIDE,TVBTN_INFO,
    TVBTN_RED,TVBTN_GREEN,TVBTN_YELLOW,TVBTN_BLUE,
    TVBTN_PLAY,TVBTN_PAUSE,TVBTN_STOP,TVBTN_REWIND,TVBTN_FFWD,
    TVBTN_COUNT
};

// ─── IR command ───────────────────────────────────────────────────────────────

struct IRCommand {
    uint32_t code;    // 0 = unsupported on this model
    uint8_t  repeats;
};

// ─── On-screen button descriptor ─────────────────────────────────────────────

struct RemoteButtonDesc {
    TVBtn       id;
    int         rx,ry,rw,rh;
    const char* label;
    bool        is_nav;
    bool        is_color;
    SDL_Color   face;
    int         nav_up,nav_down,nav_left,nav_right;
};

// ─── Brand colour theme ───────────────────────────────────────────────────────

struct BrandTheme {
    SDL_Color body,primary,accent,btn,btn_text;
};

// ─── Model descriptor ─────────────────────────────────────────────────────────

struct TVRemoteModel {
    const char*             brand;
    const char*             model;
    int                     year;
    IRProtocol              protocol;
    uint32_t                cec_vendor;
    const char*             cec_osd_name_prefix;
    IRCommand               ir[TVBTN_COUNT];
    const RemoteButtonDesc* layout;
    int                     layout_count;
    BrandTheme              theme;
};

// ─── Public API ───────────────────────────────────────────────────────────────

void                 tv_remote_set_model(const TVRemoteModel* model);
const TVRemoteModel* tv_remote_get_model();
const TVRemoteModel* tv_remote_find(const char* brand, const char* model_str);

bool tv_remote_is_open();
void tv_remote_open();
void tv_remote_close();

bool tv_remote_handle_input(VPADStatus* vpad, bool tp_tapped, int tp_x, int tp_y);
void tv_remote_draw(SDL_Renderer* ren, TTF_Font* fsm, TTF_Font* fmd, TTF_Font* flg);

// Fire IR for a button immediately — called from main.cpp on VPAD_BUTTON_TV press.
// No-op if no model is set or the button has code == 0.
void tv_remote_fire(TVBtn btn);
