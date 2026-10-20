#!/bin/bash
# Cross-build SDL 1.2.15 as a static library with the framebuffer driver.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
: "${TOOLCHAIN_ROOT:?TOOLCHAIN_ROOT is required}"

SDL_VERSION="1.2.15"
SDL_URL="https://www.libsdl.org/release/SDL-${SDL_VERSION}.tar.gz"
CACHE="$ROOT/third_party/cache"
SRC="$ROOT/third_party/src/SDL-${SDL_VERSION}"
PREFIX="$ROOT/deps"
TARBALL="$CACHE/SDL-${SDL_VERSION}.tar.gz"
GCC="$TOOLCHAIN_ROOT/bin/aarch64-none-linux-gnu-gcc"

if [[ ! -x "$GCC" ]]; then
    echo "compiler not found: $GCC" >&2
    exit 1
fi

if [[ -f "$PREFIX/lib/libSDL.a" ]]; then
    echo "libSDL.a already built: $PREFIX/lib/libSDL.a"
    exit 0
fi

mkdir -p "$CACHE" "$ROOT/third_party/src"
if [[ ! -f "$TARBALL" ]]; then
    echo "downloading $SDL_URL"
    curl -fL --retry 3 -o "$TARBALL.part" "$SDL_URL"
    mv "$TARBALL.part" "$TARBALL"
fi

rm -rf "$SRC"
tar -xzf "$TARBALL" -C "$ROOT/third_party/src"
# SDL 1.2.15 自带的 config.sub 还不认识 aarch64。
cp "$ROOT/third_party/gnuconfig/config.sub" "$SRC/build-scripts/config.sub"
cp "$ROOT/third_party/gnuconfig/config.guess" "$SRC/build-scripts/config.guess"

export PATH="$TOOLCHAIN_ROOT/bin:$PATH"
export CC="$GCC"
export AR="$TOOLCHAIN_ROOT/bin/aarch64-none-linux-gnu-ar"
export RANLIB="$TOOLCHAIN_ROOT/bin/aarch64-none-linux-gnu-ranlib"
export STRIP="$TOOLCHAIN_ROOT/bin/aarch64-none-linux-gnu-strip"

cd "$SRC"
./configure \
    --host=aarch64-none-linux-gnu \
    --prefix="$PREFIX" \
    --enable-static \
    --disable-shared \
    --enable-video-fbcon \
    --enable-video-dummy \
    --disable-video-x11 \
    --disable-video-dga \
    --disable-video-directfb \
    --disable-video-ggi \
    --disable-video-svga \
    --disable-video-vgl \
    --disable-video-wscons \
    --disable-video-aalib \
    --disable-video-caca \
    --disable-video-qtopia \
    --disable-video-picogui \
    --disable-video-photon \
    --disable-video-ps2gs \
    --disable-video-ps3 \
    --disable-video-opengl \
    --disable-video-nanox \
    --disable-sdl-dlopen \
    --disable-alsa \
    --disable-esd \
    --disable-pulseaudio \
    --disable-arts \
    --disable-nas \
    --enable-oss \
    --enable-dummyaudio \
    --disable-input-tslib

make -j"$(nproc)"
make install
echo "installed $PREFIX/lib/libSDL.a"
