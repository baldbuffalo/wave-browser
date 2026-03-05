#include <coreinit/core.h>
#include <coreinit/filesystem.h>
#include <coreinit/screen.h>
#include <coreinit/cache.h>
#include <coreinit/memdefaultheap.h>
#include <proc_ui/procui.h>
#include <vpad/input.h>
#include <nn/ac.h>
#include <curl/curl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// ----------------------------------------------------------------
// Bump this every release to match your GitHub tag
// ----------------------------------------------------------------
#define CURRENT_VERSION "v1.0.0"
#define GITHUB_API_URL  "https://api.github.com/repos/baldbuffalo/wave-browser/releases/latest"
#define DOWNLOAD_PATH   "fs:/vol/external01/wave-browser-update.rpx"

// ----------------------------------------------------------------
// OSScreen helpers
// ----------------------------------------------------------------
static void *s_tv_buf  = NULL;
static void *s_drc_buf = NULL;

static void screen_init(void) {
    OSScreenInit();
    size_t tv_size  = OSScreenGetBufferSizeEx(SCREEN_TV);
    size_t drc_size = OSScreenGetBufferSizeEx(SCREEN_DRC);
    s_tv_buf  = MEMAllocFromDefaultHeapEx(tv_size,  0x100);
    s_drc_buf = MEMAllocFromDefaultHeapEx(drc_size, 0x100);
    OSScreenSetBufferEx(SCREEN_TV,  s_tv_buf);
    OSScreenSetBufferEx(SCREEN_DRC, s_drc_buf);
    OSScreenEnableEx(SCREEN_TV,  1);
    OSScreenEnableEx(SCREEN_DRC, 1);
}

static void screen_deinit(void) {
    OSScreenEnableEx(SCREEN_TV,  0);
    OSScreenEnableEx(SCREEN_DRC, 0);
    if (s_tv_buf)  { MEMFreeToDefaultHeap(s_tv_buf);  s_tv_buf  = NULL; }
    if (s_drc_buf) { MEMFreeToDefaultHeap(s_drc_buf); s_drc_buf = NULL; }
}

static void screen_clear(void) {
    OSScreenClearBufferEx(SCREEN_TV,  0x00000000);
    OSScreenClearBufferEx(SCREEN_DRC, 0x00000000);
}

static void screen_print(uint32_t row, const char *msg) {
    OSScreenPutFontEx(SCREEN_TV,  0, row, msg);
    OSScreenPutFontEx(SCREEN_DRC, 0, row, msg);
}

static void screen_flip(void) {
    DCFlushRange(s_tv_buf,  OSScreenGetBufferSizeEx(SCREEN_TV));
    DCFlushRange(s_drc_buf, OSScreenGetBufferSizeEx(SCREEN_DRC));
    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);
}

static void screen_progress(uint32_t row, double pct) {
    char bar[64];
    int filled = (int)(pct / 5.0); // 20 chars = 100%
    if (filled > 20) filled = 20;
    char inner[21];
    for (int i = 0; i < 20; i++) inner[i] = (i < filled) ? '#' : '.';
    inner[20] = '\0';
    snprintf(bar, sizeof(bar), "Downloading: [%s] %d%%", inner, (int)pct);
    screen_print(row, bar);
}

// ----------------------------------------------------------------
// curl helpers
// ----------------------------------------------------------------
typedef struct { char *data; size_t size; } Buffer;

static size_t write_cb(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total   = size * nmemb;
    Buffer *buf    = (Buffer *)userp;
    char *p        = realloc(buf->data, buf->size + total + 1);
    if (!p) return 0;
    buf->data = p;
    memcpy(buf->data + buf->size, contents, total);
    buf->size += total;
    buf->data[buf->size] = '\0';
    return total;
}

static size_t file_write_cb(void *contents, size_t size, size_t nmemb, void *userp) {
    return fwrite(contents, size, nmemb, (FILE *)userp);
}

static int progress_cb(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
                       curl_off_t ultotal, curl_off_t ulnow) {
    (void)clientp; (void)ultotal; (void)ulnow;
    if (dltotal <= 0) return 0;
    double pct = (double)dlnow / (double)dltotal * 100.0;
    screen_clear();
    screen_print(0, "Wave Browser");
    screen_print(2, "Downloading update...");
    screen_progress(4, pct);
    screen_flip();
    return 0;
}

// ----------------------------------------------------------------
// Check GitHub for a newer version
// Returns 1 if update available, fills out_tag
// ----------------------------------------------------------------
static int check_for_update(char *out_tag, size_t tag_size) {
    CURL *curl = curl_easy_init();
    if (!curl) return 0;

    Buffer buf = { malloc(1), 0 };
    if (!buf.data) { curl_easy_cleanup(curl); return 0; }
    buf.data[0] = '\0';

    curl_easy_setopt(curl, CURLOPT_URL,            GITHUB_API_URL);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,      "wave-browser/1.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &buf);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        10L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    int update_available = 0;
    if (res == CURLE_OK) {
        char *tag_ptr = strstr(buf.data, "\"tag_name\":\"");
        if (tag_ptr) {
            tag_ptr += strlen("\"tag_name\":\"");
            char *end = strchr(tag_ptr, '"');
            if (end) {
                size_t len = (size_t)(end - tag_ptr);
                if (len >= tag_size) len = tag_size - 1;
                strncpy(out_tag, tag_ptr, len);
                out_tag[len] = '\0';
                if (strcmp(out_tag, CURRENT_VERSION) != 0)
                    update_available = 1;
            }
        }
    }

    free(buf.data);
    return update_available;
}

// ----------------------------------------------------------------
// Download update RPX to SD card with live progress bar
// ----------------------------------------------------------------
static int download_update(const char *url) {
    FILE *f = fopen(DOWNLOAD_PATH, "wb");
    if (!f) return 1;

    CURL *curl = curl_easy_init();
    if (!curl) { fclose(f); return 1; }

    curl_easy_setopt(curl, CURLOPT_URL,              url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,        "wave-browser/1.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,    file_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,        f);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION,   1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS,       0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_cb);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,          120L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    fclose(f);
    return (res == CURLE_OK) ? 0 : 1;
}

// ----------------------------------------------------------------
// Splash sequence:
//   "Loading..." -> "Checking for updates..." ->
//   "Up to date!" OR download with progress bar
// ----------------------------------------------------------------
static void run_splash(void) {
    // Step 1: Loading
    screen_clear();
    screen_print(0, "Wave Browser");
    screen_print(2, "Loading...");
    screen_flip();

    ACInitialize();
    ACConnect();
    curl_global_init(CURL_GLOBAL_ALL);

    // Step 2: Checking for updates
    screen_clear();
    screen_print(0, "Wave Browser");
    screen_print(2, "Checking for updates...");
    screen_flip();

    char latest_tag[64] = {0};
    int has_update = check_for_update(latest_tag, sizeof(latest_tag));

    if (!has_update) {
        screen_clear();
        screen_print(0, "Wave Browser");
        screen_print(2, "Up to date!");
        screen_flip();
        for (int i = 0; i < 90; i++) usleep(16000); // ~1.5 sec
        return;
    }

    // Step 3: Show what version was found
    char msg[128];
    snprintf(msg, sizeof(msg), "Update found: %s  (current: %s)", latest_tag, CURRENT_VERSION);
    screen_clear();
    screen_print(0, "Wave Browser");
    screen_print(2, msg);
    screen_print(4, "Starting download...");
    screen_flip();
    usleep(1000000); // 1 sec

    // Build download URL — expects asset named wave-browser.rpx on the release
    char download_url[256];
    snprintf(download_url, sizeof(download_url),
        "https://github.com/baldbuffalo/wave-browser/releases/download/%s/wave-browser.rpx",
        latest_tag);

    int ok = download_update(download_url);

    // Step 4: Result
    screen_clear();
    screen_print(0, "Wave Browser");
    if (ok == 0) {
        screen_print(2, "Update downloaded to SD card!");
        screen_print(4, "Restart the app to apply it.");
    } else {
        screen_print(2, "Download failed.");
        screen_print(4, "Starting current version...");
    }
    screen_flip();
    for (int i = 0; i < 180; i++) usleep(16000); // ~3 sec
}

// ----------------------------------------------------------------
// MAIN
// ----------------------------------------------------------------
int main(void) {
    ProcUIInit(NULL);
    VPADInit();
    screen_init();

    run_splash();

    while (ProcUIIsRunning()) {
        VPADStatus vpad;
        VPADReadError error;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &error);
        ProcUIProcessMessages(TRUE);
        usleep(16000);
    }

    curl_global_cleanup();
    ACFinalize();
    screen_deinit();
    ProcUIShutdown();
    return 0;
}
