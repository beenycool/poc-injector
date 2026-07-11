#pragma once
#include <windows.h>
#include <setjmp.h>

extern "C" {
    typedef LONG (WINAPI *PVECTORED_EXCEPTION_HANDLER_FUNC)(struct _EXCEPTION_POINTERS*);
    WINBASEAPI PVOID WINAPI AddVectoredExceptionHandler(ULONG First, PVECTORED_EXCEPTION_HANDLER_FUNC Handler);
    WINBASEAPI ULONG WINAPI RemoveVectoredExceptionHandler(PVOID Handle);
}

static thread_local struct { jmp_buf env; DWORD code; }* g_seh_ctx = nullptr;

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
        auto* _seh_old = g_seh_ctx; \
        struct { jmp_buf env; DWORD code; } _seh_data = {}; \
        g_seh_ctx = &_seh_data; \
        if (setjmp(_seh_data.env) == 0)

#define SEH_EXCEPT(code_var) \
        RemoveVectoredExceptionHandler(_seh_h); \
        g_seh_ctx = _seh_old; \
    } else { \
        code_var = _seh_data.code; \
        RemoveVectoredExceptionHandler(_seh_h); \
        g_seh_ctx = _seh_old;
