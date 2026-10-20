cmake_minimum_required(VERSION 3.16)
project(linux_console_emu C)

set(DEPS_PREFIX "${CMAKE_SOURCE_DIR}/deps" CACHE PATH "Static libraries built by third_party")

add_subdirectory(apps/hello)

if(EXISTS "${DEPS_PREFIX}/lib/libSDL.a")
    add_subdirectory(apps/sdl_fb)
else()
    message(FATAL_ERROR "missing ${DEPS_PREFIX}/lib/libSDL.a, run ./build.sh")
endif()
