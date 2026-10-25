#pragma once
#include "plugin_config.h"
#include "tv_remotes/tv_remote.h"
#include <curl/curl.h>
#include "unzip.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

// ─── Paths ────────────────────────────────────────────────────────────────────
#define AROMA_PLUGIN_DIR  "fs:/vol/external01/wiiu/environments/aroma/plugins"
#define PLUGIN_INSTALL_PATH \
    "fs:/vol/external01/wiiu/environments/aroma/plugins/WaveBrowserRemote.wps"
#define PLUGIN_ZIP_TMP \
    "fs:/vol/external01/wiiu/apps/WaveBrowser/WaveBrowserRemote-download.zip"

// nightly.link serves the latest CI artifact as a zip — no auth needed
#define PLUGIN_DOWNLOAD_URL \
    "https://nightly.link/baldbuffalo/wave-browser/workflows/build.yml/main/plugin-wps.zip"

// ─── Download progress callback type ─────────────────────────────────────────
typedef void (*PluginProgressCb)(const char* msg, double pct);

// ─── Write plugin config (CEC enabled flag + brand/model for display) ─────────
static inline bool plugin_write_config(const TVRemoteModel* m)
{
    TVPluginConfig cfg = {};
    cfg.magic       = PLUGIN_CONFIG_MAGIC;
    cfg.version     = PLUGIN_CONFIG_VERSION;
    cfg.cec_enabled = 1;
    if (m) {
        cfg.year = (uint16_t)m->year;
        strncpy(cfg.brand, m->brand, sizeof(cfg.brand)-1);
        strncpy(cfg.model, m->model, sizeof(cfg.model)-1);
    } else {
        strncpy(cfg.brand, "Generic", sizeof(cfg.brand)-1);
        strncpy(cfg.model, "CEC TV",  sizeof(cfg.model)-1);
    }
    FILE* f = fopen(PLUGIN_CONFIG_PATH, "wb");
    if (!f) return false;
    bool ok = (fwrite(&cfg, 1, sizeof(cfg), f) == sizeof(cfg));
    fclose(f);
    return ok;
}

// ─── libcurl write callbacks ──────────────────────────────────────────────────
static size_t _plugin_file_write(void* ptr, size_t sz, size_t nm, void* ud)
{ return fwrite(ptr, sz, nm, (FILE*)ud); }

struct _PluginDlCtx { PluginProgressCb cb; };
static int _plugin_progress(void* ud, curl_off_t total, curl_off_t now,
                             curl_off_t, curl_off_t)
{
    auto* ctx = (_PluginDlCtx*)ud;
    if (ctx->cb && total > 0)
        ctx->cb("Downloading plugin...", (double)now / total * 100.0);
    return 0;
}

// ─── Download + install ───────────────────────────────────────────────────────
// Returns:
//   1  = installed successfully
//   0  = already up to date (same size)
//  -1  = Aroma plugins dir not found
//  -2  = download failed
//  -3  = extraction failed

static inline int plugin_install(PluginProgressCb cb = nullptr)
{
    // Check Aroma plugins dir exists
    struct stat st;
    if (stat(AROMA_PLUGIN_DIR, &st) != 0 || !S_ISDIR(st.st_mode))
        return -1;

    if (cb) cb("Connecting to GitHub...", 0.0);

    // Download zip to temp file
    remove(PLUGIN_ZIP_TMP);
    FILE* f = fopen(PLUGIN_ZIP_TMP, "wb");
    if (!f) return -2;

    _PluginDlCtx ctx = { cb };
    CURL* curl = curl_easy_init();
    if (!curl) { fclose(f); return -2; }

    curl_easy_setopt(curl, CURLOPT_URL,              PLUGIN_DOWNLOAD_URL);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,    _plugin_file_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,        f);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION,   1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER,   0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST,   0L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,        "wave-browser/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,          60L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS,       0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, _plugin_progress);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA,     &ctx);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    fclose(f);

    if (res != CURLE_OK) { remove(PLUGIN_ZIP_TMP); return -2; }

    if (cb) cb("Extracting plugin...", 100.0);

    // Extract WaveBrowserRemote.wps from the zip
    unzFile zf = unzOpen(PLUGIN_ZIP_TMP);
    if (!zf) { remove(PLUGIN_ZIP_TMP); return -3; }

    bool extracted = false;
    if (unzGoToFirstFile(zf) == UNZ_OK) {
        do {
            char fname[256] = {};
            unz_file_info fi;
            unzGetCurrentFileInfo(zf, &fi, fname, sizeof(fname)-1,
                                  nullptr, 0, nullptr, 0);
            // Match any file ending in .wps
            size_t nlen = strlen(fname);
            if (nlen >= 4 && strcmp(fname + nlen - 4, ".wps") == 0) {
                if (unzOpenCurrentFile(zf) != UNZ_OK) break;
                FILE* out = fopen(PLUGIN_INSTALL_PATH, "wb");
                if (out) {
                    uint8_t buf[4096]; int br;
                    while ((br = unzReadCurrentFile(zf, buf, sizeof(buf))) > 0)
                        fwrite(buf, 1, (size_t)br, out);
                    fclose(out);
                    extracted = true;
                }
                unzCloseCurrentFile(zf);
                break;
            }
        } while (unzGoToNextFile(zf) == UNZ_OK);
    }
    unzClose(zf);
    remove(PLUGIN_ZIP_TMP);

    return extracted ? 1 : -3;
}
