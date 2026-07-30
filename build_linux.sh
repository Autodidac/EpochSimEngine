#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

command -v git >/dev/null || { echo "git is required"; exit 1; }
command -v cmake >/dev/null || { echo "CMake 3.28 or newer is required"; exit 1; }
command -v ninja >/dev/null || { echo "Ninja is required"; exit 1; }

export VCPKG_ROOT="${VCPKG_ROOT:-$ROOT/.deps/vcpkg}"
if [[ ! -f "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" ]]; then
    echo "Bootstrapping vcpkg into $VCPKG_ROOT..."
    git clone https://github.com/microsoft/vcpkg.git "$VCPKG_ROOT"
    git -C "$VCPKG_ROOT" checkout 04a735608afac5844e86fc91d6ba2112cac613c1
    "$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics
fi

cmake --preset linux-ninja
cmake --build --preset linux-release --parallel
ctest --preset linux-release

echo "Built: build/linux/fastfreddy_testbed"
