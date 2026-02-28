#include <coreinit/core.h>
#include <coreinit/filesystem.h>
#include <proc_ui/procui.h>
#include <vpad/input.h>
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

// -------------------- Progress struct --------------------
struct DownloadProgress {
    ProcUIElement *text;
    ProcUIElement *bar;
    curl_off_t downloaded;
    curl_off_t total;
};

// -------------------- Progress callback for CURL --------------------
static int ProgressCallback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
    struct DownloadProgress *prog = (struct DownloadProgress*)clientp;

    if (dltotal > 0) {
        float progress = (float)dlnow / (float)dltotal;
        if (progress > 1.0f) progress = 1.0f;

        prog->downloaded = dlnow;
        prog->total = dltotal;

        char buf[64];
        snprintf(buf, sizeof(buf), "%lld KB / %lld KB", dlnow / 1024, dltotal / 1024);
        ProcUITextSetString(prog->text, buf);
        ProcUIProgressBarSetValue(prog->bar, progress);
        ProcUIProcessMessages(FALSE);
    }
    return 0;
}

// -------------------- Fetch latest release info --------------------
int fetch_latest_release(char *out_tag, size_t tag_size, char *out_asset_url, size_t url_size)
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

    // Parse tag_name
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

    // Parse first asset download URL (browser_download_url)
    char *url_ptr = strstr(chunk.memory, "\"browser_download_url\":\"");
    if (url_ptr) {
        url_ptr += 24;
        char *end = strchr(url_ptr, '"');
        if (end) {
            size_t len = end - url_ptr;
            if (len >= url_size) len = url_size - 1;
            strncpy(out_asset_url, url_ptr, len);
            out_asset_url[len] = 0;
        }
    }

    curl_easy_cleanup(curl);
    free(chunk.memory);
    return 0;
}

// -------------------- RPX Entry --------------------
__asm__(".global __rpx_start\n\t"
      "__rpx_start: b main");

int main(void)
{
    VPADInit();
    curl_global_init(CURL_GLOBAL_DEFAULT);

    ProcUIInit(NULL);

    // Layer and UI elements
    ProcUILayer *layer = ProcUILayerCreate();
    ProcUILayerSet(layer, 0, 0, 1280, 720);

    ProcUIElement *statusText = ProcUITextCreate(layer, "Loading...", 640, 360, 1.0f);
    ProcUIElement *progressBar = ProcUIProgressBarCreate(layer, 440, 400, 400, 20);
    ProcUIProgressBarSetColor(progressBar, 0x00FF00FF);

    ProcUILayerAddElement(layer, statusText);
    ProcUILayerAddElement(layer, progressBar);
    ProcUILayerShow(layer);
    ProcUIProcessMessages(FALSE);

    // Loading screen
    ProcUITextSetString(statusText, "Loading...");
    ProcUIProgressBarSetValue(progressBar, 0.0f);
    ProcUIProcessMessages(FALSE);
    usleep(800000);

    // Check updates
    ProcUITextSetString(statusText, "Checking for updates...");
    ProcUIProgressBarSetValue(progressBar, 0.0f);
    ProcUIProcessMessages(FALSE);

    char latest_tag[64] = {0};
    char asset_url[256] = {0};
    int update_needed = 0;

    if (fetch_latest_release(latest_tag, sizeof(latest_tag), asset_url, sizeof(asset_url)) == 0) {
        if (strcmp(latest_tag, CURRENT_VERSION) != 0 && strlen(asset_url) > 0) {
            update_needed = 1;
        }
    }

    // Download update if needed
    if (update_needed) {
        ProcUITextSetString(statusText, "Downloading update...");
        ProcUIProgressBarSetValue(progressBar, 0.0f);
        ProcUIProcessMessages(FALSE);

        CURL *curl = curl_easy_init();
        if (curl) {
            FILE *fp = fopen("/vol/external_sd/update_file.wuhb", "wb"); // save to SD or USB
            if (fp) {
                struct DownloadProgress prog = {statusText, progressBar, 0, 0};
                curl_easy_setopt(curl, CURLOPT_URL, asset_url);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
                curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
                curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &prog);
                curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                curl_easy_perform(curl);
                fclose(fp);
            }
            curl_easy_cleanup(curl);
        }
    }

    // Final loading
    ProcUITextSetString(statusText, "Loading...");
    ProcUIProgressBarSetValue(progressBar, 1.0f);
    ProcUIProcessMessages(FALSE);
    usleep(500000);

    // Browser UI launch would go here

    while (1) {
        VPADStatus vpad;
        VPADReadError error;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &error);
        usleep(50000);
        ProcUIProcessMessages(FALSE);
    }

    curl_global_cleanup();
    ProcUIShutdown();
    return 0;
}
