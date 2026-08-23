#include <windows.h>
#include <string>

namespace wave {
std::wstring NavigationUrl(const std::wstring& input);
}

static HWND g_addressBar = nullptr;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_addressBar = CreateWindowExW(
                0, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                12, 12, 760, 32,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            SendMessageW(g_addressBar, EM_SETCUEBANNER, TRUE,
                         reinterpret_cast<LPARAM>(L"Search or enter address"));
            return 0;
        }
        case WM_SIZE:
            if (g_addressBar) {
                MoveWindow(g_addressBar, 12, 12,
                           LOWORD(lParam) > 24 ? LOWORD(lParam) - 24 : 1, 32, TRUE);
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    const wchar_t className[] = L"WaveBrowserWindow";
    WNDCLASSW wc{};
    wc.hInstance = instance;
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = className;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0, className, L"Wave Browser",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1100, 800,
        nullptr, nullptr, instance, nullptr);

    if (!hwnd) return 1;
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
