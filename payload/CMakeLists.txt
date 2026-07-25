cmake_minimum_required(VERSION 3.16)
project(mcpayload LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================================
# Dependencies (fetched at build time, not committed)
# ============================================================================
include(FetchContent)

# ImGui — sources only (no CMakeLists), we compile directly
FetchContent_Declare(
    imgui
    GIT_REPOSITORY  https://github.com/ocornut/imgui.git
    GIT_TAG         v1.91.0
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

if(APPLE)
    # =========================================================================
    # macOS: fishhook for symbol rebinding (instead of MinHook)
    # =========================================================================
    # fishhook doesn't have a CMakeLists.txt, so we fetch and compile directly
    FetchContent_Declare(
        fishhook
        GIT_REPOSITORY  https://github.com/nicklockwood/fishhook.git
        GIT_TAG         0.3
    )
    FetchContent_GetProperties(fishhook)
    if(NOT fishhook_POPULATED)
        FetchContent_Populate(fishhook)
    endif()

    # Enable Objective-C++ for .mm files
    enable_language(OBJCXX)

    # ImGui sources for macOS (OSX + OpenGL3 backends)
    set(IMGUI_SRC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_osx.mm
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )

    # =========================================================================
    # Payload dylib (macOS)
    # =========================================================================
    add_library(mcpayload SHARED
        src/main.cpp
        src/overlay_macos.mm
        src/gui.cpp
        src/java_stubs.cpp
        ${IMGUI_SRC}
        ${fishhook_SOURCE_DIR}/fishhook.c
    )

    target_include_directories(mcpayload PRIVATE
        src
        ${CMAKE_SOURCE_DIR}/include
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        ${fishhook_SOURCE_DIR}
    )

    target_link_libraries(mcpayload PRIVATE
        "-framework OpenGL"
        "-framework Cocoa"
        "-framework Carbon"
        "-framework CoreGraphics"
        "-framework IOKit"
        "-framework AppKit"
        pthread
    )

    target_compile_options(mcpayload PRIVATE
        -O2
        -Wall
        # Silence warnings from third-party code
        -Wno-unused-parameter
        -Wno-deprecated-declarations  # OpenGL deprecation warnings
    )

    # .mm files need Objective-C++ compilation
    set_source_files_properties(
        src/overlay_macos.mm
        ${imgui_SOURCE_DIR}/backends/imgui_impl_osx.mm
        PROPERTIES
        COMPILE_FLAGS "-x objective-c++ -fobjc-arc"
    )

    set_target_properties(mcpayload PROPERTIES
        SUFFIX ".dylib"
        PREFIX "lib"
    )

elseif(WIN32)
    # =========================================================================
    # Windows: MinHook for inline hooking
    # =========================================================================
    FetchContent_Declare(
        minhook
        GIT_REPOSITORY  https://github.com/TsudaKageyu/minhook.git
        GIT_TAG         master
    )
    FetchContent_MakeAvailable(minhook)

    # ImGui sources for Windows (Win32 + OpenGL3 backends)
    set(IMGUI_SRC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )

    # =========================================================================
    # Payload DLL (Windows)
    # =========================================================================
    add_library(mcpayload SHARED
        src/main.cpp
        src/overlay.cpp
        src/gui.cpp
        src/java_stubs.cpp
        ${IMGUI_SRC}
    )

    target_include_directories(mcpayload PRIVATE
        src
        ${CMAKE_SOURCE_DIR}/include
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )

    target_link_libraries(mcpayload PRIVATE
        kernel32
        user32
        gdi32
        opengl32
        minhook
    )

    if(MSVC)
        target_compile_options(mcpayload PRIVATE
            /O2
            /Ob2
            /GS-
            /MT
        )
        # Silence common third-party warnings
        target_compile_options(mcpayload PRIVATE /wd4100 /wd4127 /wd4201 /wd4505 /wd4996)
        target_link_options(mcpayload PRIVATE /EMITPOGOPHASEINFO)
        set_target_properties(mcpayload PROPERTIES LINK_FLAGS "/DYNAMICBASE /NXCOMPAT")
    endif()
endif()
