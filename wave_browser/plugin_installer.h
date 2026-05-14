#pragma once
#include "plugin_config.h"
#include "tv_remotes/tv_remote.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define AROMA_PLUGIN_DIR    "fs:/vol/external01/wiiu/environments/aroma/plugins"
#define PLUGIN_INSTALL_PATH "fs:/vol/external01/wiiu/environments/aroma/plugins/WaveBrowserRemote.wps"

#if __has_include("plugin_data.h")
#  include "plugin_data.h"
#  define HAVE_PLUGIN_BINARY 1
#else
#  define HAVE_PLUGIN_BINARY 0
#endif

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

static inline int plugin_install()
{
#if !HAVE_PLUGIN_BINARY
    return -2;
#else
    struct stat st;
    if (stat(AROMA_PLUGIN_DIR, &st) != 0 || !S_ISDIR(st.st_mode)) return -1;
    if (stat(PLUGIN_INSTALL_PATH, &st) == 0 &&
        (size_t)st.st_size == plugin_wps_len) return 0;
    FILE* f = fopen(PLUGIN_INSTALL_PATH, "wb");
    if (!f) return -1;
    bool ok = (fwrite(plugin_wps_data, 1, plugin_wps_len, f) == plugin_wps_len);
    fclose(f);
    return ok ? 1 : -1;
#endif
}
