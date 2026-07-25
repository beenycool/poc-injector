#pragma once

#include <string>
#include <optional>

#ifdef __APPLE__

#include <sys/types.h>
#include <libproc.h>

namespace inject {

struct target_process {
    pid_t pid;
    std::string name;
    std::string exe_path;
};

struct find_target_result {
    pid_t pid;
};

std::optional<find_target_result> find_process_by_name(
    const std::string& target_exe_name);

} // namespace inject

#elif defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>
#undef WIN32_LEAN_AND_MEAN

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

#endif
