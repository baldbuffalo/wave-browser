#pragma once
#include "tv_remote.h"   // TVRemoteModel lives here now

// ─── Model registry ──────────────────────────────────────────────────────────
// Every brand/year/model.cpp calls MODEL_REGISTER(var) which uses a GCC
// constructor to add itself to the global table at startup — no manual
// table editing needed when adding a new model file.

int                  model_registry_count();
const TVRemoteModel* model_registry_get(int index);

#ifdef __cplusplus
void model_registry_add(const TVRemoteModel* m);

#define MODEL_REGISTER(var)                                      \
    static void __attribute__((constructor)) _reg_##var() {     \
        model_registry_add(&var);                               \
    }
#endif
