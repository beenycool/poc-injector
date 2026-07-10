#pragma once

#include <windows.h>
#include <string>
#include <optional>

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
