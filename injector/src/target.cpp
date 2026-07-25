#include <cstdio>
#include <string>
#include <optional>
#include "target.hpp"
#include "error.hpp"

namespace inject {

#ifdef __APPLE__

// =========================================================================
// macOS: process discovery via libproc
// =========================================================================
#include <libproc.h>
#include <vector>

std::optional<find_target_result> find_process_by_name(
    const std::string& target_exe_name)
{
    log_info("Searching for process: %s", target_exe_name.c_str());

    // First call: get number of processes
    int count = proc_listallpids(nullptr, 0);
    if (count <= 0) {
        log_error("proc_listallpids");
        return std::nullopt;
    }

    std::vector<pid_t> pids(count * 2); // over-allocate for safety
    count = proc_listallpids(pids.data(),
                             static_cast<int>(pids.size() * sizeof(pid_t)));
    if (count <= 0) {
        log_error("proc_listallpids");
        return std::nullopt;
    }

    for (int i = 0; i < count; i++) {
        if (pids[i] == 0) continue;

        char procname[256] = {};
        proc_name(pids[i], procname, sizeof(procname));

        if (target_exe_name == procname) {
            log_info("Found %s — PID: %d", procname, (int)pids[i]);
            return find_target_result{ pids[i] };
        }
    }

    log_info("Process '%s' not found in snapshot",
             target_exe_name.c_str());
    return std::nullopt;
}

#elif defined(_WIN32)

// =========================================================================
// Windows: process discovery via Toolhelp32
// =========================================================================
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>

std::optional<find_target_result> find_process_by_name(
    const std::string& target_exe_name)
{
    log_info("Searching for process: %s", target_exe_name.c_str());

    HANDLE snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPPROCESS, 0);

    if (snapshot == INVALID_HANDLE_VALUE) {
        log_error("CreateToolhelp32Snapshot");
        return std::nullopt;
    }

    PROCESSENTRY32 pe = {};
    pe.dwSize = sizeof(pe);

    if (!Process32First(snapshot, &pe)) {
        log_error("Process32First");
        CloseHandle(snapshot);
        return std::nullopt;
    }

    std::optional<find_target_result> found;

    do {
        if (target_exe_name == pe.szExeFile) {
            found = find_target_result{ pe.th32ProcessID };
            log_info("Found %s — PID: %lu",
                     pe.szExeFile, pe.th32ProcessID);
            break;
        }
    } while (Process32Next(snapshot, &pe));

    if (!found) {
        DWORD last_err = GetLastError();
        if (last_err != ERROR_NO_MORE_FILES) {
            log_error("Process32Next");
        }
    }

    CloseHandle(snapshot);

    if (!found) {
        log_info("Process '%s' not found in snapshot",
                 target_exe_name.c_str());
    }

    return found;
}

#endif

} // namespace inject
