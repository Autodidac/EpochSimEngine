# SandHybrid Mission Ledger

`missioncache.md` is the authoritative mission registry. This ledger records the current release handoff and the non-negotiable publication gate; it never replaces or closes active mission acceptance criteria.

## Current release

- Current release: `v2.5.16`, published as the latest normal public GitHub release for the paused-player editing and real Blueprint-slot recovery.
- State: v2.5.16 is published from commit `aa497b8e75bb30a55db1b96a94f35c3fc75dd8c3`. Its branch and tag workflows passed native Windows/Linux Release builds, tests, packaging, and uploads; GitHub reports it as latest, non-draft, and non-prerelease with exactly two packages and two checksums. Selection marquee, copied-world chunks, thumbnails, Blueprint persistence, repeated packaged reset stress, and every other unaccepted mission remain active.
- Public prerelease tags and `-test` release names are forbidden.

## v2.5.15 publication record

- Release: https://github.com/Autodidac/EpochSimEngine/releases/tag/v2.5.15
- Commit: `0eac09f0795f4c1b025f703a8fffb6b171039756`
- Windows SHA-256: `46015c86a22d48d80e4fb6f69d553dd3ef24580ea82dfeb72511a779ac462435`
- Linux SHA-256: `4575e9949774366cacf2f73333c43c33fdd4fb59b415c28158affad09b8e27b4`
- Acceptance result: stable publication and native package gates passed. Packaged Windows observation accepted sidebar ownership, the hard-coded suspended hive location, and corrected logical cursor ownership. Macro-tile motion, Half Water motion, chamber contents, complete pause/edit/reset cycling, and broad cross-system runtime acceptance remain active.

## v2.5.16 publication record

- Release: https://github.com/Autodidac/EpochSimEngine/releases/tag/v2.5.16
- Commit: `aa497b8e75bb30a55db1b96a94f35c3fc75dd8c3`
- Windows published SHA-256: `8d2311ff24de30ee6ff245c9359dcc2ff7881326894c89fbb0eda31c151b8154`
- Linux published SHA-256: `60585da68cfb634767745d4ba1865f24f3ec424daa5bc97289ed3b210e5d565f`
- Both native Release builds pass 26/26 tests. Both archives carry the platform executable, 12 compiled shaders, installed public headers including `blueprint.hpp`, library/development files, launchers, and canonical package documentation.
- The packaged Windows executable publishes Designer slot 1, enters `PAUSED`, commits a transactional world paste, renders Inventory/Blueprint slot state beside the world, and exits cleanly. Broader visual/runtime missions remain active.
- GitHub reports the package asset digests above, and both published checksum files name the same package hashes.

## Recovery baselines

- Macro tiles: v2.5.3 moving full-liquid/full-gas eligibility, exact displacement into full uniform compatible targets even when their scheduling metadata is fine-active, intact-packet exclusion from non-fallback fine passes, and same-tick fine fallback when a packet cannot commit.
- Half Water: v2.4.1 fine-owned conserved fall, merge, dedicated passes, and bounded two-to-four-cell attraction. A half cannot sleep with fall/merge/attraction work pending and cannot wander through generic diagonal Water flow.
- Beehive: exact hard-coded pre-PR19 Ecosystem model restored by SimpleSandSim Fix36: queen centers (512,234)/(512,232), loose Wood perch x=-37..29 and y=-16..-13, shell 25 <= radius² < 92, chamber radius² < 25, queen center, exit x=1..10 with |y|<=1, and deterministic Empty/Honey/Pollen chamber contents. SandHybrid actor/ecology behavior remains SandHybrid-owned.
- Pause: simulation and presentation clocks freeze while direct editor mutations remain live, including in character scenes, and do not advance or replay the simulation.
- Inventory: sidebar-only with exactly `INVENTORY` and `BLUEPRINTS` subtabs. Blueprint slots report real occupied/empty state and activate world placement only when occupied.
- Designer: sidebar-only authoring grid with exactly `INVENTORY` and `BLUEPRINTS` subtabs; the main viewport continues to show the world. Editor and Designer cursor controls are isolated. Designer publication copies exact authored material layout into a shared named slot without mutating the world.
- Cursor: visible world position, shape, radius, and Blueprint footprint use logical-window input and the same effective edit mapping; framebuffer scaling never creates a sidebar-edge ghost.
- Blueprint placement: validate the complete transformed footprint before mutation, preserve exact map-chunk `SceneCell` state, canonicalize authored static-model cells, and clear queued paste across reset/load boundaries.

## Publication gate

Before publication, the exact release source must pass Windows and Linux C++23 Release builds, all shaders and deterministic tests, install/package/archive steps, and SHA-256 generation. Runtime and visual missions remain non-COMPLETE until the packaged executable is observed. Publication evidence must include the stable release URL, tag, commit, asset names, checksums, and packaged runtime observations.