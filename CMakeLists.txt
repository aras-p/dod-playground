cmake_minimum_required(VERSION 3.12)
project(dod_playground)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)

find_package(X11 REQUIRED)
find_package(OpenGL REQUIRED)

add_compile_definitions(
        SOKOL_GLCORE33)

include_directories(
        ${X11_INCLUDE_DIR})

add_executable(dod_playground
        source/external/sokol.c
        source/external/sokol_app.h
        source/external/sokol_gfx.h
        source/external/sokol_time.h
        source/external/stb_easy_font.h
        source/external/stb_image.h
        source/application.c
        source/game.cpp
        source/game.h)

target_link_libraries(dod_playground
        OpenGL::GL
        ${X11_LIBRARIES}
        dl)
