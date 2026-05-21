#pragma once
#include <wkc/wkcbase.h>

// Clipboard format flags — used by wkcPasteboardIsFormatAvailablePeer
enum {
    WKC_CLIPBOARD_FORMAT_TEXT        = 0x01,
    WKC_CLIPBOARD_FORMAT_HTML        = 0x02,
    WKC_CLIPBOARD_FORMAT_IMAGE       = 0x04,
    WKC_CLIPBOARD_FORMAT_URI         = 0x08,
    WKC_CLIPBOARD_FORMAT_SMART_PASTE = 0x10,
};
