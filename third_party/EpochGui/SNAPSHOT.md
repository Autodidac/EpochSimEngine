# EpochGui snapshot

SandHybrid carries the complete vendored EpochGui dependency from the GitHub-independent canonical mirror at https://epoch.adamrushford.chatgpt.site/git/EpochGui.git.

- Version: v0.89.29
- Commit: 8882503ac579add67456459986983ad7fd7c96db
- Annotated tag object: 873dc976322428631fc4e5090ae78966802bea31
- Immutable source archive: https://epoch.adamrushford.chatgpt.site/downloads/releases/v0.89.29/EpochGui-v0.89.29-source.tar.gz
- Archive size: 111,063 bytes
- Archive SHA-256: 7dd5ea3ed165e3d0d175c31e8db29f5637ba66d4219745d3541805948dd39ac7
- Snapshot contents: 53 upstream files with byte/file-mode parity recorded by the mirror

The source snapshot is complete rather than a selected-header copy. SandHybrid's integration delta is confined to CMakeLists.txt: the dependency floor remains CMake 3.28 for the supported package toolchains; the upstream static C++23 module build remains the default on MSVC/Clang, while EPOCHGUI_BUILD_MODULES exposes the same complete current compatibility-header surface as an interface target on GNU toolchains that cannot scan these modules. Supported upstream tests are built whenever the module target is enabled; optional rounded-rectangle and fallback-input tests remain controlled by their upstream options.