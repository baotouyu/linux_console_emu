set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(TOOLCHAIN_ROOT "/home/yuwei/samba/yuwei_work/chip_sdk/Rockship/RK3576/rk3576_purple_pi_oh2_linux_sdk/prebuilts/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu" CACHE PATH "aarch64-none-linux-gnu toolchain root")

set(CMAKE_C_COMPILER "${TOOLCHAIN_ROOT}/bin/aarch64-none-linux-gnu-gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_ROOT}/bin/aarch64-none-linux-gnu-g++")
set(CMAKE_SYSROOT "${TOOLCHAIN_ROOT}/aarch64-none-linux-gnu/libc")

set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
