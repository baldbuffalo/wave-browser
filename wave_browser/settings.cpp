#include "settings.h"
#include <stdio.h>
#include <string.h>

#define SETTINGS_PATH "fs:/vol/external01/wave-browser-settings.cfg"

WaveSettings g_settings;

void settings_load() {
    FILE* f = fopen(SETTINGS_PATH, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        int val;
        if (sscanf(line, "improved_multitasking=%d", &val) == 1)
            g_settings.improved_multitasking = (val != 0);
    }
    fclose(f);
}

void settings_save() {
    FILE* f = fopen(SETTINGS_PATH, "w");
    if (!f) return;
    fprintf(f, "improved_multitasking=%d\n", g_settings.improved_multitasking ? 1 : 0);
    fclose(f);
}
