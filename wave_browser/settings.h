#pragma once

struct WaveSettings {
    bool improved_multitasking = false;
};

extern WaveSettings g_settings;

void settings_load();
void settings_save();
