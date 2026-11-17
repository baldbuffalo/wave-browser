#include "ui_focus.h"

#include <stdint.h>
#include <limits.h>

namespace {
constexpr int MAX_FOCUS_ITEMS = 256;
constexpr int NO_FOCUS_ID = -1;

struct FocusItem {
    int id;
    SDL_Rect bounds;
    UiFocusCallback on_focus;
    UiFocusCallback on_activate;
    void* user_data;
};

FocusItem items[MAX_FOCUS_ITEMS];
int item_count = 0;
int current_id = NO_FOCUS_ID;
int notified_id = NO_FOCUS_ID;

int find_item(int id)
{
    for (int i = 0; i < item_count; ++i)
        if (items[i].id == id) return i;
    return -1;
}

void select_item(int index)
{
    if (index < 0 || index >= item_count) {
        current_id = NO_FOCUS_ID;
        notified_id = NO_FOCUS_ID;
        return;
    }

    current_id = items[index].id;
    if (notified_id != current_id) {
        notified_id = current_id;
        if (items[index].on_focus)
            items[index].on_focus(items[index].user_data);
    }
}

int center_x(const SDL_Rect& r) { return r.x + r.w / 2; }
int center_y(const SDL_Rect& r) { return r.y + r.h / 2; }
}

extern "C" {

void ui_focus_begin_frame(void)
{
    item_count = 0;
}

bool ui_focus_register(int id, int x, int y, int width, int height,
                       UiFocusCallback on_focus,
                       UiFocusCallback on_activate,
                       void* user_data)
{
    if (id == NO_FOCUS_ID || width <= 0 || height <= 0) return false;

    int index = find_item(id);
    if (index < 0) {
        if (item_count >= MAX_FOCUS_ITEMS) return false;
        index = item_count++;
    }

    items[index].id = id;
    items[index].bounds = {x, y, width, height};
    items[index].on_focus = on_focus;
    items[index].on_activate = on_activate;
    items[index].user_data = user_data;
    return id == current_id;
}

void ui_focus_end_frame(void)
{
    if (item_count == 0) {
        current_id = NO_FOCUS_ID;
        notified_id = NO_FOCUS_ID;
        return;
    }

    int current = find_item(current_id);
    if (current < 0) {
        // A screen change or removed control invalidated the old focus.
        // Start at the first control registered by the new screen.
        select_item(0);
    } else if (notified_id != current_id) {
        select_item(current);
    }
}

bool ui_focus_move(int dx, int dy)
{
    if ((dx == 0 && dy == 0) || item_count == 0) return false;
    int source = find_item(current_id);
    if (source < 0) return false;

    const SDL_Rect& from = items[source].bounds;
    const int sx = center_x(from);
    const int sy = center_y(from);
    long long best_score = LLONG_MAX;
    int best = -1;

    for (int i = 0; i < item_count; ++i) {
        if (i == source) continue;
        const SDL_Rect& to = items[i].bounds;
        int vx = center_x(to) - sx;
        int vy = center_y(to) - sy;

        int primary = (dx != 0) ? vx * dx : vy * dy;
        if (primary <= 0) continue;
        int perpendicular = (dx != 0) ? (vy < 0 ? -vy : vy)
                                      : (vx < 0 ? -vx : vx);
        long long score = (long long)primary + (long long)perpendicular * 3;
        if (score < best_score) {
            best_score = score;
            best = i;
        }
    }

    if (best < 0) return false;
    select_item(best);
    return true;
}

bool ui_focus_activate(void)
{
    int index = find_item(current_id);
    if (index < 0 || !items[index].on_activate) return false;
    items[index].on_activate(items[index].user_data);
    return true;
}

bool ui_focus_activate_id(int id)
{
    int index = find_item(id);
    if (index < 0) return false;
    select_item(index);
    return ui_focus_activate();
}

void ui_focus_set_current(int id)
{
    if (id == NO_FOCUS_ID) return;
    current_id = id;
    int index = find_item(id);
    if (index >= 0) select_item(index);
}

int ui_focus_current_id(void) { return current_id; }

bool ui_focus_is_focused(int id) { return current_id == id; }

void ui_focus_draw_current(SDL_Renderer* renderer)
{
    if (!renderer) return;
    int index = find_item(current_id);
    if (index < 0) return;

    const SDL_Rect& r = items[index].bounds;
    SDL_SetRenderDrawColor(renderer, 0x42, 0x85, 0xF4, 0xFF);
    for (int t = 0; t < 2; ++t) {
        SDL_Rect outline = {r.x - 2 + t, r.y - 2 + t,
                            r.w + 4 - t * 2, r.h + 4 - t * 2};
        SDL_RenderDrawRect(renderer, &outline);
    }
}

}
