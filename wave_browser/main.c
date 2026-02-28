#include <coreinit/core.h>
#include <coreinit/filesystem.h>
#include <proc_ui/procui.h>
#include <vpad/input.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>

#define CURRENT_VERSION "v0.1.0"
#define GITHUB_API "https://api.github.com/repos/baldbuffalo/wave-browser/releases/latest"

// -------------------- Memory callback --------------------
struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
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

// -------------------- CURL progress callback --------------------
static int ProgressCallback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    if (dltotal > 0) {
        float progress = (float)dlnow / (float)dltotal;
        char buf[64];
        snprintf(buf, sizeof(buf), "Downloading update: %.1fMB of %.1fMB", dlnow / 1024.0f / 1024.0f, dltotal / 1024.0f / 1024.0f);
        ProcUIDisplayText(buf, 0, 0);
        ProcUIDisplayProgress(progress);
    }
    return 0;
}

// -------------------- Fetch latest release --------------------
int fetch_latest_release(char *out_tag, size_t tag_size, curl_off_t *asset_size, char *asset_url, size_t url_size) {
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

    // Parse first asset url and size (assume first asset)
    char *asset_ptr = strstr(chunk.memory, "\"browser_download_url\":\"");
    if (asset_ptr) {
        asset_ptr += 23;
        char *end = strchr(asset_ptr, '"');
        if (end) {
            size_t len = end - asset_ptr;
            if (len >= url_size) len = url_size - 1;
            strncpy(asset_url, asset_ptr, len);
            asset_url[len] = 0;
        }
    }

    char *size_ptr = strstr(chunk.memory, "\"size\":");
    if (size_ptr) {
        size_ptr += 7;
        *asset_size = strtoll(size_ptr, NULL, 10);
    }

    curl_easy_cleanup(curl);
    free(chunk.memory);
    return 0;
}

// -------------------- Download asset --------------------
int download_asset(const char *url, curl_off_t total_size) {
    CURL *curl = curl_easy_init();
    if (!curl) return 1;

    FILE *fp = tmpfile(); // download to temp memory/file
    if (!fp) return 1;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "wave-browser");

    // Progress function
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, NULL);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

    CURLcode res = curl_easy_perform(curl);
    fclose(fp);
    curl_easy_cleanup(curl);
    return (res == CURLE_OK) ? 0 : 1;
}

// -------------------- RPX Entry Point --------------------
__asm__(".global __rpx_start\n\t"
        "__rpx_start: b main");

int main(void) {
    ProcUIInit(NULL);
    VPADInit();
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // 1️⃣ Splash: Loading...
    ProcUIDisplayText("Loading...", 0, 0);
    ProcUIDisplayProgress(0.2f);
    usleep(500000);

    // 2️⃣ Splash: Checking for updates...
    ProcUIDisplayText("Checking for updates...", 0, 0);
    ProcUIDisplayProgress(0.4f);

    char latest_tag[64] = {0};
    char asset_url[256] = {0};
    curl_off_t asset_size = 0;

    int has_update = 0;
    if (fetch_latest_release(latest_tag, sizeof(latest_tag), &asset_size, asset_url, sizeof(asset_url)) == 0) {
        if (strcmp(latest_tag, CURRENT_VERSION) != 0 && asset_size > 0) {
            has_update = 1;
        }
    }

    // 3️⃣ If update exists → download with real progress
    if (has_update) {
        download_asset(asset_url, asset_size);
    }

    // 4️⃣ Done loading, launch browser UI
    ProcUIDisplayText("Loading browser...", 0, 0);
    ProcUIDisplayProgress(1.0f);
    usleep(300000);

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
