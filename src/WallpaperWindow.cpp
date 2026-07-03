#include "WallpaperWindow.h"
#include <cstdlib>

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            PostQuitMessage(0);
            return 0;
        }
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool WallpaperWindow::Create(const wchar_t* title, int w, int h) {
    hInst = GetModuleHandleW(nullptr);

    WNDCLASSEXW wcx = {};
    wcx.cbSize        = sizeof(WNDCLASSEXW);
    wcx.style         = CS_HREDRAW | CS_VREDRAW;
    wcx.lpfnWndProc   = WndProc;
    wcx.hInstance     = hInst;
    wcx.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wcx.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcx.lpszClassName = L"WEWallpaperWindow";

    if (!RegisterClassExW(&wcx)) return false;

    RECT rc = {0, 0, w, h};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    int winW = rc.right - rc.left;
    int winH = rc.bottom - rc.top;

    hwnd = CreateWindowExW(0, L"WEWallpaperWindow", title,
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, winW, winH,
        nullptr, nullptr, hInst, nullptr);

    if (!hwnd) return false;
    return true;
}

int WallpaperWindow::Run() {
    running = true;
    MSG msg = {};
    while (running) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (running) Sleep(1);
    }
    return (int)msg.wParam;
}

void WallpaperWindow::Close() {
    if (hwnd) {
        DestroyWindow(hwnd);
        hwnd = nullptr;
    }
}
