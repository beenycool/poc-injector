#include "inject.hpp"
#include "error.hpp"
#include <cstdio>

namespace inject {

#ifdef __APPLE__

// =========================================================================
// macOS: DYLD_INSERT_LIBRARIES approach
//
// Instead of injecting into a running process, we launch the target
// (typically java -jar ...) with the dylib preloaded via the
// DYLD_INSERT_LIBRARIES environment variable. The dylib's
// __attribute__((constructor)) runs automatically at load time.
// =========================================================================
#include <unistd.h>
#include <cstdlib>
#include <sys/wait.h>
#include <sys/stat.h>

static bool file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && !S_ISDIR(st.st_mode);
}

// Resolve the java executable path
static std::string find_java_executable() {
    // 1. Check JAVA_HOME
    const char* java_home = getenv("JAVA_HOME");
    if (java_home && java_home[0]) {
        std::string path = std::string(java_home) + "/bin/java";
        if (file_exists(path.c_str())) {
            return path;
        }
    }

    // 2. Check common paths
    const char* common_paths[] = {
        "/usr/bin/java",
        "/usr/local/bin/java",
        "/opt/homebrew/bin/java",
    };
    for (auto p : common_paths) {
        if (file_exists(p)) {
            return p;
        }
    }

    // 3. Fall back to "java" and let PATH resolve it
    return "java";
}

std::optional<inject_result> perform_injection(const inject_args& args) {
    log_info("=== DYLD_INSERT injection ===");
    log_info("Dylib path: %s", args.dylib_path.c_str());

    // Validate dylib exists
    if (!file_exists(args.dylib_path.c_str())) {
        log_info("ERROR: Dylib path not found: %s", args.dylib_path.c_str());
        return std::nullopt;
    }

    // Resolve java path
    std::string java_path = args.target_executable.empty()
        ? find_java_executable()
        : args.target_executable;

    log_info("[1/3] Java executable: %s", java_path.c_str());

    // Set DYLD_INSERT_LIBRARIES
    if (setenv("DYLD_INSERT_LIBRARIES", args.dylib_path.c_str(), 1) != 0) {
        log_error("setenv(DYLD_INSERT_LIBRARIES)");
        return std::nullopt;
    }
    log_info("[2/3] DYLD_INSERT_LIBRARIES set to: %s", args.dylib_path.c_str());

    // Fork and exec
    pid_t child = fork();
    if (child < 0) {
        log_error("fork");
        return std::nullopt;
    }

    if (child == 0) {
        // Child process: exec java with the dylib preloaded
        if (!args.jar_path.empty()) {
            execlp(java_path.c_str(), "java", "-jar",
                   args.jar_path.c_str(), nullptr);
        } else {
            execlp(java_path.c_str(), "java", nullptr);
        }
        // If exec returns, it failed
        perror("execlp");
        _exit(127);
    }

    // Parent: unset the env var so it doesn't leak
    unsetenv("DYLD_INSERT_LIBRARIES");

    log_info("[3/3] Child process launched — PID: %d", (int)child);

    // Wait briefly to detect immediate failures (e.g. bad java path)
    int status = 0;
    pid_t wr = waitpid(child, &status, WNOHANG);
    if (wr == child) {
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            log_info("WARNING: Child exited immediately with code %d",
                     WEXITSTATUS(status));
        }
    }

    inject_result result = {};
    result.child_pid = child;

    log_info("=== Injection complete ===");
    return result;
}

#elif defined(_WIN32)

// =========================================================================
// Windows: Classic DLL injection via CreateRemoteThread(LoadLibraryA)
// =========================================================================

static const char* step_name(inject_step s) {
    switch (s) {
    case inject_step::open_process:         return "OpenProcess";
    case inject_step::virtual_alloc:        return "VirtualAllocEx";
    case inject_step::write_path:           return "WriteProcessMemory(path)";
    case inject_step::load_library_addr:    return "GetProcAddress(LoadLibraryA)";
    case inject_step::create_remote_thread: return "CreateRemoteThread";
    case inject_step::done:                 return "done";
    }
    return "?";
}

static void cleanup_on_failure(inject_state& state) {
    if (state.h_thread) {
        CloseHandle(state.h_thread);
        state.h_thread = nullptr;
    }
    if (state.remote_addr && state.h_process) {
        VirtualFreeEx(state.h_process, state.remote_addr, 0, MEM_RELEASE);
        state.remote_addr = nullptr;
    }
    if (state.h_process) {
        CloseHandle(state.h_process);
        state.h_process = nullptr;
    }
}

static bool enable_debug_privilege() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                          &token)) {
        log_error("OpenProcessToken");
        return false;
    }

    TOKEN_PRIVILEGES tp = {};
    if (!LookupPrivilegeValueA(nullptr, SE_DEBUG_NAME, &tp.Privileges[0].Luid)) {
        log_error("LookupPrivilegeValue(SeDebugPrivilege)");
        CloseHandle(token);
        return false;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), nullptr, nullptr)) {
        log_error("AdjustTokenPrivileges");
        CloseHandle(token);
        return false;
    }

    CloseHandle(token);
    log_info("SeDebugPrivilege enabled");
    return true;
}

std::optional<inject_result> perform_injection(const inject_args& args) {
    inject_state state = {};
    state.step = inject_step::open_process;

    log_info("=== Injection pipeline for PID %lu ===", args.target_pid);
    log_info("DLL path: %s", args.dll_path.c_str());

    enable_debug_privilege();

    // ---------------------------------------------------------------
    // Step 1 — OpenProcess
    // ---------------------------------------------------------------
    state.h_process = OpenProcess(
        PROCESS_CREATE_THREAD
        | PROCESS_QUERY_INFORMATION
        | PROCESS_VM_OPERATION
        | PROCESS_VM_WRITE
        | PROCESS_VM_READ,
        FALSE,
        args.target_pid);

    if (!state.h_process) {
        log_error("OpenProcess");
        return std::nullopt;
    }
    log_info("[1/5] OpenProcess — OK (handle=0x%p)", state.h_process);

    // ---------------------------------------------------------------
    // Step 2 — VirtualAllocEx
    // ---------------------------------------------------------------
    state.path_size = args.dll_path.size() + 1; // include null terminator

    state.step = inject_step::virtual_alloc;
    state.remote_addr = VirtualAllocEx(
        state.h_process,
        nullptr,
        state.path_size,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE);

    if (!state.remote_addr) {
        log_error("VirtualAllocEx");
        cleanup_on_failure(state);
        return std::nullopt;
    }
    log_info("[2/5] VirtualAllocEx — OK (remote=0x%p, size=%zu)",
             state.remote_addr, state.path_size);

    // ---------------------------------------------------------------
    // Step 3 — WriteProcessMemory (DLL path string)
    // ---------------------------------------------------------------
    state.step = inject_step::write_path;

    SIZE_T bytes_written = 0;
    if (!WriteProcessMemory(
            state.h_process,
            state.remote_addr,
            args.dll_path.c_str(),
            state.path_size,
            &bytes_written)) {

        log_error("WriteProcessMemory");
        cleanup_on_failure(state);
        return std::nullopt;
    }

    if (bytes_written != state.path_size) {
        log_info("[WARN] WriteProcessMemory wrote %zu of %zu bytes",
                 bytes_written, state.path_size);
    }
    log_info("[3/5] WriteProcessMemory — OK (%zu bytes)", bytes_written);

    // ---------------------------------------------------------------
    // Step 4 — Get kernel32!LoadLibraryA address
    // ---------------------------------------------------------------
    state.step = inject_step::load_library_addr;

    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    if (!k32) {
        log_error("GetModuleHandle(kernel32.dll)");
        cleanup_on_failure(state);
        return std::nullopt;
    }

    auto pLoadLibraryA = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        GetProcAddress(k32, "LoadLibraryA"));

    if (!pLoadLibraryA) {
        log_error("GetProcAddress(LoadLibraryA)");
        cleanup_on_failure(state);
        return std::nullopt;
    }
    log_info("[4/5] LoadLibraryA — resolved (kernel32=0x%p, addr=0x%p)",
             k32, pLoadLibraryA);

    // ---------------------------------------------------------------
    // Step 5 — CreateRemoteThread(LoadLibraryA, remote_dll_path)
    // ---------------------------------------------------------------
    state.step = inject_step::create_remote_thread;

    state.h_thread = CreateRemoteThread(
        state.h_process,
        nullptr,                // default security
        0,                      // default stack size
        pLoadLibraryA,          // thread start address
        state.remote_addr,      // parameter → LPCSTR dll_path
        0,                      // run immediately
        nullptr);               // thread ID (not needed)

    if (!state.h_thread) {
        log_error("CreateRemoteThread");
        cleanup_on_failure(state);
        return std::nullopt;
    }
    log_info("[5/5] CreateRemoteThread — OK (thread=0x%p)", state.h_thread);

    // ---------------------------------------------------------------
    // Wait briefly for the remote thread to finish
    // ---------------------------------------------------------------
    DWORD wait = WaitForSingleObject(state.h_thread, 5000);
    switch (wait) {
    case WAIT_OBJECT_0:
        log_info("Remote thread finished (LoadLibraryA returned)");

        {
            DWORD exit_code = 0;
            if (GetExitCodeThread(state.h_thread, &exit_code)) {
                if (exit_code == 0) {
                    log_info("  WARNING: LoadLibraryA returned NULL — "
                             "the DLL may have failed to load.");
                    log_info("  Check: 32/64-bit match, dependency DLLs, "
                             "DllMain return value.");
                } else {
                    log_info("  DLL base address: 0x%p",
                             reinterpret_cast<void*>(
                                 static_cast<uintptr_t>(exit_code)));
                }
            }
        }
        break;
    case WAIT_TIMEOUT:
        log_info("Remote thread still running after 5 s (DllMain may be blocked)");
        break;
    case WAIT_FAILED:
        log_error("WaitForSingleObject(remote_thread)");
        break;
    }

    inject_result result = {};
    result.remote_thread = state.h_thread;
    result.remote_mem    = state.remote_addr;

    CloseHandle(state.h_process);
    state.h_process = nullptr;

    log_info("=== Injection complete ===");
    return result;
}

#endif

} // namespace inject
