#include <coreinit/core.h>
#include <coreinit/filesystem.h>
#include <proc_ui/procui.h>
#include <vpad/input.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>

#define CURRENT_VERSION "v0.1.0"
#define GITHUB_API "https://api.github.com/repos/baldbuffalo/wave-browser/releases/latest"

struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct*)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) return 0;

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

// -------------------- Fetch latest release --------------------
int fetch_latest_release(char *out_tag, size_t tag_size)
{
    CURL *curl = curl_easy_init();
    if (!curl) return 1;

    struct MemoryStruct chunk = {0};
    chunk.memory = malloc(1);
    chunk.size = 0;

    curl_easy_setopt(curl, CURLOPT_URL, GITHUB_API);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "wave-browser");

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        curl_easy_cleanup(curl);
        free(chunk.memory);
        return 1;
    }

    char *tag_ptr = strstr(chunk.memory, "\"tag_name\":\"");
    if (tag_ptr) {
        tag_ptr += 12;
        char *end = strchr(tag_ptr, '"');
        if (end) {
            size_t len = end - tag_ptr;
            if (len >= tag_size) len = tag_size - 1;
            strncpy(out_tag, tag_ptr, len);
            out_tag[len] = 0;
        }
    }

    curl_easy_cleanup(curl);
    free(chunk.memory);
    return 0;
}

// -------------------- Download with progress --------------------
struct DownloadProgress {
    ProcUIElement *text;
    ProcUIElement *bar;
};

static int ProgressCallback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
    struct DownloadProgress *prog = (struct DownloadProgress*)clientp;

    if (dltotal > 0) {
        float progress = (float)dlnow / (float)dltotal;
        ProcUIProgressBarSetValue(prog->bar, progress);

        char buf[128];
        snprintf(buf, sizeof(buf), "%lld of %lld bytes", dlnow, dltotal);
        ProcUITextSetString(prog->text, buf);
    }
    return 0;
}

// -------------------- RPX Entry --------------------
__asm__(".global __rpx_start\n\t"
        "__rpx_start: b main");

int main(void)
{
    // Init UI and controller
    ProcUIInit(NULL);
    VPADInit();
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Splash screen
    ProcUILayer *layer = ProcUILayerCreate();
    ProcUILayerSet(layer, 0, 0, 1280, 720);

    ProcUIElement *statusText = ProcUITextCreate(layer, "Loading...", 640, 360, 1.0f);
    ProcUIElement *progressBar = ProcUIProgressBarCreate(layer, 440, 400, 400, 20); // green bar
    ProcUIProgressBarSetColor(progressBar, 0x00FF00FF);

    ProcUILayerAddElement(layer, statusText);
    ProcUILayerAddElement(layer, progressBar);

    ProcUILayerShow(layer);

    // Give a small delay for splash
    sleep(1);

    // Checking updates
    ProcUITextSetString(statusText, "Checking for updates...");
    ProcUIProgressBarSetValue(progressBar, 0.0f);

    char latest_tag[64] = {0};
    int has_update = 0;

    if (fetch_latest_release(latest_tag, sizeof(latest_tag)) == 0) {
        if (strcmp(latest_tag, CURRENT_VERSION) != 0) {
            has_update = 1;
        }
    }

    // If update available, download it
    if (has_update) {
        ProcUITextSetString(statusText, "Downloading update...");

        struct DownloadProgress prog = {statusText, progressBar};
        CURL *curl = curl_easy_init();
        if (curl) {
            FILE *fp = fopen("/tmp/update.bin", "wb");
            curl_easy_setopt(curl, CURLOPT_URL, "https://github.com/baldbuffalo/wave-browser/releases/latest/download/wave_browser.rpx");
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &prog);
            curl_easy_perform(curl);
            fclose(fp);
            curl_easy_cleanup(curl);
        }
    }

    // Loading complete
    ProcUITextSetString(statusText, "Loading complete!");
    ProcUIProgressBarSetValue(progressBar, 1.0f);

    sleep(1);

    // Main loop
    while (ProcUIIsRunning()) {
        VPADStatus vpad;
        VPADReadError error;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &error);
        usleep(50000);
    }

    curl_global_cleanup();
    ProcUIShutdown();
    return 0;
}
