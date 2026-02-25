#include <whb/proc.h>
#include <vpad/input.h>
#include <gx2/display.h>
#include <gx2/mem.h>
#include <gx2/shaders.h>
#include <gx2/texture.h>
#include <gx2/swap.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <curl/curl.h>

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define CURRENT_VERSION "v0.1.0"
#define GITHUB_API "https://api.github.com/repos/baldbuffalo/wave-browser/releases/latest"

typedef struct {
    float x, y, width, height;
    float progress;
} ProgressBar;

typedef struct {
    float r,g,b,a;
} Color;

ProgressBar splash_bar = {320, 600, 640, 40, 0.0f};

void draw_rect(float x, float y, float w, float h, Color color) {
    (void)x; (void)y; (void)w; (void)h; (void)color;
}

void draw_progress_bar(ProgressBar *bar) {
    Color background = {0.2f,0.2f,0.2f,1};
    Color fill = {0.0f,0.8f,0.0f,1};
    draw_rect(bar->x, bar->y, bar->width, bar->height, background);
    draw_rect(bar->x, bar->y, bar->width * bar->progress, bar->height, fill);
}

void draw_text(const char *text, float x, float y, Color color) {
    (void)text; (void)x; (void)y; (void)color;
}

struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct*)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if(!ptr) return 0;

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

int fetch_latest_release(char *out_tag, size_t tag_size, char *out_url, size_t url_size) {
    CURL *curl = curl_easy_init();
    if(!curl) return 1;

    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;

    curl_easy_setopt(curl, CURLOPT_URL, GITHUB_API);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "wave-browser");

    CURLcode res = curl_easy_perform(curl);
    if(res != CURLE_OK) {
        curl_easy_cleanup(curl);
        free(chunk.memory);
        return 1;
    }

    char *tag_ptr = strstr(chunk.memory, "\"tag_name\":\"");
    if(tag_ptr) {
        tag_ptr += 12;
        char *end = strchr(tag_ptr,'"');
        if(end) {
            size_t len = end - tag_ptr;
            if(len >= tag_size) len = tag_size - 1;
            strncpy(out_tag, tag_ptr, len);
            out_tag[len] = 0;
        }
    }

    char *url_ptr = strstr(chunk.memory, "\"browser_download_url\":\"");
    if(url_ptr) {
        url_ptr += 23;
        char *end = strchr(url_ptr,'"');
        if(end) {
            size_t len = end - url_ptr;
            if(len >= url_size) len = url_size - 1;
            strncpy(out_url, url_ptr, len);
            out_url[len] = 0;
        }
    }

    curl_easy_cleanup(curl);
    free(chunk.memory);
    return 0;
}

typedef struct {
    FILE *fp;
    curl_off_t total;
    curl_off_t downloaded;
} DownloadContext;

static int progress_callback(void *p,
    curl_off_t dltotal,
    curl_off_t dlnow,
    curl_off_t ultotal,
    curl_off_t ulnow) {

    DownloadContext *ctx = (DownloadContext*)p;

    ctx->total = dltotal;
    ctx->downloaded = dlnow;

    if(dltotal > 0) {
        splash_bar.progress = (float)dlnow / (float)dltotal;
        if(splash_bar.progress > 1.0f)
            splash_bar.progress = 1.0f;

        draw_progress_bar(&splash_bar);
    }

    (void)ultotal;
    (void)ulnow;

    return 0;
}

int download_file(const char *url, const char *dest) {
    CURL *curl = curl_easy_init();
    if(!curl) return 1;

    FILE *fp = fopen(dest,"wb");
    if(!fp) {
        curl_easy_cleanup(curl);
        return 1;
    }

    DownloadContext ctx;
    ctx.fp = fp;
    ctx.total = 0;
    ctx.downloaded = 0;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "wave-browser");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);

    CURLcode res = curl_easy_perform(curl);

    fclose(fp);
    curl_easy_cleanup(curl);

    if(res != CURLE_OK) {
        remove(dest);
        return 1;
    }

    return 0;
}

int main(void) {

    WHBProcInit();
    VPADInit();
    curl_global_init(CURL_GLOBAL_DEFAULT);

    draw_text("Wave Browser", SCREEN_WIDTH/2, SCREEN_HEIGHT/2, (Color){1,1,1,1});
    usleep(500000);

    draw_text("Checking for updates...", SCREEN_WIDTH/2, SCREEN_HEIGHT/2+50, (Color){1,1,1,1});

    char latest_tag[64] = {0};
    char download_url[256] = {0};

    if(fetch_latest_release(latest_tag, sizeof(latest_tag),
                            download_url, sizeof(download_url)) == 0) {

        if(strcmp(latest_tag, CURRENT_VERSION) != 0) {

            draw_text("Update found. Downloading...", SCREEN_WIDTH/2,
                      SCREEN_HEIGHT/2+100, (Color){1,1,1,1});

            mkdir("wave-update",0755);
            splash_bar.progress = 0.0f;

            if(download_file(download_url, "wave-update/wave-browser.rpx") == 0) {
                draw_text("Download complete. Restart app.",
                          SCREEN_WIDTH/2, SCREEN_HEIGHT/2+150,
                          (Color){1,1,1,1});
            } else {
                draw_text("Download failed.",
                          SCREEN_WIDTH/2, SCREEN_HEIGHT/2+150,
                          (Color){1,1,1,1});
            }

        } else {
            draw_text("You are up to date.",
                      SCREEN_WIDTH/2, SCREEN_HEIGHT/2+100,
                      (Color){1,1,1,1});
        }

    } else {
        draw_text("Update check failed.",
                  SCREEN_WIDTH/2, SCREEN_HEIGHT/2+100,
                  (Color){1,1,1,1});
    }

    while(WHBProcIsRunning()) {
        VPADStatus vpad;
        VPADReadError error;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &error);
        (void)vpad;
        (void)error;
        usleep(50000);
    }

    curl_global_cleanup();
    WHBProcShutdown();
    return 0;
}
