#pragma once

#ifdef __APPLE__
#include <pthread.h>
#else
#include <windows.h>
#endif

struct MinecraftState {
#ifdef __APPLE__
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#else
    CRITICAL_SECTION cs;
#endif
    double posX = 0.0;
    double posY = 0.0;
    double posZ = 0.0;
    bool   menuOpen = false;
    bool   running  = true;
};

extern MinecraftState g_state;

#ifdef __APPLE__
inline void state_init()    { pthread_mutex_init(&g_state.mutex, nullptr); }
inline void state_cleanup() { pthread_mutex_destroy(&g_state.mutex); }
inline void state_lock()    { pthread_mutex_lock(&g_state.mutex); }
inline void state_unlock()  { pthread_mutex_unlock(&g_state.mutex); }
#else
inline void state_init()    { InitializeCriticalSection(&g_state.cs); }
inline void state_cleanup() { DeleteCriticalSection(&g_state.cs); }
inline void state_lock()    { EnterCriticalSection(&g_state.cs); }
inline void state_unlock()  { LeaveCriticalSection(&g_state.cs); }
#endif
