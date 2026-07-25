#pragma once

#ifdef __APPLE__
// macOS: overlay discovers GL context from inside the hook — no window handle needed
bool overlay_init();
#elif defined(_WIN32)
#include <windows.h>
bool overlay_init(HWND hwnd);
#endif

void overlay_shutdown();
