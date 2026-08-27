# EpochGui snapshot

SandHybrid carries the complete vendored EpochGui dependency from the GitHub-independent canonical mirror at https://epoch.adamrushford.chatgpt.site/git/EpochGui.git.

- Version: v0.89.27
- Commit: b23f283dd9b0d6021dccd8fbc2235306418aa66a
- Annotated tag object: 214faacc0b45b87b010a7fa6db53c68ee5a89859
- Immutable source archive: https://epoch.adamrushford.chatgpt.site/downloads/releases/v0.89.27/EpochGui-v0.89.27-source.tar.gz
- Archive size: 96,950 bytes
- Archive SHA-256: 842f9e6372a9742b1a0eaf72b4ac456a0d1f0a596888cac0b0dbbccbca3e01a0
- Snapshot contents: 50 upstream files with byte/file-mode parity recorded by the mirror

The source snapshot is complete rather than a selected-header copy. SandHybrid's integration delta is confined to CMakeLists.txt: the dependency floor remains CMake 3.28 for the supported package toolchains; the upstream static C++23 module build remains the default on MSVC/Clang, while EPOCHGUI_BUILD_MODULES exposes the same complete current compatibility-header surface as an interface target on GNU toolchains that cannot scan these modules. Supported upstream tests are built whenever the module target is enabled; optional rounded-rectangle and fallback-input tests remain controlled by their upstream options.