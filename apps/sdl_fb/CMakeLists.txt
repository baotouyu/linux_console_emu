add_executable(sdl_fb main.c)
target_include_directories(sdl_fb PRIVATE "${DEPS_PREFIX}/include" "${DEPS_PREFIX}/include/SDL")
target_link_libraries(sdl_fb "${DEPS_PREFIX}/lib/libSDL.a" pthread dl m)
