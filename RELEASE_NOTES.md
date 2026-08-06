# SandHybrid v2.5.17

Stable packaged-state recovery for macro Water/gas tiles, Half Water, the hard-coded suspended Beehive, and high-DPI cursor ownership.

## Corrected behavior

- Ordinary full Water created by CPU Fill, Blueprint placement, PPM import, or normalization can no longer inherit the reserved Half Water flag from random auxiliary bits.
- Complete 8x8 Water and Hydrogen packets execute exact macro displacement through the production Vulkan pipelines. A blocked Water packet remains conserved and performs same-tick fine fallback.
- Half Water falls before lateral work, attracts only across a clear two-to-four-cell gap, merges without losing represented half-units, and is created only by the accepted supplied-ledge branch.
- Sandbox, Ecosystem, Beehive painting, and loaded-map normalization share the exact Fix36 pre-PR19 prefab entropy. The hard-coded Ecosystem reset now reproduces the same shell, chamber, exit, queen, wide Wood perch, and Empty/Honey/Pollen contents.
- Fragment rendering now converts physical framebuffer pixels to logical window coordinates before applying the shared UI/world layout. The preview, committed edit, camera, shape, and radius use one mapping; moving into the sidebar removes the world preview instead of clamping a ghost to the edge.
- Paused painting remains live. A Windows Release capture at 150% display scaling shows `PAUSED`, a committed Sand edit under the centered cursor, and no world cursor after entering the sidebar.

## Packaged Vulkan acceptance

The installed executable exposes an opt-in deterministic state-readback gate:

```text
sandhybrid --world-size compact --runtime-acceptance-report runtime-acceptance.json
```

It runs the shipped compute pipelines and resident buffers in one canonical 640x360 authored envelope, writes JSON, exits 0 only when every check passes, and exits 3 for a behavioral mismatch. It covers exact liquid/gas macro packets, blocked fine fallback, Half Water fall/attraction/merge/ledge creation, and the generated Ecosystem hive contents. Normal startup is unchanged when the option is absent.

## Validation and active work

The exact-source native Windows and Linux C++23 Release builds each pass 27/27 deterministic and shader/interface tests, and both executables pass all seven focused Vulkan checks (NVIDIA RTX 5080 on Windows; Mesa llvmpipe on Linux). Fresh Windows and Linux installs also pass the same seven checks. Fresh ZIP/tarball content audits pass and both SHA-256 files verify. Stable publication remains the final gate. `missioncache.md` remains authoritative: broader liquid settling, wet granular parity, scene cycling, machinery, save/load, selection, reset stress, and every other unaccepted criterion remain active rather than being reported complete from this focused recovery.

The stable tag workflow publishes exactly these four assets:

- SandHybrid-Windows-x64-v2.5.17.zip
- SandHybrid-Windows-x64-v2.5.17.zip.sha256
- SandHybrid-Linux-x64-v2.5.17.tar.gz
- SandHybrid-Linux-x64-v2.5.17.tar.gz.sha256
