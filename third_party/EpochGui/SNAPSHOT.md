# EpochGui snapshot

EpochSimEngine's optional Vulkan backend, used by the SandHybrid demo, carries the complete vendored EpochGui dependency from the GitHub-independent canonical mirror at https://epoch.adamrushford.chatgpt.site/git/EpochGui.git.

- Version: v0.89.30
- Commit: b97167423373b9a7af3f821dcf91d8a71613dbf2
- Annotated tag object: e02ef23806b3af1584884e5eb3f6c0cf3bb5f91f
- Current source alias: https://epoch.adamrushford.chatgpt.site/downloads/sources/EpochGui-main.tar.gz
- Archive size: 113,598 bytes
- Archive SHA-256: c42bcdaa91953ef7b59a38453733431a5a73c5109df6ab151f6d78d68d734026
- Snapshot contents: 53 upstream files with byte/file-mode parity recorded by the mirror

Release v2.5.28 revalidation on 2026-09-04: the canonical Site mirror HEAD/main and peeled v0.89.30 tag still resolve to the same exact commit above. The complete existing snapshot remains current; no selective update or older pin is used.

The source alias was byte/file-mode compared with the exact v0.89.30 mirror checkout and contains the same 53 committed upstream files; the snapshot is complete rather than a selected-header copy. EpochSimEngine's integration delta is confined to CMakeLists.txt: the dependency floor remains CMake 3.28 for the supported package toolchains; the upstream static C++23 module build remains the default on MSVC/Clang, while EPOCHGUI_BUILD_MODULES exposes the complete current compatibility-header surface as an interface target on GNU toolchains that cannot scan these modules. Supported upstream tests, including the now-mandatory Input suite, are built whenever the module target is enabled; optional rounded-rectangle remains controlled by its upstream option.
