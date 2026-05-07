#include "tv_remote.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ═══════════════════════════════════════════════════════════════════════════
//  Brand table
// ═══════════════════════════════════════════════════════════════════════════

const char* const TV_BRAND_NAMES[] = {
    "Amazon Fire TV", "AOC",          "Bang & Olufsen", "Beko",
    "BenQ",           "Changhong",    "Coby",           "Dynex",
    "Element",        "Emerson",      "Funai",          "Grundig",
    "Haier",          "Hisense",      "Hitachi",        "iFFalcon",
    "Insignia",       "JVC",          "Konka",          "LG",
    "Loewe",          "Magnavox",     "Medion",         "Metz",
    "Mitsubishi",     "Nokia TV",     "Onn",            "Panasonic",
    "Philips",        "Pioneer",      "Polaroid",       "ProScan",
    "RCA",            "Roku TV",      "Samsung",        "Sanyo",
    "Sceptre",        "Seiki",        "Sharp",          "Skyworth",
    "Sony",           "TCL",          "Thomson",        "Toshiba",
    "Vestel",         "Vizio",        "Westinghouse",   "Xiaomi",
};
const int TV_BRAND_COUNT = (int)(sizeof(TV_BRAND_NAMES) / sizeof(TV_BRAND_NAMES[0]));

// ═══════════════════════════════════════════════════════════════════════════
//  Per-brand colour theme
// ═══════════════════════════════════════════════════════════════════════════

struct BrandTheme {
    SDL_Color body;      // remote body fill
    SDL_Color primary;   // brand colour (header bar, power btn)
    SDL_Color accent;    // OK / nav ring colour
    SDL_Color btn;       // regular button face
    SDL_Color btn_text;  // regular button label
};

// Indexed identically to TV_BRAND_NAMES[]
static const BrandTheme BRAND_THEMES[] = {
    // Amazon Fire TV   body          primary            accent             btn             btn_text
    { {0x23,0x23,0x23,0xFF}, {0x00,0x78,0xD7,0xFF}, {0x00,0xB2,0xFF,0xFF}, {0x38,0x38,0x38,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // AOC
    { {0x1A,0x1A,0x1A,0xFF}, {0x00,0xA1,0xDE,0xFF}, {0x00,0xC8,0xFF,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Bang & Olufsen   (light body, gold accent)
    { {0xF0,0xF0,0xF0,0xFF}, {0xB8,0x96,0x0A,0xFF}, {0x8B,0x6F,0x00,0xFF}, {0xD8,0xD8,0xD8,0xFF}, {0x1A,0x1A,0x1A,0xFF} },
    // Beko
    { {0x1A,0x1A,0x1A,0xFF}, {0xE3,0x1B,0x23,0xFF}, {0xFF,0x50,0x50,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // BenQ
    { {0x1A,0x1A,0x1A,0xFF}, {0x00,0x57,0xA8,0xFF}, {0x00,0x8C,0xFF,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Changhong
    { {0x1A,0x1A,0x1A,0xFF}, {0xE3,0x1B,0x23,0xFF}, {0xFF,0x50,0x50,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Coby
    { {0x1A,0x1A,0x1A,0xFF}, {0x00,0x4B,0x87,0xFF}, {0x00,0x8C,0xFF,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Dynex
    { {0x1A,0x1A,0x1A,0xFF}, {0x00,0x6F,0xBA,0xFF}, {0x00,0xA0,0xFF,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Element
    { {0x1A,0x1A,0x1A,0xFF}, {0x00,0x9B,0xDE,0xFF}, {0x00,0xC8,0xFF,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Emerson
    { {0x1A,0x1A,0x1A,0xFF}, {0x00,0x49,0x9E,0xFF}, {0x00,0x80,0xFF,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Funai
    { {0x1A,0x1A,0x1A,0xFF}, {0x00,0x38,0x91,0xFF}, {0x00,0x70,0xFF,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Grundig
    { {0x1A,0x1A,0x1A,0xFF}, {0x00,0x4F,0x9A,0xFF}, {0x00,0x80,0xFF,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Haier
    { {0x1A,0x1A,0x1A,0xFF}, {0x00,0x71,0xB9,0xFF}, {0x00,0xA8,0xFF,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Hisense
    { {0x1A,0x1A,0x1A,0xFF}, {0x00,0x3D,0xA5,0xFF}, {0x00,0x70,0xFF,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Hitachi
    { {0x1A,0x1A,0x1A,0xFF}, {0xCC,0x00,0x00,0xFF}, {0xFF,0x40,0x40,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // iFFalcon
    { {0x1A,0x1A,0x1A,0xFF}, {0xE3,0x1B,0x23,0xFF}, {0xFF,0x50,0x50,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Insignia        (Best Buy orange)
    { {0x1A,0x1A,0x1A,0xFF}, {0xFF,0x6B,0x00,0xFF}, {0xFF,0xA0,0x00,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // JVC
    { {0x1A,0x1A,0x1A,0xFF}, {0x00,0x3B,0x8E,0xFF}, {0x00,0x70,0xFF,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Konka
    { {0x1A,0x1A,0x1A,0xFF}, {0x00,0x5B,0xAA,0xFF}, {0x00,0x90,0xFF,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // LG              (red)
    { {0x1A,0x1A,0x1A,0xFF}, {0xA5,0x00,0x34,0xFF}, {0xFF,0x30,0x60,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Loewe           (light/premium)
    { {0xF2,0xF2,0xF2,0xFF}, {0x2B,0x2B,0x2B,0xFF}, {0x80,0x80,0x80,0xFF}, {0xD8,0xD8,0xD8,0xFF}, {0x1A,0x1A,0x1A,0xFF} },
    // Magnavox
    { {0x1A,0x1A,0x1A,0xFF}, {0x00,0x2D,0x72,0xFF}, {0x00,0x60,0xFF,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Medion
    { {0x1A,0x1A,0x1A,0xFF}, {0xCC,0x00,0x00,0xFF}, {0xFF,0x40,0x40,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Metz
    { {0x1A,0x1A,0x1A,0xFF}, {0xCC,0x00,0x00,0xFF}, {0xFF,0x40,0x40,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Mitsubishi
    { {0x1A,0x1A,0x1A,0xFF}, {0xCC,0x00,0x00,0xFF}, {0xFF,0x40,0x40,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Nokia TV
    { {0x1A,0x1A,0x1A,0xFF}, {0x00,0x54,0x87,0xFF}, {0x00,0x90,0xFF,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Onn             (Walmart blue)
    { {0x1A,0x1A,0x1A,0xFF}, {0x00,0x71,0xCE,0xFF}, {0x00,0xA8,0xFF,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Panasonic
    { {0x1A,0x1A,0x1A,0xFF}, {0x00,0x47,0xAB,0xFF}, {0x00,0x80,0xFF,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Philips
    { {0x1A,0x1A,0x1A,0xFF}, {0x00,0x14,0x89,0xFF}, {0x00,0x50,0xFF,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Pioneer
    { {0x1A,0x1A,0x1A,0xFF}, {0x00,0x40,0x9A,0xFF}, {0x00,0x70,0xFF,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Polaroid        (monochrome)
    { {0xF0,0xF0,0xF0,0xFF}, {0x10,0x10,0x10,0xFF}, {0x40,0x40,0x40,0xFF}, {0xD0,0xD0,0xD0,0xFF}, {0x10,0x10,0x10,0xFF} },
    // ProScan
    { {0x1A,0x1A,0x1A,0xFF}, {0x00,0x38,0x91,0xFF}, {0x00,0x70,0xFF,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // RCA
    { {0x1A,0x1A,0x1A,0xFF}, {0x00,0x38,0x91,0xFF}, {0x00,0x70,0xFF,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Roku TV         (purple)
    { {0x1A,0x1A,0x1A,0xFF}, {0x6C,0x25,0xBE,0xFF}, {0xA0,0x60,0xFF,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Samsung         (blue)
    { {0x1A,0x1A,0x1A,0xFF}, {0x14,0x28,0xA0,0xFF}, {0x18,0x50,0xFF,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Sanyo
    { {0x1A,0x1A,0x1A,0xFF}, {0x00,0x4F,0x9A,0xFF}, {0x00,0x80,0xFF,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Sceptre
    { {0x1A,0x1A,0x1A,0xFF}, {0xC8,0x10,0x26,0xFF}, {0xFF,0x40,0x60,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Seiki
    { {0x1A,0x1A,0x1A,0xFF}, {0x00,0x4B,0x87,0xFF}, {0x00,0x80,0xFF,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Sharp
    { {0x1A,0x1A,0x1A,0xFF}, {0x00,0x30,0x87,0xFF}, {0x00,0x60,0xFF,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Skyworth
    { {0x1A,0x1A,0x1A,0xFF}, {0xE3,0x1B,0x23,0xFF}, {0xFF,0x50,0x50,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Sony
    { {0x12,0x12,0x12,0xFF}, {0x00,0x30,0x87,0xFF}, {0x00,0x60,0xFF,0xFF}, {0x28,0x28,0x28,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // TCL             (red)
    { {0x1A,0x1A,0x1A,0xFF}, {0xE3,0x18,0x37,0xFF}, {0xFF,0x50,0x50,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Thomson
    { {0x1A,0x1A,0x1A,0xFF}, {0xE3,0x1B,0x23,0xFF}, {0xFF,0x50,0x50,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Toshiba
    { {0x1A,0x1A,0x1A,0xFF}, {0xEE,0x1C,0x25,0xFF}, {0xFF,0x50,0x50,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Vestel
    { {0x1A,0x1A,0x1A,0xFF}, {0x00,0x5B,0xAA,0xFF}, {0x00,0x90,0xFF,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Vizio
    { {0x1A,0x1A,0x1A,0xFF}, {0x00,0x6F,0xBA,0xFF}, {0x00,0xA8,0xFF,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Westinghouse
    { {0x1A,0x1A,0x1A,0xFF}, {0xD4,0x00,0x27,0xFF}, {0xFF,0x40,0x50,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
    // Xiaomi          (orange)
    { {0x1A,0x1A,0x1A,0xFF}, {0xFF,0x62,0x00,0xFF}, {0xFF,0x90,0x00,0xFF}, {0x30,0x30,0x30,0xFF}, {0xFF,0xFF,0xFF,0xFF} },
};

// ═══════════════════════════════════════════════════════════════════════════
//  Button definition
// ═══════════════════════════════════════════════════════════════════════════
//
//  All positions are relative to the remote panel's top-left corner.
//  up/down/left/right are indices into the button array (-1 = boundary).

enum RBtnID {
    // Row 0: top strip
    RB_POWER=0, RB_MUTE, RB_INPUT,
    // Row 1: numbers + vol/ch labels
    RB_1, RB_2, RB_3, RB_VOL_UP, RB_CH_UP,
    RB_4, RB_5, RB_6, RB_VOL_DN, RB_CH_DN,
    RB_7, RB_8, RB_9,
    RB_STAR, RB_0, RB_HASH,
    // Navigation ring
    RB_UP, RB_LEFT, RB_OK, RB_RIGHT, RB_DOWN,
    // Side buttons
    RB_BACK, RB_HOME, RB_MENU,
    // Colour row
    RB_RED, RB_GREEN, RB_YELLOW, RB_BLUE,
    // Info / guide
    RB_INFO, RB_GUIDE,
    RB_COUNT
};

struct BtnDef {
    int rx, ry, rw, rh;   // position inside remote panel
    const char* label;
    int up, down, left, right;
    bool is_nav;           // true → use accent colour for focus ring
    bool is_color;         // true → coloured button face
    SDL_Color color_face;  // used when is_color
};

// Panel is 420 wide × 660 tall, placed at (430, 30) on the 1280×720 screen.
#define RP_X  430
#define RP_Y   30
#define RP_W  420
#define RP_H  660

// Button sizes
#define BTN_SM   44   // number-pad button size (square)
#define BTN_MD   58   // wide button
#define BTN_TALL 36   // height for wide buttons

// Column starts (relative to panel)
#define COL_A   24    // numpad col 1
#define COL_B   80    // numpad col 2
#define COL_C  136    // numpad col 3
#define COL_D  230    // vol/ch col 1
#define COL_E  290    // vol/ch col 2
#define COL_F  350    // side right col (back/home/menu)

// Row starts
#define ROW_0   20    // power / mute / input
#define ROW_1   86    // 1 2 3 + vol+/ch+
#define ROW_2  140    // 4 5 6 + vol-/ch-
#define ROW_3  194    // 7 8 9
#define ROW_4  248    // * 0 #
#define ROW_5  322    // nav ring up
#define ROW_6  372    // nav ring mid (left/ok/right) + back/home
#define ROW_7  422    // nav ring down + menu
#define ROW_8  494    // colour row
#define ROW_9  546    // info / guide

static const BtnDef BTNS[RB_COUNT] = {
    // RB_POWER
    { COL_A, ROW_0, 70, BTN_TALL, "POWER",  -1, RB_1,       -1,       RB_MUTE,  false, false, {} },
    // RB_MUTE
    { 154, ROW_0, 70, BTN_TALL,   "MUTE",   -1, RB_VOL_UP,  RB_POWER, RB_INPUT, false, false, {} },
    // RB_INPUT
    { 310, ROW_0, 86, BTN_TALL,   "INPUT",  -1, RB_CH_UP,   RB_MUTE,  -1,       false, false, {} },

    // RB_1
    { COL_A, ROW_1, BTN_SM, BTN_SM, "1",  RB_POWER,  RB_4,      -1,    RB_2,      false, false, {} },
    // RB_2
    { COL_B, ROW_1, BTN_SM, BTN_SM, "2",  RB_MUTE,   RB_5,      RB_1,  RB_3,      false, false, {} },
    // RB_3
    { COL_C, ROW_1, BTN_SM, BTN_SM, "3",  RB_MUTE,   RB_6,      RB_2,  RB_VOL_UP, false, false, {} },
    // RB_VOL_UP
    { COL_D, ROW_1, BTN_SM, BTN_SM, "V+", RB_INPUT,  RB_VOL_DN, RB_3,  RB_CH_UP,  false, false, {} },
    // RB_CH_UP
    { COL_E, ROW_1, BTN_SM, BTN_SM, "C+", RB_INPUT,  RB_CH_DN,  RB_VOL_UP, -1,    false, false, {} },

    // RB_4
    { COL_A, ROW_2, BTN_SM, BTN_SM, "4",  RB_1,  RB_7,  -1,    RB_5,      false, false, {} },
    // RB_5
    { COL_B, ROW_2, BTN_SM, BTN_SM, "5",  RB_2,  RB_8,  RB_4,  RB_6,      false, false, {} },
    // RB_6
    { COL_C, ROW_2, BTN_SM, BTN_SM, "6",  RB_3,  RB_9,  RB_5,  RB_VOL_DN, false, false, {} },
    // RB_VOL_DN
    { COL_D, ROW_2, BTN_SM, BTN_SM, "V-", RB_VOL_UP, RB_UP, RB_6,  RB_CH_DN, false, false, {} },
    // RB_CH_DN
    { COL_E, ROW_2, BTN_SM, BTN_SM, "C-", RB_CH_UP, RB_UP, RB_VOL_DN, -1,  false, false, {} },

    // RB_7
    { COL_A, ROW_3, BTN_SM, BTN_SM, "7",  RB_4,  RB_STAR, -1,    RB_8,  false, false, {} },
    // RB_8
    { COL_B, ROW_3, BTN_SM, BTN_SM, "8",  RB_5,  RB_0,    RB_7,  RB_9,  false, false, {} },
    // RB_9
    { COL_C, ROW_3, BTN_SM, BTN_SM, "9",  RB_6,  RB_HASH, RB_8,  -1,    false, false, {} },

    // RB_STAR
    { COL_A, ROW_4, BTN_SM, BTN_SM, "*",  RB_7,  RB_UP,   -1,    RB_0,  false, false, {} },
    // RB_0
    { COL_B, ROW_4, BTN_SM, BTN_SM, "0",  RB_8,  RB_UP,   RB_STAR,RB_HASH,false,false,{} },
    // RB_HASH
    { COL_C, ROW_4, BTN_SM, BTN_SM, "#",  RB_9,  RB_UP,   RB_0,  RB_BACK,false, false, {} },

    // RB_UP
    { 170, ROW_5, BTN_SM, BTN_SM, "\xe2\x96\xb2", RB_STAR,  RB_OK,   RB_VOL_DN, RB_BACK, true, false, {} },
    // RB_LEFT
    { 116, ROW_6, BTN_SM, BTN_SM, "\xe2\x97\x84", RB_UP,    RB_DOWN, -1,         RB_OK,   true, false, {} },
    // RB_OK
    { 170, ROW_6, BTN_SM, BTN_SM, "OK",            RB_UP,    RB_DOWN, RB_LEFT,    RB_RIGHT,true,false, {} },
    // RB_RIGHT
    { 224, ROW_6, BTN_SM, BTN_SM, "\xe2\x96\xba", RB_UP,    RB_DOWN, RB_OK,      RB_BACK, true, false, {} },
    // RB_DOWN
    { 170, ROW_7, BTN_SM, BTN_SM, "\xe2\x96\xbc", RB_OK,    RB_RED,  RB_VOL_DN,  RB_MENU, true, false, {} },

    // RB_BACK
    { COL_F, ROW_5, 60, BTN_TALL, "BACK",  RB_HASH,  RB_HOME, RB_UP,    -1,    false, false, {} },
    // RB_HOME
    { COL_F, ROW_6, 60, BTN_TALL, "HOME",  RB_BACK,  RB_MENU, RB_RIGHT, -1,    false, false, {} },
    // RB_MENU
    { COL_F, ROW_7, 60, BTN_TALL, "MENU",  RB_HOME,  RB_BLUE, RB_DOWN,  -1,    false, false, {} },

    // RB_RED
    { 24,  ROW_8, 82, BTN_TALL, "RED",    RB_DOWN,  RB_INFO,  -1,       RB_GREEN, false, true, {0xE3,0x18,0x37,0xFF} },
    // RB_GREEN
    { 112, ROW_8, 82, BTN_TALL, "GREEN",  RB_DOWN,  RB_INFO,  RB_RED,   RB_YELLOW,false,true, {0x00,0xB0,0x40,0xFF} },
    // RB_YELLOW
    { 200, ROW_8, 82, BTN_TALL, "YELLOW", RB_MENU,  RB_GUIDE, RB_GREEN, RB_BLUE,  false,true, {0xE0,0xB0,0x00,0xFF} },
    // RB_BLUE
    { 288, ROW_8, 82, BTN_TALL, "BLUE",   RB_MENU,  RB_GUIDE, RB_YELLOW,-1,       false,true, {0x14,0x28,0xA0,0xFF} },

    // RB_INFO
    { 24,  ROW_9, 170, BTN_TALL, "INFO",  RB_RED,   -1,  -1,       RB_GUIDE, false, false, {} },
    // RB_GUIDE
    { 206, ROW_9, 170, BTN_TALL, "GUIDE", RB_YELLOW,-1,  RB_INFO,  -1,       false, false, {} },
};

// ═══════════════════════════════════════════════════════════════════════════
//  Module state
// ═══════════════════════════════════════════════════════════════════════════

static bool s_open        = false;
static int  s_brand_idx   = 0;
static int  s_focus_btn   = RB_OK;

static SDL_Renderer* s_ren = nullptr;
static TTF_Font*     s_fsm = nullptr;
static TTF_Font*     s_fmd = nullptr;
static TTF_Font*     s_flg = nullptr;

// ═══════════════════════════════════════════════════════════════════════════
//  Public API
// ═══════════════════════════════════════════════════════════════════════════

void tv_remote_set_brand(int idx) { s_brand_idx = (idx >= 0 && idx < TV_BRAND_COUNT) ? idx : 0; }
int  tv_remote_get_brand()        { return s_brand_idx; }
bool tv_remote_is_open()          { return s_open; }
void tv_remote_open()             { s_open = true; s_focus_btn = RB_OK; }
void tv_remote_close()            { s_open = false; }

// ═══════════════════════════════════════════════════════════════════════════
//  Drawing helpers
// ═══════════════════════════════════════════════════════════════════════════

static void r_fill(int x, int y, int w, int h, SDL_Color c)
{
    SDL_SetRenderDrawColor(s_ren, c.r, c.g, c.b, c.a);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderFillRect(s_ren, &r);
}

static void r_outline(int x, int y, int w, int h, SDL_Color c, int t = 1)
{
    SDL_SetRenderDrawColor(s_ren, c.r, c.g, c.b, c.a);
    for (int d = 0; d < t; d++) {
        SDL_Rect r = {x+d, y+d, w-d*2, h-d*2};
        SDL_RenderDrawRect(s_ren, &r);
    }
}

static void r_text(TTF_Font* font, const char* text, int x, int y, SDL_Color c, int align = 0)
{
    if (!font || !text || !text[0]) return;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text, c);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(s_ren, surf);
    int tw = surf->w, th = surf->h;
    SDL_FreeSurface(surf);
    if (!tex) return;
    if      (align == 1) x -= tw / 2;
    else if (align == 2) x -= tw;
    SDL_Rect dst = {x, y - th, tw, th};
    SDL_RenderCopy(s_ren, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
}

// ─── Stub: send an IR / HDMI-CEC command to the TV ───────────────────────────
// Replace with real IR blaster / CEC calls if the hardware API becomes available.
static void send_tv_command(int btn_id)
{
    (void)btn_id;
    // TODO: implement platform IR / CEC dispatch here
}

// ═══════════════════════════════════════════════════════════════════════════
//  Draw
// ═══════════════════════════════════════════════════════════════════════════

void tv_remote_draw(SDL_Renderer* ren,
                    TTF_Font* fsm, TTF_Font* fmd, TTF_Font* flg)
{
    s_ren = ren; s_fsm = fsm; s_fmd = fmd; s_flg = flg;

    const BrandTheme& th = BRAND_THEMES[s_brand_idx];
    SDL_Color white  = {0xFF,0xFF,0xFF,0xFF};
    SDL_Color gray   = {0x80,0x80,0x80,0xFF};
    SDL_Color shadow = {0x00,0x00,0x00,0x80};

    // ── Dim backdrop ─────────────────────────────────────────────────────────
    SDL_SetRenderDrawBlendMode(s_ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(s_ren, 0x00, 0x00, 0x00, 180);
    SDL_Rect full = {0, 0, 1280, 720};
    SDL_RenderFillRect(s_ren, &full);
    SDL_SetRenderDrawBlendMode(s_ren, SDL_BLENDMODE_NONE);

    // ── Remote body shadow ───────────────────────────────────────────────────
    r_fill(RP_X + 6, RP_Y + 6, RP_W, RP_H, shadow);

    // ── Remote body ──────────────────────────────────────────────────────────
    r_fill(RP_X, RP_Y, RP_W, RP_H, th.body);
    r_outline(RP_X, RP_Y, RP_W, RP_H, th.primary, 3);

    // ── Header bar ───────────────────────────────────────────────────────────
    r_fill(RP_X, RP_Y, RP_W, 44, th.primary);
    r_text(s_fmd, TV_BRAND_NAMES[s_brand_idx],
           RP_X + RP_W/2, RP_Y + 30, white, 1);

    // Close [X] hint
    r_text(s_fsm, "B: close", RP_X + RP_W - 8, RP_Y + 30, white, 2);

    // ── Each button ──────────────────────────────────────────────────────────
    for (int i = 0; i < RB_COUNT; i++) {
        const BtnDef& b = BTNS[i];
        int bx = RP_X + b.rx;
        int by = RP_Y + 44 + b.ry;   // 44 = header height

        bool focused = (i == s_focus_btn);

        // Face colour
        SDL_Color face;
        if (i == RB_POWER)            face = th.primary;
        else if (b.is_nav)            face = th.accent;
        else if (b.is_color)          face = b.color_face;
        else                          face = th.btn;

        // Special power button is taller
        r_fill(bx, by, b.rw, b.rh, face);

        // Focus ring
        if (focused)
            r_outline(bx - 2, by - 2, b.rw + 4, b.rh + 4, th.accent, 2);
        else
            r_outline(bx, by, b.rw, b.rh, gray, 1);

        // Label
        SDL_Color lblcol = (b.is_color || i == RB_POWER) ? white : th.btn_text;
        if (i == RB_POWER && th.btn_text.r > 200 && th.btn_text.g > 200)
            lblcol = white;   // always white on power
        r_text(s_fmd, b.label, bx + b.rw/2, by + b.rh/2 + 8, lblcol, 1);
    }

    // ── Hint bar ─────────────────────────────────────────────────────────────
    r_text(s_fsm,
           "\xe2\x96\xb2\xe2\x96\xbc\xe2\x97\x84\xe2\x96\xba: navigate  A: press  B: close remote",
           RP_X + RP_W/2, RP_Y + RP_H - 6, gray, 1);

    SDL_RenderPresent(s_ren);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Input
// ═══════════════════════════════════════════════════════════════════════════

bool tv_remote_handle_input(VPADStatus* vpad,
                             bool tp_tapped, int tp_x, int tp_y)
{
    if (!s_open) return false;

    uint32_t btn = vpad->trigger;

    // Close
    if (btn & VPAD_BUTTON_B) { s_open = false; return false; }

    // D-pad navigation
    if (btn & VPAD_BUTTON_UP) {
        int n = BTNS[s_focus_btn].up;
        if (n >= 0) s_focus_btn = n;
    }
    if (btn & VPAD_BUTTON_DOWN) {
        int n = BTNS[s_focus_btn].down;
        if (n >= 0) s_focus_btn = n;
    }
    if (btn & VPAD_BUTTON_LEFT) {
        int n = BTNS[s_focus_btn].left;
        if (n >= 0) s_focus_btn = n;
    }
    if (btn & VPAD_BUTTON_RIGHT) {
        int n = BTNS[s_focus_btn].right;
        if (n >= 0) s_focus_btn = n;
    }

    // Activate focused button
    if (btn & VPAD_BUTTON_A) {
        send_tv_command(s_focus_btn);
        // Mirror nav-ring to GamePad VPAD would need CEC/IR; stub for now.
    }

    // Touch – find which button was tapped
    if (tp_tapped) {
        for (int i = 0; i < RB_COUNT; i++) {
            const BtnDef& b = BTNS[i];
            int bx = RP_X + b.rx;
            int by = RP_Y + 44 + b.ry;
            if (tp_x >= bx && tp_x < bx + b.rw &&
                tp_y >= by && tp_y < by + b.rh) {
                s_focus_btn = i;
                send_tv_command(i);
                break;
            }
        }
        // Tapped outside remote → close
        if (tp_x < RP_X || tp_x >= RP_X + RP_W ||
            tp_y < RP_Y || tp_y >= RP_Y + RP_H) {
            s_open = false;
            return false;
        }
    }

    return true;
}
