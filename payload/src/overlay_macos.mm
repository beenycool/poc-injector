// overlay_macos.mm — macOS overlay implementation
// Uses fishhook to intercept CGLFlushDrawable + ImGui with OSX/OpenGL3 backends.
//
// This file is Objective-C++ (.mm) because ImGui's OSX backend requires
// NSView/NSEvent types from Cocoa.

#ifdef __APPLE__

#include <cstdio>
#include <dlfcn.h>

#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <Carbon/Carbon.h>          // kVK_RightShift
#include <CoreGraphics/CoreGraphics.h>

#import <Cocoa/Cocoa.h>

#include <fishhook.h>

#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_osx.h"

#include "shared_state.h"
#include "gui.h"
#include "overlay.h"

#ifndef GL_SHADING_LANGUAGE_VERSION
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C
#endif
#ifndef GL_FRAMEBUFFER_BINDING
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#endif

static CGLError (*original_CGLFlushDrawable)(CGLContextObj) = nullptr;

struct OverlayState {
    bool           initialized = false;
    NSView*        nsView      = nil;
    ImGuiContext*  ctx         = nullptr;
    id             eventMonitor = nil;
};

static OverlayState g_ol;

#define OVERLAY_LOG(fmt, ...) do {                                      \
    char _buf[512];                                                     \
    int _n = snprintf(_buf, sizeof(_buf), "[OVERLAY] " fmt "\n",        \
                      ##__VA_ARGS__);                                   \
    if (_n > 0) {                                                       \
        fprintf(stderr, "%s", _buf); fflush(stderr);                    \
        const char* _home = getenv("HOME");                             \
        if (_home && _home[0]) {                                        \
            char _p[512];                                               \
            snprintf(_p, sizeof(_p), "%s/Downloads/mcpayload_debug.log", _home); \
            FILE* _f = fopen(_p, "a");                                  \
            if (_f) { fputs(_buf, _f); fclose(_f); }                    \
        }                                                               \
        FILE* _tmpf = fopen("/tmp/mcpayload_debug.log", "a");           \
        if (_tmpf) { fputs(_buf, _tmpf); fclose(_tmpf); }               \
    }                                                                   \
} while(0)

#define OVERLAY_LOG_ERR(fmt, ...) do {                                  \
    char _buf[512];                                                     \
    int _n = snprintf(_buf, sizeof(_buf), "[OVERLAY] ERROR: " fmt "\n", \
                      ##__VA_ARGS__);                                   \
    if (_n > 0) {                                                       \
        fprintf(stderr, "%s", _buf); fflush(stderr);                    \
        const char* _home = getenv("HOME");                             \
        if (_home && _home[0]) {                                        \
            char _p[512];                                               \
            snprintf(_p, sizeof(_p), "%s/Downloads/mcpayload_debug.log", _home); \
            FILE* _f = fopen(_p, "a");                                  \
            if (_f) { fputs(_buf, _f); fclose(_f); }                    \
        }                                                               \
        FILE* _tmpf = fopen("/tmp/mcpayload_debug.log", "a");           \
        if (_tmpf) { fputs(_buf, _tmpf); fclose(_tmpf); }               \
    }                                                                   \
} while(0)

//============================================================================
// Toggle helper — thread-safe, handles cursor visibility
//============================================================================
static void toggle_menu() {
    bool newState;
    state_lock();
    newState = !g_state.menuOpen;
    g_state.menuOpen = newState;
    state_unlock();

    if (newState) {
        // Show cursor when menu opens
        CGDisplayShowCursor(kCGDirectMainDisplay);
        CGAssociateMouseAndMouseCursorPosition(true);
    } else {
        // Hide cursor when menu closes (game recaptures)
        CGDisplayHideCursor(kCGDirectMainDisplay);
        CGAssociateMouseAndMouseCursorPosition(true);
    }
}

//============================================================================
// Hooked CGLFlushDrawable — called every frame by OpenGL
//============================================================================
static CGLError hooked_CGLFlushDrawable(CGLContextObj ctx) {
    if (!g_ol.initialized) {
        //==================================================================
        // First call — one-time ImGui init
        //==================================================================
        OVERLAY_LOG("=== hooked_CGLFlushDrawable FIRST CALL ===");

        // Get the NSOpenGLContext and NSView from the current CGL context
        @autoreleasepool {
            NSOpenGLContext* nsglCtx = [NSOpenGLContext currentContext];
            if (!nsglCtx) {
                OVERLAY_LOG("NSOpenGLContext currentContext is nil, passing through");
                return original_CGLFlushDrawable(ctx);
            }

            g_ol.nsView = [nsglCtx view];
            if (!g_ol.nsView) {
                OVERLAY_LOG("NSOpenGLContext view is nil, passing through");
                return original_CGLFlushDrawable(ctx);
            }

            NSWindow* window = [g_ol.nsView window];
            if (window) {
                OVERLAY_LOG("Window title: %s",
                            [[window title] UTF8String] ?: "nil");
                NSRect frame = [g_ol.nsView frame];
                OVERLAY_LOG("View frame: %.0fx%.0f", frame.size.width, frame.size.height);
            }
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

        // Init ImGui
        OVERLAY_LOG("Creating ImGui context...");
        IMGUI_CHECKVERSION();
        g_ol.ctx = ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.IniFilename = nullptr;
        ImGui::StyleColorsDark();
        OVERLAY_LOG("ImGui context created (0x%p)", g_ol.ctx);

        if (!ImGui_ImplOSX_Init(g_ol.nsView)) {
            OVERLAY_LOG_ERR("ImGui_ImplOSX_Init failed");
            return original_CGLFlushDrawable(ctx);
        }
        OVERLAY_LOG("ImGui_ImplOSX_Init OK");

        if (!ImGui_ImplOpenGL3_Init()) {
            OVERLAY_LOG_ERR("ImGui_ImplOpenGL3_Init failed");
            return original_CGLFlushDrawable(ctx);
        }
        OVERLAY_LOG("ImGui_ImplOpenGL3_Init OK");

        // Install a local event monitor for keyboard events
        // This allows us to intercept key events for the menu toggle
        g_ol.eventMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:
            (NSEventMaskKeyDown | NSEventMaskKeyUp |
             NSEventMaskFlagsChanged |
             NSEventMaskLeftMouseDown | NSEventMaskLeftMouseUp |
             NSEventMaskRightMouseDown | NSEventMaskRightMouseUp |
             NSEventMaskMouseMoved | NSEventMaskScrollWheel)
            handler:^NSEvent*(NSEvent* event) {
                // Forward events to ImGui when menu is open
                bool isMenuOpen = false;
                state_lock();
                isMenuOpen = g_state.menuOpen;
                state_unlock();

                if (isMenuOpen) {
                    // Block keyboard/mouse events from reaching the game
                    if ([event type] == NSEventTypeKeyDown ||
                        [event type] == NSEventTypeKeyUp ||
                        [event type] == NSEventTypeLeftMouseDown ||
                        [event type] == NSEventTypeLeftMouseUp ||
                        [event type] == NSEventTypeRightMouseDown ||
                        [event type] == NSEventTypeRightMouseUp ||
                        [event type] == NSEventTypeMouseMoved) {
                        return nil; // swallow the event
                    }
                }

                return event;
            }];
        OVERLAY_LOG("NSEvent local monitor installed");

        GLenum gle = glGetError();
        if (gle != GL_NO_ERROR) {
            OVERLAY_LOG("GL error after ImGui init: 0x%04X", gle);
        }

        g_ol.initialized = true;
        OVERLAY_LOG("=== ImGui initialization complete ===");
    }

    //========================================================================
    // Per-frame: Right Shift toggle via CGEventSourceKeyState
    //========================================================================
    {
        static bool prev_rshift = false;
        bool cur_rshift = CGEventSourceKeyState(
            kCGEventSourceStateCombinedSessionState, kVK_RightShift);
        if (cur_rshift && !prev_rshift) {
            toggle_menu();
            bool nowOpen = false;
            state_lock();
            nowOpen = g_state.menuOpen;
            state_unlock();
            OVERLAY_LOG("Right Shift toggle: menuOpen=%d", (int)nowOpen);
        }
        prev_rshift = cur_rshift;
    }

    //========================================================================
    // Per-frame: Frame counter & periodic diagnostics
    //========================================================================
    {
        static int frame_count = 0;
        frame_count++;
        if (frame_count == 1 || frame_count % 60 == 0) {
            OVERLAY_LOG("CGLFlushDrawable frame %d | menuOpen=%d", frame_count,
                        (int)g_state.menuOpen);
        }
    }

    //========================================================================
    // ImGui frame
    //========================================================================
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplOSX_NewFrame(g_ol.nsView);
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

    return original_CGLFlushDrawable(ctx);
}

//============================================================================
// Public API
//============================================================================
bool overlay_init() {
    OVERLAY_LOG("overlay_init entered (macOS fishhook mode)");

    // Install fishhook on CGLFlushDrawable
    struct rebinding rebindings[] = {
        {"CGLFlushDrawable", (void*)hooked_CGLFlushDrawable,
         (void**)&original_CGLFlushDrawable}
    };
    int result = rebind_symbols(rebindings, 1);
    if (result != 0) {
        OVERLAY_LOG_ERR("fishhook rebind_symbols failed (result=%d)", result);
        return false;
    }

    OVERLAY_LOG("Hook installed on CGLFlushDrawable via fishhook");
    return true;
}

void overlay_shutdown() {
    // Remove event monitor
    if (g_ol.eventMonitor) {
        [NSEvent removeMonitor:g_ol.eventMonitor];
        g_ol.eventMonitor = nil;
    }

    // Restore CGLFlushDrawable
    if (original_CGLFlushDrawable) {
        struct rebinding rebindings[] = {
            {"CGLFlushDrawable", (void*)original_CGLFlushDrawable, nullptr}
        };
        rebind_symbols(rebindings, 1);
    }

    // Reset the view
    if (g_ol.nsView) {
        g_ol.nsView = nil;
    }

    if (g_ol.initialized) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplOSX_Shutdown();
        ImGui::DestroyContext();
        g_ol.initialized = false;
    }
}

#endif // __APPLE__
