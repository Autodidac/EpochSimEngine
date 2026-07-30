# FastFreddy Vulkan Water Testbed

A focused C++23 cross-platform testbed extracted from the useful parts of `fastfreddy`:

- water only
- aligned 8×8 Terraria-style stone blocks
- EpochGui rounded-rectangle panels and embedded bitmap font
- SDL3 forced to its Vulkan renderer
- vcpkg manifest mode through CMake
- Visual Studio 2022 project generation on Windows
- Ninja build on Linux

The application refuses to start if SDL does not create the `vulkan` renderer. It queries the renderer's Vulkan physical device and prints the device and API version.

## Water contract

Water mass is stored in half-units:

- faint water pixel = 1 half-unit
- full-color water pixel = 2 half-units
- two half-units combine into one full cell
- movement never creates or deletes water mass
- a half cell receives two movement substeps per 60 Hz tick
- a full cell receives one movement substep per tick

A water cell hanging over a stone lip does not begin falling until the edge cell is full and the immediately trailing cell contains at least one half-unit. That is exactly three half-units: one full cell followed by one faint half cell.

## Controls

| Input | Action |
|---|---|
| `1` | Water |
| `2` | Stone block |
| `3` | Eraser |
| Left drag | Paint selected tool |
| Right drag | Erase |
| `R` | Reset demo scene |
| `Space` | Pause/resume |
| `Esc` | Exit |

Stone painting always snaps to and fills an aligned 8×8 block.

## Windows

Requirements: Git, CMake 3.28+, Visual Studio 2022 with Desktop C++.

```bat
build_windows.bat Release
run_windows.bat Release
```

The build script automatically clones and bootstraps the pinned vcpkg baseline when `VCPKG_ROOT` is not set. CMake manifest mode installs SDL3 with Vulkan support and Vulkan itself, then generates:

```text
build\windows\FastFreddyWaterTestbed.sln
build\windows\Release\fastfreddy_testbed.exe
```

## Linux

```bash
sudo apt update
sudo apt install -y git cmake ninja-build g++-14 \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev \
  libxi-dev libxfixes-dev libxkbcommon-dev
CXX=g++-14 ./build_linux.sh
./run_linux.sh
```

## Dependency-free core validation

```bash
cmake -S . -B build/core -G Ninja \
  -DFASTFREDDY_BUILD_APP=OFF \
  -DFASTFREDDY_BUILD_TESTS=ON
cmake --build build/core
ctest --test-dir build/core --output-on-failure
```
