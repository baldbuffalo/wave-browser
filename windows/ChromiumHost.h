#pragma once

#include <windows.h>
#include <string>

namespace wave {

// Chromium host boundary. The Chromium build supplies the browser/content
// implementation; Wave owns the native desktop window and navigation UI.
class ChromiumHost {
public:
    bool Initialize(HWND parent);
    void Navigate(const std::wstring& url);
    void Resize(int width, int height);
    void Shutdown();

private:
    HWND parent_ = nullptr;
};

}  // namespace wave
