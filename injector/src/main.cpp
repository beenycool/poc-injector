#include "target.hpp"
#include "inject.hpp"
#include "error.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

using namespace inject;

static void print_usage(const char* exe_name) {
    fprintf(stderr,
        "Usage: %s <dll_path> [target_exe]\n"
        "\n"
        "  dll_path     Full path to the payload DLL to inject.\n"
        "  target_exe   Process name (default: javaw.exe).\n"
        "\n"
        "Example:\n"
        "  %s C:\\payloads\\hook.dll javaw.exe\n",
        exe_name, exe_name);
}

int main(int argc, char* argv[]) {
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
}
