#include "target.hpp"
#include "error.hpp"
#include <cstdio>
#include <windows.h>
#include <tlhelp32.h>

namespace inject {

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

    PROCESSENTRY32A pe = {};
    pe.dwSize = sizeof(pe);

    if (!Process32FirstA(snapshot, &pe)) {
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
    } while (Process32NextA(snapshot, &pe));

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

} // namespace inject
