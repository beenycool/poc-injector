#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <windows.h>

#include "jni_structures.h"
#include "seh_compat.h"

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

static void log_debug(const char* fmt, ...) {
    char msg[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    char fullPath[MAX_PATH] = "mcpayload_debug.log";
    const char* profile = getenv("USERPROFILE");
    if (profile && profile[0] != '\0') {
        snprintf(fullPath, sizeof(fullPath),
                 "%s\\Downloads\\mcpayload_debug.log", profile);
    }

    FILE* f = nullptr;
    if (fopen_s(&f, fullPath, "a") == 0 && f) {
        setvbuf(f, NULL, _IONBF, 0);
        DWORD ms = GetTickCount();
        fprintf(f, "[%lu] %s\n", ms, msg);
        fclose(f);
    }

    OutputDebugStringA(msg);
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
    log_debug("Step 5: Attached to JVM successfully!");

    char buf[256];
    bool didCrash = false;
    DWORD code;

    // --- Thread diagnostics ---
    uintptr_t javaThread = (uintptr_t)env - 0x2C0;
    log_debug("JavaThread = env - 0x2C0 = 0x%p", (void*)javaThread);
    log_debug("env->functions (fn table)  = 0x%p", (void*)env->functions);
    log_debug("env->functions->GetVersion = 0x%p", (void*)env->functions->GetVersion);
    log_debug("env->functions->FindClass  = 0x%p", (void*)env->functions->FindClass);
    log_debug("env->functions->DefineClass= 0x%p", (void*)env->functions->DefineClass);
    log_debug("JavaThread[0x50] (flags)   = 0x%02X", *(uint8_t*)(javaThread + 0x50));
    log_debug("JavaThread+0x348 (state)   = 0x%08X", *(uint32_t*)(javaThread + 0x348));

    snprintf(buf, sizeof(buf), "Thread diag: JavaThread=0x%p, GetVersion=0x%p",
             (void*)javaThread, (void*)env->functions->GetVersion);
    MessageBoxA(NULL, buf, "Step 5a: Thread Diagnosics", MB_OK);

    // --- Test 0: GetVersion (known good) ---
    log_debug("Test 0: GetVersion...");
    jint jni_ver = 0;
    SEH_TRY {
        jni_ver = env->functions->GetVersion(env);
    } SEH_EXCEPT(code) {
        code = GetExceptionCode();
        log_debug("CRASH Test 0: GetVersion! Code=0x%08X", code);
        snprintf(buf, sizeof(buf), "CRASH GetVersion! Code=0x%08X", code);
        MessageBoxA(NULL, buf, "Debug", MB_OK);
    }
    log_debug("Test 0: GetVersion = 0x%08X", (unsigned)jni_ver);
    snprintf(buf, sizeof(buf), "GetVersion = 0x%08X", (unsigned)jni_ver);
    MessageBoxA(NULL, buf, "Debug", MB_OK);

    // --- Test 1: ExceptionClear (simple, no params) ---
    log_debug("Test 1: ExceptionClear...");
    SEH_TRY {
        env->functions->ExceptionClear(env);
        log_debug("Test 1: ExceptionClear OK");
    } SEH_EXCEPT(code) {
        code = GetExceptionCode();
        log_debug("CRASH Test 1: ExceptionClear! Code=0x%08X", code);
        snprintf(buf, sizeof(buf), "CRASH ExceptionClear! Code=0x%08X", code);
        MessageBoxA(NULL, buf, "Debug", MB_OK);
    }

    // --- Test 2: NewStringUTF (JNI string creation, needs no classloading) ---
    log_debug("Test 2: NewStringUTF...");
    jstring testStr = nullptr;
    SEH_TRY {
        testStr = env->functions->NewStringUTF(env, "HelloWorld");
        log_debug("Test 2: NewStringUTF = 0x%p", testStr);
    } SEH_EXCEPT(code) {
        code = GetExceptionCode();
        log_debug("CRASH Test 2: NewStringUTF! Code=0x%08X", code);
        snprintf(buf, sizeof(buf), "CRASH NewStringUTF! Code=0x%08X", code);
        MessageBoxA(NULL, buf, "Debug", MB_OK);
    }

    // --- Test 3: FindClass("java/lang/Object") — bootstrap class ---
    log_debug("Test 3: FindClass(java/lang/Object)...");
    jclass objClass = nullptr;
    SEH_TRY {
        objClass = env->functions->FindClass(env, "java/lang/Object");
        log_debug("Test 3: java/lang/Object = 0x%p", objClass);
    } SEH_EXCEPT(code) {
        code = GetExceptionCode();
        log_debug("CRASH Test 3: FindClass(java/lang/Object)! Code=0x%08X", code);
        snprintf(buf, sizeof(buf), "CRASH FindClass(Object)! Code=0x%08X", code);
        MessageBoxA(NULL, buf, "Debug", MB_OK);
    }
    if (!objClass) {
        env->functions->ExceptionClear(env);
        log_debug("Test 3: java/lang/Object not found (cleared exception)");
    }

    // --- Test 4: FindClass("java/lang/String") — another bootstrap ---
    log_debug("Test 4: FindClass(java/lang/String)...");
    jclass strClass = nullptr;
    SEH_TRY {
        strClass = env->functions->FindClass(env, "java/lang/String");
        log_debug("Test 4: java/lang/String = 0x%p", strClass);
    } SEH_EXCEPT(code) {
        code = GetExceptionCode();
        log_debug("CRASH Test 4: FindClass(java/lang/String)! Code=0x%08X", code);
        snprintf(buf, sizeof(buf), "CRASH FindClass(String)! Code=0x%08X", code);
        MessageBoxA(NULL, buf, "Debug", MB_OK);
    }
    if (!strClass) {
        env->functions->ExceptionClear(env);
        log_debug("Test 4: java/lang/String not found (cleared exception)");
    }

    // --- Test 5: FindClass("net/minecraft/client/Minecraft") — original test ---
    log_debug("Test 5: FindClass(net/minecraft/client/Minecraft)...");
    MessageBoxA(NULL, "Test 5: About to FindClass Minecraft", "Debug", MB_OK);
    jclass minecraftClass = nullptr;
    didCrash = false;

    SEH_TRY {
        minecraftClass = env->functions->FindClass(env, "net/minecraft/client/Minecraft");
    } SEH_EXCEPT(code) {
        didCrash = true;
        code = GetExceptionCode();
        log_debug("CRASH Test 5: FindClass(Minecraft)! Code=0x%08X", code);
        snprintf(buf, sizeof(buf), "CRASH FindClass(Minecraft)! Code=0x%08X", code);
        MessageBoxA(NULL, buf, "Debug", MB_OK);
    }

    if (minecraftClass) {
        log_debug("Test 5: Found Minecraft Client Class!");
        MessageBoxA(NULL, "Test 5: Found Minecraft Client Class!", "Debug", MB_OK);
    } else if (!didCrash) {
        log_debug("Test 5: Minecraft class not found — clearing exception");
        env->functions->ExceptionClear(env);
        MessageBoxA(NULL, "Test 5: Minecraft class not found", "Debug", MB_OK);
    }

    // --- Summary ---
    log_debug("=== JNI Test Summary ===");
    log_debug("  GetVersion          = 0x%08X (%s)", (unsigned)jni_ver, jni_ver ? "OK" : "FAIL");
    log_debug("  ExceptionClear      = %s", "OK");
    log_debug("  NewStringUTF        = 0x%p (%s)", testStr, testStr ? "OK" : "FAIL");
    log_debug("  FindClass(Object)   = 0x%p (%s)", objClass, objClass ? "FOUND" : "NULL");
    log_debug("  FindClass(String)   = 0x%p (%s)", strClass, strClass ? "FOUND" : "NULL");
    log_debug("  FindClass(Minecraft)= 0x%p (%s)", minecraftClass, minecraftClass ? "FOUND" : (didCrash ? "CRASH" : "NULL"));
    log_debug("========================");

    // --- Main keep-alive loop ---
    log_debug("Step 6: Entering main loop");
    MessageBoxA(NULL, "Step 6: Main loop. Press END to detach.", "Debug", MB_OK);
    while (!GetAsyncKeyState(VK_END)) {
        Sleep(100);
    }

    // --- Clean shutdown ---
    log_debug("Step 7: Detaching from JVM...");
    MessageBoxA(NULL, "Step 7: Detaching...", "Debug", MB_OK);
    jint detach_rc = vm->functions->DetachCurrentThread(&vm);
    log_debug("DetachCurrentThread rc=%d", (int)detach_rc);

    snprintf(buf, sizeof(buf), "Detach rc=%d. Thread exiting.", (int)detach_rc);
    MessageBoxA(NULL, buf, "Debug", MB_OK);
    log_debug("Payload thread exiting cleanly");
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
