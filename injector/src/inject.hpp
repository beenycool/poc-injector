#pragma once

#include <string>
#include <optional>

#ifdef __APPLE__

#include <sys/types.h>

namespace inject {

struct inject_args {
    pid_t       target_pid;         // ignored in DYLD mode; used if runtime injection
    std::string dylib_path;         // full absolute path to the payload .dylib
    std::string target_executable;  // e.g. "java" — the command to launch
    std::string jar_path;           // e.g. "Minecraft.jar" — passed as -jar arg
};

struct inject_result {
    pid_t child_pid;               // PID of the launched process
};

// On macOS, injection uses DYLD_INSERT_LIBRARIES: fork + exec the target
// with the dylib preloaded.
std::optional<inject_result> perform_injection(const inject_args& args);

} // namespace inject

#elif defined(_WIN32)

#include <windows.h>

namespace inject {

struct inject_args {
    DWORD   target_pid;
    std::string dll_path;      // full absolute path to the payload DLL
};

struct inject_result {
    HANDLE  remote_thread;     // handle to the created remote thread
    void*   remote_mem;        // remote base address of the DLL path string
};

enum class inject_step {
    open_process = 0,
    virtual_alloc,
    write_path,
    load_library_addr,
    create_remote_thread,
    done,
};

struct inject_state {
    inject_step step;
    HANDLE h_process;
    HANDLE h_thread;
    void*  remote_addr;
    size_t path_size;
};

std::optional<inject_result> perform_injection(const inject_args& args);

} // namespace inject

#endif
