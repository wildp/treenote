cmake_minimum_required (VERSION 3.23.2)
project(treenote
        VERSION 1.1)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(DEFAULT_BUILD_TYPE "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to '${DEFAULT_BUILD_TYPE}' as none was specified")
endif()

add_executable(treenote)
target_compile_features(treenote PRIVATE cxx_std_26)
target_compile_options(treenote PRIVATE -Wall -Wextra)

target_sources(treenote PRIVATE
        src/core/tree.cpp
        src/core/editor.cpp
        src/core/tree_op.cpp
        src/core/tree_string.cpp
        src/core/legacy_tree_string.cpp
        src/core/buffer.cpp
        src/core/utf8.cpp
        src/tui/keymap.cpp
        src/tui/main.cpp
        src/tui/read_helper.cpp
        src/tui/window.cpp
)

find_package(Curses REQUIRED)
target_include_directories(treenote PRIVATE CURSES_INCLUDE_DIR)

target_compile_options(treenote PRIVATE -fno-rtti)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    target_compile_options(treenote PRIVATE -O1 -fsanitize=address -fno-omit-frame-pointer -fsanitize=leak -fsanitize=undefined)
    target_link_options(treenote BEFORE PUBLIC -fsanitize=undefined PUBLIC -fsanitize=address)
endif()

target_link_libraries(treenote ${CURSES_LIBRARIES})

install(TARGETS treenote)

# Compiler Options Hardening #

target_compile_options(treenote PRIVATE 
        -Wformat -Wformat=2
        -Wconversion
        -Wimplicit-fallthrough
        -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3
        -D_GLIBCXX_ASSERTIONS
        -fstrict-flex-arrays=3
        -fstack-clash-protection
        -fstack-protector-strong
)

target_link_options(treenote PRIVATE "LINKER:-z,nodlopen,-z,noexecstack,-z,relro,-z,now,--as-needed,--no-copy-dt-needed-entries")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(treenote PRIVATE -Wtrampolines -Wbidi-chars=any)
endif()

if (CMAKE_CXX_LINK_PIE_SUPPORTED)
    set_target_properties(treenote PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
endif()

if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    target_compile_options(treenote PRIVATE -fcf-protection=full)
endif()

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    target_compile_options(treenote PRIVATE -Werror)
else()
    target_compile_options(treenote PRIVATE
            -fno-delete-null-pointer-checks
            -fno-strict-overflow
            -fno-strict-aliasing
            -ftrivial-auto-var-init=zero
    )
endif()



