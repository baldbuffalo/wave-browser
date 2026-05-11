#include "settings.h"
#include "ui_common.h"
#include "tv_remotes/tv_remote.h"
#include "tv_remotes/model_registry.h"
#include "tv_remotes/tv_detect.h"

#include <vpad/input.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ─── Global settings instance ────────────────────────────────────────────────

WaveSettings g_settings;

// ─── Key helpers ─────────────────────────────────────────────────────────────

void settings_make_key(const char* brand, int year, const char* model,
                        char* out, int out_len)
{
    snprintf(out, (size_t)out_len, "%s/%d/%s", brand, year, model);
}

static void apply_model_key(const char* key)
{
    if (!key || !key[0]) { tv_remote_set_model(nullptr); return; }
    int n = model_registry_count();
    for (int i = 0; i < n; i++) {
        const TVRemoteModel* m = model_registry_get(i);
        char k[128];
        settings_make_key(m->brand, m->year, m->model, k, sizeof(k));
        if (strcmp(k, key) == 0) { tv_remote_set_model(m); return; }
    }
    tv_remote_set_model(nullptr);
}

// ─── Persistence ─────────────────────────────────────────────────────────────

void settings_load()
{
    FILE* f = fopen(SETTINGS_PATH, "r");
    if (!f) { apply_model_key(g_settings.tv_model_key); return; }
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        size_t ln = strlen(line);
        if (ln && line[ln-1] == '\n') line[--ln] = '\0';
        int n = 0;
        if      (sscanf(line, "improved_multitasking=%d", &n) == 1)
            g_settings.improved_multitasking = (n != 0);
        else if (sscanf(line, "improved_remote=%d", &n) == 1)
            g_settings.improved_remote = (n != 0);
        else if (strncmp(line, "tv_model_key=", 13) == 0)
            snprintf(g_settings.tv_model_key, sizeof(g_settings.tv_model_key), "%s", line+13);
    }
    fclose(f);
    apply_model_key(g_settings.tv_model_key);
}

void settings_save()
{
    FILE* f = fopen(SETTINGS_PATH, "w");
    if (!f) return;
    fprintf(f, "improved_multitasking=%d\n", g_settings.improved_multitasking ? 1 : 0);
    fprintf(f, "improved_remote=%d\n",       g_settings.improved_remote       ? 1 : 0);
    fprintf(f, "tv_model_key=%s\n",          g_settings.tv_model_key);
    fclose(f);
}

// ─── UI state machine ────────────────────────────────────────────────────────
//
//  PAGE_MAIN      – top-level toggle rows + "TV Remote Setup" entry
//  PAGE_BRAND     – scrollable list of unique brands
//  PAGE_YEAR      – list of years for selected brand
//  PAGE_MODEL     – list of models for selected brand+year
//  PAGE_DETECTING – animated CEC probe screen
//  PAGE_DETECT_RESULT – show what was found

enum SettingsPage {
    PAGE_MAIN = 0,
    PAGE_BRAND,
    PAGE_YEAR,
    PAGE_MODEL,
    PAGE_DETECTING,
    PAGE_DETECT_RESULT,
};

// ─── Panel geometry ──────────────────────────────────────────────────────────

#define PANEL_X   160
#define PANEL_Y    60
#define PANEL_W   960
#define PANEL_H   600
#define HEADER_H   52
#define ROW_H      56
#define ROW_PAD     8
#define LIST_X    (PANEL_X + 32)
#define LIST_TOP  (PANEL_Y + HEADER_H + 12)
#define ROWS_VISIBLE 8

static SettingsPage s_page      = PAGE_MAIN;
static int          s_sel       = 0;  // cursor within current list
static int          s_scroll    = 0;  // scroll offset within current list

// Drill-down selections
static char  s_brand_sel[64]   = {};
static int   s_year_sel        = 0;

// Unique brand / year arrays built on demand
static char  s_brands[64][64];
static int   s_brand_count     = 0;
static int   s_years[32];
static int   s_year_count      = 0;
// Model index array for selected brand+year
static int   s_model_indices[64];
static int   s_model_count     = 0;

// Main page rows
#define ROW_MULTITASK  0
#define ROW_REMOTE     1
#define ROW_TV_SETUP   2
#define ROW_MAIN_COUNT 3

// ─── List builders ───────────────────────────────────────────────────────────

static void build_brands()
{
    s_brand_count = 0;
    int n = model_registry_count();
    for (int i = 0; i < n; i++) {
        const TVRemoteModel* m = model_registry_get(i);
        bool found = false;
        for (int j = 0; j < s_brand_count; j++)
            if (strcasecmp(s_brands[j], m->brand) == 0) { found = true; break; }
        if (!found && s_brand_count < 64)
            strncpy(s_brands[s_brand_count++], m->brand, 63);
    }
    // sort alphabetically
    for (int i = 0; i < s_brand_count-1; i++)
        for (int j = i+1; j < s_brand_count; j++)
            if (strcasecmp(s_brands[i], s_brands[j]) > 0) {
                char tmp[64]; strcpy(tmp, s_brands[i]);
                strcpy(s_brands[i], s_brands[j]); strcpy(s_brands[j], tmp);
            }
}

static void build_years()
{
    s_year_count = 0;
    int n = model_registry_count();
    for (int i = 0; i < n; i++) {
        const TVRemoteModel* m = model_registry_get(i);
        if (strcasecmp(m->brand, s_brand_sel) != 0) continue;
        bool found = false;
        for (int j = 0; j < s_year_count; j++)
            if (s_years[j] == m->year) { found = true; break; }
        if (!found && s_year_count < 32) s_years[s_year_count++] = m->year;
    }
    // sort descending (newest first)
    for (int i = 0; i < s_year_count-1; i++)
        for (int j = i+1; j < s_year_count; j++)
            if (s_years[i] < s_years[j]) { int t=s_years[i]; s_years[i]=s_years[j]; s_years[j]=t; }
}

static void build_models()
{
    s_model_count = 0;
    int n = model_registry_count();
    for (int i = 0; i < n; i++) {
        const TVRemoteModel* m = model_registry_get(i);
        if (strcasecmp(m->brand, s_brand_sel) != 0) continue;
        if (m->year != s_year_sel) continue;
        if (s_model_count < 64) s_model_indices[s_model_count++] = i;
    }
}

// ─── Draw helpers ────────────────────────────────────────────────────────────

static void draw_panel(SDL_Renderer* ren, TTF_Font* font_lg, const char* title)
{
    ui_dim_overlay(ren);
    ui_rect(ren, PANEL_X+6, PANEL_Y+6, PANEL_W, PANEL_H, {0,0,0,0x80});
    ui_rect(ren, PANEL_X, PANEL_Y, PANEL_W, PANEL_H, {0xF4,0xF4,0xF4,0xFF});
    ui_outline(ren, PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COL_BLUE, 2);
    ui_rect(ren, PANEL_X, PANEL_Y, PANEL_W, HEADER_H, COL_BG_TOP);
    ui_text(ren, font_lg, title, PANEL_X+PANEL_W/2, PANEL_Y+HEADER_H-10, COL_WHITE, 1);
}

static void draw_list_row(SDL_Renderer* ren, TTF_Font* font_md,
                           int idx_in_visible, const char* label,
                           bool focused, bool checked = false)
{
    int ry = LIST_TOP + idx_in_visible * ROW_H;
    SDL_Color bg = focused ? SDL_Color{0xE0,0xEA,0xFF,0xFF} : SDL_Color{0xF4,0xF4,0xF4,0xFF};
    ui_rect(ren, PANEL_X+4, ry, PANEL_W-8, ROW_H-4, bg);
    if (focused) ui_outline(ren, PANEL_X+4, ry, PANEL_W-8, ROW_H-4, COL_FOCUS_RING, 2);
    SDL_Color lc = focused ? COL_BLUE : SDL_Color{0x1A,0x1A,0x1A,0xFF};
    ui_text(ren, font_md, label, LIST_X, ry+ROW_H-12, lc, 0);
    if (checked) {
        ui_text(ren, font_md, "\xe2\x9c\x93", PANEL_X+PANEL_W-48, ry+ROW_H-12,
                {0x00,0x99,0x00,0xFF}, 0);
    }
}

static void draw_hint(SDL_Renderer* ren, TTF_Font* font_sm, const char* hint)
{
    ui_text(ren, font_sm, hint, PANEL_X+PANEL_W/2, PANEL_Y+PANEL_H-10, COL_GRAY, 1);
}

// ─── Main page ───────────────────────────────────────────────────────────────

static void draw_main(SDL_Renderer* ren, TTF_Font* fsm, TTF_Font* fmd, TTF_Font* flg)
{
    draw_panel(ren, flg, "Settings");

    const char* labels[ROW_MAIN_COUNT] = {
        "Improved Multitasking",
        "Improved Remote Control",
        "TV Remote Setup",
    };

    for (int i = 0; i < ROW_MAIN_COUNT; i++) {
        int ry = LIST_TOP + i * (ROW_H + 4);
        bool focused = (i == s_sel);
        SDL_Color bg = focused ? SDL_Color{0xE0,0xEA,0xFF,0xFF} : SDL_Color{0xF8,0xF8,0xF8,0xFF};
        ui_rect(ren, PANEL_X+4, ry, PANEL_W-8, ROW_H, bg);
        if (focused) ui_outline(ren, PANEL_X+4, ry, PANEL_W-8, ROW_H, COL_FOCUS_RING, 2);
        SDL_Color lc = focused ? COL_BLUE : SDL_Color{0x1A,0x1A,0x1A,0xFF};
        ui_text(ren, fmd, labels[i], LIST_X, ry+ROW_H-10, lc, 0);

        if (i == ROW_MULTITASK || i == ROW_REMOTE) {
            bool on = (i == ROW_MULTITASK) ? g_settings.improved_multitasking
                                           : g_settings.improved_remote;
            const char* val = on ? "ON" : "OFF";
            SDL_Color vc = on ? SDL_Color{0x00,0x99,0x00,0xFF}
                              : SDL_Color{0x99,0x00,0x00,0xFF};
            ui_text(ren, fmd, val, PANEL_X+PANEL_W-40, ry+ROW_H-10, vc, 2);
        } else {
            // TV Remote row: show current model key or "Not configured"
            const char* key = g_settings.tv_model_key[0]
                ? g_settings.tv_model_key : "Not configured";
            SDL_Color vc = g_settings.tv_model_key[0]
                ? SDL_Color{0x00,0x99,0x00,0xFF}
                : SDL_Color{0x88,0x88,0x88,0xFF};
            // Truncate if long
            char buf[48];
            strncpy(buf, key, 46); buf[46] = '\0';
            ui_text(ren, fsm, buf, PANEL_X+PANEL_W-40, ry+ROW_H-10, vc, 2);
            ui_text(ren, fsm, "\xe2\x96\xba", PANEL_X+PANEL_W-12, ry+ROW_H-10, lc, 2);
        }

        if (i < ROW_MAIN_COUNT-1)
            ui_rect(ren, PANEL_X+16, ry+ROW_H, PANEL_W-32, 1, COL_TOOLBAR_LINE);
    }
    draw_hint(ren, fsm, "\xe2\x96\xb2\xe2\x96\xbc: select  A/\xe2\x97\x84\xe2\x96\xba: toggle  B: save & close");
    SDL_RenderPresent(ren);
}

// ─── Brand page ──────────────────────────────────────────────────────────────

static void draw_brand(SDL_Renderer* ren, TTF_Font* fsm, TTF_Font* fmd, TTF_Font* flg)
{
    draw_panel(ren, flg, "Select Brand");
    for (int v = 0; v < ROWS_VISIBLE && (s_scroll+v) < s_brand_count; v++) {
        int idx = s_scroll + v;
        bool focused = (idx == s_sel);
        bool checked = (strcmp(s_brands[idx], s_brand_sel) == 0);
        draw_list_row(ren, fmd, v, s_brands[idx], focused, checked);
    }
    char pg[32]; snprintf(pg, sizeof(pg), "%d/%d", s_sel+1, s_brand_count);
    ui_text(ren, fsm, pg, PANEL_X+PANEL_W-16, PANEL_Y+HEADER_H+4, COL_GRAY, 2);
    draw_hint(ren, fsm, "\xe2\x96\xb2\xe2\x96\xbc: scroll  A: select brand  B: back");
    SDL_RenderPresent(ren);
}

// ─── Year page ───────────────────────────────────────────────────────────────

static void draw_year(SDL_Renderer* ren, TTF_Font* fsm, TTF_Font* fmd, TTF_Font* flg)
{
    char hdr[128]; snprintf(hdr, sizeof(hdr), "%s – Select Year", s_brand_sel);
    draw_panel(ren, flg, hdr);
    for (int v = 0; v < ROWS_VISIBLE && (s_scroll+v) < s_year_count; v++) {
        int idx = s_scroll + v;
        bool focused = (idx == s_sel);
        bool checked = (s_years[idx] == s_year_sel);
        char buf[8]; snprintf(buf, sizeof(buf), "%d", s_years[idx]);
        draw_list_row(ren, fmd, v, buf, focused, checked);
    }
    draw_hint(ren, fsm, "\xe2\x96\xb2\xe2\x96\xbc: scroll  A: select year  B: back");
    SDL_RenderPresent(ren);
}

// ─── Model page ──────────────────────────────────────────────────────────────

static void draw_model(SDL_Renderer* ren, TTF_Font* fsm, TTF_Font* fmd, TTF_Font* flg)
{
    char hdr[128]; snprintf(hdr, sizeof(hdr), "%s %d – Select Model", s_brand_sel, s_year_sel);
    draw_panel(ren, flg, hdr);
    for (int v = 0; v < ROWS_VISIBLE && (s_scroll+v) < s_model_count; v++) {
        int idx = s_scroll + v;
        const TVRemoteModel* m = model_registry_get(s_model_indices[idx]);
        bool focused = (idx == s_sel);
        // Is this the currently saved model?
        char k[128]; settings_make_key(m->brand, m->year, m->model, k, sizeof(k));
        bool checked = (strcmp(k, g_settings.tv_model_key) == 0);
        draw_list_row(ren, fmd, v, m->model, focused, checked);
    }
    draw_hint(ren, fsm, "\xe2\x96\xb2\xe2\x96\xbc: scroll  A: select model  B: back");
    SDL_RenderPresent(ren);
}

// ─── Detecting page ──────────────────────────────────────────────────────────

static int s_detect_anim = 0;

static void draw_detecting(SDL_Renderer* ren, TTF_Font* fsm, TTF_Font* fmd, TTF_Font* flg)
{
    draw_panel(ren, flg, "Auto-Detecting TV");
    const char* spinner[] = {"|","/"," -","\\"};
    char buf[64];
    snprintf(buf, sizeof(buf), "Probing HDMI-CEC...  %s", spinner[(s_detect_anim/8)%4]);
    s_detect_anim++;
    ui_text(ren, fmd, buf, PANEL_X+PANEL_W/2, PANEL_Y+HEADER_H+120, COL_ADDR_TEXT, 1);
    ui_text(ren, fsm,
        "The WiiU is sending CEC commands to detect your TV brand and model.\n"
        "Make sure your TV is connected via HDMI and powered on.",
        PANEL_X+PANEL_W/2, PANEL_Y+HEADER_H+180, COL_GRAY, 1);

    if (tv_detect_done()) {
        s_page = PAGE_DETECT_RESULT;
        s_sel  = 0;
        const TVDetectResult* res = tv_detect_result();
        if (res->found && res->model) {
            const TVRemoteModel* m = res->model;
            settings_make_key(m->brand, m->year, m->model,
                              g_settings.tv_model_key, sizeof(g_settings.tv_model_key));
            tv_remote_set_model(m);
            g_settings.detect_state = 2;
        } else {
            g_settings.detect_state = 3;
        }
    }
    draw_hint(ren, fsm, "B: cancel");
    SDL_RenderPresent(ren);
}

// ─── Detect result page ──────────────────────────────────────────────────────

static void draw_detect_result(SDL_Renderer* ren, TTF_Font* fsm, TTF_Font* fmd, TTF_Font* flg)
{
    draw_panel(ren, flg, "Detection Result");
    if (g_settings.detect_state == 2) {
        const TVRemoteModel* m = tv_remote_get_model();
        char buf[128];
        snprintf(buf, sizeof(buf), "Found: %s %s (%d)",
                 m ? m->brand : "?", m ? m->model : "?", m ? m->year : 0);
        ui_text(ren, fmd, buf,    PANEL_X+PANEL_W/2, LIST_TOP+60, {0x00,0x88,0x00,0xFF}, 1);
        ui_text(ren, fsm, "Model saved. Press A to accept or B to pick manually.",
                PANEL_X+PANEL_W/2, LIST_TOP+120, COL_GRAY, 1);
    } else {
        ui_text(ren, fmd, "No TV detected via CEC.",
                PANEL_X+PANEL_W/2, LIST_TOP+60, {0xCC,0x00,0x00,0xFF}, 1);
        ui_text(ren, fsm, "Check HDMI cable and TV CEC setting, or pick manually.",
                PANEL_X+PANEL_W/2, LIST_TOP+120, COL_GRAY, 1);
    }
    draw_hint(ren, fsm, "A: accept & close  B: pick manually");
    SDL_RenderPresent(ren);
}

// ─── Public draw ─────────────────────────────────────────────────────────────

void settings_draw(SDL_Renderer* ren, TTF_Font* fsm, TTF_Font* fmd, TTF_Font* flg)
{
    switch (s_page) {
        case PAGE_MAIN:          draw_main(ren, fsm, fmd, flg); break;
        case PAGE_BRAND:         draw_brand(ren, fsm, fmd, flg); break;
        case PAGE_YEAR:          draw_year(ren, fsm, fmd, flg); break;
        case PAGE_MODEL:         draw_model(ren, fsm, fmd, flg); break;
        case PAGE_DETECTING:     draw_detecting(ren, fsm, fmd, flg); break;
        case PAGE_DETECT_RESULT: draw_detect_result(ren, fsm, fmd, flg); break;
    }
}

// ─── Input helpers ───────────────────────────────────────────────────────────

static void list_nav(int count, uint32_t btn)
{
    if (btn & VPAD_BUTTON_UP) {
        s_sel--;
        if (s_sel < 0) s_sel = count - 1;
        if (s_sel < s_scroll) s_scroll = s_sel;
        if (s_scroll > s_sel - ROWS_VISIBLE + 1)
            s_scroll = (s_sel - ROWS_VISIBLE + 1 > 0) ? s_sel - ROWS_VISIBLE + 1 : 0;
    }
    if (btn & VPAD_BUTTON_DOWN) {
        s_sel++;
        if (s_sel >= count) s_sel = 0;
        if (s_sel >= s_scroll + ROWS_VISIBLE) s_scroll = s_sel - ROWS_VISIBLE + 1;
        if (s_scroll < 0) s_scroll = 0;
    }
}

// ─── Public input ────────────────────────────────────────────────────────────

bool settings_handle_input(VPADStatus* vpad)
{
    uint32_t btn = vpad->trigger;

    switch (s_page) {

    // ── Main ──────────────────────────────────────────────────────────────────
    case PAGE_MAIN:
        if (btn & VPAD_BUTTON_UP)   s_sel = (s_sel > 0) ? s_sel-1 : ROW_MAIN_COUNT-1;
        if (btn & VPAD_BUTTON_DOWN) s_sel = (s_sel < ROW_MAIN_COUNT-1) ? s_sel+1 : 0;

        if ((btn & VPAD_BUTTON_A) || (btn & VPAD_BUTTON_LEFT) || (btn & VPAD_BUTTON_RIGHT)) {
            if (s_sel == ROW_MULTITASK)
                g_settings.improved_multitasking = !g_settings.improved_multitasking;
            else if (s_sel == ROW_REMOTE)
                g_settings.improved_remote = !g_settings.improved_remote;
            else if (s_sel == ROW_TV_SETUP && (btn & VPAD_BUTTON_A)) {
                // Enter brand picker; also offer auto-detect via SELECT
                build_brands();
                s_page = PAGE_BRAND; s_sel = 0; s_scroll = 0;
            }
        }
        // SELECT on main page → start CEC detect
        if ((btn & VPAD_BUTTON_MINUS) && s_sel == ROW_TV_SETUP) {
            tv_detect_start();
            s_page = PAGE_DETECTING;
            s_detect_anim = 0;
        }
        if (btn & VPAD_BUTTON_B) { settings_save(); s_sel = 0; s_page = PAGE_MAIN; return false; }
        break;

    // ── Brand ─────────────────────────────────────────────────────────────────
    case PAGE_BRAND:
        list_nav(s_brand_count, btn);
        if (btn & VPAD_BUTTON_A) {
            strncpy(s_brand_sel, s_brands[s_sel], sizeof(s_brand_sel)-1);
            build_years();
            s_page = PAGE_YEAR; s_sel = 0; s_scroll = 0;
        }
        if (btn & VPAD_BUTTON_B) { s_page = PAGE_MAIN; s_sel = ROW_TV_SETUP; }
        break;

    // ── Year ──────────────────────────────────────────────────────────────────
    case PAGE_YEAR:
        list_nav(s_year_count, btn);
        if (btn & VPAD_BUTTON_A) {
            s_year_sel = s_years[s_sel];
            build_models();
            s_page = PAGE_MODEL; s_sel = 0; s_scroll = 0;
        }
        if (btn & VPAD_BUTTON_B) {
            build_brands();
            s_page = PAGE_BRAND; s_sel = 0; s_scroll = 0;
        }
        break;

    // ── Model ─────────────────────────────────────────────────────────────────
    case PAGE_MODEL:
        list_nav(s_model_count, btn);
        if (btn & VPAD_BUTTON_A) {
            const TVRemoteModel* m = model_registry_get(s_model_indices[s_sel]);
            settings_make_key(m->brand, m->year, m->model,
                              g_settings.tv_model_key, sizeof(g_settings.tv_model_key));
            tv_remote_set_model(m);
            settings_save();
            s_page = PAGE_MAIN; s_sel = ROW_TV_SETUP;
        }
        if (btn & VPAD_BUTTON_B) {
            build_years();
            s_page = PAGE_YEAR; s_sel = 0; s_scroll = 0;
        }
        break;

    // ── Detecting ─────────────────────────────────────────────────────────────
    case PAGE_DETECTING:
        // draw_detecting() transitions to RESULT automatically
        if (btn & VPAD_BUTTON_B) { s_page = PAGE_MAIN; s_sel = ROW_TV_SETUP; }
        break;

    // ── Detect result ─────────────────────────────────────────────────────────
    case PAGE_DETECT_RESULT:
        if (btn & VPAD_BUTTON_A) {
            // Accept the auto-detected model and close settings
            settings_save();
            s_page = PAGE_MAIN; s_sel = ROW_TV_SETUP;
            return false;
        }
        if (btn & VPAD_BUTTON_B) {
            // Fall through to manual picker
            build_brands();
            s_page = PAGE_BRAND; s_sel = 0; s_scroll = 0;
        }
        break;
    }

    return true;
}
