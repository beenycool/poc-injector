#include <cstdio>
#include <cstdlib>
#include <windows.h>

#include "jni_structures.h"

static volatile LONG g_thread_started = 0;

static void dbg_print(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    OutputDebugStringA(buf);
    OutputDebugStringA("\n");
}

static DWORD WINAPI payload_thread(LPVOID /*param*/) {
    MessageBoxA(NULL, "Step 1: DLL thread started and running!", "Debug", MB_OK);

    dbg_print("[PAYLOAD] Thread started, allocating console...");

    if (!AllocConsole()) {
        MessageBoxA(NULL, "Step 2: AllocConsole FAILED!", "Debug", MB_OK);
        dbg_print("[PAYLOAD] AllocConsole failed (GLE=%lu)", GetLastError());
        return 1;
    }
    MessageBoxA(NULL, "Step 2: AllocConsole OK", "Debug", MB_OK);

    FILE* f_out = nullptr;
    FILE* f_err = nullptr;
    freopen_s(&f_out, "CONOUT$", "w", stdout);
    freopen_s(&f_err, "CONOUT$", "w", stderr);

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole != INVALID_HANDLE_VALUE) {
        SetConsoleTitleA("Minecraft Payload — Debug Console");
    }

    MessageBoxA(NULL, "Step 3: About to look up jvm.dll...", "Debug", MB_OK);

    JvmDll jvm;
    if (!jvm.init()) {
        MessageBoxA(NULL, "Step 3: jvm.dll NOT FOUND in process!", "Debug", MB_OK);
        printf("[PAYLOAD] FATAL: Cannot locate jvm.dll via GetModuleHandleA.\n");
        printf("          Is this process actually running a JVM?\n");
        fflush(stdout);
        return 1;
    }
    MessageBoxA(NULL, "Step 3: jvm.dll FOUND — proceeding to JNI_GetCreatedJavaVMs", "Debug", MB_OK);

    printf("[PAYLOAD] jvm.dll found at base 0x%p\n", (void*)jvm.base);
    printf("[PAYLOAD] JNI_GetCreatedJavaVMs resolved at 0x%p\n\n",
           (void*)jvm.JNI_GetCreatedJavaVMs);

    JavaVM vm   = nullptr;
    jsize   count = 0;

    for (int attempt = 0; attempt < 60; ++attempt) {
        jint rc = jvm.JNI_GetCreatedJavaVMs(&vm, 1, &count);
        if (rc == 0 && count > 0) {
            printf("[PAYLOAD] JNI_GetCreatedJavaVMs returned %d VM(s) after %d attempt(s)\n",
                   (int)count, attempt + 1);
            break;
        }

        if (attempt == 0) {
            printf("[PAYLOAD] JVM not created yet — waiting for JVM boot...\n");
        }
        printf("  Attempt %d: rc=%d, count=%d\n", attempt + 1, (int)rc, (int)count);
        Sleep(1000);
    }

    if (count == 0) {
        printf("[PAYLOAD] FATAL: JVM never appeared after 60 retries.\n");
        fflush(stdout);
        return 1;
    }

    MessageBoxA(NULL, "Step 4: Got JavaVM — about to AttachCurrentThread", "Debug", MB_OK);

    printf("[PAYLOAD] JavaVM* = 0x%p\n\n", vm);

    JNIEnv env = nullptr;
    jint attach_rc = vm->functions->AttachCurrentThread(&vm, &env, nullptr);
    if (attach_rc != 0) {
        printf("[PAYLOAD] AttachCurrentThread FAILED with rc=%d\n", (int)attach_rc);
        fflush(stdout);
        return 1;
    }

    printf("[PAYLOAD] AttachCurrentThread — SUCCESS\n");
    printf("[PAYLOAD] JNIEnv* = 0x%p\n", env);

    MessageBoxA(NULL, "Step 5: Attached to JVM successfully!", "Debug", MB_OK);

    // --- Diagnostic: verify env validity & vtable entry ---
    jint jni_ver = env->GetVersion(env);
    printf("[PAYLOAD] GetVersion = 0x%08X — env still valid\n", (unsigned)jni_ver);
    printf("[PAYLOAD] vtable[0x30] (FindClass) = 0x%p\n",
           *(void**)((char*)env + 0x30));
    fflush(stdout);

    MessageBoxA(NULL, "Step 5b: About to call FindClass", "Debug", MB_OK);

    // --- SEH-protected FindClass ---
    jclass minecraftClass = nullptr;
    bool    didCrash       = false;

    __try {
        minecraftClass = env->FindClass(env, "net/minecraft/client/Minecraft");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        didCrash = true;
        DWORD code = GetExceptionCode();
        printf("[PAYLOAD] *** SEH: FindClass CRASHED  Code=0x%08X ***\n", code);
        printf("[PAYLOAD] Thread stays alive — press END to detach.\n");
        fflush(stdout);
        MessageBoxA(NULL, "FindClass crashed (SEH caught). Thread alive — press END.",
                    "Debug", MB_OK);
    }

    if (!didCrash) {
        if (minecraftClass) {
            printf("[PAYLOAD] FindClass SUCCESS\n"); fflush(stdout);
            MessageBoxA(NULL, "Found Minecraft Client Class!", "Debug", MB_OK);
        } else {
            printf("[PAYLOAD] FindClass NULL — clearing exception\n"); fflush(stdout);
            env->ExceptionClear(env);
            MessageBoxA(NULL, "Class not found.", "Debug", MB_OK);
        }
    }

    // --- Main keep-alive loop ---
    printf("[PAYLOAD] Main loop active (press END to exit)\n"); fflush(stdout);
    MessageBoxA(NULL, "Step 6: Main loop. Press END to detach.", "Debug", MB_OK);

    while (!GetAsyncKeyState(VK_END)) {
        Sleep(100);
    }

    // --- Clean shutdown ---
    printf("[PAYLOAD] Detaching from JVM...\n"); fflush(stdout);
    MessageBoxA(NULL, "Step 7: Detaching...", "Debug", MB_OK);

    jint detach_rc = vm->functions->DetachCurrentThread(&vm);
    printf("[PAYLOAD] DetachCurrentThread rc=%d\n", (int)detach_rc); fflush(stdout);

    MessageBoxA(NULL, "Safely detached. Thread exiting cleanly.", "Debug", MB_OK);
    fflush(stdout);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID /*lpvReserved*/) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);

        dbg_print("[PAYLOAD] DllMain(DLL_PROCESS_ATTACH) — launching worker thread");

        HANDLE hThread = CreateThread(
            nullptr,              // default security
            0,                    // default stack
            payload_thread,        // thread proc
            nullptr,              // no parameter
            0,                    // run immediately
            nullptr);             // thread ID (not needed)

        if (hThread) {
            CloseHandle(hThread);
            InterlockedExchange(&g_thread_started, 1);
        } else {
            dbg_print("[PAYLOAD] CreateThread failed (GLE=%lu)", GetLastError());
            return FALSE;
        }
    }
    return TRUE;
}
