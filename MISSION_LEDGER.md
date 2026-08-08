# SandHybrid Mission Ledger

`missioncache.md` is the authoritative mission registry. This ledger records the current release handoff and the non-negotiable publication gate; it never replaces or closes active mission acceptance criteria.

## Current release

- Current release: `v2.5.20`, published as GitHub's latest normal public release from commit `cdd5a1474104bd8165f0f9e199dfde4712a73a5f`.
- State: replacement tag workflow `31270727984` passed both native Release matrices, tests, packages, checksums, and stable publication; tagged hygiene workflow `31270727990` also passed. The downloaded public checksum files match the package digests. Many-bee performance telemetry, broad visual scene review, generated/tool/loaded hive observation, Water leveling, and the complete cross-system scene cycle remain active; no affected visual/runtime mission is marked COMPLETE.
- Public prerelease tags and `-test` release names are forbidden.

## v2.5.20 publication record

- Scope: cap rendering at one complete fixed simulation submit per presented frame with stale-debt shedding; restore the pictured Fix29 hive body, contents, and nine structural Wood tiles; force complete structural Stone foundations in every scene; restore full Water ledge flow plus supplied Half Water splitting; and update the complete EpochGui dependency to `d8decc9` / v0.88.75.
- Native/package gate: exact-source Windows Release passes 31/31 tests, including EpochGui's three default suites; Linux Release passes all 28 SandHybrid tests against the same current compatibility headers because its GCC toolchain cannot scan C++ module dependencies. Fresh installed packages contain the executable, 12 shaders, 20 headers, development files, and 14 canonical documents. Windows/NVIDIA RTX 5080 and Linux/Mesa llvmpipe packages each pass the same 19 production Vulkan checks, including all nine foundations and cell-for-cell Sandbox/Ecosystem Fix29 hives with zero mismatches.
- Visual/runtime status: the packaged Windows build launched visibly and produced a release capture without a startup/runtime failure. Many-bee timing telemetry, broad visual scene review, generated/tool/loaded hive observation, Water leveling, and the complete cross-system scene cycle remain active; no affected mission is marked COMPLETE from the focused checks.
- Publication evidence: https://github.com/Autodidac/EpochSimEngine/releases/tag/v2.5.20 is an ordinary stable release with `isDraft=false` and `isPrerelease=false`. The tag resolves to commit `cdd5a1474104bd8165f0f9e199dfde4712a73a5f`; replacement release workflow `31270727984` and tagged hygiene workflow `31270727990` passed. Published Windows SHA-256 is `7fb9d980b6ee943bab3ce5da8c7a10d32cbcb729801b9c7282053412cfe841ea`; Linux SHA-256 is `9884c9642b48374fdbc2a15cf9330984ccc3515b65bba9784def56a297571fc6`. Both downloaded public checksum files match the package digests.
- Initial tag run `31269830501` was not published: Linux passed completely and all 28 Windows SandHybrid tests passed, but three registered EpochGui tests were not built because the dependency remained `EXCLUDE_FROM_ALL`. EpochGui is now part of the default supported Windows build; the complete replacement tag run passed.

## v2.5.19 publication record

- Release: https://github.com/Autodidac/EpochSimEngine/releases/tag/v2.5.19; ordinary stable tag and visible latest release, with no prerelease marker or `-test` suffix.
- Scope: restore scene-specific full-tile surfaces and authored empty interiors; add Designer Clear and bounded Fill uploads; recover supported breathable players; halve macro movement rate, double cohesion allowance, and drive gameplay from fixed simulation ticks.
- Native/package gate: Windows and Linux Release builds each pass 28/28 tests locally and in GitHub Actions. Fresh installed packages contain the executable, 12 shaders, 20 headers, development files, and 14 canonical documents; Windows/NVIDIA and Linux/Mesa llvmpipe packages pass identical nine-check Vulkan readback, including exact 64-cell packets, conserved 56+8 blocked fallback, Half Water, and both exact hives.
- Published package SHA-256: Windows `d264d63244dcece287cc1b266159ea436f90c911dfb595d1b8497ad818a141d5`; Linux `966487220c0a28304aa524c8103c2740efc39592e87fb43d6edf24fee1755737`. Both public checksum files name the same package digests.
- No affected mission is marked COMPLETE from these focused results. Broader scenic visual review, leveling, wet granular, machinery, save/load, bee-lifecycle, and complete scene-cycle acceptance remain active in `missioncache.md`.

## v2.5.18 publication record

- Release: https://github.com/Autodidac/EpochSimEngine/releases/tag/v2.5.18; ordinary stable tag and visible latest release, with no prerelease marker or `-test` suffix.
- Scope: align every generated resident surface to the authored grass-over-dirt plateau, restore Half Water to an early fine-only conserved state with a chemistry firewall, replace Blueprint full-world round trips with bounded validated uploads, retain the exact pictured Fix36/pre-PR19 hard-coded hive, and synchronize the selected EpochGui font contract.
- Native/package gate: Windows and Linux Release builds each pass all 27/27 tests. Fresh installed packages contain the executable, 12 shaders, 20 headers, development files, and 14 canonical documents; Windows/NVIDIA RTX 5080 and Linux/Mesa llvmpipe packages both pass all nine focused Vulkan checks with identical conserved counts. Main and tag workflows rebuilt, tested, archived, checksummed, uploaded, and published the exact source successfully.
- Published package SHA-256: Windows `d075e79e9e6020b5832cd70afe2485652e64645466981f7fbbbe9569e99dd2c7`; Linux `8fc4d6db26191c9f92ba89470599680c509a732c2a6f3e3519cdafd8e83b40e1`. Both public checksum files match GitHub asset digests.
- No affected mission is marked COMPLETE from these focused results. Broader per-row visual/runtime acceptance remains active in missioncache.md.

## v2.5.17 publication record

- Release: https://github.com/Autodidac/EpochSimEngine/releases/tag/v2.5.17; ordinary stable tag and visible release, with no prerelease marker or `-test` suffix.
- Focus: recover observed production macro Water/gas packets, Half Water state/conservation (including drip persistence), exact hard-coded Fix36 hive contents in both sandbox and ecosystem, and logical/physical cursor ownership without closing broader mission criteria.
- Implemented evidence: both exact-source native Release executables run a packaged Vulkan state-readback mode against focused dispatches of the production tile/macro/fine pipelines and return nonzero on any mismatch. Movement dispatch is bounded to the seeded first 192 columns and rows and omits unrelated chemistry, actor, effect, and sunlight work; full 640x360 reset still generates the hive.
- Visual evidence: on a 150% Windows display, direct Release-window capture shows `PAUSED`, a committed Sand edit beneath the centered world cursor, and no world cursor after the logical pointer enters the sidebar.
- Native/package gate: exact-source Windows and Linux Release builds each pass 27/27 tests. Fresh Windows (NVIDIA RTX 5080) and Linux (Mesa llvmpipe) installed packages both pass all nine focused Vulkan checks with identical conserved counts. Fresh ZIP/tarball audits find the executables, 12 shaders, 20 public headers, CMake development package, and 14 canonical docs; both SHA-256 files verify. Both GitHub workflows and stable publication passed.
- Published package SHA-256: Windows `1aa90d0cbafc0b2703ebff3a8ecca9e5c30005578f663a3b5a77f4944735be44`; Linux `edc3690b08ea1c65bdb2ff2f79ce3341f3adf8517a8c4eb1617987d156d574bd`. The downloaded public assets match both checksum files and GitHub asset digests.
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
- Beehive: photographed Fix29 Ecosystem body/content with queen centers `(512,234)`/`(512,232)`, shell `28 <= radius^2 < 108`, chamber `radius^2 < 28`, right exit `x=1..12` and `|y|<=1`, canonical entropy seed `0xD17A55DE`, and nine complete structural Wood tiles at local `x=472..543`, `y=216..223`. Reset/tool/load share one cell-for-cell model; SandHybrid actor/ecology behavior remains SandHybrid-owned.
- Pause: simulation and presentation clocks freeze while direct editor mutations remain live, including in character scenes, and do not advance or replay the simulation.
- Inventory: sidebar-only with exactly `INVENTORY` and `BLUEPRINTS` subtabs. Blueprint slots report real occupied/empty state and activate world placement only when occupied.
- Designer: sidebar-only authoring grid with exactly `INVENTORY` and `BLUEPRINTS` subtabs; the main viewport continues to show the world. Editor and Designer cursor controls are isolated. Designer publication copies exact authored material layout into a shared named slot without mutating the world.
- Cursor: visible world position, shape, radius, and Blueprint footprint use logical-window input and the same effective edit mapping; framebuffer scaling never creates a sidebar-edge ghost.
- Blueprint placement: validate the complete transformed footprint before mutation, preserve exact map-chunk `SceneCell` state, canonicalize authored static-model cells, and clear queued paste across reset/load boundaries.

## Publication gate

Before publication, the exact release source must pass Windows and Linux C++23 Release builds, all shaders and deterministic tests, install/package/archive steps, and SHA-256 generation. Runtime and visual missions remain non-COMPLETE until the packaged executable is observed. Publication evidence must include the stable release URL, tag, commit, asset names, checksums, and packaged runtime observations.
