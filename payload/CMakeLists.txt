cmake_minimum_required(VERSION 3.16)
project(mcpayload LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(mcpayload SHARED
    src/main.cpp
)

target_include_directories(mcpayload PRIVATE
    src
    ${CMAKE_SOURCE_DIR}/ghidra-mcp/fun-doc
)

target_link_libraries(mcpayload PRIVATE
    kernel32
    user32
)

if(MSVC)
    target_compile_options(mcpayload PRIVATE
        /O2
        /Ob2
        /GS-
        /MT
    )
    target_link_options(mcpayload PRIVATE
        /EMITPOGOPHASEINFO
    )
    set_target_properties(mcpayload PROPERTIES
        LINK_FLAGS "/DYNAMICBASE /NXCOMPAT"
    )
endif()
