# EpochSimEngine development and validation

## Required workflow

1. Read `missioncache.md` and `MISSION_LEDGER.md` completely.
2. Preserve every non-COMPLETE mission and acceptance criterion.
3. Cache new contradictions before implementation and add truthful evidence in the same source commit.
4. Keep library, optional runtime, and bundled example ownership separate.
5. Build and test native Windows and Linux Release packages before publication.

## Architecture

The native event thread owns Win32/XCB input and window events. A dedicated Vulkan thread owns fixed-rate simulation, compute dispatch, readback, and presentation. Canonical 16-byte cells are globally authoritative. Aligned tile and chunk metadata provide macro movement, stability, active-area rejection, and sleep without replacing cells.

Each scheduler iteration submits at most one complete fixed 60 Hz tick and discards stale debt. A 30 FPS presentation cap permits intervening simulation-only iterations; rates above 60 FPS permit render-only presentations. A late tick never creates a catch-up burst or duplicates one-shot input. Debug collection is disabled when hidden and must remain read-only. Live MAP refresh rolls across up to 64 contiguous, tile-aligned resident row bands so no presented frame copies the complete Large field. A separate typed MAP tile snapshot follows those same bands; frozen MAP cells never borrow live Queen metadata. Explicit save/export may still copy its declared payload. Normal fixed ticks may not copy the complete resident world.

Chemistry keeps its software-Vulkan-safe monolithic kernel shallow. Conservation corrections that would expand its selector graph run in a separate compact post-pass over the same clipped active rectangle before scratch is copied back to the canonical buffer. `sandhybrid_runtime_shaders` always copies generated SPIR-V beside the executable, including incremental shader-only builds; generated/runtime hash equality is part of native validation.

Sunlight keeps attenuated per-cell intensity and a width-word tail encoding each column's first opaque row plus one (zero means uninitialized). The same top-down pass calculates both; grass checks direct exterior exposure rather than treating cave gas as sky. Only the complete eight-Stone top containment shell is exempt from shading; interior roofs, incomplete top strips, and weather attenuation remain effective. Already-emitted tracked rain swaps its exact Water payload through gas or Vacuum on its existing fixed cadence, without advancing dry Cloud or waking settled pools.

## Validation layers

Beehive editing owns one complete colony per press, never one global tool colony. Authored homes keep their existing metadata; out-of-district persistent homes use the shared tagged global-home codec. Load/reset rebuilds only the derived ecology index (feature flags, feature locations and Bee counts) once so paused/off-camera Queens render and population accounting sees their owners. This indexing does not classify physical ownership, alter canonical cells or low occupancy counts, age stability, or advance macro movement/failure budgets. Normal ticks remain bounded. A new hive cannot erase a nearby accepted hive, and independent hives cannot suppress each other's replacement budget. No-Flower search is a bounded six-bee motion state, not a source of food or material.

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

EpochSimEngine is the sole project, library, source-package, and engine-release name. SandHybrid refers only to the demo. Legacy API/build identifiers, save-format markers, exact old artifact filenames, URLs, and historical evidence are compatibility records and must not be rewritten as though their bytes or published provenance changed. New library examples use `epochsimengine/*` headers and `epochsimengine::`; new source archives use `EpochSimEngine-vVERSION-source.zip` or `.tar.gz`. Demo archives may remain `SandHybrid-Windows-x64-vVERSION.zip` and `SandHybrid-Linux-x64-vVERSION.tar.gz`, explicitly labelled as demos.

User and package documentation lives in this small lowercase set: `library.md`, `sandhybrid.md`, and `development.md`, with the root README as the landing page. Historical uppercase design notes remain source-only while their still-active acceptance details are migrated; internal mission ledgers, runtime reports, rewrite notes, and release working files must not be copied into the runtime package root.

## Release handoff

Intermediate commits are reviewable checkpoints, not releases. A final Site handoff contains the exact clean commit/ref, displayed version/status/date, committed-tree-only source intent, artifact filenames/bytes/SHA-256, Windows/Linux build/test/runtime evidence, visual/performance evidence, mutation status, blockers, and explicit publication authorization.
