# AGENT.md — Developer & AI Agent Context

> [!IMPORTANT]
> **BUILD RULE**: NEVER build the project locally on developer machines. ALWAYS push changes to GitHub and rely on GitHub Actions (`.github/workflows/build.yml`) to compile binaries for both macOS and Windows.
>
> **TARGET ARCHITECTURE**: The primary macOS development and testing environment is an **Apple Silicon Mac (`arm64` CPU)**. GitHub Actions automatically compiles the macOS artifacts on Apple Silicon (`arm64`) runners.

---

## Project Overview

`poc-injector` is a cross-platform (Windows & macOS) proof-of-concept DLL / dylib injection framework and JNI-based Minecraft payload overlay.

- **Windows Target**: Injects `mcpayload.dll` into `javaw.exe` using `CreateRemoteThread` + `LoadLibraryA`, hooks `wglSwapBuffers` via **MinHook**, and renders an ImGui debug overlay.
- **macOS Target**: Preloads `libmcpayload.dylib` into a Java process via `DYLD_INSERT_LIBRARIES`, hooks `CGLFlushDrawable` via **fishhook**, and renders an ImGui debug overlay using Cocoa (`NSView`) & OpenGL3 on Apple Silicon (`arm64`).

---

## Repository Structure & Branches

### Active Branches
- **`main`**: Primary branch containing full cross-platform `#ifdef __APPLE__` / `#ifdef _WIN32` unified code.
- **`feature/macos`**: Feature branch focused on macOS-specific developments, launcher scripts, and fishhook overlay integrations.
- **`feature/windows`**: Feature branch focused on Windows-specific developments, MinHook, and Win32 injection pipelines.

---

## Key Technical Learnings & Architecture

### 1. Injection & Process Discovery

| OS | Injection Mechanism | Process Discovery | Entry Point |
|---|---|---|---|
| **Windows** | 5-step pipeline: `OpenProcess` $\rightarrow$ `VirtualAllocEx` $\rightarrow$ `WriteProcessMemory` $\rightarrow$ `GetProcAddress(LoadLibraryA)` $\rightarrow$ `CreateRemoteThread` | Toolhelp32 API (`CreateToolhelp32Snapshot`, `Process32First`/`Next`) | `BOOL WINAPI DllMain(...)` (`DLL_PROCESS_ATTACH`) |
| **macOS (`arm64`)** | `DYLD_INSERT_LIBRARIES` pre-loading (`fork()` + `execvp()` in launcher or `run.sh`) | `libproc` API (`proc_listallpids()`, `proc_name()`) | `__attribute__((constructor))` static initializer |

> **macOS Gotcha**: macOS System Integrity Protection (SIP) automatically strips `DYLD_INSERT_LIBRARIES` when executing binaries under protected system paths (such as `/usr/bin/java`). Users must use a non-Apple-signed third-party JDK (e.g., Homebrew OpenJDK, Adoptium, or Zulu).

---

### 2. Graphics Hooking & Overlay Rendering

| Feature | Windows | macOS (`arm64`) |
|---|---|---|
| **Hook Library** | MinHook | fishhook (`rebind_symbols`) |
| **Hooked Symbol** | `wglSwapBuffers` (`opengl32.dll`) | `CGLFlushDrawable` (`OpenGL.framework`) |
| **ImGui Backend** | `imgui_impl_win32.cpp` + `imgui_impl_opengl3.cpp` | `imgui_impl_osx.mm` + `imgui_impl_opengl3.cpp` |
| **Window Intercept** | `SetWindowLongPtrW(GWLP_WNDPROC)` subclassing | `NSEvent` local event monitor on `NSView` |
| **Menu Toggle** | `GetAsyncKeyState(VK_RSHIFT)` | `CGEventSourceKeyState(..., kVK_RightShift)` |
| **Cursor Control** | `ShowCursor()` / `ClipCursor()` | `CGDisplayShowCursor()` / `CGDisplayHideCursor()` |

---

### 3. Cross-Platform Abstractions

- **SEH Exception Handling (`include/seh_compat.h`)**:
  - *Windows*: Native `__try` / `__except` (MSVC) or VEH + `setjmp`/`longjmp` (MinGW).
  - *macOS*: POSIX signal handling (`sigaction` for `SIGSEGV`/`SIGBUS` + `sigsetjmp`/`siglongjmp`).
- **Shared State Locking (`payload/src/shared_state.h`)**:
  - *Windows*: `CRITICAL_SECTION` (`InitializeCriticalSection`, `EnterCriticalSection`).
  - *macOS*: `pthread_mutex_t` (`PTHREAD_MUTEX_INITIALIZER`, `pthread_mutex_lock`).
- **Dynamic JVM Discovery (`include/jni_structures.h`)**:
  - *Windows*: `GetModuleHandleA("jvm.dll")` + `GetProcAddress("JNI_GetCreatedJavaVMs")`.
  - *macOS*: `dlopen("libjvm.dylib", RTLD_NOLOAD)` + `dlsym("JNI_GetCreatedJavaVMs")`.

---

## CI/CD & Build Workflow

> **REMINDER**: Never trigger local compilation commands. All builds are automatically handled by GitHub Actions upon pushing to GitHub.

### GitHub Actions CI (`.github/workflows/build.yml`)
- Multi-OS matrix build running on `windows-latest` and `macos-latest` (Apple Silicon `arm64`).
- Automatically produces pre-compiled artifacts:
  - `mcinjector-windows` (`mcinjector.exe`, `mcpayload.dll`, `run.bat`)
  - `mcinjector-macos` (`mcinjector`, `libmcpayload.dylib`, `run.sh` for ARM64 macOS)
