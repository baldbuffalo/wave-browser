#include <coreinit/core.h>
#include <coreinit/filesystem.h>
#include <proc_ui/procui.h>
#include <proc_ui/widgets.h>  // <-- important for ProcUIElement, layers, progress bars
#include <vpad/input.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>

#define CURRENT_VERSION "v0.1.0"
#define GITHUB_API "https://api.github.com/repos/baldbuffalo/wave-browser/releases/latest"

// -------------------- Memory callback for CURL --------------------
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

// -------------------- Fetch latest GitHub release --------------------
int fetch_latest_release(char *out_tag, size_t tag_size)
{
    CURL *curl = curl_easy_init();
    if (!curl) return 1;

    struct MemoryStruct chunk;
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

// -------------------- RPX Entry Point --------------------
__asm__(".global __rpx_start\n\t"
        "__rpx_start: b main");

// -------------------- Progress struct for UI --------------------
typedef struct {
    ProcUIElement *text;
    ProcUIElement *bar;
} ProgressUI;

// -------------------- Progress callback --------------------
void ProgressCallback(ProgressUI *prog, float progress, const char *message)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "%s %.1f%%", message, progress * 100.0f);

    ProcUITextSetString(prog->text, buf);
    ProcUIProgressBarSetValue(prog->bar, progress);

    ProcUIProcessMessages(TRUE);
}

// -------------------- Main --------------------
int main(void)
{
    // Init systems
    VPADInit();
    ProcUIInit(NULL);
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // -------------------- Splash Screen --------------------
    ProcUILayer *layer = ProcUILayerCreate();
    ProcUILayerSet(layer, 0, 0, 1280, 720);

    ProcUIElement *statusText = ProcUITextCreate(layer, "Loading...", 640, 360, 1.0f);
    ProcUIElement *progressBar = ProcUIProgressBarCreate(layer, 440, 400, 400, 20); // green
    ProcUIProgressBarSetColor(progressBar, 0x00FF00FF);

    ProcUILayerAddElement(layer, statusText);
    ProcUILayerAddElement(layer, progressBar);
    ProcUILayerShow(layer);

    ProgressUI progUI = {statusText, progressBar};

    // Simulate initial loading
    for (int i = 0; i <= 100; i += 20) {
        ProgressCallback(&progUI, i / 100.0f, "Loading...");
        usleep(200000); // 0.2s
    }

    // -------------------- Check for updates --------------------
    ProgressCallback(&progUI, 0.0f, "Checking for updates...");
    char latest_tag[64] = {0};
    int update_available = 0;

    if (fetch_latest_release(latest_tag, sizeof(latest_tag)) == 0) {
        if (strcmp(latest_tag, CURRENT_VERSION) != 0) {
            update_available = 1;
        }
    }

    // -------------------- Download update if available --------------------
    if (update_available) {
        for (int i = 0; i <= 100; i += 10) {
            ProgressCallback(&progUI, i / 100.0f, "Downloading update...");
            usleep(150000); // simulate download
        }
    }

    // -------------------- Done loading --------------------
    ProgressCallback(&progUI, 1.0f, "Loading complete");

    // Here you would show your browser UI instead of loop
    while (ProcUIIsRunning()) {
        VPADStatus vpad;
        VPADReadError error;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &error);
        usleep(50000);
        ProcUIProcessMessages(TRUE);
    }

    curl_global_cleanup();
    ProcUIShutdown();
    return 0;
}
