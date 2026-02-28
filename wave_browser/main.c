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

// -------------------- CURL download progress callback --------------------
struct DownloadProgress {
    double total;
    double downloaded;
};

static int ProgressCallback(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow)
{
    struct DownloadProgress *prog = (struct DownloadProgress *)clientp;
    prog->total = dltotal;
    prog->downloaded = dlnow;

    float progress = 0.0f;
    if (dltotal > 0.0)
        progress = (float)(dlnow / dltotal);

    char buf[128];
    snprintf(buf, sizeof(buf), "Downloading update: %.1fMB / %.1fMB", dlnow / (1024*1024.0), dltotal / (1024*1024.0));

    // Show progress and text on screen
    ProcUIShowMessage(buf);
    ProcUIShowProgress(progress);

    ProcUIProcessMessages(TRUE);

    return 0;
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

int main(void)
{
    VPADInit();
    ProcUIInit(NULL);
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Show initial splash
    ProcUIShowMessage("Loading...");
    ProcUIShowProgress(0.0f);
    ProcUIProcessMessages(TRUE);

    // Check for updates
    ProcUIShowMessage("Checking for updates...");
    ProcUIShowProgress(0.0f);
    ProcUIProcessMessages(TRUE);

    char latest_tag[64] = {0};
    if (fetch_latest_release(latest_tag, sizeof(latest_tag)) == 0)
    {
        if (strcmp(latest_tag, CURRENT_VERSION) != 0)
        {
            // Simulate downloading update with CURL
            CURL *curl = curl_easy_init();
            if (curl)
            {
                struct DownloadProgress prog = {0};

                curl_easy_setopt(curl, CURLOPT_URL, "https://example.com/fake_update_file"); // replace with actual asset URL
                curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
                curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
                curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &prog);
                curl_easy_setopt(curl, CURLOPT_USERAGENT, "wave-browser");
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
                struct MemoryStruct dummy = {0};
                dummy.memory = malloc(1);
                dummy.size = 0;
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &dummy);

                curl_easy_perform(curl);
                free(dummy.memory);
                curl_easy_cleanup(curl);
            }
        }
    }

    // Done loading
    ProcUIShowMessage("Loading browser...");
    ProcUIShowProgress(1.0f);
    ProcUIProcessMessages(TRUE);

    // Main loop
    while (1)
    {
        VPADStatus vpad;
        VPADReadError error;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &error);

        // Here, you would launch your actual browser UI

        usleep(50000);
        ProcUIProcessMessages(TRUE);
    }

    curl_global_cleanup();
    ProcUIShutdown();
    return 0;
}
