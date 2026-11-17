#pragma once

#include <SDL.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*UiFocusCallback)(void* user_data);

// Rebuild the current screen's focusable controls once per rendered frame.
// Controls absent from the current screen are automatically removed.
void ui_focus_begin_frame(void);
bool ui_focus_register(int id, int x, int y, int width, int height,
                       UiFocusCallback on_focus,
                       UiFocusCallback on_activate,
                       void* user_data);
void ui_focus_end_frame(void);

// Directional navigation chooses the nearest registered control in that
// direction using its rendered bounds, not a screen-specific row/column map.
bool ui_focus_move(int dx, int dy);
bool ui_focus_activate(void);
bool ui_focus_activate_id(int id);
void ui_focus_set_current(int id);
int  ui_focus_current_id(void);
bool ui_focus_is_focused(int id);
void ui_focus_draw_current(SDL_Renderer* renderer);

#ifdef __cplusplus
}
#endif
