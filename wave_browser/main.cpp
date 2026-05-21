#include <proc_ui/procui.h>
#include <padscore/wpad.h>
#include <padscore/kpad.h>
#include <coreinit/foreground.h>
#include <coreinit/memdefaultheap.h>
#include <sysapp/launch.h>
#include <vpad/input.h>
#include <nn/swkbd.h>
#include <curl/curl.h>
#include <sndcore2/core.h>

#include "font_data.h"
#include "settings.h"
#include "ui_common.h"
#include "webkit_engine.h"

// WiiU Pro Controller — extensionType value and button masks.
// Values from devkitPro WUT padscore/kpad.h (Classic Controller Pro layout).
// Pro Controller data lives in kpad.classic (same KPADClassicStatus struct).
#ifndef WPAD_EXT_PRO_CONTROLLER
#  define WPAD_EXT_PRO_CONTROLLER    31
#endif
#ifndef WPAD_CLASSIC_BUTTON_UP
#  define WPAD_CLASSIC_BUTTON_UP     0x0001
#  define WPAD_CLASSIC_BUTTON_LEFT   0x0002
#  define WPAD_CLASSIC_BUTTON_ZR     0x0004
#  define WPAD_CLASSIC_BUTTON_X      0x0008
#  define WPAD_CLASSIC_BUTTON_A      0x0010
#  define WPAD_CLASSIC_BUTTON_Y      0x0020
#  define WPAD_CLASSIC_BUTTON_B      0x0040
#  define WPAD_CLASSIC_BUTTON_ZL     0x0080
#  define WPAD_CLASSIC_BUTTON_R      0x0200
#  define WPAD_CLASSIC_BUTTON_PLUS   0x0400
#  define WPAD_CLASSIC_BUTTON_HOME   0x0800
#  define WPAD_CLASSIC_BUTTON_MINUS  0x1000
#  define WPAD_CLASSIC_BUTTON_L      0x2000
#  define WPAD_CLASSIC_BUTTON_DOWN   0x4000
#  define WPAD_CLASSIC_BUTTON_RIGHT  0x8000
#endif

#include <SDL.h>
#include <SDL_ttf.h>
#include "unzip.h"

#include <sys/stat.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <math.h>

// ─── Install / update paths ───────────────────────────────────────────────────

#define INSTALL_DIR     "fs:/vol/external01/wiiu/apps/WaveBrowser"
#define INSTALL_WUHB    "fs:/vol/external01/wiiu/apps/WaveBrowser/WaveBrowser.wuhb"
#define INSTALL_META    "fs:/vol/external01/wiiu/apps/WaveBrowser/meta.xml"
#define RUN_ID_PATH     "fs:/vol/external01/wiiu/apps/WaveBrowser/wave-browser-run-id.txt"
#define SESSION_PATH    "fs:/vol/external01/wiiu/apps/WaveBrowser/wave-browser-session.cfg"
#define ZIP_TMP_PATH    "fs:/vol/external01/wave-browser-update.zip"
#define ZIP_FOLDER_PFX  "WaveBrowser/"

#define MAX_UPDATE_ATTEMPTS 3

#define ACTIONS_API_URL \
    "https://api.github.com/repos/baldbuffalo/wave-browser/actions/runs" \
    "?branch=main&status=success&per_page=1"

#define ARTIFACT_ZIP_URL \
    "https://nightly.link/baldbuffalo/wave-browser/workflows/build.yml/main/WaveBrowser.zip"

// ─── Tab constants ────────────────────────────────────────────────────────────

#define MAX_TABS  8
#define MAX_URL   512

#define TOOLBAR_H    48
#define TAB_BAR_H    36
#define TAB_W       180
#define TAB_H        34
#define ADDR_BAR_X  120
#define ADDR_BAR_W  860
#define ADDR_BAR_H   32
#define ADDR_BAR_Y    8
#define BTN_SIZE     36
#define BTN_BACK_X    8
#define BTN_FWD_X    52
#define BTN_RELOAD_X 96
#define CONTENT_Y   (TOOLBAR_H + TAB_BAR_H)
#define CONTENT_H   (TV_H - CONTENT_Y)

#define GEAR_BTN_X   (TV_W - 44)
#define GEAR_BTN_Y    6
#define GEAR_BTN_SIZE 36

#define SWKBD_WORK_SIZE 0x20000

#define STICK_DEAD    0.45f
#define STICK_REPEAT  18

// ─── Tab state ────────────────────────────────────────────────────────────────

typedef struct { char url[MAX_URL]; char title[64]; } Tab;

static Tab s_tabs[MAX_TABS];
static int s_tab_count  = 1;
static int s_active_tab = 0;

// ─── UI state ─────────────────────────────────────────────────────────────────

static bool s_in_settings       = false;
static bool s_show_tab_switcher = false;

// ─── Focus ────────────────────────────────────────────────────────────────────

static int s_focus_row = 0;
static int s_focus_col = 3;
static int s_stick_held   = 0;  // GamePad left stick repeat
static int s_wii_stick_held = 0;  // Nunchuk stick repeat

// ─── Touch state ──────────────────────────────────────────────────────────────

// Touch state – updated every frame by poll_touch()
static bool s_tp_touching = false;  // finger is currently down
static bool s_tp_pressed  = false;  // finger JUST went down this frame (use for UI hits)
static bool s_tp_released = false;  // finger JUST lifted this frame
static int  s_tp_x = 0, s_tp_y = 0;          // position when pressed (TV coords)
static int  s_tp_cur_x = 0, s_tp_cur_y = 0;  // live position while held

static void poll_touch(VPADStatus* vpad)
{
    VPADTouchData tp;
    VPADGetTPCalibratedPointEx(VPAD_CHAN_0, VPAD_TP_854X480, &tp, &vpad->tpNormal);

    bool touching = (tp.touched != 0) && (tp.validity == VPAD_VALID);

    // Update live position whenever finger is down
    if (touching) {
        s_tp_cur_x = (int)((float)tp.x * TV_W / DRC_W);
        s_tp_cur_y = (int)((float)tp.y * TV_H / DRC_H);
    }

    s_tp_pressed  = !s_tp_touching && touching;   // went down THIS frame
    s_tp_released = s_tp_touching  && !touching;  // lifted THIS frame

    // Snapshot position at the moment the finger lands — used for all hit tests
    if (s_tp_pressed) {
        s_tp_x = s_tp_cur_x;
        s_tp_y = s_tp_cur_y;
    }

    s_tp_touching = touching;
}

// ─── SDL globals ──────────────────────────────────────────────────────────────

static SDL_Window*   s_window   = nullptr;
static SDL_Renderer* s_renderer = nullptr;
static TTF_Font*     s_font_sm  = nullptr;
static TTF_Font*     s_font_md  = nullptr;
static TTF_Font*     s_font_lg  = nullptr;
static TTF_Font*     s_font_xl  = nullptr;

// ─── ProcUI ───────────────────────────────────────────────────────────────────

static bool s_running = true;

static uint32_t SaveCallback(void*)
{
    OSSavesDone_ReadyToRelease();
    return 0;
}

// ─── Session persistence ──────────────────────────────────────────────────────

static void save_session()
{
    FILE* f = fopen(SESSION_PATH, "w");
    if (!f) return;
    fprintf(f, "tab_count=%d\nactive_tab=%d\n", s_tab_count, s_active_tab);
    for (int i = 0; i < s_tab_count; i++)
        fprintf(f, "url_%d=%s\ntitle_%d=%s\n", i, s_tabs[i].url, i, s_tabs[i].title);
    fclose(f);
}

static void load_session()
{
    FILE* f = fopen(SESSION_PATH, "r");
    if (!f) return;
    char line[MAX_URL + 32];
    while (fgets(line, sizeof(line), f)) {
        size_t ln = strlen(line);
        if (ln && line[ln-1] == '\n') line[--ln] = '\0';
        int n;
        if      (sscanf(line, "tab_count=%d",  &n) == 1) s_tab_count  = (n > 0 && n <= MAX_TABS) ? n : 1;
        else if (sscanf(line, "active_tab=%d", &n) == 1) s_active_tab = n;
        else if (sscanf(line, "url_%d=",   &n) == 1) { char* eq = strchr(line,'='); if (eq && n>=0 && n<MAX_TABS) strncpy(s_tabs[n].url,   eq+1, MAX_URL-1); }
        else if (sscanf(line, "title_%d=", &n) == 1) { char* eq = strchr(line,'='); if (eq && n>=0 && n<MAX_TABS) strncpy(s_tabs[n].title, eq+1, 63); }
    }
    fclose(f);
    if (s_active_tab >= s_tab_count) s_active_tab = s_tab_count - 1;
}

// ─── Thin SDL wrappers ────────────────────────────────────────────────────────

static void sdl_rect(int x,int y,int w,int h,SDL_Color c)                        { ui_rect(s_renderer,x,y,w,h,c); }
static void sdl_outline(int x,int y,int w,int h,SDL_Color c,int t=1)             { ui_outline(s_renderer,x,y,w,h,c,t); }
static void sdl_text(TTF_Font* f,const char* t,int x,int y,SDL_Color c,int a=0)  { ui_text(s_renderer,f,t,x,y,c,a); }

static void draw_gear_icon(int x, int y, int size, SDL_Color c)
{
    int bh = size / 6; if (bh < 2) bh = 2;
    int gap = (size - bh * 3) / 2;
    for (int i = 0; i < 3; i++) sdl_rect(x, y + i*(bh+gap), size, bh, c);
}

// ─── Splash ───────────────────────────────────────────────────────────────────

static void draw_splash(const char* status, double pct)
{
    ui_gradient(s_renderer, COL_BG_TOP, COL_BG_BOT);
    sdl_text(s_font_xl, "Wave Browser", TV_W/2, TV_H/2 - 10, COL_WHITE, 1);
    if (status && status[0])
        sdl_text(s_font_lg, status, TV_W/2, TV_H/2 + 60, COL_WHITE_DIM, 1);
    if (pct >= 0.0) {
        int bx = (TV_W - 800) / 2, by = TV_H - 110;
        ui_progress_bar(s_renderer, bx, by, 800, 20, pct);
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", (int)pct);
        sdl_text(s_font_lg, buf, TV_W/2, TV_H - 68, COL_WHITE, 1);
    }
    SDL_RenderPresent(s_renderer);
}

// ─── Tab switcher overlay ─────────────────────────────────────────────────────

static void draw_tab_switcher()
{
    ui_dim_overlay(s_renderer);
    sdl_text(s_font_lg, "Open Tabs", TV_W/2, 54, COL_WHITE, 1);

    const int CW=210, CH=130, CG=24;
    int total_w = s_tab_count*CW + (s_tab_count-1)*CG;
    int sx = (TV_W - total_w) / 2, cy = TV_H/2 - CH/2;

    for (int i = 0; i < s_tab_count; i++) {
        int cx = sx + i*(CW+CG);
        bool active = (i == s_active_tab);
        sdl_rect(cx+3, cy+3, CW, CH, {0x00,0x00,0x00,0x60});
        sdl_rect(cx, cy, CW, CH, COL_WHITE);
        if (active) for (int d=0;d<3;d++) sdl_outline(cx-d,cy-d,CW+d*2,CH+d*2,COL_BLUE);
        else sdl_outline(cx, cy, CW, CH, COL_WHITE_DIM);
        sdl_rect(cx, cy, CW, 28, COL_FAVICON_BG);
        const char* title = s_tabs[i].title[0] ? s_tabs[i].title : "New Tab";
        char short_title[20]; strncpy(short_title,title,18); short_title[18]='\0';
        sdl_text(s_font_sm, short_title, cx+CW/2, cy+20, COL_WHITE, 1);
        if (s_tabs[i].url[0]) {
            char su[28]; strncpy(su,s_tabs[i].url,26); su[26]='\0';
            sdl_text(s_font_sm, su, cx+8, cy+58, COL_GRAY, 0);
        } else {
            sdl_text(s_font_sm, "New Tab", cx+CW/2, cy+CH/2+10, COL_GRAY, 1);
        }
        char num[4]; snprintf(num,sizeof(num),"%d",i+1);
        sdl_rect(cx+CW-22, cy+32, 18, 18, active ? COL_BLUE : COL_GRAY);
        sdl_text(s_font_sm, num, cx+CW-13, cy+48, COL_WHITE, 1);
    }
    sdl_text(s_font_sm, "ZL/ZR: switch  |  A: open  |  B/SELECT: close",
             TV_W/2, TV_H-16, COL_WHITE_DIM, 1);
    SDL_RenderPresent(s_renderer);
}

static void handle_switcher_input(VPADStatus* vpad)
{
    uint32_t btn = vpad->trigger;
    if ((btn & VPAD_BUTTON_MINUS) || (btn & VPAD_BUTTON_B)) { s_show_tab_switcher = false; return; }
    if ((btn & VPAD_BUTTON_ZL) && s_active_tab > 0)               s_active_tab--;
    if ((btn & VPAD_BUTTON_ZR) && s_active_tab < s_tab_count - 1) s_active_tab++;
    if (btn & VPAD_BUTTON_A) { s_show_tab_switcher = false; return; }

    if (s_tp_pressed) {
        const int CW=210, CH=130, CG=24;
        int total_w=s_tab_count*CW+(s_tab_count-1)*CG;
        int sx=(TV_W-total_w)/2, cy=TV_H/2-CH/2;
        for (int i=0;i<s_tab_count;i++) {
            int cx=sx+i*(CW+CG);
            if (s_tp_x>=cx&&s_tp_x<cx+CW&&s_tp_y>=cy&&s_tp_y<cy+CH) {
                s_active_tab=i; s_show_tab_switcher=false; break;
            }
        }
    }
}

// ─── Browser UI ───────────────────────────────────────────────────────────────

static void draw_tab(int idx, bool active)
{
    int tx = idx*TAB_W, ty = TOOLBAR_H;
    sdl_rect(tx, ty+2, TAB_W-2, TAB_H, active ? COL_TAB_ACTIVE : COL_TAB_INACTIVE);
    sdl_rect(tx+8, ty+10, 14, 14, COL_FAVICON_BG);
    if (s_focus_row==1 && s_focus_col==idx)
        sdl_outline(tx+1, ty+2, TAB_W-3, TAB_H, COL_FOCUS_RING, 2);
    const char* title = s_tabs[idx].title[0] ? s_tabs[idx].title : "New Tab";
    char trunc[24]; strncpy(trunc,title,20); trunc[20]='\0';
    sdl_text(s_font_sm, trunc,    tx+28,       ty+TAB_H-4, COL_TAB_TEXT,  0);
    sdl_text(s_font_sm, "x",      tx+TAB_W-18, ty+TAB_H-4, COL_CLOSE_BTN, 0);
    if (active) sdl_rect(tx, ty+TAB_H, TAB_W-2, 2, COL_TAB_ACTIVE);
}

static void draw_browser_ui()
{
    sdl_rect(0, 0, TV_W, TV_H,      COL_CONTENT_BG);
    sdl_rect(0, 0, TV_W, TOOLBAR_H, COL_CHROME_BG);

    bool f0=(s_focus_row==0&&s_focus_col==0);
    bool f1=(s_focus_row==0&&s_focus_col==1);
    bool f2=(s_focus_row==0&&s_focus_col==2);
    bool f3=(s_focus_row==0&&s_focus_col==3);
    bool f4=(s_focus_row==0&&s_focus_col==4);

    sdl_rect(BTN_BACK_X,   6, BTN_SIZE, BTN_SIZE, COL_CHROME_BG);
    if (f0) ui_focus_ring(s_renderer, BTN_BACK_X, 6, BTN_SIZE, BTN_SIZE);
    sdl_text(s_font_md, "<", BTN_BACK_X+BTN_SIZE/2, TOOLBAR_H-8, COL_GRAY, 1);

    sdl_rect(BTN_FWD_X,    6, BTN_SIZE, BTN_SIZE, COL_CHROME_BG);
    if (f1) ui_focus_ring(s_renderer, BTN_FWD_X, 6, BTN_SIZE, BTN_SIZE);
    sdl_text(s_font_md, ">", BTN_FWD_X+BTN_SIZE/2, TOOLBAR_H-8, COL_GRAY, 1);

    sdl_outline(BTN_RELOAD_X+4, 10, BTN_SIZE-8, BTN_SIZE-8, COL_GRAY);
    if (f2) ui_focus_ring(s_renderer, BTN_RELOAD_X, 6, BTN_SIZE, BTN_SIZE);

    sdl_rect(ADDR_BAR_X, ADDR_BAR_Y, ADDR_BAR_W, ADDR_BAR_H, COL_ADDR_BG);
    sdl_outline(ADDR_BAR_X, ADDR_BAR_Y, ADDR_BAR_W, ADDR_BAR_H,
                f3 ? COL_FOCUS_RING : COL_ADDR_BORDER, f3 ? 2 : 1);
    const char* url = s_tabs[s_active_tab].url[0]
        ? s_tabs[s_active_tab].url : "Select and press A to enter URL";
    sdl_text(s_font_md, url, ADDR_BAR_X+10, ADDR_BAR_Y+ADDR_BAR_H-4,
             s_tabs[s_active_tab].url[0] ? COL_ADDR_TEXT : COL_GRAY, 0);

    sdl_rect(GEAR_BTN_X, GEAR_BTN_Y, GEAR_BTN_SIZE, GEAR_BTN_SIZE, COL_CHROME_BG);
    if (f4) ui_focus_ring(s_renderer, GEAR_BTN_X, GEAR_BTN_Y, GEAR_BTN_SIZE, GEAR_BTN_SIZE);
    draw_gear_icon(GEAR_BTN_X+6, GEAR_BTN_Y+8, GEAR_BTN_SIZE-12, COL_GRAY);

    sdl_rect(0, TOOLBAR_H-1, TV_W, 1, COL_TOOLBAR_LINE);
    sdl_rect(0, TOOLBAR_H,   TV_W, TAB_BAR_H, COL_CHROME_BG);

    for (int i = 0; i < s_tab_count; i++) draw_tab(i, i == s_active_tab);

    int ntx = s_tab_count * TAB_W;
    bool fn = (s_focus_row==1 && s_focus_col==s_tab_count);
    sdl_text(s_font_md, "+", ntx+8, TOOLBAR_H+TAB_BAR_H-6, fn ? COL_FOCUS_RING : COL_NEW_TAB_BTN, 0);
    if (fn) sdl_outline(ntx+2, TOOLBAR_H+2, 28, TAB_H, COL_FOCUS_RING, 2);

    sdl_rect(0, TOOLBAR_H+TAB_BAR_H-1, TV_W, 1, COL_TOOLBAR_LINE);
    sdl_text(s_font_xl, "New Tab", TV_W/2, CONTENT_Y+CONTENT_H/2+24, COL_GRAY, 1);

    const char* hint = g_settings.improved_multitasking
        ? "\xe2\x96\xb2\xe2\x96\xbc: rows  \xe2\x97\x84\xe2\x96\xba: move  A: select  B: back  X: new tab  Y: close tab  SELECT: tab view"
        : "\xe2\x96\xb2\xe2\x96\xbc: rows  \xe2\x97\x84\xe2\x96\xba: move  A: select  B: back  X: new tab  Y: close tab";
    sdl_text(s_font_sm, hint, TV_W/2, TV_H-8, COL_GRAY, 1);

    SDL_RenderPresent(s_renderer);
}

// ─── On-screen keyboard ───────────────────────────────────────────────────────

static void open_url_keyboard()
{
    void* workMem = MEMAllocFromDefaultHeapEx(SWKBD_WORK_SIZE, 0x1000);
    if (!workMem) return;

    nn::swkbd::CreateArg ca = {};
    ca.workMemory = workMem;
    ca.regionType = nn::swkbd::RegionType::Europe;
    if (!nn::swkbd::Create(ca)) { MEMFreeToDefaultHeap(workMem); return; }

    nn::swkbd::AppearArg aa = {};
    if (!nn::swkbd::AppearInputForm(aa)) {
        nn::swkbd::Destroy(); MEMFreeToDefaultHeap(workMem); return;
    }

    char result[MAX_URL] = {};
    bool done = false;
    while (!done) {
        VPADStatus vp; VPADReadError ve;
        VPADRead(VPAD_CHAN_0, &vp, 1, &ve);
        nn::swkbd::ControllerInfo ci = {}; ci.vpad = &vp;
        nn::swkbd::Calc(ci);
        bool isFirst = false;
        if (nn::swkbd::IsDecideOkButton(&isFirst)) {
            const char16_t* str = nn::swkbd::GetInputFormString();
            if (str) { int i=0; while(str[i]&&i<MAX_URL-1){result[i]=(char)(str[i]&0x7F);i++;} result[i]='\0'; }
            done = true;
        } else if (nn::swkbd::IsDecideCancelButton(&isFirst)) done = true;
        if (!done) { nn::swkbd::DrawTV(); nn::swkbd::DrawDRC(); SDL_RenderPresent(s_renderer); }
        usleep(16000);
    }
    nn::swkbd::DisappearInputForm(); nn::swkbd::Destroy(); MEMFreeToDefaultHeap(workMem);
    if (result[0]) {
        memcpy(s_tabs[s_active_tab].url,   result, MAX_URL-1);
        memcpy(s_tabs[s_active_tab].title, result, 63);
        s_tabs[s_active_tab].url[MAX_URL-1] = '\0';
        s_tabs[s_active_tab].title[63] = '\0';
        // Tell the WebKit engine to load this URL
        webkit_engine_navigate(result);
    }
}

// ─── Network helpers ──────────────────────────────────────────────────────────

typedef struct { char* data; size_t size; } Buffer;

static size_t write_cb(void* c, size_t sz, size_t nm, void* up)
{
    size_t total = sz*nm; Buffer* b=(Buffer*)up;
    char* p=(char*)realloc(b->data, b->size+total+1); if(!p) return 0;
    b->data=p; memcpy(b->data+b->size,c,total); b->size+=total; b->data[b->size]='\0'; return total;
}

static size_t file_write_cb(void* c, size_t sz, size_t nm, void* up)
{ return fwrite(c,sz,nm,(FILE*)up); }

static int progress_cb(void*, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t)
{ if(dltotal>0) draw_splash("Downloading update...", (double)dlnow/dltotal*100.0); return 0; }

static void curl_common(CURL* curl)
{
    curl_easy_setopt(curl, CURLOPT_USERAGENT,      "wave-browser/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
}

static char* fetch_string(const char* url)
{
    CURL* curl = curl_easy_init(); if(!curl) return nullptr;
    Buffer buf = {(char*)malloc(1), 0}; buf.data[0]='\0';
    curl_easy_setopt(curl, CURLOPT_URL,           url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,     &buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,       15L);
    curl_common(curl);
    CURLcode res = curl_easy_perform(curl); curl_easy_cleanup(curl);
    if (res != CURLE_OK) { free(buf.data); return nullptr; }
    return buf.data;
}

static int fetch_file(const char* url, const char* path)
{
    FILE* f = fopen(path,"wb"); if(!f) return 1;
    CURL* curl = curl_easy_init(); if(!curl){fclose(f);return 1;}
    curl_easy_setopt(curl, CURLOPT_URL,              url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,    file_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,        f);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS,       0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_cb);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,          180L);
    curl_common(curl);
    CURLcode res = curl_easy_perform(curl); curl_easy_cleanup(curl); fclose(f);
    return (res == CURLE_OK) ? 0 : 1;
}

// ─── Run-ID helpers ───────────────────────────────────────────────────────────

static long long read_stored_run_id()
{ FILE* f=fopen(RUN_ID_PATH,"r"); if(!f) return 0; long long id=0; fscanf(f,"%lld",&id); fclose(f); return id; }

static void write_run_id(long long id)
{ FILE* f=fopen(RUN_ID_PATH,"w"); if(!f) return; fprintf(f,"%lld",id); fclose(f); }

// ─── File backup / restore ────────────────────────────────────────────────────

static char* file_backup(const char* path, size_t* out_len)
{
    *out_len=0; FILE* f=fopen(path,"rb"); if(!f) return nullptr;
    fseek(f,0,SEEK_END); size_t len=(size_t)ftell(f); fseek(f,0,SEEK_SET);
    char* buf=(char*)malloc(len+1); if(!buf){fclose(f);return nullptr;}
    fread(buf,1,len,f); buf[len]='\0'; fclose(f); *out_len=len; return buf;
}

static void file_restore(const char* path, const char* buf, size_t len)
{ if(!buf||len==0) return; FILE* f=fopen(path,"wb"); if(!f) return; fwrite(buf,1,len,f); fclose(f); }

// ─── Install directory helpers ────────────────────────────────────────────────

static void mkdir_p(const char* path)
{
    char tmp[512]; strncpy(tmp,path,sizeof(tmp)-1); tmp[sizeof(tmp)-1]='\0';
    for (char* p=tmp+1;*p;p++){if(*p=='/'){*p='\0';mkdir(tmp,0777);*p='/';}} mkdir(tmp,0777);
}

static void remove_old_install()
{ remove(INSTALL_WUHB); remove(INSTALL_META); remove(RUN_ID_PATH); remove(SESSION_PATH); rmdir(INSTALL_DIR); }

// ─── ZIP extraction ───────────────────────────────────────────────────────────

static int extract_zip_to_dir(const char* zip_path, const char* out_dir)
{
    unzFile zf = unzOpen(zip_path); if(!zf) return 1;
    mkdir_p(out_dir);
    const size_t pfx_len = strlen(ZIP_FOLDER_PFX);
    int result = 0;
    if (unzGoToFirstFile(zf) == UNZ_OK) {
        do {
            char fname[256]={0}; unz_file_info fi;
            unzGetCurrentFileInfo(zf,&fi,fname,sizeof(fname)-1,nullptr,0,nullptr,0);
            size_t nlen=strlen(fname);
            if (nlen>0&&fname[nlen-1]=='/') continue;
            const char* rel = (strncmp(fname,ZIP_FOLDER_PFX,pfx_len)==0) ? fname+pfx_len : fname;
            if (rel[0]=='\0') continue;
            char out_path[512]; snprintf(out_path,sizeof(out_path),"%s/%s",out_dir,rel);
            if (unzOpenCurrentFile(zf)!=UNZ_OK){result=1;break;}
            FILE* out=fopen(out_path,"wb"); if(!out){unzCloseCurrentFile(zf);result=1;break;}
            uint8_t buf[8192]; int br; bool ok=true;
            while((br=unzReadCurrentFile(zf,buf,sizeof(buf)))>0)
                if(fwrite(buf,1,(size_t)br,out)!=(size_t)br){ok=false;break;}
            fclose(out); unzCloseCurrentFile(zf);
            if(!ok||br<0){result=1;break;}
        } while(unzGoToNextFile(zf)==UNZ_OK);
    }
    unzClose(zf); return result;
}

// ─── Update splash ────────────────────────────────────────────────────────────

static void run_splash_and_update()
{
    draw_splash("Loading...", -1.0);
    curl_global_init(CURL_GLOBAL_ALL);
    draw_splash("Checking for updates...", -1.0);

    char* json = fetch_string(ACTIONS_API_URL);
    if (!json) { draw_splash("No network. Starting...", -1.0); usleep(16000*90); return; }

    long long latest_run_id = 0;
    const char* id_ptr = strstr(json, "\"id\":");
    if (id_ptr) { id_ptr+=5; while(*id_ptr==' ')id_ptr++; latest_run_id=atoll(id_ptr); }
    free(json);

    if (latest_run_id == 0) { draw_splash("Couldn't read run ID. Starting...", -1.0); usleep(16000*90); return; }
    if (latest_run_id == read_stored_run_id()) { draw_splash("You're up to date!", -1.0); usleep(16000*60); return; }

    char msg[80]; snprintf(msg,sizeof(msg),"New build #%lld found. Downloading...",latest_run_id);
    draw_splash(msg, 0.0); usleep(800000);

    size_t session_len=0; char* session_buf=file_backup(SESSION_PATH,&session_len);
    remove(ZIP_TMP_PATH);
    if (fetch_file(ARTIFACT_ZIP_URL, ZIP_TMP_PATH) != 0) {
        draw_splash("Download failed. Starting anyway...", -1.0); usleep(16000*120);
        free(session_buf); return;
    }

    bool success = false;
    for (int attempt=1; attempt<=MAX_UPDATE_ATTEMPTS&&!success; attempt++) {
        if (attempt>1) {
            char rm[72]; snprintf(rm,sizeof(rm),"Retrying extraction... (%d/%d)",attempt,MAX_UPDATE_ATTEMPTS);
            draw_splash(rm, 100.0); usleep(2000000);
        }
        draw_splash("Removing old version...", 100.0); remove_old_install();
        draw_splash("Installing update...",    100.0);
        if (extract_zip_to_dir(ZIP_TMP_PATH, INSTALL_DIR) != 0) {
            draw_splash("Extraction failed.", -1.0); usleep(16000*60); continue;
        }
        write_run_id(latest_run_id);
        file_restore(SESSION_PATH, session_buf, session_len);
        success = true;
    }
    remove(ZIP_TMP_PATH); free(session_buf);

    if (!success) { draw_splash("Update failed. Starting anyway...", -1.0); usleep(16000*180); return; }
    draw_splash("Update installed! Please restart Wave Browser.", 100.0); usleep(16000*300);
}

// ─── Touch hit test ───────────────────────────────────────────────────────────

static bool touch_hit(int tx,int ty,int x,int y,int w,int h)
{ return tx>=x&&tx<x+w&&ty>=y&&ty<y+h; }

// ─── Focus helpers ────────────────────────────────────────────────────────────

static void focus_clamp()
{
    if (s_focus_row==0) {
        if (s_focus_col<0) s_focus_col=0;
        if (s_focus_col>4) s_focus_col=4;
    } else {
        if (s_focus_col<0) s_focus_col=0;
        if (s_focus_col>s_tab_count) s_focus_col=s_tab_count;
    }
}

static void focus_activate()
{
    if (s_focus_row==0) {
        switch(s_focus_col) {
            case 0: break;
            case 1: break;
            case 2: break;
            case 3: open_url_keyboard(); break;
            case 4: s_in_settings = true; settings_open(); break;
        }
    } else {
        if (s_focus_col < s_tab_count) {
            s_active_tab = s_focus_col;
        } else if (s_tab_count < MAX_TABS) {
            memset(&s_tabs[s_tab_count],0,sizeof(Tab));
            strcpy(s_tabs[s_tab_count].title,"New Tab");
            s_active_tab = s_tab_count++;
            s_focus_col  = s_active_tab;
        }
    }
}


// ─── Wii Remote input ─────────────────────────────────────────────────────────

static void handle_wii_remote_input(KPADStatus* kp)
{
    if (!kp) return;
    uint32_t btn = kp->trigger;

    if (btn & WPAD_BUTTON_UP) {
        if (s_focus_row == 1) { s_focus_row = 0; s_focus_col = 3; }
    }
    if (btn & WPAD_BUTTON_DOWN) {
        if (s_focus_row == 0) { s_focus_row = 1; s_focus_col = s_active_tab; }
    }
    if (btn & WPAD_BUTTON_LEFT) {
        s_focus_col--;
        focus_clamp();
        if (s_focus_row == 1 && s_focus_col < s_tab_count)
            s_active_tab = s_focus_col;
    }
    if (btn & WPAD_BUTTON_RIGHT) {
        s_focus_col++;
        focus_clamp();
        if (s_focus_row == 1 && s_focus_col < s_tab_count)
            s_active_tab = s_focus_col;
    }

    if (btn & WPAD_BUTTON_A)
        focus_activate();

    if (btn & WPAD_BUTTON_B) {
        if (s_show_tab_switcher) { s_show_tab_switcher = false; }
        else if (s_in_settings)  { s_in_settings = false; }
    }

    if ((btn & WPAD_BUTTON_PLUS) && s_tab_count < MAX_TABS) {
        memset(&s_tabs[s_tab_count], 0, sizeof(Tab));
        strcpy(s_tabs[s_tab_count].title, "New Tab");
        s_active_tab = s_tab_count++;
        s_focus_row = 1;
        s_focus_col = s_active_tab;
    }

    if (btn & WPAD_BUTTON_MINUS) {
        if (g_settings.improved_multitasking) {
            s_show_tab_switcher = true;
        } else if (s_tab_count > 1) {
            int ci = (s_focus_row == 1 && s_focus_col < s_tab_count)
                     ? s_focus_col : s_active_tab;
            for (int i = ci; i < s_tab_count - 1; i++) s_tabs[i] = s_tabs[i+1];
            s_tab_count--;
            if (s_active_tab >= s_tab_count) s_active_tab = s_tab_count - 1;
            s_active_tab = (s_focus_col < s_tab_count) ? s_focus_col : s_tab_count - 1;
            s_focus_col  = s_active_tab;
        }
    }

    if (btn & WPAD_BUTTON_1) {
        s_focus_row = 0;
        s_focus_col = 2;
        focus_activate();
    }

    if (btn & WPAD_BUTTON_2) {
        s_focus_row = 0;
        s_focus_col = 3;
        open_url_keyboard();
    }

    if (kp->extensionType == WPAD_EXT_NUNCHUK ||
        kp->extensionType == WPAD_EXT_MPLUS_NUNCHUK)
    {
        float nx = kp->nunchuk.stick.x;
        float ny = kp->nunchuk.stick.y;
        bool any = (nx < -STICK_DEAD || nx > STICK_DEAD ||
                    ny > STICK_DEAD  || ny < -STICK_DEAD);
        if (any) {
            s_wii_stick_held++;
            bool fire = (s_wii_stick_held == 1 || s_wii_stick_held >= STICK_REPEAT);
            if (s_wii_stick_held >= STICK_REPEAT) s_wii_stick_held = STICK_REPEAT;
            if (fire) {
                if (ny > STICK_DEAD && s_focus_row == 1) {
                    s_focus_row = 0; s_focus_col = 3;
                }
                if (ny < -STICK_DEAD && s_focus_row == 0) {
                    s_focus_row = 1; s_focus_col = s_active_tab;
                }
                if (nx < -STICK_DEAD) {
                    s_focus_col--;
                    focus_clamp();
                    if (s_focus_row == 1 && s_focus_col < s_tab_count)
                        s_active_tab = s_focus_col;
                }
                if (nx > STICK_DEAD) {
                    s_focus_col++;
                    focus_clamp();
                    if (s_focus_row == 1 && s_focus_col < s_tab_count)
                        s_active_tab = s_focus_col;
                }
            }
        } else {
            s_wii_stick_held = 0;
        }

        uint32_t nbtn = kp->nunchuk.trigger;
        if ((nbtn & WPAD_NUNCHUK_BUTTON_C) && s_tab_count < MAX_TABS) {
            memset(&s_tabs[s_tab_count], 0, sizeof(Tab));
            strcpy(s_tabs[s_tab_count].title, "New Tab");
            s_active_tab = s_tab_count++;
            s_focus_row = 1;
            s_focus_col = s_active_tab;
        }

        if ((nbtn & WPAD_NUNCHUK_BUTTON_Z) && g_settings.improved_multitasking)
            s_show_tab_switcher = true;
    }
}

// ─── Main input handler ───────────────────────────────────────────────────────

static void handle_input(VPADStatus* vpad)
{
    uint32_t btn = vpad->trigger;

    if (btn&VPAD_BUTTON_UP)    { if(s_focus_row==1){s_focus_row=0;s_focus_col=3;} }
    if (btn&VPAD_BUTTON_DOWN)  { if(s_focus_row==0){s_focus_row=1;s_focus_col=s_active_tab;} }
    if (btn&VPAD_BUTTON_LEFT)  { s_focus_col--; focus_clamp(); if(s_focus_row==1&&s_focus_col<s_tab_count) s_active_tab=s_focus_col; }
    if (btn&VPAD_BUTTON_RIGHT) { s_focus_col++; focus_clamp(); if(s_focus_row==1&&s_focus_col<s_tab_count) s_active_tab=s_focus_col; }

    float sx=vpad->leftStick.x, sy=vpad->leftStick.y;
    bool any=(sx<-STICK_DEAD||sx>STICK_DEAD||sy>STICK_DEAD||sy<-STICK_DEAD);
    if (any) {
        s_stick_held++;
        bool fire = (s_stick_held==1||s_stick_held>=STICK_REPEAT);
        if (s_stick_held>=STICK_REPEAT) s_stick_held=STICK_REPEAT;
        if (fire) {
            if (sy> STICK_DEAD&&s_focus_row==1){s_focus_row=0;s_focus_col=3;}
            if (sy<-STICK_DEAD&&s_focus_row==0){s_focus_row=1;s_focus_col=s_active_tab;}
            if (sx<-STICK_DEAD){s_focus_col--;focus_clamp();if(s_focus_row==1&&s_focus_col<s_tab_count)s_active_tab=s_focus_col;}
            if (sx> STICK_DEAD){s_focus_col++;focus_clamp();if(s_focus_row==1&&s_focus_col<s_tab_count)s_active_tab=s_focus_col;}
        }
    } else s_stick_held=0;

    if ((btn&VPAD_BUTTON_ZL)&&s_active_tab>0)              { s_active_tab--;s_focus_row=1;s_focus_col=s_active_tab; }
    if ((btn&VPAD_BUTTON_ZR)&&s_active_tab<s_tab_count-1)  { s_active_tab++;s_focus_row=1;s_focus_col=s_active_tab; }
    if (btn&VPAD_BUTTON_A) focus_activate();

    if ((btn&VPAD_BUTTON_X)&&s_tab_count<MAX_TABS) {
        memset(&s_tabs[s_tab_count],0,sizeof(Tab)); strcpy(s_tabs[s_tab_count].title,"New Tab");
        s_active_tab=s_tab_count++; s_focus_row=1; s_focus_col=s_active_tab;
    }
    if ((btn&VPAD_BUTTON_Y)&&s_tab_count>1) {
        int ci=(s_focus_row==1&&s_focus_col<s_tab_count)?s_focus_col:s_active_tab;
        for(int i=ci;i<s_tab_count-1;i++) s_tabs[i]=s_tabs[i+1];
        s_tab_count--;
        if(s_active_tab>=s_tab_count) s_active_tab=s_tab_count-1;
        s_active_tab = (s_focus_col<s_tab_count)?s_focus_col:s_tab_count-1;
        s_focus_col  = s_active_tab;
    }
    if ((btn&VPAD_BUTTON_MINUS)&&g_settings.improved_multitasking) s_show_tab_switcher=true;

    // Touch — fires on finger-DOWN (s_tp_pressed) for instant response
    if (s_tp_pressed) {
        int tx=s_tp_x, ty=s_tp_y;

        if (touch_hit(tx,ty,GEAR_BTN_X,GEAR_BTN_Y,GEAR_BTN_SIZE,GEAR_BTN_SIZE)) {
            s_in_settings=true; settings_open(); return;
        }
        if (touch_hit(tx,ty,ADDR_BAR_X,ADDR_BAR_Y,ADDR_BAR_W,ADDR_BAR_H)) {
            s_focus_row=0; s_focus_col=3; open_url_keyboard(); return;
        }
        if (touch_hit(tx,ty,BTN_BACK_X,  6,BTN_SIZE,BTN_SIZE)) {
            s_focus_row=0; s_focus_col=0; return;
        }
        if (touch_hit(tx,ty,BTN_FWD_X,   6,BTN_SIZE,BTN_SIZE)) {
            s_focus_row=0; s_focus_col=1; return;
        }
        if (touch_hit(tx,ty,BTN_RELOAD_X, 6,BTN_SIZE,BTN_SIZE)) {
            s_focus_row=0; s_focus_col=2; return;
        }

        for (int i=0;i<s_tab_count;i++) {
            if (s_tab_count>1 &&
                touch_hit(tx,ty, i*TAB_W+TAB_W-20, TOOLBAR_H, 20, TAB_H)) {
                for(int j=i;j<s_tab_count-1;j++) s_tabs[j]=s_tabs[j+1];
                s_tab_count--;
                if(s_active_tab>=s_tab_count) s_active_tab=s_tab_count-1;
                s_focus_row=1; s_focus_col=s_active_tab; return;
            }
            if (touch_hit(tx,ty, i*TAB_W, TOOLBAR_H, TAB_W-20, TAB_H)) {
                s_active_tab=i; s_focus_row=1; s_focus_col=i; return;
            }
        }

        if (s_tab_count<MAX_TABS &&
            touch_hit(tx,ty, s_tab_count*TAB_W, TOOLBAR_H, 40, TAB_BAR_H)) {
            memset(&s_tabs[s_tab_count],0,sizeof(Tab));
            strcpy(s_tabs[s_tab_count].title,"New Tab");
            s_active_tab=s_tab_count++; s_focus_row=1; s_focus_col=s_active_tab;
        }
    }
}

// ─── Entry point ──────────────────────────────────────────────────────────────

int main(int, char**)
{
    ProcUIInitEx(SaveCallback, nullptr);
    AXInit();
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    s_window   = SDL_CreateWindow("Wave Browser",
                     SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, TV_W, TV_H, 0);
    s_renderer = SDL_CreateRenderer(s_window, -1,
                     SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    s_font_sm = TTF_OpenFontRW(SDL_RWFromConstMem(font_data, font_data_len), 0, 13);
    s_font_md = TTF_OpenFontRW(SDL_RWFromConstMem(font_data, font_data_len), 0, 15);
    s_font_lg = TTF_OpenFontRW(SDL_RWFromConstMem(font_data, font_data_len), 0, 28);
    s_font_xl = TTF_OpenFontRW(SDL_RWFromConstMem(font_data, font_data_len), 0, 48);

    VPADInit();
    WPADEnableURCC(true);
    WPADEnableWiiRemote(true);
    KPADInit();

    // Init WebKit engine (stub if libWebKitWKC.a not present)
    webkit_engine_init(TV_W, TV_H);

    memset(s_tabs, 0, sizeof(s_tabs));
    strncpy(s_tabs[0].title, "New Tab", 63);

    settings_load();
    if (g_settings.improved_multitasking) load_session();

    run_splash_and_update();

    while (s_running) {
        ProcUIStatus status = ProcUIProcessMessages(TRUE);

        if (status == PROCUI_STATUS_EXITING) {
            s_running = false;

        } else if (status == PROCUI_STATUS_RELEASE_FOREGROUND) {
            if (g_settings.improved_multitasking) save_session();
            ProcUIDrawDoneRelease();

        } else if (status == PROCUI_STATUS_IN_FOREGROUND) {
            VPADStatus vpad; VPADReadError error;
            VPADRead(VPAD_CHAN_0, &vpad, 1, &error);

            {
                KPADStatus kpad;
                for (int ch = 0; ch < 4; ch++) {
                    int32_t nread = KPADRead((WPADChan)ch, &kpad, 1);
                    if (nread <= 0) continue;

                    // ── Wii Remote buttons ───────────────────────────────────
                    if (kpad.trigger & WPAD_BUTTON_UP)    vpad.trigger |= VPAD_BUTTON_UP;
                    if (kpad.trigger & WPAD_BUTTON_DOWN)  vpad.trigger |= VPAD_BUTTON_DOWN;
                    if (kpad.trigger & WPAD_BUTTON_LEFT)  vpad.trigger |= VPAD_BUTTON_LEFT;
                    if (kpad.trigger & WPAD_BUTTON_RIGHT) vpad.trigger |= VPAD_BUTTON_RIGHT;
                    if (kpad.trigger & WPAD_BUTTON_A) vpad.trigger |= VPAD_BUTTON_A;
                    if (kpad.trigger & WPAD_BUTTON_B) vpad.trigger |= VPAD_BUTTON_B;

                    // ── Nunchuk stick ────────────────────────────────────────
                    if (kpad.extensionType == WPAD_EXT_NUNCHUK ||
                        kpad.extensionType == WPAD_EXT_MPLUS_NUNCHUK)
                    {
                        float nx = kpad.nunchuk.stick.x;
                        float ny = kpad.nunchuk.stick.y;
                        if (fabsf(nx) > fabsf(vpad.leftStick.x)) vpad.leftStick.x = nx;
                        if (fabsf(ny) > fabsf(vpad.leftStick.y)) vpad.leftStick.y = ny;
                    }

                    // ── WiiU Pro Controller (extensionType == 31) ────────────
                    // Button data lives in kpad.classic (KPADClassicStatus).
                    // Sticks: kpad.classic.leftStick / rightStick (range -1..1).
                    if (kpad.extensionType == WPAD_EXT_PRO_CONTROLLER)
                    {
                        uint32_t c = kpad.classic.trigger;
                        // Face buttons → GamePad face buttons
                        if (c & WPAD_CLASSIC_BUTTON_A)     vpad.trigger |= VPAD_BUTTON_A;
                        if (c & WPAD_CLASSIC_BUTTON_B)     vpad.trigger |= VPAD_BUTTON_B;
                        if (c & WPAD_CLASSIC_BUTTON_X)     vpad.trigger |= VPAD_BUTTON_X;
                        if (c & WPAD_CLASSIC_BUTTON_Y)     vpad.trigger |= VPAD_BUTTON_Y;
                        // D-pad
                        if (c & WPAD_CLASSIC_BUTTON_UP)    vpad.trigger |= VPAD_BUTTON_UP;
                        if (c & WPAD_CLASSIC_BUTTON_DOWN)  vpad.trigger |= VPAD_BUTTON_DOWN;
                        if (c & WPAD_CLASSIC_BUTTON_LEFT)  vpad.trigger |= VPAD_BUTTON_LEFT;
                        if (c & WPAD_CLASSIC_BUTTON_RIGHT) vpad.trigger |= VPAD_BUTTON_RIGHT;
                        // Triggers / shoulders
                        if (c & WPAD_CLASSIC_BUTTON_ZL)    vpad.trigger |= VPAD_BUTTON_ZL;
                        if (c & WPAD_CLASSIC_BUTTON_ZR)    vpad.trigger |= VPAD_BUTTON_ZR;
                        if (c & WPAD_CLASSIC_BUTTON_L)     vpad.trigger |= VPAD_BUTTON_L;
                        if (c & WPAD_CLASSIC_BUTTON_R)     vpad.trigger |= VPAD_BUTTON_R;
                        // Menu buttons
                        if (c & WPAD_CLASSIC_BUTTON_PLUS)  vpad.trigger |= VPAD_BUTTON_PLUS;
                        if (c & WPAD_CLASSIC_BUTTON_MINUS) vpad.trigger |= VPAD_BUTTON_MINUS;
                        // Left stick → GamePad left stick (navigation, scrolling)
                        float lx = kpad.classic.leftStick.x;
                        float ly = kpad.classic.leftStick.y;
                        if (fabsf(lx) > fabsf(vpad.leftStick.x))  vpad.leftStick.x = lx;
                        if (fabsf(ly) > fabsf(vpad.leftStick.y))  vpad.leftStick.y = ly;
                        // Right stick → GamePad right stick
                        float rx = kpad.classic.rightStick.x;
                        float ry = kpad.classic.rightStick.y;
                        if (fabsf(rx) > fabsf(vpad.rightStick.x)) vpad.rightStick.x = rx;
                        if (fabsf(ry) > fabsf(vpad.rightStick.y)) vpad.rightStick.y = ry;
                        // Pro Controller has no Wii Remote-specific buttons —
                        // all inputs already merged into vpad above, skip wii handler.
                        continue;
                    }

                    if (!s_in_settings && !s_show_tab_switcher)
                        handle_wii_remote_input(&kpad);
                }
            }
            poll_touch(&vpad);

            // WebKit engine tick (timers, layout, JS)
            webkit_engine_tick();

            if (s_in_settings) {
                if (!settings_handle_input(&vpad, s_tp_pressed, s_tp_x, s_tp_y)) s_in_settings = false;
                else settings_draw(s_renderer, s_font_sm, s_font_md, s_font_lg);

            } else if (s_show_tab_switcher) {
                handle_switcher_input(&vpad);
                draw_tab_switcher();

            } else {
                handle_input(&vpad);
                draw_browser_ui();
            }
        }
    }

    if (g_settings.improved_multitasking) save_session();
    webkit_engine_shutdown();
    curl_global_cleanup();

    TTF_CloseFont(s_font_sm); TTF_CloseFont(s_font_md);
    TTF_CloseFont(s_font_lg); TTF_CloseFont(s_font_xl);
    TTF_Quit();
    SDL_DestroyRenderer(s_renderer);
    SDL_DestroyWindow(s_window);
    SDL_Quit();
    AXQuit();
    KPADShutdown();
    ProcUIShutdown();
    return 0;
}
