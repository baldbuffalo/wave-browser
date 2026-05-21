#pragma once
#include <wkc/wkcbase.h>

enum {
    WKC_MEDIA_UISTRING_BROADCAST = 0,
    WKC_MEDIA_UISTRING_LOADING   = 1,
};

#ifdef __cplusplus
struct WKCMediaPlayerProcs;
struct WKCMediaPlayerCallbacks;
#else
typedef struct WKCMediaPlayerProcs WKCMediaPlayerProcs;
typedef struct WKCMediaPlayerCallbacks WKCMediaPlayerCallbacks;
#endif
