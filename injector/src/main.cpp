#include "target.hpp"
#include "inject.hpp"
#include "error.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

#ifdef __APPLE__
#include <sys/stat.h>
#include <unistd.h>
#endif

using namespace inject;

static void print_usage(const char* exe_name) {
#ifdef __APPLE__
    fprintf(stderr,
        "Usage: %s <dylib_path> [jar_path] [java_executable]\n"
        "\n"
        "  dylib_path        Full path to the payload .dylib to inject.\n"
        "  jar_path           Minecraft .jar path (optional, launches java -jar).\n"
        "  java_executable    Java binary path (default: auto-detect).\n"
        "\n"
        "The injector launches Minecraft with the dylib preloaded via\n"
        "DYLD_INSERT_LIBRARIES. SIP must be disabled or a third-party\n"
        "JDK must be used (Apple-signed binaries strip DYLD_ env vars).\n"
        "\n"
        "Example:\n"
        "  %s ./build/lib/libmcpayload.dylib Minecraft.jar\n",
        exe_name, exe_name);
#else
    fprintf(stderr,
        "Usage: %s <dll_path> [target_exe]\n"
        "\n"
        "  dll_path     Full path to the payload DLL to inject.\n"
        "  target_exe   Process name (default: javaw.exe).\n"
        "\n"
        "Example:\n"
        "  %s C:\\payloads\\hook.dll javaw.exe\n",
        exe_name, exe_name);
#endif
}

int main(int argc, char* argv[]) {
#ifdef __APPLE__
    // =====================================================================
    // macOS: DYLD_INSERT_LIBRARIES launcher mode
    // =====================================================================
    if (argc < 2 || argc > 4) {
        print_usage(argc > 0 ? argv[0] : "mcinjector");
        return 1;
    }

    std::string dylib_path = argv[1];
    std::string jar_path   = (argc >= 3) ? argv[2] : "";
    std::string java_exe   = (argc >= 4) ? argv[3] : "";

    // --- Validate dylib path exists ---
    struct stat st;
    if (stat(dylib_path.c_str(), &st) != 0 || S_ISDIR(st.st_mode)) {
        log_info("Dylib path not found or is a directory: %s", dylib_path.c_str());
        return 1;
    }

    // --- Perform injection (launch with DYLD_INSERT) ---
    inject_args args;
    args.target_pid       = 0; // not used in DYLD mode
    args.dylib_path       = dylib_path;
    args.target_executable = java_exe;
    args.jar_path          = jar_path;

    auto result = perform_injection(args);

    if (!result) {
        fprintf(stderr, "FATAL: Injection/launch failed.\n");
        return 1;
    }

    log_info("Launched with PID %d. Injector exiting.", (int)result->child_pid);
    return 0;

#elif defined(_WIN32)
    // =====================================================================
    // Windows: Classic DLL injection into running process
    // =====================================================================
    if (argc < 2 || argc > 3) {
        print_usage(argc > 0 ? argv[0] : "injector.exe");
        return 1;
    }

    std::string dll_path = argv[1];
    std::string target_exe = (argc >= 3) ? argv[2] : "javaw.exe";

    // --- Validate DLL path exists ---
    DWORD attrs = GetFileAttributesA(dll_path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        log_info("DLL path not found or is a directory: %s", dll_path.c_str());
        return 1;
    }

    // --- Find target process ---
    auto target = find_process_by_name(target_exe);
    if (!target) {
        fprintf(stderr, "ERROR: Cannot find process '%s'.\n",
                target_exe.c_str());
        return 1;
    }

    // --- Perform injection ---
    inject_args args;
    args.target_pid = target->pid;
    args.dll_path = dll_path;

    auto result = perform_injection(args);

    if (!result) {
        fprintf(stderr, "FATAL: Injection pipeline failed.\n");
        return 1;
    }

    // --- Cleanup ---
    if (result->remote_thread) {
        CloseHandle(result->remote_thread);
    }

    log_info("Injector exiting successfully.");
    return 0;

#endif
}
