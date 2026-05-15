#include "tv_remote.h"
#include "model_registry.h"
#include "../ui_common.h"

#include <string.h>
#include <stdio.h>
#include <stdint.h>

// ─── WUT CCR IR blaster ───────────────────────────────────────────────────────
// The WiiU GamePad has a 940 nm IR LED driven by CCR.
// nn::ccr::CcSendIr() takes an array of on/off durations in ~26.3 µs ticks.
// VPAD_BUTTON_TV is intercepted in main.cpp; we fire our own burst instead.

// CEC/IR is handled by the Aroma plugin — no CCR needed in the app

// ─── Pulse builder ────────────────────────────────────────────────────────────

#define MAX_PULSES 256
static uint16_t s_pulses[MAX_PULSES];
static int      s_pulse_count;

static inline void pulse_reset()              { s_pulse_count = 0; }
static inline void pulse_add(uint16_t t)      { if (s_pulse_count < MAX_PULSES) s_pulses[s_pulse_count++] = t; }
static inline uint16_t us2t(uint32_t us)      { return (uint16_t)((us * 10u) / 263u); }
static inline void ms(uint32_t m, uint32_t s) { pulse_add(us2t(m)); pulse_add(us2t(s)); }

static void blast()
{
    // No-op: TV control is via HDMI-CEC in the Aroma plugin
    (void)s_pulses; (void)s_pulse_count;
}

// ─── Protocol encoders ────────────────────────────────────────────────────────

static void send_nec(uint32_t code, int rep)
{
    for (int r = 0; r <= rep; r++) {
        pulse_reset();
        ms(9000,4500);
        for (int i = 31; i >= 0; i--)
            ((code>>i)&1) ? ms(562,1688) : ms(562,562);
        pulse_add(us2t(562));
        blast();
        if (r < rep) { pulse_reset(); ms(9000,2250); pulse_add(us2t(562)); blast(); }
    }
}

static void send_samsung(uint32_t code, int rep)
{
    for (int r = 0; r <= rep; r++) {
        pulse_reset();
        ms(4500,4500);
        for (int i = 31; i >= 0; i--)
            ((code>>i)&1) ? ms(560,1690) : ms(560,560);
        pulse_add(us2t(560));
        blast();
    }
}

static void send_sircs(uint32_t code, int bits, int rep)
{
    int total = (rep < 2) ? 2 : rep;
    for (int r = 0; r <= total; r++) {
        pulse_reset();
        ms(2400,600);
        for (int i = 0; i < bits; i++)
            ((code>>i)&1) ? ms(1200,600) : ms(600,600);
        blast();
    }
}

static void send_rc5(uint32_t code, int rep)
{
    static uint8_t toggle = 0; toggle ^= 1;
    uint8_t  sys = (uint8_t)((code>>8)&0x1F);
    uint8_t  cmd = (uint8_t)(code&0x3F);
    uint16_t frame = (1u<<13)|(1u<<12)|((uint16_t)toggle<<11)|((uint16_t)sys<<6)|cmd;
    for (int r = 0; r <= rep; r++) {
        pulse_reset();
        for (int i = 13; i >= 0; i--)
            ms(889,889);   // biphase – both 1 and 0 are 889+889; polarity handled by CCR
        (void)frame;
        blast();
    }
}

static void send_rc6(uint32_t code, int rep)
{
    for (int r = 0; r <= rep; r++) {
        pulse_reset();
        ms(2666,889);
        ms(444,444);
        for (int i = 2; i >= 0; i--) ms(444,444);
        ms(889,889);
        for (int i = 15; i >= 0; i--) ms(444,444);
        (void)code;
        blast();
    }
}

static void send_panasonic(uint32_t code, int rep)
{
    for (int r = 0; r <= rep; r++) {
        pulse_reset();
        ms(3456,1728);
        for (int i = 31; i >= 0; i--)
            ((code>>i)&1) ? ms(432,1296) : ms(432,432);
        pulse_add(us2t(432));
        blast();
    }
}

static void send_jvc(uint32_t code, int rep)
{
    bool first = true;
    for (int r = 0; r <= rep; r++) {
        pulse_reset();
        if (first) { ms(8400,4200); first = false; }
        for (int i = 15; i >= 0; i--)
            ((code>>i)&1) ? ms(525,1575) : ms(525,525);
        pulse_add(us2t(525));
        blast();
    }
}

// ─── Generic dispatch ─────────────────────────────────────────────────────────

static void dispatch_ir(const TVRemoteModel* m, TVBtn btn)
{
    if (!m) return;
    const IRCommand& c = m->ir[btn];
    if (c.code == 0) return;
    switch (m->protocol) {
        case IRProtocol::NEC:       send_nec(c.code, c.repeats);       break;
        case IRProtocol::ROKU:      send_nec(c.code, c.repeats);       break;
        case IRProtocol::SAMSUNG:   send_samsung(c.code, c.repeats);   break;
        case IRProtocol::SIRCS_12:  send_sircs(c.code,12,c.repeats);   break;
        case IRProtocol::SIRCS_15:  send_sircs(c.code,15,c.repeats);   break;
        case IRProtocol::SIRCS_20:  send_sircs(c.code,20,c.repeats);   break;
        case IRProtocol::RC5:       send_rc5(c.code, c.repeats);       break;
        case IRProtocol::RC6:       send_rc6(c.code, c.repeats);       break;
        case IRProtocol::PANASONIC: send_panasonic(c.code, c.repeats); break;
        case IRProtocol::JVC:       send_jvc(c.code, c.repeats);       break;
        default: break;
    }
}

// ─── Default button layout ────────────────────────────────────────────────────

#define RP_X     430
#define RP_Y      30
#define RP_W     420
#define RP_H     660
#define HEADER_H  44

static const RemoteButtonDesc DEFAULT_LAYOUT[] = {
//  id               rx   ry   rw   rh   label            nav    clr   face                     U   D   L   R
{ TVBTN_POWER,       12,  16,  70,  36,  "POWER",         false, false,{},                     -1,  3, -1,  1 },
{ TVBTN_MUTE,       154,  16,  70,  36,  "MUTE",          false, false,{},                     -1,  6, -1,  2 },
{ TVBTN_INPUT,      310,  16,  86,  36,  "INPUT",         false, false,{},                     -1,  7,  1, -1 },
{ TVBTN_1,           12,  72,  44,  44,  "1",             false, false,{},                      0,  8, -1,  4 },
{ TVBTN_2,           68,  72,  44,  44,  "2",             false, false,{},                      1,  9,  3,  5 },
{ TVBTN_3,          124,  72,  44,  44,  "3",             false, false,{},                      1, 10,  4,  6 },
{ TVBTN_VOL_UP,     228,  72,  44,  44,  "V+",            false, false,{},                      2, 11,  5,  7 },
{ TVBTN_CH_UP,      284,  72,  44,  44,  "C+",            false, false,{},                      2, 12,  6, -1 },
{ TVBTN_4,           12, 126,  44,  44,  "4",             false, false,{},                      3, 13, -1,  9 },
{ TVBTN_5,           68, 126,  44,  44,  "5",             false, false,{},                      4, 14,  8, 10 },
{ TVBTN_6,          124, 126,  44,  44,  "6",             false, false,{},                      5, 15,  9, 11 },
{ TVBTN_VOL_DOWN,   228, 126,  44,  44,  "V-",            false, false,{},                      6, 16, 10, 12 },
{ TVBTN_CH_DOWN,    284, 126,  44,  44,  "C-",            false, false,{},                      7, 16, 11, -1 },
{ TVBTN_7,           12, 180,  44,  44,  "7",             false, false,{},                      8, 17, -1, 14 },
{ TVBTN_8,           68, 180,  44,  44,  "8",             false, false,{},                      9, 18, 13, 15 },
{ TVBTN_9,          124, 180,  44,  44,  "9",             false, false,{},                     10, 19, 14, -1 },
{ TVBTN_0,           68, 234,  44,  44,  "0",             false, false,{},                     14, 20, -1, -1 },
{ TVBTN_UP,         170, 290,  44,  44,  "\xe2\x96\xb2",  true,  false,{},                    -1, 19, 18, 20 },
{ TVBTN_LEFT,       116, 340,  44,  44,  "\xe2\x97\x84",  true,  false,{},                    17, 21, -1, 19 },
{ TVBTN_OK,         170, 340,  44,  44,  "OK",            true,  false,{},                    17, 21, 18, 20 },
{ TVBTN_RIGHT,      224, 340,  44,  44,  "\xe2\x96\xba",  true,  false,{},                    17, 21, 19, -1 },
{ TVBTN_DOWN,       170, 390,  44,  44,  "\xe2\x96\xbc",  true,  false,{},                    19, 26, 18, 20 },
{ TVBTN_BACK,       352, 290,  60,  36,  "BACK",          false, false,{},                    -1, 23, 17, -1 },
{ TVBTN_HOME,       352, 340,  60,  36,  "HOME",          false, false,{},                    22, 24, 19, -1 },
{ TVBTN_MENU,       352, 390,  60,  36,  "MENU",          false, false,{},                    23, 25, 21, -1 },
{ TVBTN_INFO,       352, 440,  60,  36,  "INFO",          false, false,{},                    24, -1, 21, -1 },
{ TVBTN_RED,         12, 460,  82,  36,  "RED",           false, true, {0xE3,0x18,0x37,0xFF}, 21, 30, -1, 27 },
{ TVBTN_GREEN,      104, 460,  82,  36,  "GREEN",         false, true, {0x00,0xB0,0x40,0xFF}, 21, 30, 26, 28 },
{ TVBTN_YELLOW,     196, 460,  82,  36,  "YELLOW",        false, true, {0xE0,0xB0,0x00,0xFF}, 24, 31, 27, 29 },
{ TVBTN_BLUE,       288, 460,  82,  36,  "BLUE",          false, true, {0x14,0x28,0xA0,0xFF}, 24, 31, 28, -1 },
{ TVBTN_PLAY,        12, 510,  80,  36,  "PLAY",          false, false,{},                    26, -1, -1, 31 },
{ TVBTN_PAUSE,      100, 510,  80,  36,  "PAUSE",         false, false,{},                    27, -1, 30, 32 },
{ TVBTN_STOP,       188, 510,  80,  36,  "STOP",          false, false,{},                    28, -1, 31, 33 },
{ TVBTN_REWIND,     276, 510,  68,  36,  "REW",           false, false,{},                    29, -1, 32, 34 },
{ TVBTN_FFWD,       352, 510,  52,  36,  "FFW",           false, false,{},                    29, -1, 33, -1 },
};
static const int DEFAULT_LAYOUT_COUNT = (int)(sizeof(DEFAULT_LAYOUT)/sizeof(DEFAULT_LAYOUT[0]));

// ─── Module state ─────────────────────────────────────────────────────────────

static bool                  s_open      = false;
static const TVRemoteModel*  s_model     = nullptr;
static int                   s_focus_idx = 19; // OK button

// ─── Public API ───────────────────────────────────────────────────────────────

void tv_remote_set_model(const TVRemoteModel* m) { s_model = m; }
const TVRemoteModel* tv_remote_get_model()        { return s_model; }

const TVRemoteModel* tv_remote_find(const char* brand, const char* model_str)
{
    int n = model_registry_count();
    for (int i = 0; i < n; i++) {
        const TVRemoteModel* m = model_registry_get(i);
        if (brand     && m->brand && strcasecmp(m->brand, brand) != 0)          continue;
        if (model_str && m->model &&
            strncasecmp(m->model, model_str, strlen(model_str)) != 0)           continue;
        return m;
    }
    return nullptr;
}

bool tv_remote_is_open() { return s_open; }
void tv_remote_open()    { s_open = true; s_focus_idx = 19; }
void tv_remote_close()   { s_open = false; }

// ─── tv_remote_fire ───────────────────────────────────────────────────────────
// Called by main.cpp on every VPAD_BUTTON_TV press, whether the overlay is
// open or not.  When the overlay is open it fires the currently focused button.
// When the overlay is closed it fires TVBTN_POWER (most common single-press use).

void tv_remote_fire(TVBtn btn)
{
    dispatch_ir(s_model, btn);
}

// ─── Layout helpers ───────────────────────────────────────────────────────────

static const RemoteButtonDesc* active_layout(int* out_count)
{
    if (s_model && s_model->layout && s_model->layout_count > 0) {
        *out_count = s_model->layout_count;
        return s_model->layout;
    }
    *out_count = DEFAULT_LAYOUT_COUNT;
    return DEFAULT_LAYOUT;
}

static const BrandTheme& active_theme()
{
    static const BrandTheme FALLBACK = {
        {0x22,0x22,0x22,0xFF},{0x1A,0x73,0xE8,0xFF},
        {0x34,0xA8,0x53,0xFF},{0x44,0x44,0x44,0xFF},{0xFF,0xFF,0xFF,0xFF}
    };
    return s_model ? s_model->theme : FALLBACK;
}

// ─── Draw ─────────────────────────────────────────────────────────────────────

void tv_remote_draw(SDL_Renderer* ren, TTF_Font* fsm, TTF_Font* fmd, TTF_Font* /*flg*/)
{
    const BrandTheme& th = active_theme();
    SDL_Color white  = {0xFF,0xFF,0xFF,0xFF};
    SDL_Color gray   = {0x80,0x80,0x80,0xFF};
    SDL_Color shadow = {0x00,0x00,0x00,0x80};

    ui_dim_overlay(ren);

    ui_rect(ren, RP_X+6, RP_Y+6, RP_W, RP_H, shadow);
    ui_rect(ren, RP_X,   RP_Y,   RP_W, RP_H, th.body);
    ui_outline(ren, RP_X, RP_Y, RP_W, RP_H, th.primary, 3);

    ui_rect(ren, RP_X, RP_Y, RP_W, HEADER_H, th.primary);
    char hdr[80];
    if (s_model)
        snprintf(hdr, sizeof(hdr), "%s  %s  (%d)", s_model->brand, s_model->model, s_model->year);
    else
        snprintf(hdr, sizeof(hdr), "TV Remote – no model set");
    ui_text(ren, fmd, hdr,        RP_X+RP_W/2, RP_Y+30, white, 1);
    ui_text(ren, fsm, "B: close", RP_X+RP_W-8, RP_Y+30, white, 2);

    int layout_count;
    const RemoteButtonDesc* layout = active_layout(&layout_count);

    for (int i = 0; i < layout_count; i++) {
        const RemoteButtonDesc& b = layout[i];
        int bx = RP_X + b.rx;
        int by = RP_Y + HEADER_H + b.ry;
        bool focused   = (i == s_focus_idx);
        bool supported = !s_model || (s_model->ir[b.id].code != 0);

        SDL_Color face;
        if      (!supported)        face = {0x2A,0x2A,0x2A,0xFF};
        else if (b.id==TVBTN_POWER) face = th.primary;
        else if (b.is_nav)          face = th.accent;
        else if (b.is_color)        face = b.face;
        else                        face = th.btn;

        ui_rect(ren, bx, by, b.rw, b.rh, face);

        if (focused) ui_outline(ren, bx-2, by-2, b.rw+4, b.rh+4, th.accent, 2);
        else         ui_outline(ren, bx,   by,   b.rw,   b.rh,   gray);

        SDL_Color lbl = (b.is_color || b.id==TVBTN_POWER) ? white : th.btn_text;
        if (!supported) lbl = {0x55,0x55,0x55,0xFF};
        ui_text(ren, fmd, b.label, bx+b.rw/2, by+b.rh/2+8, lbl, 1);
    }

    // Hint: TV button = fire focused; A = fire focused; tap = fire tapped
    ui_text(ren, fsm,
        "\xe2\x96\xb2\xe2\x96\xbc\xe2\x97\x84\xe2\x96\xba: navigate  "
        "A / TV\xe2\x96\xa0: fire  tap: fire  B: close",
        RP_X+RP_W/2, RP_Y+RP_H-6, gray, 1);

    SDL_RenderPresent(ren);
}

// ─── Input ────────────────────────────────────────────────────────────────────

bool tv_remote_handle_input(VPADStatus* vpad, bool tp_tapped, int tp_x, int tp_y)
{
    if (!s_open) return false;

    int layout_count;
    const RemoteButtonDesc* layout = active_layout(&layout_count);
    if (s_focus_idx >= layout_count) s_focus_idx = 0;

    uint32_t btn = vpad->trigger;

    if (btn & VPAD_BUTTON_B) { s_open = false; return false; }

    // D-pad navigation
    auto nav = [&](int dir) {
        int next = -1;
        switch (dir) {
            case 0: next = layout[s_focus_idx].nav_up;    break;
            case 1: next = layout[s_focus_idx].nav_down;  break;
            case 2: next = layout[s_focus_idx].nav_left;  break;
            case 3: next = layout[s_focus_idx].nav_right; break;
        }
        if (next >= 0 && next < layout_count) s_focus_idx = next;
    };
    if (btn & VPAD_BUTTON_UP)    nav(0);
    if (btn & VPAD_BUTTON_DOWN)  nav(1);
    if (btn & VPAD_BUTTON_LEFT)  nav(2);
    if (btn & VPAD_BUTTON_RIGHT) nav(3);

    // A fires the focused button
    if (btn & VPAD_BUTTON_A)
        dispatch_ir(s_model, layout[s_focus_idx].id);

    // Physical TV button on the GamePad fires the focused button
    if (btn & VPAD_BUTTON_TV)
        dispatch_ir(s_model, layout[s_focus_idx].id);

    // Touch: move focus + fire
    if (tp_tapped) {
        bool in_panel = (tp_x>=RP_X && tp_x<RP_X+RP_W && tp_y>=RP_Y && tp_y<RP_Y+RP_H);
        if (!in_panel) { s_open = false; return false; }
        for (int i = 0; i < layout_count; i++) {
            const RemoteButtonDesc& b = layout[i];
            int bx = RP_X+b.rx, by = RP_Y+HEADER_H+b.ry;
            if (tp_x>=bx && tp_x<bx+b.rw && tp_y>=by && tp_y<by+b.rh) {
                s_focus_idx = i;
                dispatch_ir(s_model, b.id);
                break;
            }
        }
    }

    return true;
}
