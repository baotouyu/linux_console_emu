#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$ROOT/build"
TOOLCHAIN_FILE="$ROOT/cmake/toolchain-aarch64-linux.cmake"
DEFAULT_TOOLCHAIN_ROOT="/home/yuwei/samba/yuwei_work/chip_sdk/Rockship/RK3576/rk3576_purple_pi_oh2_linux_sdk/prebuilts/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu"
TOOLCHAIN_ROOT="${TOOLCHAIN_ROOT:-$DEFAULT_TOOLCHAIN_ROOT}"

usage() {
    cat <<EOF
Usage: ./build.sh [command]

Commands:
  build    Configure the aarch64 toolchain and compile (default)
  clean    Remove the build directory
  help     Show this help

Environment:
  TOOLCHAIN_ROOT    aarch64-none-linux-gnu toolchain root
                    default: $DEFAULT_TOOLCHAIN_ROOT
EOF
}

build() {
    local gcc="$TOOLCHAIN_ROOT/bin/aarch64-none-linux-gnu-gcc"
    if [[ ! -x "$gcc" ]]; then
        echo "compiler not found: $gcc" >&2
        exit 1
    fi
    echo "toolchain: $TOOLCHAIN_ROOT"
    cmake -S "$ROOT" -B "$BUILD_DIR" \
        -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
        -DTOOLCHAIN_ROOT="$TOOLCHAIN_ROOT"
    cmake --build "$BUILD_DIR"
}

case "${1:-build}" in
    build)
        build
        ;;
    clean)
        rm -rf "$BUILD_DIR"
        echo "removed $BUILD_DIR"
        ;;
    help|-h|--help)
        usage
        ;;
    *)
        usage >&2
        exit 1
        ;;
esac
