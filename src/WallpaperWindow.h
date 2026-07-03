#pragma once
#include <windows.h>

struct WallpaperWindow {
    HWND     hwnd  = nullptr;
    HINSTANCE hInst = nullptr;
    bool     running = false;

    bool Create(const wchar_t* title, int w, int h);
    int  Run();
    void Close();
    HWND Handle() const { return hwnd; }
};
