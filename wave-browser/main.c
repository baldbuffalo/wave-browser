#include <whb/log.h>
#include <whb/application.h>
#include <vpad/input.h>
#include <gx2/init.h>
#include <gx2/display.h>
#include <gx2/mem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define REPO_USER "YourUsername"
#define REPO_NAME "wave-browser"
#define CURRENT_VERSION "v0.1"

// --- Progress bar struct ---
typedef struct {
    float x, y;
    float width, height;
    float progress; // 0.0 - 1.0
} ProgressBar;

ProgressBar splash_bar = {320, 600, 640, 40, 0.0f};

// --- Splash screen drawing ---
void draw_text(const char *text) {
    WHBLogPrintf("%s\n", text); // Replace with GX2 text rendering later
}

void draw_progress_bar(ProgressBar *bar) {
    // Outer border
    WHBLogPrintf("Draw border at %.1f,%.1f size %.1fx%.1f\n",
                 bar->x - 5, bar->y - 5, bar->width + 10, bar->height + 10);

    // Background
    WHBLogPrintf("Draw background at %.1f,%.1f size %.1fx%.1f\n",
                 bar->x, bar->y, bar->width, bar->height);

    // Progress fill (green)
    WHBLogPrintf("Draw progress at %.1f,%.1f size %.1fx%.1f progress %.2f\n",
                 bar->x, bar->y, bar->width * bar->progress, bar->height, bar->progress);
}

// --- Download progress callback ---
void download_progress(size_t downloaded, size_t total) {
    splash_bar.progress = (float)downloaded / (float)total;
    char buf[128];
    sprintf(buf, "Updating: %.2f / %.2f MB", downloaded / 1e6, total / 1e6);
    draw_text(buf);
    draw_progress_bar(&splash_bar);
}

// --- Placeholder HTTP + JSON functions ---
// Replace these with actual libcurl / WUT networking + cJSON for GitHub API
int fetch_latest_release_tag(const char *user, const char *repo, char *out_tag, size_t out_size) {
    // Example: simulate version "v0.2" from GitHub
    strncpy(out_tag, "v0.2", out_size);
    return 0; // 0 = success
}

int download_file(const char *url, const char *dest_path, void (*progress_cb)(size_t, size_t)) {
    // Example: simulate a 50 MB download
    size_t total = 50 * 1024 * 1024;
    size_t downloaded = 0;
    size_t step = 1024 * 1024; // 1 MB steps
    while (downloaded < total) {
        downloaded += step;
        if (downloaded > total) downloaded = total;
        progress_cb(downloaded, total);
        usleep(50000); // simulate network delay
    }
    return 0;
}

int main(void) {
    WHBLogInit();
    VPADInit();

    draw_text("Loading...");
    for (int i = 0; i <= 10; i++) {
        splash_bar.progress = i / 10.0f;
        draw_progress_bar(&splash_bar);
        usleep(100000);
    }

    draw_text("Checking for updates...");
    char latest_tag[16];
    int has_update = 0;

    if (fetch_latest_release_tag(REPO_USER, REPO_NAME, latest_tag, sizeof(latest_tag)) == 0) {
        if (strcmp(latest_tag, CURRENT_VERSION) != 0) {
            has_update = 1;
            draw_text("Update found!");

            // Ensure temp folder exists
            struct stat st = {0};
            if (stat("sd:/wiiu/apps/wave-temp", &st) == -1) {
                mkdir("sd:/wiiu/apps/wave-temp", 0777);
            }

            // Download main RPX
            char url[256];
            snprintf(url, sizeof(url),
                     "https://github.com/%s/%s/releases/download/%s/wave.rpx",
                     REPO_USER, REPO_NAME, latest_tag);
            download_file(url, "sd:/wiiu/apps/wave-temp/wave.rpx", download_progress);

            // Download updater RPX
            snprintf(url, sizeof(url),
                     "https://github.com/%s/%s/releases/download/%s/wave-updater.rpx",
                     REPO_USER, REPO_NAME, latest_tag);
            download_file(url, "sd:/wiiu/apps/wave-temp/wave-updater.rpx", download_progress);

            draw_text("Update complete! Press Relaunch to apply.");
        }
    }

    if (!has_update) {
        draw_text("Wave is up to date!");
    }

    WHBLogDeinit();
    return 0;
}
