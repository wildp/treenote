cmake_minimum_required (VERSION 3.23.2)
project(treenote
        VERSION 1.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(DEFAULT_BUILD_TYPE "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to '${DEFAULT_BUILD_TYPE}' as none was specified")
endif()

set(CMAKE_CXX_STANDARD 26)
add_compile_options(-Wall -Wextra)

find_package(Curses REQUIRED)

add_executable(treenote src/core/tree.cpp
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

target_include_directories(treenote PRIVATE CURSES_INCLUDE_DIR)

add_compile_options(-fno-rtti)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    add_compile_options(-O1)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -fsanitize=leak -fsanitize=undefined)
    target_link_options(treenote BEFORE PUBLIC -fsanitize=undefined PUBLIC -fsanitize=address)
endif()

target_link_libraries(treenote ${CURSES_LIBRARIES})

install(TARGETS treenote)

# Compiler Options Hardening #

add_compile_options(-Wformat -Wformat=2 -Wconversion -Wimplicit-fallthrough
                    -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3
                    -D_GLIBCXX_ASSERTIONS
                    -fstrict-flex-arrays=3
                    -fstack-clash-protection -fstack-protector-strong
                    -Wl,-z,nodlopen -Wl,-z,noexecstack
                    -Wl,-z,relro -Wl,-z,now
                    -Wl,--as-needed -Wl,--no-copy-dt-needed-entries
        )

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    add_compile_options(-Wtrampolines -Wbidi-chars=any)
endif()

add_compile_options(-fPIE -pie)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    add_compile_options(-fcf-protection=full)
endif()

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    add_compile_options(-Werror)
else()
    add_compile_options(-fno-delete-null-pointer-checks -fno-strict-overflow -fno-strict-aliasing -ftrivial-auto-var-init=zero)
    add_compile_options(-Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion)
endif()



