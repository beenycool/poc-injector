/*
 * SEH compatibility — cross-platform structured exception handling.
 *
 * Windows (MSVC):  native __try/__except
 * Windows (MinGW): VEH + setjmp/longjmp fallback
 * macOS / POSIX:   signal(SIGSEGV/SIGBUS) + sigsetjmp/siglongjmp
 *
 * WARNING (MSVC): SEH_TRY / SEH_EXCEPT blocks cannot contain C++ objects
 * with non-trivial destructors. Under MSVC this triggers compiler error
 * C2712 ("Cannot use __try in a function that requires object unwinding").
 * Keep SEH-protected scope plain-C — no std::string, no RAII wrappers
 * that allocate on the stack inside SEH_TRY { }.
 *
 * WARNING (macOS): signal-based SEH is not fully reentrant. Avoid nested
 * SEH_TRY blocks on the same thread. The handler restores the previous
 * signal disposition on exit.
 */
#pragma once

#ifdef __APPLE__

// =========================================================================
// macOS / POSIX: signal-based SEH replacement
// =========================================================================
#include <signal.h>
#include <setjmp.h>
#include <cstdlib>
#include <cstdint>
#include <unistd.h>

struct SehFrame { sigjmp_buf env; int signal_code; };
static thread_local SehFrame* g_seh_ctx = nullptr;

inline void seh_signal_handler(int sig) {
    if (g_seh_ctx) {
        g_seh_ctx->signal_code = sig;
        siglongjmp(g_seh_ctx->env, 1);
    }
    _exit(128 + sig);
}

#define SEH_TRY \
    { \
        struct sigaction _sa_new = {}, _sa_old_segv = {}, _sa_old_bus = {}; \
        _sa_new.sa_handler = seh_signal_handler; \
        sigemptyset(&_sa_new.sa_mask); \
        sigaction(SIGSEGV, &_sa_new, &_sa_old_segv); \
        sigaction(SIGBUS,  &_sa_new, &_sa_old_bus); \
        SehFrame* _seh_old = g_seh_ctx; \
        SehFrame _seh_data = {}; \
        g_seh_ctx = &_seh_data; \
        if (sigsetjmp(_seh_data.env, 1) == 0) {

#define SEH_EXCEPT(code_var) \
            sigaction(SIGSEGV, &_sa_old_segv, nullptr); \
            sigaction(SIGBUS,  &_sa_old_bus, nullptr); \
            g_seh_ctx = _seh_old; \
        } else { \
            (code_var) = _seh_data.signal_code; \
            sigaction(SIGSEGV, &_sa_old_segv, nullptr); \
            sigaction(SIGBUS,  &_sa_old_bus, nullptr); \
            g_seh_ctx = _seh_old; \
        } \
    }

#elif defined(_WIN32)

// =========================================================================
// Windows SEH
// =========================================================================
#include <windows.h>

#ifdef _MSC_VER

// MSVC: native SEH — maps directly to __try/__except
#define SEH_TRY __try
#define SEH_EXCEPT(code_var) __except((code_var) = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER)

#else

// MinGW: VEH + setjmp/longjmp fallback
#include <setjmp.h>

extern "C" {
    typedef LONG (WINAPI *PVECTORED_EXCEPTION_HANDLER_FUNC)(struct _EXCEPTION_POINTERS*);
    WINBASEAPI PVOID WINAPI AddVectoredExceptionHandler(ULONG First, PVECTORED_EXCEPTION_HANDLER_FUNC Handler);
    WINBASEAPI ULONG WINAPI RemoveVectoredExceptionHandler(PVOID Handle);
}

struct SehFrame { jmp_buf env; DWORD code; };
static thread_local SehFrame* g_seh_ctx = nullptr;

inline LONG WINAPI SehCompatHandler(struct _EXCEPTION_POINTERS* ep) {
    if (g_seh_ctx) {
        g_seh_ctx->code = ep->ExceptionRecord->ExceptionCode;
        longjmp(g_seh_ctx->env, 1);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

#define SEH_TRY \
    { \
        PVOID _seh_h = AddVectoredExceptionHandler(1, (PVECTORED_EXCEPTION_HANDLER_FUNC)SehCompatHandler); \
        SehFrame* _seh_old = g_seh_ctx; \
        SehFrame _seh_data = {}; \
        g_seh_ctx = &_seh_data; \
        if (setjmp(_seh_data.env) == 0) {

#define SEH_EXCEPT(code_var) \
            RemoveVectoredExceptionHandler(_seh_h); \
            g_seh_ctx = _seh_old; \
        } else { \
            (code_var) = _seh_data.code; \
            RemoveVectoredExceptionHandler(_seh_h); \
            g_seh_ctx = _seh_old; \
        } \
    }

#endif // _MSC_VER vs MinGW

#endif // __APPLE__ vs _WIN32
