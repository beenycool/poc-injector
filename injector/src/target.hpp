#pragma once

#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <optional>

namespace inject {

struct target_process {
    DWORD pid;
    std::string name;
    std::string exe_path;
};

struct find_target_result {
    DWORD pid;
};

std::optional<find_target_result> find_process_by_name(
    const std::string& target_exe_name);

} // namespace inject
