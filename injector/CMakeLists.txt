cmake_minimum_required(VERSION 3.16)
project(mcinjector LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(mcinjector
    src/main.cpp
    src/target.cpp
    src/inject.cpp
)

target_include_directories(mcinjector PRIVATE src)

if(APPLE)
    # macOS: libproc is part of the system SDK, no extra libs needed
    # pthread is linked automatically on macOS
    target_compile_options(mcinjector PRIVATE
        -O2
        -Wall
        -Wextra
    )
elseif(WIN32)
    # Toolhelp32 APIs are part of kernel32.lib on modern Windows SDKs
    target_link_libraries(mcinjector PRIVATE
        kernel32
        advapi32
    )

    # Optimize for small binary
    if(MSVC)
        target_compile_options(mcinjector PRIVATE
            /O2         # max speed
            /Ob2        # aggressive inlining
            /GS-        # no buffer security check (injector, not a service)
            /MT         # static CRT (no VCRedist dependency)
        )
        target_link_options(mcinjector PRIVATE
            /EMITPOGOPHASEINFO
        )
        set_target_properties(mcinjector PROPERTIES
            LINK_FLAGS "/DYNAMICBASE /NXCOMPAT"
        )
    endif()
endif()
