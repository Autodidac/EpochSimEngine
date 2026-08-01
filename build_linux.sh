#!/usr/bin/env bash
set -euo pipefail

CONFIG="${1:-Release}"
BUILD_DIR="build/linux-${CONFIG}"
WINDOWS_VCPKG_ROOT="/mnt/c/Users/iammi/source/repos/vcpkg"

if [[ -z "${VCPKG_ROOT:-}" ]]; then
    if [[ -x "$HOME/vcpkg/vcpkg" ]]; then
        VCPKG_ROOT="$HOME/vcpkg"
    elif [[ -x "$WINDOWS_VCPKG_ROOT/vcpkg" ]]; then
        VCPKG_ROOT="$WINDOWS_VCPKG_ROOT"
    else
        echo "ERROR: Set VCPKG_ROOT to a Linux-bootstrapped vcpkg checkout."
        echo "       For WSL, the existing checkout can be bootstrapped with:"
        echo "       cd $WINDOWS_VCPKG_ROOT && ./bootstrap-vcpkg.sh"
        exit 1
    fi
fi

if [[ ! -f "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" || ! -x "$VCPKG_ROOT/vcpkg" ]]; then
    echo "ERROR: VCPKG_ROOT does not contain a Linux vcpkg executable and toolchain: $VCPKG_ROOT"
    exit 1
fi

command -v cmake >/dev/null || { echo "ERROR: cmake is required."; exit 1; }
command -v ninja >/dev/null || { echo "ERROR: ninja is required."; exit 1; }
command -v pkg-config >/dev/null || { echo "ERROR: pkg-config is required."; exit 1; }
pkg-config --exists xcb || {
    echo "ERROR: XCB development files are required. Debian/Ubuntu: sudo apt install libxcb1-dev"
    exit 1
}

cmake -S . -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE="$CONFIG" \
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_TARGET_TRIPLET=x64-linux \
    -DBUILD_TESTING=OFF
cmake --build "$BUILD_DIR" --parallel
echo "Built: $PWD/$BUILD_DIR/sandhybrid"
