#pragma once

#include <windows.h>
#include <string>

namespace inject {

struct win32_error {
    DWORD code;
    std::string function_name;

    win32_error(DWORD c, const char* fn) : code(c), function_name(fn) {}

    std::string format() const {
        LPSTR buf = nullptr;
        DWORD len = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER
            | FORMAT_MESSAGE_FROM_SYSTEM
            | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, code,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPSTR>(&buf), 0, nullptr);

        std::string msg;
        if (len && buf) {
            msg = buf;
            LocalFree(buf);
        } else {
            msg = "unknown error";
        }
        return function_name + " failed (0x"
               + to_hex(code) + "): " + msg;
    }

private:
    static std::string to_hex(DWORD code) {
        char hex[16];
        snprintf(hex, sizeof(hex), "%08lX", code);
        return hex;
    }
};

inline std::string format_get_last_error(const char* function_name) {
    return win32_error(GetLastError(), function_name).format();
}

inline void log_error(const char* function_name) {
    auto msg = format_get_last_error(function_name);
    OutputDebugStringA(msg.c_str());
    OutputDebugStringA("\n");
    fprintf(stderr, "[ERR] %s\n", msg.c_str());
}

inline void log_info(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    OutputDebugStringA("[INF] ");
    OutputDebugStringA(buf);
    OutputDebugStringA("\n");
    fprintf(stdout, "[INF] %s\n", buf);
}

} // namespace inject
