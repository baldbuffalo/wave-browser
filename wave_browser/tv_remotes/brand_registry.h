#pragma once

#include "brands.h"

// ─── Global brand registry ───────────────────────────────────────────────────
// Assembled in brand_registry.cpp from the individual per-brand .cpp files.
// Indexed identically to the string names exposed through tv_remote.h.

extern const BrandEntry* const TV_BRANDS[];
extern const int               TV_BRAND_COUNT;
