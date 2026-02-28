#include <coreinit/core.h>
#include <coreinit/debug.h>
#include <coreinit/filesystem.h>
#include <proc_ui/procui.h>
#include <vpad/input.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>

#define GITHUB_API "https://api.github.com/repos/baldbuffalo/wave-browser/releases/latest"

__asm__(".global __rpx_start\n\t"
        "__rpx_start: b main");

// -------------------- CURL Memory Struct --------------------
struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
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

// -------------------- Read version from meta.xml --------------------
int read_local_version(char *out_version, size_t size)
{
    FILE *file = fopen("meta.xml", "r");
    if (!file)
        return 1;

    char buffer[2048];
    size_t len = fread(buffer, 1, sizeof(buffer) - 1, file);
    buffer[len] = 0;
    fclose(file);

    char *ver_ptr = strstr(buffer, "<version>");
    if (!ver_ptr)
        return 1;

    ver_ptr += 9;
    char *end = strstr(ver_ptr, "</version>");
    if (!end)
        return 1;

    size_t ver_len = end - ver_ptr;
    if (ver_len >= size)
        ver_len = size - 1;

    strncpy(out_version, ver_ptr, ver_len);
    out_version[ver_len] = 0;

    return 0;
}

// -------------------- Normalize Version --------------------
// Removes leading 'v' if present
void normalize_version(char *version)
{
    if (version[0] == 'v' || version[0] == 'V') {
        memmove(version, version + 1, strlen(version));
    }
}

// -------------------- Fetch latest GitHub release --------------------
int fetch_latest_release(char *out_tag, size_t tag_size)
{
    CURL *curl = curl_easy_init();
    if (!curl)
        return 1;

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

// -------------------- MAIN --------------------
int main(void)
{
    ProcUIInit(NULL);
    VPADInit();
    curl_global_init(CURL_GLOBAL_DEFAULT);

    OSReport("Wave Browser starting...\n");

    char local_version[64] = {0};
    char latest_version[64] = {0};

    if (read_local_version(local_version, sizeof(local_version)) != 0) {
        OSReport("Failed to read local version.\n");
    } else {
        normalize_version(local_version);
        OSReport("Local version: %s\n", local_version);
    }

    if (fetch_latest_release(latest_version, sizeof(latest_version)) == 0) {
        normalize_version(latest_version);
        OSReport("Latest version: %s\n", latest_version);

        if (strcmp(local_version, latest_version) != 0) {
            OSReport("Update available!\n");
        } else {
            OSReport("App is up to date.\n");
        }
    } else {
        OSReport("Failed to check GitHub release.\n");
    }

    // Basic loop
    while (ProcUIIsRunning()) {
        VPADStatus vpad;
        VPADReadError error;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &error);

        ProcUIProcessMessages(TRUE);
        usleep(16000);
    }

    curl_global_cleanup();
    ProcUIShutdown();
    return 0;
}
