#include <windows.h>
#include <string>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

namespace wave {

// Minimal Windows Wave address/search bar. Chromium remains the page engine;
// this component only translates user input into a navigation URL.
std::wstring NavigationUrl(const std::wstring& input) {
    if (input.empty()) return L"https://www.google.com/";

    if (input.rfind(L"http://", 0) == 0 || input.rfind(L"https://", 0) == 0)
        return input;

    if (input.find(L' ') == std::wstring::npos && input.find(L'.') != std::wstring::npos)
        return L"https://" + input;

    std::wstring encoded;
    for (wchar_t c : input) {
        if (c == L' ') encoded += L'+';
        else encoded += c;
    }
    return L"https://www.google.com/search?q=" + encoded;
}

}  // namespace wave
