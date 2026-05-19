#include "settings.h"
#include "plugin_installer.h"
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
        else if (strncmp(line, "tv_model_key=", 13) == 0)
            snprintf(g_settings.tv_model_key, sizeof(g_settings.tv_model_key), "%.*s",
                     (int)(sizeof(g_settings.tv_model_key)-1), line+13);
    }
    fclose(f);
    apply_model_key(g_settings.tv_model_key);
}

void settings_save()
{
    FILE* f = fopen(SETTINGS_PATH, "w");
    if (!f) return;
    fprintf(f, "improved_multitasking=%d\n", g_settings.improved_multitasking ? 1 : 0);
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
    // TV Remote setup wizard (runs after model is confirmed)
    PAGE_SETUP_INTRO,      // Step 1: Welcome + what we're about to do
    PAGE_SETUP_POINT,      // Step 2: Point GamePad at TV
    PAGE_SETUP_TEST,       // Step 3: Press TV button to test IR
    PAGE_SETUP_RESULT,     // Step 4: Did TV respond? Yes / No
    PAGE_SETUP_INSTALL,    // Step 5: Install plugin explanation
    PAGE_SETUP_INSTALLING, // Step 6: Installing... spinner
    PAGE_SETUP_DONE,       // Step 7: Done / reboot notice
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

void settings_open()
{
    s_page   = PAGE_MAIN;
    s_sel    = 0;
    s_scroll = 0;
}


// Wizard state
static int  s_setup_anim         = 0;   // spinner frame counter
static bool s_setup_test_fired   = false;
static bool s_setup_tv_responded = false;  // user answered yes/no
static int  s_setup_install_result = 0;   // 0=pending 1=ok -1=fail -2=no binary
static int  s_setup_no_count     = 0;   // how many times user said "no"

// Install progress (written by callback, read by draw_setup_installing)
static double s_install_pct      = 0.0;
static char   s_install_msg[64]  = {};

static void plugin_progress_cb(const char* msg, double pct)
{
    if (msg) strncpy(s_install_msg, msg, sizeof(s_install_msg)-1);
    s_install_pct = pct;
}

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
#define ROW_TV_SETUP   1
#define ROW_MAIN_COUNT 2

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

// Full-screen settings constants
#define SETTINGS_HEADER_H  72
#define SETTINGS_ROW_H    100
#define SETTINGS_ROW_PAD   24
#define SETTINGS_ROW_X     48
#define SETTINGS_ROW_W    (TV_W - 96)
#define SETTINGS_ROW_START 100

static void draw_main(SDL_Renderer* ren, TTF_Font* fsm, TTF_Font* fmd, TTF_Font* flg)
{
    // Full screen white background
    ui_rect(ren, 0, 0, TV_W, TV_H, {0xFF,0xFF,0xFF,0xFF});

    // White header box with blue bottom border
    ui_rect(ren, 0, 0, TV_W, SETTINGS_HEADER_H, {0xFF,0xFF,0xFF,0xFF});
    ui_rect(ren, 0, SETTINGS_HEADER_H, TV_W, 3, COL_BLUE);
    // "Wiiu Settings" title left-aligned with icon feel
    ui_text(ren, flg, "Wiiu Settings", 40, SETTINGS_HEADER_H - 18, COL_ADDR_TEXT, 0);
    // B to close hint on right
    ui_text(ren, fsm, "B: save & close", TV_W - 16, SETTINGS_HEADER_H - 10, COL_GRAY, 2);

    struct RowInfo {
        const char* title;
        const char* description;
    };
    const RowInfo rows[ROW_MAIN_COUNT] = {
        {
            "Improved Multitasking",
            "Keeps tabs open when switching apps and restores your session on next launch"
        },
        {
            "TV Remote Setup",
            "Configure your TV model so the GamePad TV button controls your TV via HDMI-CEC"
        },
    };

    for (int i = 0; i < ROW_MAIN_COUNT; i++) {
        int ry = SETTINGS_ROW_START + i * (SETTINGS_ROW_H + SETTINGS_ROW_PAD);
        bool focused = (i == s_sel);

        // Row background
        SDL_Color bg = focused
            ? SDL_Color{0xE8,0xF0,0xFF,0xFF}
            : SDL_Color{0xF8,0xF8,0xF8,0xFF};
        ui_rect(ren, SETTINGS_ROW_X, ry, SETTINGS_ROW_W, SETTINGS_ROW_H, bg);

        // Left accent bar when focused
        if (focused)
            ui_rect(ren, SETTINGS_ROW_X, ry, 5, SETTINGS_ROW_H, COL_BLUE);

        // Subtle border
        SDL_Color border = focused ? COL_BLUE : SDL_Color{0xE0,0xE0,0xE0,0xFF};
        ui_outline(ren, SETTINGS_ROW_X, ry, SETTINGS_ROW_W, SETTINGS_ROW_H, border, focused ? 2 : 1);

        int text_x = SETTINGS_ROW_X + 24;
        SDL_Color title_c = focused ? COL_BLUE : COL_ADDR_TEXT;

        // Title
        ui_text(ren, fmd, rows[i].title, text_x, ry + 36, title_c, 0);

        // Description
        ui_text(ren, fsm, rows[i].description, text_x, ry + 68, COL_GRAY, 0);

        // Right-side value / arrow
        if (i == ROW_MULTITASK) {
            bool on = g_settings.improved_multitasking;
            // Toggle pill
            SDL_Color pill_bg = on
                ? SDL_Color{0x00,0x99,0x00,0xFF}
                : SDL_Color{0xCC,0xCC,0xCC,0xFF};
            ui_rect(ren, SETTINGS_ROW_X + SETTINGS_ROW_W - 84, ry + 30, 68, 36, pill_bg);
            ui_text(ren, fmd, on ? "ON" : "OFF",
                    SETTINGS_ROW_X + SETTINGS_ROW_W - 50,
                    ry + 56, COL_WHITE, 1);
        } else {
            // TV Setup: show configured model or "Set up" + arrow
            const char* key = g_settings.tv_model_key[0]
                ? g_settings.tv_model_key : "Not set up";
            SDL_Color vc = g_settings.tv_model_key[0]
                ? SDL_Color{0x00,0x88,0x00,0xFF}
                : SDL_Color{0x88,0x88,0x88,0xFF};
            char buf[40]; strncpy(buf, key, 38); buf[38] = '\0';
            ui_text(ren, fsm, buf,
                    SETTINGS_ROW_X + SETTINGS_ROW_W - 16,
                    ry + 42, vc, 2);
            // Arrow indicator
            ui_text(ren, fmd, "\xe2\x96\xba",
                    SETTINGS_ROW_X + SETTINGS_ROW_W - 16,
                    ry + 72, focused ? COL_BLUE : COL_GRAY, 2);
        }
    }

    // Bottom hint bar
    ui_rect(ren, 0, TV_H - 40, TV_W, 40, {0xF2,0xF2,0xF2,0xFF});
    ui_rect(ren, 0, TV_H - 40, TV_W, 1, {0xE0,0xE0,0xE0,0xFF});
    ui_text(ren, fsm,
            "\xe2\x96\xb2\xe2\x96\xbc: move  A: select / toggle  B: save & close",
            TV_W/2, TV_H - 12, COL_GRAY, 1);

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
    char hdr[128]; snprintf(hdr, sizeof(hdr), "%s \xe2\x80\x93 Select Year", s_brand_sel);
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
    char hdr[128]; snprintf(hdr, sizeof(hdr), "%s %d \xe2\x80\x93 Select Model", s_brand_sel, s_year_sel);
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


// ─── TV Remote Setup Wizard ───────────────────────────────────────────────────

static void draw_setup_step(SDL_Renderer* ren, TTF_Font* fsm, TTF_Font* fmd,
                             TTF_Font* flg, int step, int total,
                             const char* title, const char* body1,
                             const char* body2, const char* hint,
                             const char* btn_a, const char* btn_b)
{
    ui_dim_overlay(ren);

    // Panel
    ui_rect(ren, PANEL_X+6, PANEL_Y+6, PANEL_W, PANEL_H, {0,0,0,0x80});
    ui_rect(ren, PANEL_X, PANEL_Y, PANEL_W, PANEL_H, {0xF4,0xF4,0xF4,0xFF});
    ui_outline(ren, PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COL_BLUE, 2);

    // Header
    ui_rect(ren, PANEL_X, PANEL_Y, PANEL_W, HEADER_H, COL_BG_TOP);
    char hdr[80];
    snprintf(hdr, sizeof(hdr), "TV Remote Setup  (%d / %d)", step, total);
    ui_text(ren, flg, hdr, PANEL_X+PANEL_W/2, PANEL_Y+HEADER_H-10, COL_WHITE, 1);

    // Step dots
    int dot_y = PANEL_Y + HEADER_H + 18;
    int dot_spacing = 24;
    int dots_w = total * dot_spacing;
    int dot_x = PANEL_X + PANEL_W/2 - dots_w/2;
    for (int i = 0; i < total; i++) {
        SDL_Color dc = (i+1 == step) ? COL_BLUE : COL_GRAY;
        ui_rect(ren, dot_x + i*dot_spacing + 4, dot_y, 12, 12, dc);
    }

    // Title
    ui_text(ren, fmd, title,  PANEL_X+PANEL_W/2, PANEL_Y+HEADER_H+60, COL_ADDR_TEXT, 1);

    // Body lines
    if (body1 && body1[0])
        ui_text(ren, fmd, body1, PANEL_X+PANEL_W/2, PANEL_Y+HEADER_H+110, COL_ADDR_TEXT, 1);
    if (body2 && body2[0])
        ui_text(ren, fmd, body2, PANEL_X+PANEL_W/2, PANEL_Y+HEADER_H+150, COL_GRAY, 1);

    // Action buttons at bottom
    if (btn_a && btn_a[0]) {
        ui_rect(ren, PANEL_X+60, PANEL_Y+PANEL_H-80, (PANEL_W-160)/2, 44,
                {0x42,0x85,0xF4,0xFF});
        ui_text(ren, fmd, btn_a,
                PANEL_X+60 + (PANEL_W-160)/4, PANEL_Y+PANEL_H-52,
                COL_WHITE, 1);
    }
    if (btn_b && btn_b[0]) {
        ui_rect(ren, PANEL_X+PANEL_W/2+20, PANEL_Y+PANEL_H-80, (PANEL_W-160)/2, 44,
                {0xEE,0xEE,0xEE,0xFF});
        ui_text(ren, fmd, btn_b,
                PANEL_X+PANEL_W/2+20 + (PANEL_W-160)/4, PANEL_Y+PANEL_H-52,
                COL_ADDR_TEXT, 1);
    }

    // Bottom hint
    if (hint && hint[0])
        ui_text(ren, fsm, hint, PANEL_X+PANEL_W/2, PANEL_Y+PANEL_H-10, COL_GRAY, 1);

    SDL_RenderPresent(ren);
}

static void draw_setup_intro(SDL_Renderer* ren, TTF_Font* fsm, TTF_Font* fmd, TTF_Font* flg)
{
    const TVRemoteModel* m = tv_remote_get_model();
    char brand_model[80] = "Unknown model";
    if (m) snprintf(brand_model, sizeof(brand_model), "%s %s (%d)", m->brand, m->model, m->year);

    draw_setup_step(ren, fsm, fmd, flg, 1, 5,
        "Make Your GamePad a Real TV Remote",
        brand_model,
        "We will test the IR signal, then install a system plugin",
        "A: Let\'s go",
        "A  Let\'s go", nullptr);
}

static void draw_setup_point(SDL_Renderer* ren, TTF_Font* fsm, TTF_Font* fmd, TTF_Font* flg)
{
    draw_setup_step(ren, fsm, fmd, flg, 2, 5,
        "Point the GamePad at your TV",
        "Hold the GamePad so its top edge faces your TV",
        "The IR emitter is on the top edge of the GamePad",
        "A: Ready",
        "A  Ready", nullptr);
}

static void draw_setup_test(SDL_Renderer* ren, TTF_Font* fsm, TTF_Font* fmd, TTF_Font* flg)
{
    s_setup_anim++;
    const char* pulses[] = {
        "Sending IR  |||||",
        "Sending IR  ||||",
        "Sending IR  |||",
        "Sending IR  ||||",
    };
    const char* anim = pulses[(s_setup_anim/8) % 4];

    // Before fired: tell user where the button is and to press A when done
    // After fired: show animation and confirm to advance
    draw_setup_step(ren, fsm, fmd, flg, 3, 5,
        "Fire IR at your TV",
        s_setup_test_fired
            ? anim
            : "Press the TV button (bottom-left of GamePad face, beside \xe2\x96\xbc)",
        s_setup_test_fired
            ? "Watch your TV \xe2\x80\x94 it should turn on or off"
            : "Then press A to continue",
        "TV button fires IR  |  A: continue",
        "A  Done, next", nullptr);
}

static void draw_setup_result(SDL_Renderer* ren, TTF_Font* fsm, TTF_Font* fmd, TTF_Font* flg)
{
    char sub[80] = "";
    if (s_setup_no_count > 0)
        snprintf(sub, sizeof(sub), "(Attempt %d \xe2\x80\x94 try pointing closer to the TV)", s_setup_no_count+1);

    draw_setup_step(ren, fsm, fmd, flg, 4, 5,
        "Did your TV respond?",
        sub,
        "A = Yes, it worked!     B = No, try again",
        "A: Yes    B: No, try again",
        "A  Yes!", "B  Try again");
}

static void draw_setup_install(SDL_Renderer* ren, TTF_Font* fsm, TTF_Font* fmd, TTF_Font* flg)
{
    draw_setup_step(ren, fsm, fmd, flg, 5, 5,
        "Install System Plugin",
        "This installs WaveBrowserRemote into Aroma so the",
        "TV button works even when Wave Browser is closed",
        "A: Install",
        "A  Install Plugin", nullptr);
}

static void draw_setup_installing(SDL_Renderer* ren, TTF_Font* fsm, TTF_Font* fmd, TTF_Font* flg)
{
    s_setup_anim++;
    const char* spin[] = {"|", "/", "-", "\\"};

    // Show download progress if available, otherwise generic spinner
    char buf[64];
    if (s_install_msg[0] && s_install_pct > 0.0)
        snprintf(buf, sizeof(buf), "%s  %.0f%%", s_install_msg, s_install_pct);
    else if (s_install_msg[0])
        snprintf(buf, sizeof(buf), "%s  %s", s_install_msg, spin[(s_setup_anim/6)%4]);
    else
        snprintf(buf, sizeof(buf), "Installing...  %s", spin[(s_setup_anim/6)%4]);

    draw_setup_step(ren, fsm, fmd, flg, 5, 5,
        buf, "", "", "", nullptr, nullptr);
}

static void draw_setup_done(SDL_Renderer* ren, TTF_Font* fsm, TTF_Font* fmd, TTF_Font* flg)
{
    const char* msg  = "";
    const char* sub  = "";

    if (s_setup_install_result == 1) {
        msg = "Plugin installed successfully!";
        sub = "Reboot your WiiU to activate \xe2\x80\x94 TV button will work everywhere";
    } else if (s_setup_install_result == -1) {
        msg = "Download or install failed";
        sub = "Check your internet connection and that Aroma is installed";
    } else if (s_setup_install_result == -2) {
        msg = "Plugin binary not available in this build";
        sub = "TV button works while Wave Browser is open";
    } else {
        msg = "Setup complete";
        sub = "";
    }

    draw_setup_step(ren, fsm, fmd, flg, 5, 5,
        msg, sub, "", "A: Done", "A  Done", nullptr);
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
        case PAGE_SETUP_INTRO:      draw_setup_intro(ren, fsm, fmd, flg);     break;
        case PAGE_SETUP_POINT:      draw_setup_point(ren, fsm, fmd, flg);     break;
        case PAGE_SETUP_TEST:       draw_setup_test(ren, fsm, fmd, flg);      break;
        case PAGE_SETUP_RESULT:     draw_setup_result(ren, fsm, fmd, flg);    break;
        case PAGE_SETUP_INSTALL:    draw_setup_install(ren, fsm, fmd, flg);   break;
        case PAGE_SETUP_INSTALLING: draw_setup_installing(ren, fsm, fmd, flg);break;
        case PAGE_SETUP_DONE:       draw_setup_done(ren, fsm, fmd, flg);      break;
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

bool settings_handle_input(VPADStatus* vpad, bool tp_pressed, int tp_x, int tp_y)
{
    uint32_t btn = vpad->trigger;

    switch (s_page) {

    // ── Main ──────────────────────────────────────────────────────────────────
    case PAGE_MAIN:
        // Navigate rows with d-pad
        if (btn & VPAD_BUTTON_UP)   s_sel = (s_sel > 0) ? s_sel - 1 : ROW_MAIN_COUNT - 1;
        if (btn & VPAD_BUTTON_DOWN) s_sel = (s_sel < ROW_MAIN_COUNT - 1) ? s_sel + 1 : 0;

        // Multitasking toggle — A, left, or right all toggle it
        if (s_sel == ROW_MULTITASK &&
            ((btn & VPAD_BUTTON_A) || (btn & VPAD_BUTTON_LEFT) || (btn & VPAD_BUTTON_RIGHT)))
        {
            g_settings.improved_multitasking = !g_settings.improved_multitasking;
        }

        // TV Remote Setup — A enters brand picker
        if (s_sel == ROW_TV_SETUP && (btn & VPAD_BUTTON_A)) {
            build_brands();
            s_page = PAGE_BRAND; s_sel = 0; s_scroll = 0;
        }

        // SELECT on TV Setup row → auto-detect via CEC
        if (s_sel == ROW_TV_SETUP && (btn & VPAD_BUTTON_MINUS)) {
            tv_detect_start();
            s_page = PAGE_DETECTING;
            s_detect_anim = 0;
        }

        // Touch — tap a row to select it; tap an already-selected row to activate it
        if (tp_pressed) {
            for (int i = 0; i < ROW_MAIN_COUNT; i++) {
                int ry = SETTINGS_ROW_START + i * (SETTINGS_ROW_H + SETTINGS_ROW_PAD);
                if (tp_x >= SETTINGS_ROW_X &&
                    tp_x <  SETTINGS_ROW_X + SETTINGS_ROW_W &&
                    tp_y >= ry &&
                    tp_y <  ry + SETTINGS_ROW_H)
                {
                    if (i == ROW_MULTITASK) {
                        // Always toggle on tap
                        g_settings.improved_multitasking = !g_settings.improved_multitasking;
                    } else if (i == ROW_TV_SETUP) {
                        // Always open brand picker on tap
                        build_brands();
                        s_page = PAGE_BRAND; s_sel = 0; s_scroll = 0;
                    }
                    s_sel = i;
                    break;
                }
            }
        }

        // B — save and close settings
        if (btn & VPAD_BUTTON_B) { settings_save(); return false; }
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
            // Launch setup wizard
            s_setup_test_fired   = false;
            s_setup_tv_responded = false;
            s_setup_no_count     = 0;
            s_setup_anim         = 0;
            s_page = PAGE_SETUP_INTRO;
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
            settings_save();
            s_setup_test_fired   = false;
            s_setup_tv_responded = false;
            s_setup_no_count     = 0;
            s_setup_anim         = 0;
            s_page = PAGE_SETUP_INTRO;
        }
        if (btn & VPAD_BUTTON_B) {
            build_brands();
            s_page = PAGE_BRAND; s_sel = 0; s_scroll = 0;
        }
        break;

    case PAGE_SETUP_INTRO:
        if (btn & VPAD_BUTTON_A) { s_page = PAGE_SETUP_POINT; }
        break;

    case PAGE_SETUP_POINT:
        if (btn & VPAD_BUTTON_A) { s_setup_test_fired = false; s_page = PAGE_SETUP_TEST; }
        break;

    case PAGE_SETUP_TEST: {
        // VPAD_BUTTON_TV is often consumed by the OS before the app sees it in
        // trigger, so we accept either the TV button OR A to advance.
        // If TV button fires first it marks s_setup_test_fired; A confirms and moves on.
        bool tv_pressed = (btn & VPAD_BUTTON_TV) != 0;
        bool a_pressed  = (btn & VPAD_BUTTON_A)  != 0;

        if (tv_pressed) {
            // Mark fired — user will see the animation, then press A to confirm
            const TVRemoteModel* m = tv_remote_get_model();
            if (m && m->ir[TVBTN_POWER].code)
                s_setup_test_fired = true;
            // If TV button is visible to us, advance immediately to result
            s_page = PAGE_SETUP_RESULT;
        } else if (a_pressed) {
            // User says "I pressed the TV button" — treat as fired and advance
            s_setup_test_fired = true;
            s_page = PAGE_SETUP_RESULT;
        }
        break;
    }

    case PAGE_SETUP_RESULT:
        if (btn & VPAD_BUTTON_A) {
            // Yes — TV responded, move to install step
            s_setup_tv_responded = true;
            s_page = PAGE_SETUP_INSTALL;
        }
        if (btn & VPAD_BUTTON_B) {
            // No — go back to test, increment attempt counter
            s_setup_no_count++;
            s_setup_test_fired = false;
            s_page = PAGE_SETUP_TEST;
        }
        break;

    case PAGE_SETUP_INSTALL:
        if (btn & VPAD_BUTTON_A) {
            const TVRemoteModel* m = tv_remote_get_model();
            if (m) plugin_write_config(m);
            s_setup_anim = 0;
            s_page = PAGE_SETUP_INSTALLING;
        }
        break;

    case PAGE_SETUP_INSTALLING:
        // plugin_install() is blocking — runs download + extract this frame
        s_install_pct = 0.0;
        strncpy(s_install_msg, "Connecting to GitHub...", sizeof(s_install_msg)-1);
        s_setup_install_result = plugin_install(plugin_progress_cb);
        s_page = PAGE_SETUP_DONE;
        break;

    case PAGE_SETUP_DONE:
        if (btn & VPAD_BUTTON_A) {
            s_page = PAGE_MAIN;
            s_sel  = ROW_TV_SETUP;
        }
        break;

    }

    return true;
}
