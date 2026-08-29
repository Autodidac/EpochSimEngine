# SandHybrid v2.5.26

Post-v2.5.25 weather, material, laser, hive, debug, Nuke, and frame-pacing correction for the SandHybrid example/runtime bundled with EpochSimEngine.

## Corrected behavior

- Preserves the photographed Fix29 Sandbox, Ecosystem, and tool-placed hive bodies immediately and after 120 fixed ticks, including the exact supported shell/content/perch and 100 live district-home SandHybrid bees.
- Keeps recognizable base material colors in Debug and uses sparse state markers plus a complete damage outline; hidden/visible Debug collection remains byte-identical to production simulation.
- Renames the former Ignite Air action to `NUKE FROM SPACE`, presents six warning-light frames, then performs one deterministic GPU Atmosphere-to-Fire edit without a full-world CPU readback.
- Makes the player laser a world-to-world material transfer: it never creates Vacuum, never collects inventory, targets liquids and vegetation as well as solids, and retains the source when no deterministic body-clear gas swap can commit.
- Prevents Acid adjacency and moist Waste from inventing Water-family units. Wet Waste becomes Fertilizer while the real Water owner remains; unsupported inorganic Acid dissolution remains unchanged until a paired Acid/solute owner exists.
- Replaces synchronized bulk rain with sparse lower-edge Cloud precipitation staggered by eight-column band. Steam and Dirty Steam join or nucleate the continuous high-sky Cloud deck one-for-one, and every rain cell carries its source temperature.
- Removes the observed presentation spikes: late fixed ticks no longer suppress requested frames, live MAP refresh copies one of 16 row bands, reset no longer exports a full-world PPM, and Nuke never downloads/floods the complete world on the CPU.
- Keeps the proven software-Vulkan chemistry kernel byte-identical and applies the new conservative ownership rules in one shallow post-pass, avoiding the llvmpipe optimizer cliff.
- Makes every incremental build deploy generated SPIR-V beside the executable so runtime tests cannot silently load an older shader.

## Release verification

The exact versioned Windows Release build passes all `43/43` CTests, including ten supported EpochGui v0.89.30 suites. The native Linux Release build passes all `33/33` CTests. Fresh Windows/RTX 5080 and Linux/llvmpipe runtime trees each pass all `63/63` production Vulkan checks, including macro-packet travel/breakup, zero-jitter Water and Half Water, H2/O2 synthesis, Atmosphere respiration, finite weather and rock loops, Acid/Waste ownership, balanced Volcano output, non-collecting laser transfer, Nuke, Debug byte identity, and exact immediate/delayed hives.

The final Linux llvmpipe gate completes in `8:42.79`, peaks at `3,114,352` KiB RSS, and uses zero swap. Broad packaged visual judgment, measured interactive frame-time capture, and repeated long finite-system/save-load cycles remain active in `missioncache.md`; deterministic readback does not mark those missions COMPLETE.

## Dependency snapshot

The complete vendored EpochGui dependency is synchronized from the canonical GitHub-independent Site mirror to v0.89.30 at `b97167423373b9a7af3f821dcf91d8a71613dbf2`. The current 113,598-byte Site source alias verifies SHA-256 `c42bcdaa91953ef7b59a38453733431a5a73c5109df6ab151f6d78d68d734026` and matches the 53-file tagged tree. The documented CMake 3.28/GNU module-compatibility boundary remains the only integration delta.

## Stable release assets

- SandHybrid-Windows-x64-v2.5.26.zip
- SandHybrid-Windows-x64-v2.5.26.zip.sha256
- SandHybrid-Linux-x64-v2.5.26.tar.gz
- SandHybrid-Linux-x64-v2.5.26.tar.gz.sha256
- SandHybrid-v2.5.26-source.tar.gz
- SandHybrid-v2.5.26-source.tar.gz.sha256

Updater-facing aliases remain `SandHybrid-Windows-x64-current.zip`, `SandHybrid-Linux-x64-current.tar.gz`, and `simengine-source.tar.gz`; each must be byte-identical to its immutable v2.5.26 object and carry a matching checksum sidecar. The public release is a normal visible Site release. GitHub repository, tags, and releases remain outside this publication scope.
