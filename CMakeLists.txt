cmake_minimum_required (VERSION 3.23.2)
project(treenote)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 26)
add_compile_options(-Wall -Wextra)

find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIR})

add_executable(treenote src/treenote/tree.cpp
                        src/treenote/note.cpp
                        src/treenote/tree_op.cpp
                        src/treenote/tree_string.cpp
                        src/treenote/legacy_tree_string.cpp
                        src/treenote/note_buffer.cpp
                        src/treenote/utf8.cpp
                        src/treenote_tui/keymap.cpp
                        src/treenote_tui/main.cpp
                        src/treenote_tui/read_helper.cpp
                        src/treenote_tui/window.cpp
        )

add_compile_options(-fno-rtti)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -fsanitize=leak -fsanitize=undefined)
    target_link_options(treenote BEFORE PUBLIC -fsanitize=undefined PUBLIC -fsanitize=address)
endif()


target_link_libraries(treenote ${CURSES_LIBRARIES})
