#include <whb/log.h>
#include <whb/application.h>
#include <vpad/input.h>
#include <gx2/init.h>
#include <gx2/display.h>
#include <gx2/mem.h>
#include <gx2/shader.h>
#include <gx2/tex.h>
#include <gx2/swap.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "curl/curl.h" // Include libcurl source in repo

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define GITHUB_USER "YourUsername"
#define GITHUB_REPO "wave-browser"
#define CURRENT_VERSION "v0.1"

typedef struct {
    float x, y;
    float width, height;
    float progress; // 0.0 - 1.0
} ProgressBar;

ProgressBar splash_bar = {320, 600, 640, 40, 0.0f};

// ============ GX2 Drawing Functions ============

typedef struct {
    float r,g,b,a;
} Color;

// Draw a rectangle on screen
void draw_rect(float x, float y, float w, float h, Color color) {
    GX2Color clearColor = { (u8)(color.r*255), (u8)(color.g*255), (u8)(color.b*255), (u8)(color.a*255) };
    // Here you would implement real GX2 rectangle rendering
    // For brevity, placeholder: real implementation uses GX2Draw calls
}

// Draw progress bar on splash screen
void draw_progress_bar(ProgressBar *bar) {
    Color background = {0.2f, 0.2f, 0.2f, 1.0f};
    Color fill = {0.0f, 0.8f, 0.0f, 1.0f};

    // Draw background
    draw_rect(bar->x, bar->y, bar->width, bar->height, background);
    // Draw filled progress
    draw_rect(bar->x, bar->y, bar->width*bar->progress, bar->height, fill);
}

// Draw centered text (replace with GX2 text rendering)
void draw_text(const char *text, float x, float y, Color color) {
    // Real GX2 text rendering here
}

// ============ Libcurl Memory Callback ============
struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size*nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct*)userp;
    char *ptr = realloc(mem->memory, mem->size + realsize +1);
    if(!ptr) return 0;
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size]=0;
    return realsize;
}

// ============ GitHub Release Check ============
int fetch_latest_release_tag(char *out_tag, size_t out_size) {
    CURL *curl = curl_easy_init();
    if(!curl) return 1;

    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;

    char url[256];
    snprintf(url,sizeof(url),"https://api.github.com/repos/%s/%s/releases/latest",GITHUB_USER,GITHUB_REPO);

    curl_easy_setopt(curl,CURLOPT_URL,url);
    curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,WriteMemoryCallback);
    curl_easy_setopt(curl,CURLOPT_WRITEDATA,&chunk);
    curl_easy_setopt(curl,CURLOPT_USERAGENT,"wave-browser");

    CURLcode res = curl_easy_perform(curl);
    if(res != CURLE_OK) {
        curl_easy_cleanup(curl);
        free(chunk.memory);
        return 1;
    }

    char *tag_ptr = strstr(chunk.memory,"\"tag_name\":\"");
    if(tag_ptr) {
        tag_ptr += strlen("\"tag_name\":\"");
        char *end_ptr = strchr(tag_ptr,'"');
        if(end_ptr) {
            size_t len = end_ptr - tag_ptr;
            if(len>=out_size) len = out_size-1;
            strncpy(out_tag,tag_ptr,len);
            out_tag[len]=0;
        }
    } else strcpy(out_tag,CURRENT_VERSION);

    curl_easy_cleanup(curl);
    free(chunk.memory);
    return 0;
}

// ============ File Download ============
size_t download_write_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t written = size*nmemb;
    size_t *downloaded = (size_t*)userdata;
    *downloaded += written;
    splash_bar.progress = (float)(*downloaded)/50e6; // Example 50MB file
    draw_progress_bar(&splash_bar);
    return written;
}

int download_file(const char *url, const char *dest_path) {
    CURL *curl;
    FILE *fp;
    CURLcode res;
    size_t downloaded = 0;

    fp = fopen(dest_path,"wb");
    if(!fp) return 1;

    curl = curl_easy_init();
    if(!curl) return 1;

    curl_easy_setopt(curl,CURLOPT_URL,url);
    curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,download_write_callback);
    curl_easy_setopt(curl,CURLOPT_WRITEDATA,&downloaded);
    curl_easy_setopt(curl,CURLOPT_USERAGENT,"wave-browser");
    curl_easy_setopt(curl,CURLOPT_WRITEDATA,fp);

    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    fclose(fp);

    return (res==CURLE_OK)?0:1;
}

// ============ Main Function ============
int main(void) {
    WHBLogInit();
    VPADInit();

    // Splash: Loading
    splash_bar.progress = 0.0f;
    draw_progress_bar(&splash_bar);
    draw_text("Loading...", SCREEN_WIDTH/2, SCREEN_HEIGHT/2, (Color){1,1,1,1});
    usleep(500000);

    // Splash: Checking updates
    draw_text("Checking for updates...", SCREEN_WIDTH/2, SCREEN_HEIGHT/2+50, (Color){1,1,1,1});
    char latest_tag[32];
    if(fetch_latest_release_tag(latest_tag,sizeof(latest_tag))==0) {
        if(strcmp(latest_tag,CURRENT_VERSION)!=0) {
            draw_text("Update found! Downloading...", SCREEN_WIDTH/2, SCREEN_HEIGHT/2+100,(Color){1,1,1,1});
            mkdir("wave-temp",0755);
            char download_url[256];
            snprintf(download_url,sizeof(download_url),
                     "https://github.com/%s/%s/releases/download/%s/wave-browser.rpx",
                     GITHUB_USER,GITHUB_REPO,latest_tag);
            download_file(download_url,"wave-temp/wave-browser.rpx");
            draw_text("Update complete! Please restart the app.", SCREEN_WIDTH/2, SCREEN_HEIGHT/2+150,(Color){1,1,1,1});
        } else draw_text("No updates found.", SCREEN_WIDTH/2, SCREEN_HEIGHT/2+100,(Color){1,1,1,1});
    } else draw_text("Failed to check updates.", SCREEN_WIDTH/2, SCREEN_HEIGHT/2+100,(Color){1,1,1,1});

    draw_text("Launching Roblox login...", SCREEN_WIDTH/2, SCREEN_HEIGHT/2+200,(Color){1,1,1,1});

    // Main loop
    while(1) {
        VPADRead();
        usleep(50000);
    }

    return 0;
}
