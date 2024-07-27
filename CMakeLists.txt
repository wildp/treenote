cmake_minimum_required (VERSION 3.23.2)
project(treenote)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
add_compile_options(-Wall -Wextra)

find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIR})

add_executable(treenote src/main.cpp
                        src/tree.cpp
                        src/note.cpp
                        src/tree_op.cpp
                        src/tree_string.cpp
                        src/utf8.cpp
                        src/window.cpp
        )

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -fsanitize=leak -fsanitize=undefined)
    target_link_options(treenote BEFORE PUBLIC -fsanitize=undefined PUBLIC -fsanitize=address)
endif()


target_link_libraries(treenote ${CURSES_LIBRARIES})
