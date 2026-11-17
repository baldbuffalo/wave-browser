#include "model_registry.h"
#include <stdlib.h>

// ─── Dynamic table ───────────────────────────────────────────────────────────

static const TVRemoteModel* s_models[512];
static int                  s_count = 0;

void model_registry_add(const TVRemoteModel* m)
{
    if (!m || s_count >= 512) return;
    s_models[s_count++] = m;
}

int model_registry_count() { return s_count; }

const TVRemoteModel* model_registry_get(int index)
{
    if (index < 0 || index >= s_count) return nullptr;
    return s_models[index];
}
