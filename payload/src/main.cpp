#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

#ifdef __APPLE__
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <dlfcn.h>
#include <limits.h>
#else
#include <windows.h>
#endif

#include "jni_structures.h"
#include "seh_compat.h"
#include "shared_state.h"
#include "overlay.h"
#include "java_stubs.h"

// Cross-platform SEH code type
#ifdef __APPLE__
typedef int seh_code_t;
#else
typedef DWORD seh_code_t;
#endif

//============================================================================
// Logger — single funnel: file + debug output + console.
//============================================================================
struct Logger {
    FILE* fp = nullptr;
#ifdef __APPLE__
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#else
    CRITICAL_SECTION cs = {};
#endif
    bool cs_ok = false;
    bool has_console = false;

    void init(const char* path) {
#ifdef __APPLE__
        pthread_mutex_init(&mutex, nullptr);
#else
        InitializeCriticalSection(&cs);
#endif
        cs_ok = true;
        if (path && path[0]) {
#ifdef __APPLE__
            fp = fopen(path, "a");
#else
            fopen_s(&fp, path, "a");
#endif
            if (fp) setvbuf(fp, nullptr, _IONBF, 0);
        }
    }

    void cleanup() {
        if (fp) { fclose(fp); fp = nullptr; }
        if (cs_ok) {
#ifdef __APPLE__
            pthread_mutex_destroy(&mutex);
#else
            DeleteCriticalSection(&cs);
#endif
            cs_ok = false;
        }
    }

    void log(const char* fmt, ...) {
        char buf[1024];
        va_list ap;
        va_start(ap, fmt);
        int n = vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        if (n < 0) return;

#ifdef __APPLE__
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        unsigned long ms = (unsigned long)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#else
        DWORD ms = GetTickCount();
#endif
        char line[1280];
        int line_len = snprintf(line, sizeof(line), "[%lu] %s", ms, buf);
        if (line_len < 0) return;

#ifdef __APPLE__
        pthread_mutex_lock(&mutex);
        if (fp) { fputs(line, fp); fputc('\n', fp); }
        pthread_mutex_unlock(&mutex);
#else
        EnterCriticalSection(&cs);
        if (fp) { fputs(line, fp); fputc('\n', fp); }
        LeaveCriticalSection(&cs);

        OutputDebugStringA(line);
        OutputDebugStringA("\n");
#endif
        if (has_console) {
            fputs(line, stdout);
            fputc('\n', stdout);
            fflush(stdout);
        }
    }
};
static Logger g_log;
MinecraftState g_state;

//============================================================================
// ScopedAttach — RAII JNIEnv attach/detach.
//============================================================================
struct ScopedAttach {
    JavaVM  vm  = nullptr;
    JNIEnv  env = nullptr;
    bool    ok  = false;

    bool attach(JavaVM v) {
        vm = v;
        if (!vm || !vm->functions || !vm->functions->AttachCurrentThread) {
            return false;
        }
        ok = (vm->functions->AttachCurrentThread(&vm, &env, nullptr) == 0);
        return ok;
    }

    ~ScopedAttach() {
        if (ok && vm && vm->functions && vm->functions->DetachCurrentThread) {
            vm->functions->DetachCurrentThread(&vm);
        }
    }

    ScopedAttach() = default;
    ScopedAttach(const ScopedAttach&) = delete;
    ScopedAttach& operator=(const ScopedAttach&) = delete;
    ScopedAttach(ScopedAttach&&) = delete;
    ScopedAttach& operator=(ScopedAttach&&) = delete;

    JNIEnv get() const { return env; }
    bool   good() const { return ok; }
};

//============================================================================
// Log path
//============================================================================
static void get_log_path(char* buf, size_t bufsize) {
#ifdef __APPLE__
    const char* home = getenv("HOME");
    if (home && home[0]) {
        snprintf(buf, bufsize, "%s/Downloads/mcpayload_debug.log", home);
    } else {
        snprintf(buf, bufsize, "/tmp/mcpayload_debug.log");
    }
#else
    const char* profile = getenv("USERPROFILE");
    if (profile && profile[0]) {
        snprintf(buf, bufsize, "%s\\Downloads\\mcpayload_debug.log", profile);
    } else {
        strncpy_s(buf, bufsize, "mcpayload_debug.log", _TRUNCATE);
    }
#endif
}

//============================================================================
// Exception helpers
//============================================================================
static void clear_jni_exception(JNIEnv env) {
    if (env && env->functions && env->functions->ExceptionCheck) {
        if (env->functions->ExceptionCheck(env)) {
            env->functions->ExceptionClear(env);
        }
    }
}

//============================================================================
// Log callback for java_stubs — forwards formatted messages to g_log
//============================================================================
static void jni_log_callback(const char* msg) {
    if (msg) g_log.log("%s", msg);
}

//============================================================================
// Shutdown handling
//============================================================================
#ifdef __APPLE__
// macOS: simple volatile flag + signal handler
static volatile bool g_shutdown_flag = false;

static void signal_handler(int sig) {
    (void)sig;
    g_shutdown_flag = true;
}

static void setup_shutdown() {
    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);
}
#else
// Windows: event + console control handler
static HANDLE g_shutdown_ev = nullptr;

static BOOL WINAPI ctrl_handler(DWORD ctrl_type) {
    if (ctrl_type == CTRL_C_EVENT || ctrl_type == CTRL_CLOSE_EVENT) {
        if (g_shutdown_ev) SetEvent(g_shutdown_ev);
        return TRUE;
    }
    return FALSE;
}

static HANDLE setup_shutdown() {
    g_shutdown_ev = CreateEventA(nullptr, FALSE, FALSE, "McpayloadShutdown");
    SetConsoleCtrlHandler(ctrl_handler, TRUE);
    return g_shutdown_ev;
}
#endif

//============================================================================
// Console setup
//============================================================================
static bool setup_console() {
#ifdef __APPLE__
    // macOS: stdout/stderr already connected to terminal when loaded
    // via DYLD_INSERT_LIBRARIES. Nothing to allocate.
    g_log.has_console = true;
    g_log.log("Console output active (inherited from parent process)");
    return true;
#else
    if (!AllocConsole()) {
        g_log.log("AllocConsole failed (GLE=%lu)", GetLastError());
        return false;
    }

    FILE* dummy = nullptr;
    freopen_s(&dummy, "CONOUT$", "w", stdout);
    freopen_s(&dummy, "CONOUT$", "w", stderr);

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole != INVALID_HANDLE_VALUE) {
        SetConsoleTitleA("Minecraft Payload Debug Console");
    }

    g_log.has_console = true;
    g_log.log("Console allocated successfully");
    return true;
#endif
}

//============================================================================
// JVM discovery with exponential backoff
//============================================================================
static JavaVM wait_for_jvm(JvmDll& jvm, int max_ms = 30000) {
    JavaVM vm = nullptr;
    jsize count = 0;
    int delay = 50;
    int elapsed = 0;

    while (elapsed < max_ms) {
        jint rc = jvm.JNI_GetCreatedJavaVMs(&vm, 1, &count);
        if (rc == 0 && count > 0) {
            g_log.log("JVM found after %d ms", elapsed);
            return vm;
        }
#ifdef __APPLE__
        usleep(delay * 1000);
#else
        Sleep(delay);
#endif
        elapsed += delay;
        delay = (delay < 1000) ? delay * 2 : 1000;
        if (elapsed + delay > max_ms) {
            delay = max_ms - elapsed;
        }
    }

    g_log.log("JVM not found after %d ms", max_ms);
    return nullptr;
}

//============================================================================
// update_shared_state — reads player position via stubs, writes to g_state
//============================================================================
static void update_shared_state(Minecraft& mc) {
    EntityPlayerSP player = mc.getThePlayer();
    if (!player.isValid()) return;

    double x = 0.0, y = 0.0, z = 0.0;
    player.getPos(x, y, z);

    state_lock();
    g_state.posX = x;
    g_state.posY = y;
    g_state.posZ = z;
    state_unlock();
}

// SEH-safe helpers — no C++ objects, safe for __try in MSVC.
static jobject sehGetStaticObjectField(JNIEnv env, jclass cls, jfieldID fid, seh_code_t& code) {
    code = 0;
    jobject result = nullptr;
    SEH_TRY {
        result = env->functions->GetStaticObjectField(env, cls, fid);
    } SEH_EXCEPT(code) {}
    return result;
}

static jobject sehGetObjectField(JNIEnv env, jobject obj, jfieldID fid, seh_code_t& code) {
    code = 0;
    jobject result = nullptr;
    SEH_TRY {
        result = env->functions->GetObjectField(env, obj, fid);
    } SEH_EXCEPT(code) {}
    return result;
}

//============================================================================
// Payload thread — orchestrator
//============================================================================
#ifdef __APPLE__
static void* payload_thread(void*) {
#else
static DWORD WINAPI payload_thread(LPVOID) {
#endif
    // Logger init
#ifdef __APPLE__
    char logpath[PATH_MAX];
#else
    char logpath[MAX_PATH];
#endif
    get_log_path(logpath, sizeof(logpath));
    g_log.init(logpath);
    g_log.log("Payload thread started");

    // Stage 1: Console
    if (!setup_console()) {
        g_log.log("FATAL: Console setup failed");
#ifdef __APPLE__
        return nullptr;
#else
        return 1;
#endif
    }

    // Stage 2: Discover JVM
    JvmDll jvm;
    if (!jvm.init()) {
#ifdef __APPLE__
        g_log.log("FATAL: libjvm.dylib not found in process");
        return nullptr;
#else
        g_log.log("FATAL: jvm.dll not found in process");
        return 1;
#endif
    }
#ifdef __APPLE__
    g_log.log("libjvm.dylib found, JNI_GetCreatedJavaVMs at 0x%p",
              (void*)jvm.JNI_GetCreatedJavaVMs);
#else
    g_log.log("jvm.dll at base 0x%p, JNI_GetCreatedJavaVMs at 0x%p",
              (void*)jvm.base, (void*)jvm.JNI_GetCreatedJavaVMs);
#endif

    JavaVM vm = wait_for_jvm(jvm);
    if (!vm) {
        g_log.log("FATAL: JVM never appeared");
#ifdef __APPLE__
        return nullptr;
#else
        return 1;
#endif
    }
    g_log.log("JavaVM* = 0x%p", vm);

    // Stage 3: Attach to JVM
    ScopedAttach att;
    if (!att.attach(vm)) {
        g_log.log("FATAL: AttachCurrentThread failed");
#ifdef __APPLE__
        return nullptr;
#else
        return 1;
#endif
    }
    JNIEnv env = att.get();
    g_log.log("AttachCurrentThread SUCCESS, JNIEnv* = 0x%p", env);

    // Stage 4: Init shared state + window + overlay (ensures ImGui GUI is active immediately)
    state_init();

#ifdef __APPLE__
    // macOS: overlay discovers the GL context from inside the hook
    if (!overlay_init()) {
        g_log.log("Overlay init failed — continuing without GUI");
    } else {
        g_log.log("Overlay init succeeded (fishhook CGLFlushDrawable registered)");
    }
#else
    HWND mcHwnd = FindWindowA("LWJGL", nullptr);
    if (!mcHwnd) mcHwnd = FindWindowA("GLFW30", nullptr);
    if (!mcHwnd) mcHwnd = FindWindowA("Minecraft", nullptr);
    if (!mcHwnd) {
        struct EnumCtx { DWORD pid; HWND hwnd; };
        EnumCtx ctx;
        ctx.pid = GetCurrentProcessId();
        ctx.hwnd = nullptr;
        EnumWindows([](HWND h, LPARAM lp) -> BOOL {
            EnumCtx* c = reinterpret_cast<EnumCtx*>(lp);
            DWORD wpid = 0;
            GetWindowThreadProcessId(h, &wpid);
            if (wpid == c->pid && IsWindowVisible(h)) {
                c->hwnd = h;
                return FALSE;
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&ctx));
        mcHwnd = ctx.hwnd;
        if (mcHwnd) {
            g_log.log("Found window 0x%p via EnumWindows (PID fallback)", mcHwnd);
        } else {
            g_log.log("Could not find any visible top-level window for this process");
        }
    } else {
        g_log.log("Found window 0x%p via class name", mcHwnd);
    }

    if (!overlay_init(mcHwnd)) {
        g_log.log("Overlay init failed — continuing without GUI");
    }
#endif

    // Stage 5: Initialize JNI stubs (retryable if Minecraft class hasn't finished loading)
    bool jniReady = g_jni.init(env, jni_log_callback);
    if (!jniReady) {
        g_log.log("JNI stubs initial attempt deferred — retrying in background loop");
    }

    // Stage 6: Grab the static Minecraft singleton (if JNI ready)
    jobject mcLocal = nullptr;
    int mcRetries = 0;
    std::unique_ptr<Minecraft> g_minecraft = nullptr;

    if (jniReady) {
        while (!mcLocal && mcRetries < 50) {
            seh_code_t code;
            mcLocal = sehGetStaticObjectField(env, g_jni.minecraftClass, g_jni.theMinecraft_fid, code);
            if (code) g_log.log("CRASH GetStaticObjectField(mc): code=0x%08X", code);
            clear_jni_exception(env);
            if (!mcLocal) {
#ifdef __APPLE__
                usleep(100 * 1000);
#else
                Sleep(100);
#endif
                mcRetries++;
            }
        }
        if (mcLocal) {
            g_log.log("Minecraft singleton acquired (retries=%d)", mcRetries);
            g_minecraft = std::make_unique<Minecraft>(env, mcLocal);
        }
    }

    // Stage 7: Main loop — update state at ~60fps, defer player discovery
    {
#ifdef __APPLE__
        setup_shutdown();
#else
        HANDLE ev = setup_shutdown();
#endif
        bool canPoll = false;
        int retryCount = 0;
        int loopCount = 0;
        const int MAX_RETRIES = 600;

        g_log.log("Entering payload main loop...");

        while (true) {
            loopCount++;
            if (loopCount % 300 == 0) {
                g_log.log("Main loop active (loop %d, canPoll=%d, retryCount=%d)",
                          loopCount, (int)canPoll, retryCount);
            }

            // Check shutdown
            bool running = false;
            state_lock();
            running = g_state.running;
            state_unlock();
            if (!running) {
                g_log.log("g_state.running is false — breaking main loop");
                break;
            }

#ifdef __APPLE__
            if (g_shutdown_flag) {
                g_log.log("g_shutdown_flag set — breaking main loop");
                break;
            }
#else
            DWORD wr = WaitForSingleObject(ev, 0);
            if (wr == WAIT_OBJECT_0) break;
#endif

            // If JNI stubs were deferred, retry initializing JNI stubs periodically after game bootstrap (15s delay)
            if (!g_jni.minecraftClass && retryCount < MAX_RETRIES && loopCount > 900 && (loopCount % 120 == 0)) {
                retryCount++;
                g_log.log("Retrying JNI stubs init (attempt %d)...", retryCount);
                if (g_jni.init(env, jni_log_callback)) {
                    g_log.log("JNI stubs initialized successfully on retry %d!", retryCount);
                }
            }

            // If we have a valid player object, poll position
            if (canPoll && g_minecraft) {
                update_shared_state(*g_minecraft);
            }
            // If thePlayer field ID was resolved but player isn't ready yet,
            // try to get the player and resolve pos fields
            else if (g_jni.thePlayer_fid && retryCount < MAX_RETRIES) {
                retryCount++;
                seh_code_t sehCode;
                jobject playerLocal = sehGetObjectField(env, (jobject)g_jni.minecraftClass, g_jni.thePlayer_fid, sehCode);
                clear_jni_exception(env);

                if (playerLocal) {
                    // Resolve posX/Y/Z from the live player object
                    if (g_jni.resolvePosFields(env, playerLocal)) {
                        g_log.log("Deferred pos fields resolved after %d retries", retryCount);
                        canPoll = true;
                    }
                    env->functions->DeleteLocalRef(env, playerLocal);
                }
            }

#ifdef __APPLE__
            usleep(16 * 1000);
#else
            Sleep(16);
#endif
        }

        if (canPoll) {
            update_shared_state(*g_minecraft);
        }

#ifndef __APPLE__
        CloseHandle(ev);
#endif
    }

    g_log.log("Shutdown signal received, cleaning up...");

    // Cleanup in reverse order of init
    g_minecraft.reset();  // releases player global ref
    overlay_shutdown();
    g_jni.cleanup(env);
    state_cleanup();
    g_log.log("Payload thread exiting cleanly");
    g_log.cleanup();
#ifdef __APPLE__
    return nullptr;
#else
    return 0;
#endif
}

//============================================================================
// Entry point — DllMain (Windows) or __attribute__((constructor)) (macOS)
//============================================================================
#ifdef __APPLE__

__attribute__((constructor))
static void dylib_init() {
    pthread_t thread;
    if (pthread_create(&thread, nullptr, payload_thread, nullptr) != 0) {
        fprintf(stderr, "[PAYLOAD] pthread_create failed\n");
        return;
    }
    pthread_detach(thread);
}

#else // _WIN32

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);

        GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_PIN,
            (LPCSTR)hinstDLL, &hinstDLL);

        HANDLE hThread = CreateThread(
            nullptr, 0,
            payload_thread,
            nullptr, 0, nullptr);

        if (!hThread) {
            OutputDebugStringA("[PAYLOAD] CreateThread failed\n");
            return FALSE;
        }
    }
    return TRUE;
}

#endif
