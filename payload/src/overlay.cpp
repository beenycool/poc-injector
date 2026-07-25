#ifdef _WIN32
#include <cstdio>
#include <windows.h>
#include <GL/gl.h>

#ifndef GL_SHADING_LANGUAGE_VERSION
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C
#endif
#ifndef GL_FRAMEBUFFER_BINDING
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#endif

#include <MinHook.h>

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_opengl3.h"

#include "shared_state.h"
#include "gui.h"
#include "overlay.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

typedef BOOL (WINAPI *SwapBuffers_t)(HDC);
static SwapBuffers_t original_SwapBuffers = nullptr;

struct OverlayState {
    bool    initialized       = false;
    HWND    hwnd              = nullptr;
    WNDPROC original_wndproc  = nullptr;
    ImGuiContext* ctx         = nullptr;
};

static OverlayState g_ol;

#define OVERLAY_LOG(fmt, ...) do {                                      \
    char _buf[512];                                                     \
    int _n = snprintf(_buf, sizeof(_buf), "[OVERLAY] " fmt "\n",        \
                      ##__VA_ARGS__);                                   \
    if (_n > 0) {                                                       \
        fprintf(stderr, "%s", _buf); fflush(stderr);                    \
        OutputDebugStringA(_buf);                                       \
    }                                                                   \
} while(0)

#define OVERLAY_LOG_ERR(fmt, ...) do {                                  \
    char _buf[512];                                                     \
    int _n = snprintf(_buf, sizeof(_buf), "[OVERLAY] ERROR: " fmt "\n", \
                      ##__VA_ARGS__);                                   \
    if (_n > 0) {                                                       \
        fprintf(stderr, "%s", _buf); fflush(stderr);                    \
        OutputDebugStringA(_buf);                                       \
    }                                                                   \
} while(0)

static LRESULT CALLBACK overlay_wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

//============================================================================
// Toggle helper — thread-safe, handles cursor visibility
//============================================================================
static int g_cursorShowCount = 0;

static void toggle_menu() {
    bool newState = !g_state.menuOpen;
    EnterCriticalSection(&g_state.cs);
    g_state.menuOpen = newState;
    LeaveCriticalSection(&g_state.cs);

    if (newState) {
        g_cursorShowCount = 0;
        while (ShowCursor(TRUE) < 0)
            g_cursorShowCount++;
        g_cursorShowCount++;

        ClipCursor(nullptr);
    } else {
        for (int i = 0; i < g_cursorShowCount; i++)
            ShowCursor(FALSE);
        g_cursorShowCount = 0;

        RECT rc;
        if (GetClientRect(g_ol.hwnd, &rc)) {
            POINT pt = { rc.left, rc.top };
            ClientToScreen(g_ol.hwnd, &pt);
            rc.left = pt.x; rc.top = pt.y;
            pt.x = rc.right; pt.y = rc.bottom;
            ClientToScreen(g_ol.hwnd, &pt);
            rc.right = pt.x; rc.bottom = pt.y;
            ClipCursor(&rc);
        }
    }
}

static BOOL WINAPI hook_wglSwapBuffers(HDC hdc) {
    if (!g_ol.initialized) {
        //======================================================================
        // First call — one-time init
        //======================================================================
        OVERLAY_LOG("=== hook_wglSwapBuffers FIRST CALL ===");

        g_ol.hwnd = WindowFromDC(hdc);
        OVERLAY_LOG("WindowFromDC(hdc) returned 0x%p", g_ol.hwnd);
        if (!g_ol.hwnd) {
            OVERLAY_LOG("WindowFromDC returned null, passing through");
            return original_SwapBuffers(hdc);
        }

        // Window info
        LRESULT pre_wndproc = GetWindowLongPtrW(g_ol.hwnd, GWLP_WNDPROC);
        OVERLAY_LOG("Pre-subclass wndproc=0x%p", (void*)pre_wndproc);

        DWORD pid = 0;
        DWORD tid = GetWindowThreadProcessId(g_ol.hwnd, &pid);
        OVERLAY_LOG("Window PID=%lu TID=%lu", pid, tid);

        char class_buf[256] = "?";
        GetClassNameA(g_ol.hwnd, class_buf, sizeof(class_buf));
        OVERLAY_LOG("Window class: %s", class_buf);

        char title_buf[256] = "?";
        GetWindowTextA(g_ol.hwnd, title_buf, sizeof(title_buf));
        OVERLAY_LOG("Window title: %s", title_buf);

        RECT rc = {};
        GetClientRect(g_ol.hwnd, &rc);
        OVERLAY_LOG("Client rect: %ldx%ld", rc.right - rc.left, rc.bottom - rc.top);

        LONG style = (LONG)GetWindowLongPtrW(g_ol.hwnd, GWL_STYLE);
        LONG exstyle = (LONG)GetWindowLongPtrW(g_ol.hwnd, GWL_EXSTYLE);
        OVERLAY_LOG("Style=0x%08lX ExStyle=0x%08lX", style, exstyle);

        // Subclass WndProc
        g_ol.original_wndproc = (WNDPROC)SetWindowLongPtrW(
            g_ol.hwnd, GWLP_WNDPROC, (LONG_PTR)overlay_wndproc);
        OVERLAY_LOG("WndProc subclassed (old=0x%p, new=overlay_wndproc=0x%p)",
                    g_ol.original_wndproc, overlay_wndproc);

        // Verify subclass
        LRESULT post_wndproc = GetWindowLongPtrW(g_ol.hwnd, GWLP_WNDPROC);
        if (post_wndproc != (LONG_PTR)overlay_wndproc) {
            OVERLAY_LOG_ERR("WndProc verify FAILED! Got 0x%p, re-applying...", (void*)post_wndproc);
            SetWindowLongPtrW(g_ol.hwnd, GWLP_WNDPROC, (LONG_PTR)overlay_wndproc);
            LRESULT retry = GetWindowLongPtrW(g_ol.hwnd, GWLP_WNDPROC);
            OVERLAY_LOG("WndProc re-apply result: 0x%p (expected 0x%p, match=%d)",
                        (void*)retry, overlay_wndproc, (retry == (LONG_PTR)overlay_wndproc));
        } else {
            OVERLAY_LOG("WndProc subclass verified OK");
        }

        // GL info
        const char* gl_vendor   = (const char*)glGetString(GL_VENDOR);
        const char* gl_renderer = (const char*)glGetString(GL_RENDERER);
        const char* gl_version  = (const char*)glGetString(GL_VERSION);
        const char* glsl_version = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
        OVERLAY_LOG("GL_VENDOR: %s", gl_vendor ? gl_vendor : "NULL");
        OVERLAY_LOG("GL_RENDERER: %s", gl_renderer ? gl_renderer : "NULL");
        OVERLAY_LOG("GL_VERSION: %s", gl_version ? gl_version : "NULL");
        OVERLAY_LOG("GLSL_VERSION: %s", glsl_version ? glsl_version : "NULL");

        GLint viewport[4] = {};
        glGetIntegerv(GL_VIEWPORT, viewport);
        OVERLAY_LOG("Viewport: %d,%d %dx%d", viewport[0], viewport[1], viewport[2], viewport[3]);

        GLint fb = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fb);
        OVERLAY_LOG("Current FBO: %d", fb);

        GLint tex = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex);
        OVERLAY_LOG("Current texture: %d", tex);

        // Init ImGui
        OVERLAY_LOG("Creating ImGui context...");
        IMGUI_CHECKVERSION();
        g_ol.ctx = ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.IniFilename = nullptr;
        ImGui::StyleColorsDark();
        OVERLAY_LOG("ImGui context created (0x%p)", g_ol.ctx);

        if (!ImGui_ImplWin32_Init(g_ol.hwnd)) {
            OVERLAY_LOG_ERR("ImGui_ImplWin32_Init failed");
            return original_SwapBuffers(hdc);
        }
        OVERLAY_LOG("ImGui_ImplWin32_Init OK");

        if (!ImGui_ImplOpenGL3_Init()) {
            OVERLAY_LOG_ERR("ImGui_ImplOpenGL3_Init failed");
            return original_SwapBuffers(hdc);
        }
        OVERLAY_LOG("ImGui_ImplOpenGL3_Init OK");

        GLenum gle = glGetError();
        if (gle != GL_NO_ERROR) {
            OVERLAY_LOG("GL error after ImGui init: 0x%04X", gle);
        }

        g_ol.initialized = true;
        OVERLAY_LOG("=== ImGui initialization complete, hwnd=0x%p ===", g_ol.hwnd);
    }

    //========================================================================
    // Per-frame: Re-apply WndProc subclass if overwritten by Lunar Client
    //========================================================================
    {
        LRESULT cur = GetWindowLongPtrW(g_ol.hwnd, GWLP_WNDPROC);
        if (cur != (LONG_PTR)overlay_wndproc) {
            OVERLAY_LOG("WndProc overwritten! Got 0x%p, expected 0x%p. Re-applying...",
                        (void*)cur, overlay_wndproc);
            g_ol.original_wndproc = (WNDPROC)SetWindowLongPtrW(
                g_ol.hwnd, GWLP_WNDPROC, (LONG_PTR)overlay_wndproc);
            OVERLAY_LOG("Re-applied. New original_wndproc=0x%p", g_ol.original_wndproc);
        }
    }

    //========================================================================
    // Per-frame: Right Shift toggle via GetAsyncKeyState (bypasses WndProc)
    //========================================================================
    {
        static bool prev_rshift = false;
        bool cur_rshift = (GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0;
        if (cur_rshift && !prev_rshift) {
            toggle_menu();
            bool nowOpen = false;
            EnterCriticalSection(&g_state.cs);
            nowOpen = g_state.menuOpen;
            LeaveCriticalSection(&g_state.cs);
            OVERLAY_LOG("GetAsyncKeyState toggle: menuOpen=%d", (int)nowOpen);
        }
        prev_rshift = cur_rshift;
    }

    //========================================================================
    // Per-frame: Frame counter & periodic diagnostics
    //========================================================================
    {
        static int frame_count = 0;
        frame_count++;
        if (frame_count % 300 == 0) {
            OVERLAY_LOG("Frame %d: wndproc=0x%p (ours=%d) menuOpen=%d",
                        frame_count,
                        (void*)GetWindowLongPtrW(g_ol.hwnd, GWLP_WNDPROC),
                        (GetWindowLongPtrW(g_ol.hwnd, GWLP_WNDPROC) == (LONG_PTR)overlay_wndproc) ? 1 : 0,
                        (int)g_state.menuOpen);
        }
    }

    //========================================================================
    // ImGui frame
    //========================================================================
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    gui_render();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Check GL errors after rendering
    {
        static bool reported_gl_err = false;
        if (!reported_gl_err) {
            GLenum gle = glGetError();
            if (gle != GL_NO_ERROR) {
                OVERLAY_LOG("GL error after ImGui render: 0x%04X", gle);
            }
            reported_gl_err = true;
        }
    }

    return original_SwapBuffers(hdc);
}

//============================================================================
// WndProc — right-shift toggle (fallback) + ImGui input forwarding
//============================================================================
static LRESULT CALLBACK overlay_wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Log first few messages we receive
    {
        static int msg_count = 0;
        if (msg_count < 20) {
            OVERLAY_LOG("WndProc msg[%d]: hwnd=0x%p msg=0x%04X wParam=0x%p lParam=0x%p",
                        msg_count, hwnd, msg, (void*)wParam, (void*)lParam);
            msg_count++;
        }
    }

    // Toggle menu on Right Shift via WndProc
    if (msg == WM_KEYDOWN && wParam == VK_SHIFT && (lParam & (1 << 24))) {
        toggle_menu();
        OVERLAY_LOG("WndProc WM_KEYDOWN: Right Shift, menuOpen=%d", (int)g_state.menuOpen);
        return 0;
    }

    // When menu is open, ImGui processes input; block mouse+keyboard from game
    if (g_state.menuOpen) {
        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
            return 1;
        if ((msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) ||
            msg == WM_KEYDOWN || msg == WM_KEYUP ||
            msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP ||
            msg == WM_CHAR) {
            return 1;
        }
    }

    if (g_ol.original_wndproc) {
        return CallWindowProcW(g_ol.original_wndproc, hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

//============================================================================
// Public API
//============================================================================
bool overlay_init(HWND hwnd) {
    OVERLAY_LOG("overlay_init entered, hwnd=0x%p", hwnd);

    int mh = MH_Initialize();
    if (mh != MH_OK) {
        OVERLAY_LOG_ERR("MH_Initialize failed (MH_STATUS=%d)", mh);
        return false;
    }
    OVERLAY_LOG("MinHook initialized");

    HMODULE opengl32 = GetModuleHandleA("opengl32.dll");
    if (!opengl32) {
        OVERLAY_LOG_ERR("opengl32.dll not found (GLE=%lu)", GetLastError());
        return false;
    }
    OVERLAY_LOG("opengl32.dll at 0x%p", opengl32);

    // Scan all wgl* exports for debugging
    OVERLAY_LOG("Scanning opengl32.dll wgl* exports...");
    {
        PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)opengl32;
        PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((BYTE*)opengl32 + dos->e_lfanew);
        PIMAGE_DATA_DIRECTORY export_dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (export_dir->Size > 0) {
            PIMAGE_EXPORT_DIRECTORY exports = (PIMAGE_EXPORT_DIRECTORY)((BYTE*)opengl32 + export_dir->VirtualAddress);
            DWORD* names = (DWORD*)((BYTE*)opengl32 + exports->AddressOfNames);
            WORD* ordinals = (WORD*)((BYTE*)opengl32 + exports->AddressOfNameOrdinals);
            DWORD* funcs = (DWORD*)((BYTE*)opengl32 + exports->AddressOfFunctions);
            for (DWORD i = 0; i < exports->NumberOfNames; i++) {
                const char* name = (const char*)opengl32 + names[i];
                if (strncmp(name, "wgl", 3) == 0) {
                    OVERLAY_LOG("  Export[%d]: %s at 0x%p", (int)i, name,
                               (void*)((BYTE*)opengl32 + funcs[ordinals[i]]));
                }
            }
        }
    }

    void* target = (void*)GetProcAddress(opengl32, "wglSwapBuffers");
    if (!target) {
        OVERLAY_LOG_ERR("wglSwapBuffers not found in opengl32 (GLE=%lu)", GetLastError());
        return false;
    }
    OVERLAY_LOG("wglSwapBuffers at 0x%p", target);

    mh = MH_CreateHook(target, hook_wglSwapBuffers,
                       (void**)&original_SwapBuffers);
    if (mh != MH_OK) {
        OVERLAY_LOG_ERR("MH_CreateHook failed (MH_STATUS=%d)", mh);
        return false;
    }
    OVERLAY_LOG("Hook created on wglSwapBuffers");

    mh = MH_EnableHook(target);
    if (mh != MH_OK) {
        OVERLAY_LOG_ERR("MH_EnableHook failed (MH_STATUS=%d)", mh);
        return false;
    }

    g_ol.hwnd = hwnd;
    OVERLAY_LOG("Hook enabled and active on wglSwapBuffers, waiting for first call...");
    return true;
}

void overlay_shutdown() {
    HMODULE opengl32 = GetModuleHandleA("opengl32.dll");
    if (opengl32) {
        void* target = (void*)GetProcAddress(opengl32, "wglSwapBuffers");
        if (target) MH_DisableHook(target);
    }

    if (g_ol.hwnd && g_ol.original_wndproc) {
        SetWindowLongPtrW(g_ol.hwnd, GWLP_WNDPROC, (LONG_PTR)g_ol.original_wndproc);
    }

    if (g_ol.initialized) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        g_ol.initialized = false;
    }

    MH_Uninitialize();
}

#endif // _WIN32
