# EpochSimEngine Mission Cache

This is the **single canonical mission document** for EpochSimEngine. It contains the active backlog, permanent invariants, and archived release history. There is no separate mission ledger.

Before changing code:

1. Read this file.
2. Preserve every `OPEN`, `PARTIAL`, `REGRESSION`, or `DEFERRED` mission.
3. Update status and evidence in the same source commit.
4. Never mark visual/runtime behavior complete from compilation, token checks, or static audits alone.
5. Verify Windows and Linux Release builds before publishing.
6. Move a mission to the release archive only after its acceptance criteria are met. Reopen the same mission ID when runtime evidence contradicts an earlier result.

Status meanings: `OPEN` not implemented; `PARTIAL` code exists but acceptance is unmet; `REGRESSION` attempted behavior is visibly wrong; `DEFERRED` intentionally scheduled later with the reason retained.

# Active missions

## Simulation hierarchy and settling

| ID | Status | Mission | Acceptance criteria / evidence |
|---|---|---|---|
| MC-011 | PARTIAL | 64x64 chunk-first work rejection | Clean off-camera chunks skip tile and fine-cell work, including safe pressure and boundary halos. Runtime profiling must prove it. |
| MC-012 | REGRESSION | 8x8 bulk-element movement | Full aligned liquids, gases, falling solids, mud, and wet materials move with the same gravity, diagonal, lateral, density, and displacement semantics as fine cells. Debug shows non-zero bulk moves in real scenes. |
| MC-013 | REGRESSION | True liquid settling without hopping | Water remained active after roughly 300,000 ticks. It must level quickly, stop completely, and wake only from changed support, pressure, incoming volume, heat, reaction, actor, or tool disturbance. |
| MC-014 | PARTIAL | Fractional-water consolidation | Fractional water represents only final partial surface volume. It is not created by every lateral move, does not chase or jitter, creates no air, and leaves no isolated pockets. |
| MC-015 | PARTIAL | Hide hierarchy artifacts | Fine boundaries visually repair bulk movement without squares, popping, seams, grid-edge clumping, or diagonal one-cell ramps. |
| MC-016 | OPEN | Rename hierarchy terminology | Choose durable names for fine cells, 8x8 bulk elements, 64x64 sections, active rings, frozen rings, and streamed regions; update code, debug UI, and docs together. |
| MC-017 | REGRESSION | Prevent premature stabilization | A liquid/gas region sleeps only after unchanged volume, pressure, composition, material class, velocity/impulse, and boundary height for a bounded confirmation window. Solid stability remains separate. |
| MC-018 | OPEN | Bulk/fine parity tests | Deterministic tests prove an eligible 8x8 bulk move matches the conserved result of equivalent fine-cell movement, including Atmosphere displacement. |
| MC-019 | REGRESSION | Universal medium damping and rest | Every gas and liquid has bounded damping/friction toward rest. Density, viscosity, pressure, and buoyancy control the rate. No medium moves forever without a continuing pressure, gravity, heat, reaction, boundary, or actor impulse. Resting media wake deterministically when disturbed. |

## Atmosphere, pressure, and closed-system gas

| ID | Status | Mission | Acceptance criteria / evidence |
|---|---|---|---|
| MC-020 | REGRESSION | Universal composite Atmosphere state | Replace standalone normal-air pixels with one conserved `Atmosphere` state carrying total volume, pressure, temperature, and N2/O2/Ar/CO2/H2/He/vapor/contaminant amounts. Fire, lightning, and radiation remain effects. |
| MC-021 | OPEN | Canonical Earth-air baseline | The Atmosphere tool and authored air use configurable Earth-like air: about 78% N2, 21% O2, 0.9% Ar, trace CO2 and remaining trace gases. Packed units sum exactly with no rounding loss. |
| MC-022 | OPEN | Absorb every gas emission into air | Nitrogen, oxygen, argon, CO2, hydrogen, helium, vapor, and true gas products add conserved component volume and pressure to local Atmosphere instead of replacing it. Gas painting changes composition. |
| MC-023 | OPEN | Direct visible excess-gas settling | Do not build a nested mini-simulation. Conserved excess denser than air moves downward then sideways until stable; lighter excess moves upward then sideways. First implementation remains visible for inspection and cannot cross solids, liquids, sealed barriers, or unloaded boundaries. |
| MC-024 | OPEN | Excess reabsorption and final presentation | Stable excess reabsorbs when nearby air has capacity. Only after runtime acceptance may normal rendering hide in-transit excess; debug must always reveal its route. |
| MC-025 | PARTIAL | Respiration and combustion through composition | Life consumes O2 and returns equal represented CO2 volume. Fire/ember do the same, bounded by oxygen and fuel. Suffocation uses breathable partial fraction and enclosure. Runtime rates still need proof. |
| MC-026 | OPEN | Closed-system gas and pressure validation | Add closed-box tests and debug totals for Atmosphere volume, each component, pressure transfer, separated excess, reabsorption, and conservation error. Component totals always equal represented gas volume. |
| MC-027 | OPEN | Composition-based rendering | Balanced air renders smoothly. Validation builds visibly show separated excess; accepted builds may hide transit in normal view while component/pressure/transport debug views retain it. |
| MC-028 | OPEN | Explain corner pressure structures | Determine whether corner patterns are valid conserved pressure/composition packing or movement artifacts. Preserve only physically generated structures. |
| MC-029 | OPEN | Atmosphere tools and inspection | Cursor/card show pressure and component percentages. Gas buttons add components; the large Atmosphere button restores canonical balanced air. |

## Life, bees, ants, beetles, and player interaction

| ID | Status | Mission | Acceptance criteria / evidence |
|---|---|---|---|
| MC-030 | REGRESSION | Remove life from fine-cell swaps | Bees, ants, beetles, queens, and player do not move by exchanging material records, stand on 8x8 edges, or count as `FINE SWAPS`. Use actor/occupancy state. |
| MC-031 | OPEN | Actor occupancy and overlap | Actors occupy positions independently while overlapping air and liquids. Their volume displaces/pressurizes media conservatively; they breathe local composition and can drown or suffocate. |
| MC-032 | PARTIAL | Complete bee lifecycle | Preserve forage, pollen pickup, return, deposit, honey feeding, queen behavior, nest aging, migration, hazard death, respiration, suffocation, old-colony replacement, and 100-bee autonomous cap. Multi-cycle gameplay proof required. |
| MC-033 | REGRESSION | Readable recurring biohazard formation | Biohazard is recognizable, dominant, repeatedly returns, and does not clump on hierarchy edges or disappear because the colony dies first. |
| MC-034 | OPEN | Real ant behavior | Colonies, pheromone trails, forage/carry/home behavior, hazard and flood avoidance, and permitted digging replace generic particle wandering. |
| MC-035 | OPEN | Real beetle behavior | Beetles crawl surfaces/walls, seek food/shelter, respond to light/hazards by species rules, turn at obstacles, and do not fly or jitter as generic particles. |
| MC-036 | OPEN | Replace undefined insect-spawner box | Give the habitat an explicit species, capacity, inputs, lifecycle, and outputs, or remove it. It must not silently spawn generic insects. |
| MC-037 | OPEN | Life debug counters | Report actor moves, species counts, respiration, suffocation, births, deaths, nest returns, and medium displacement separately from material movement. |
| MC-038 | REGRESSION | Restore exact pre-PR19 suspended hives | Use FastFreddy commit `c8197b4526b74d66e2f04a6e858dd979c63c4eff`, `tools/fix36_hive_swarm.py`, as source of truth. Sandbox, Ecosystem, and buildable Bee Nest share the exact long perch, circular shell, side entrance, chamber, queen, and colony metadata. |
| MC-039 | OPEN | Player-medium collision impulses | Player collisions minimally disturb every gas and liquid, including CO2-rich air and water. Movement injects a small bounded conserved directional impulse, wakes touched bulk/fine regions, displaces pressure/volume rather than deleting it, then MC-019 damping returns the medium to rest. No special-case-only CO2 or water path. |

## UI and debug cleanup

| ID | Status | Mission | Acceptance criteria / evidence |
|---|---|---|---|
| MC-040 | OPEN | One Atmosphere tool | Rename the large `ERASER` button to `ATMOSPHERE`. It writes canonical balanced air, not void or pure oxygen. |
| MC-041 | OPEN | Remove duplicate terrain eraser | Remove the terrain-category eraser entry. Keep one clear Atmosphere tool. |
| MC-042 | OPEN | Clarify movement counters | Rename `SWAPS` to `FINE SWAPS`. Add separate `BULK MOVES`, `BULK CELLS`, `FINE REPAIR`, `ACTOR MOVES`, `GAS EXCESS MOVES`, and `PLAYER IMPULSES`. |
| MC-043 | PARTIAL | Preserve lower GPU use | Do not regress the recent GPU reduction. Timestamp overlay, grid, text, stats, bulk, fine, Atmosphere excess, actor, collision impulse, and presentation passes separately. |
| MC-044 | OPEN | Remove grid-edge coupling | Debug grid is presentation-only. No actor or medium uses visible grid-line coordinates as preferred movement/rest positions. |

## Chemistry and materials

| ID | Status | Mission | Acceptance criteria / evidence |
|---|---|---|---|
| MC-050 | OPEN | Correct fertilizer chemistry | Ember does not directly become fertilizer. Use a conserved compost path involving ash, organic waste, silt/dirt, dirty water, air, time, and heat where appropriate. |
| MC-051 | PARTIAL | Wet materials and sluicing proof | Wet sand/dirt/silt, mud, and Sluice Box exist; gameplay must prove bulk movement, drying, feed conservation, and gold/silt output without pixel-only fallback. |

## Performance, sections, and optional concurrency

| ID | Status | Mission | Acceptance criteria / evidence |
|---|---|---|---|
| MC-060 | OPEN | Camera-visible simulation guarantee | Every visible section is loaded and fully animated before presentation. |
| MC-061 | OPEN | Twelve-nearest active sections | Deterministically prioritize 12 nearest sections with configurable count and boundary halos. |
| MC-062 | OPEN | Loaded frozen ring | Outside the active radius, sections remain memory-resident but frozen and restore before visibility. |
| MC-063 | OPEN | Far-section disk streaming | Serialize clean far sections, free buffers, reload deterministically, and use versioned corruption-safe saves. |
| MC-064 | OPEN | Optional section concurrency | Reference mode is deterministic single-thread. Optional workers process independent sections with matching boundary results. |
| MC-065 | OPEN | Coroutine review | Use C++23 coroutines only for useful asynchronous streaming/I/O, never ordered Vulkan submission or per-cell hot paths. |
| MC-066 | OPEN | Safe unseen freezing | Freeze only after pending reactions, transfers, actors, impulses, pressure, and streaming dependencies resolve. |

## Library architecture and EpochEngine migration

| ID | Status | Mission | Acceptance criteria / evidence |
|---|---|---|---|
| MC-070 | OPEN | Build `EpochSimEngine` static library | C++23 static library with ownership-safe public headers; platform windowing, UI host, and `main` remain outside the library API. |
| MC-071 | OPEN | Thin `EpochSimEngine_Demo` | Small demo executable links the static library and owns native startup/events. |
| MC-072 | OPEN | Optional subsystems | Concurrency, streaming, debug, UI, actors, ecology, and factories can be disabled without forking the core. |
| MC-073 | DEFERRED | EpochEngine integration | Later migrate/rewrite into EpochEngine using canonical `epochengine::` APIs while preserving reusable `EpochSimEngine` boundaries. |

# Permanent invariants

- Material/cell state, actor state, Atmosphere composition, and medium impulse state are authoritative; hierarchy metadata only accelerates them.
- Material behavior is canonical and provenance-independent.
- Gas/liquid volume, every gas component, and pressure are conserved. No silent creation, deletion, or conversion.
- Normal air is one Atmosphere mixture, not independent oxygen, nitrogen, argon, CO2, hydrogen, helium, or vapor cells.
- Separated excess remains visible until Adam accepts its motion; only then may normal rendering hide transit while debug keeps it visible.
- Every gas and liquid can rest. Motion requires gravity/buoyancy, pressure, heat, reaction, boundary change, tool input, or actor impulse.
- Player/actor disturbance transfers bounded momentum/pressure and never erases or mints medium volume.
- 8x8 terrain regions qualify for stability only; they never reconstruct missing cells.
- Each terrain pixel takes two laser hits to dislodge; after more than half are dislodged, the represented remainder collapses rather than vanishing.
- Missed, avoided, failed, deferred, and runtime-regressed missions remain visible until accepted.

# Archived release history

## v2.4.0 — macro hierarchy baseline

Source: `84d435300b5544a36312a6e17404f36a81ee955c`

- Shipped cached 8x8 classification, supported structural solids, canonical wet state, Sluice Box baseline, and reduced debug cost.
- Runtime reopened bulk movement, liquid settling, chunk profiling, and hierarchy presentation as MC-011 through MC-018.

## v2.4.1 — colony and represented-space correction

Source: `c73d0b97804bdefb1552a3f5ce613682d414c3ff`

- Windows/Linux C++23 builds, twelve Vulkan shaders, tests, packages, and checksums passed.
- Shipped lower respiration/fire rates and autonomous 100-bee cap in code.
- Runtime reopened exact hive restoration, actor movement, composite Atmosphere, bulk/fine parity, and settling as MC-012 through MC-044.

## Carry-forward rule

New work receives a stable `MC-###` ID. Failed, missed, deferred, or regressed missions remain active. Completed missions move here with release evidence. Runtime contradiction reopens the same ID rather than creating a duplicate document or silently rewriting history.
