cmake_minimum_required(VERSION 3.12)
project(ashi_view VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4 /O2)
else()
    add_compile_options(-Wall -Wextra -pedantic -Wno-unused-parameter -O3)
endif()

include_directories(include vendor)

set(SOURCES
    src/base64.c
    src/image.c
    src/protocol.c
    src/render_halfblock.c
    src/render_iterm.c
    src/render_kitty.c
    src/resample.c
    src/stb_impl.c
    src/terminal.c
    src/main.c
)

add_executable(ashi_view ${SOURCES})

if(NOT MSVC)
    target_link_libraries(ashi_view m)
endif()

install(TARGETS ashi_view DESTINATION bin)
