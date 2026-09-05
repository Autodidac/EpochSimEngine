# Development and validation

## Required workflow

1. Read `missioncache.md` and `MISSION_LEDGER.md` completely.
2. Preserve every non-COMPLETE mission and acceptance criterion.
3. Cache new contradictions before implementation and add truthful evidence in the same source commit.
4. Keep library, optional runtime, and bundled example ownership separate.
5. Build and test native Windows and Linux Release packages before publication.

## Architecture

The native event thread owns Win32/XCB input and window events. A dedicated Vulkan thread owns fixed-rate simulation, compute dispatch, readback, and presentation. Canonical 16-byte cells are globally authoritative. Aligned tile and chunk metadata provide macro movement, stability, active-area rejection, and sleep without replacing cells.

The runtime submits at most one complete 60 Hz tick per presented frame, discards stale debt, and never suppresses a requested presentation merely because the tick was late. Debug collection is disabled when hidden and must remain read-only. Live MAP refresh rolls across 64 contiguous resident row bands so no presented frame copies the complete Large field; explicit save/export may still copy its declared payload. Normal fixed ticks may not copy the complete resident world.

Chemistry keeps its software-Vulkan-safe monolithic kernel shallow. Conservation corrections that would expand its selector graph run in a separate compact post-pass over the same clipped active rectangle before scratch is copied back to the canonical buffer. `sandhybrid_runtime_shaders` always copies generated SPIR-V beside the executable, including incremental shader-only builds; generated/runtime hash equality is part of native validation.

## Validation layers

- C++ unit/contract tests cover the platform-neutral library and CPU policies.
- Shader validators cover interfaces, invariants, and generated UI/material contracts.
- Production Vulkan acceptance reads back actual GPU state on Windows and Linux.
- The repeated finite-ledger gate runs twelve production-shader and public schema-2 save/load cycles in an isolated temporary fixture; it is separate from the fast state report and must agree on Windows and Linux.
- Package audits verify native binaries, shaders, public headers, documentation, paths, and checksums.
- The downstream package gate installs the library, builds both canonical and compatibility-target consumers, then executes both through CTest against the declared API version.
- Manual packaged observation covers visuals, input ownership, frame pacing, and long finite cycles.

Typical local commands:

```bash
python tools/generate_ui_text.py
python tools/validate_shader_contracts.py
cmake --build build/windows --config Release
ctest --test-dir build/windows -C Release --output-on-failure
```

Linux Release and llvmpipe acceptance run serially when WSL/compiler ownership is coordinated. Never overlap the fast production report, repeated finite-ledger report, compiler, or another GPU/runtime process in that lane.

## Documentation policy

User and package documentation lives in this small lowercase set: `library.md`, `sandhybrid.md`, and `development.md`, with the root README as the landing page. Historical uppercase design notes remain source-only while their still-active acceptance details are migrated; internal mission ledgers, runtime reports, rewrite notes, and release working files must not be copied into the runtime package root.

## Release handoff

Intermediate commits are reviewable checkpoints, not releases. A final Site handoff contains the exact clean commit/ref, displayed version/status/date, committed-tree-only source intent, artifact filenames/bytes/SHA-256, Windows/Linux build/test/runtime evidence, visual/performance evidence, mutation status, blockers, and explicit publication authorization.
