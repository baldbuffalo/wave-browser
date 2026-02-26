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

static size_t WriteMemoryCallback(void *contents, size_t size,
                                  size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct*)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr)
        return 0;

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

int fetch_latest_release(char *out_tag, size_t tag_size)
{
    CURL *curl = curl_easy_init();
    if (!curl)
        return 1;

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
            if (len >= tag_size)
                len = tag_size - 1;

            strncpy(out_tag, tag_ptr, len);
            out_tag[len] = 0;
        }
    }

    curl_easy_cleanup(curl);
    free(chunk.memory);
    return 0;
}

int main(void)
{
    ProcUIInit(NULL);
    VPADInit();
    curl_global_init(CURL_GLOBAL_DEFAULT);

    printf("Wave Browser\n");
    printf("Current version: %s\n", CURRENT_VERSION);
    printf("Checking for updates...\n\n");

    char latest_tag[64] = {0};

    if (fetch_latest_release(latest_tag, sizeof(latest_tag)) == 0) {
        if (strcmp(latest_tag, CURRENT_VERSION) != 0) {
            printf("Update available: %s\n", latest_tag);
        } else {
            printf("You are up to date.\n");
        }
    } else {
        printf("Update check failed.\n");
    }

    while (ProcUIIsRunning())
    {
        VPADStatus vpad;
        VPADReadError error;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &error);

        usleep(50000);
    }

    curl_global_cleanup();
    ProcUIShutdown();
    return 0;
}
