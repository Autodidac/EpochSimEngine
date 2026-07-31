# EpochSimEngine Mission Cache

This is the **single canonical mission document** for EpochSimEngine. It contains active work, permanent invariants, and archived release history. There is no separate mission ledger.

Before changing code:

1. Read this file.
2. Preserve every `OPEN`, `PARTIAL`, `REGRESSION`, or `DEFERRED` mission.
3. Update status and evidence in the same source commit.
4. Never mark visual/runtime behavior complete from compilation or static audits alone.
5. Verify Windows and Linux Release builds before publishing.
6. Move a mission to the archive only after acceptance passes. Runtime contradiction reopens the same ID.
7. During the next broad pass, attempt every active mission once. Anything unfinished or runtime-unverified remains active with evidence.

Statuses: `OPEN`, `PARTIAL`, `REGRESSION`, `DEFERRED`.

# Active missions

## Simulation hierarchy and settling

| ID | Status | Mission | Acceptance |
|---|---|---|---|
| MC-011 | PARTIAL | 64x64 section-first rejection | Clean inactive sections skip tile and fine-cell work while preserving safe pressure and boundary halos; runtime profiling proves it. |
| MC-012 | REGRESSION | 8x8 bulk-element movement | Full aligned liquids, gases, falling solids, mud, and wet materials use the same gravity, diagonal, lateral, density, erosion, and displacement rules as fine cells. Real scenes show non-zero bulk moves. A valid downward or erosive bulk move is never throttled by settling damping. |
| MC-013 | REGRESSION | True liquid settling | Water levels quickly, reaches zero motion, and wakes only from support, pressure, volume, heat, reaction, actor, or tool disturbance. |
| MC-014 | PARTIAL | Fractional-water consolidation | Fractional water represents only final surface volume; no lateral splitting loop, chasing, jitter, created air, or isolated pockets. |
| MC-015 | PARTIAL | Hide hierarchy artifacts | Fine repair prevents squares, popping, seams, grid-edge clumping, and diagonal one-cell ramps. |
| MC-016 | OPEN | Durable hierarchy terminology | Rename fine cells, 8x8 bulk elements, 64x64 sections, active starburst, frozen regions, and streamed regions in code, UI, debug, and docs together. |
| MC-017 | REGRESSION | Prevent premature stabilization | Liquid, gas, mud, and wet material sleep only after unchanged volume, pressure, composition, material class, impulse, boundary height, and erosion state for a bounded confirmation window. A region cannot sleep while a valid downhill, buoyant, pressure-transfer, erosion, or actor-driven path remains. Solid structural stability remains separate. |
| MC-018 | OPEN | Bulk/fine parity tests | Deterministic tests prove each eligible 8x8 move matches the equivalent conserved fine-cell result, including Atmosphere displacement, mud erosion, and the same rest decision. |
| MC-019 | REGRESSION | Equilibrium-only medium damping | Damping is not a per-move slowdown. Gravity, buoyancy, pressure transfer, mud erosion, falling wet material, reactions, boundary changes, tool input, and actor impulses execute at the full material-defined rate. Damping begins only after no productive move exists and reduces residual lateral oscillation toward rest. Bulk and fine representations must make the same active/rest decision. |

## Atmosphere and closed-system gas

| ID | Status | Mission | Acceptance |
|---|---|---|---|
| MC-020 | REGRESSION | Composite Atmosphere | Replace standalone normal-air pixels with one conserved state carrying volume, pressure, temperature, and N2/O2/Ar/CO2/H2/He/vapor/contaminants. Fire, lightning, and radiation remain effects. |
| MC-021 | OPEN | Earth-air baseline | Authored air and the Atmosphere tool use approximately 78% N2, 21% O2, 0.9% Ar, trace CO2, and remaining trace gases; packed units sum exactly. |
| MC-022 | OPEN | Absorb all true gases into air | Gas painting and emissions modify local Atmosphere composition and pressure instead of replacing it. |
| MC-023 | OPEN | Visible excess-gas settling | No nested mini-simulation. Excess denser than air visibly moves down then sideways until stable; lighter excess visibly moves up then sideways. It cannot cross solids, liquids, sealed barriers, paused sections, or unloaded boundaries. Productive density movement is not slowed by MC-019. |
| MC-024 | OPEN | Reabsorb excess, hide only later | Stable excess reabsorbs when compatible air has capacity. Transit remains visible until Adam accepts it; debug always exposes transport afterward. |
| MC-025 | PARTIAL | Respiration and combustion | Life and combustion convert available O2 to equal represented CO2 volume. Suffocation uses breathable partial pressure. Runtime rates require proof. |
| MC-026 | OPEN | Closed-box conservation tests | Track total Atmosphere and each component, pressure transfer, separated excess, reabsorption, and conservation error. |
| MC-027 | OPEN | Composition rendering | Balanced air renders smoothly; validation builds show excess transport clearly; accepted builds may later hide normal transit. |
| MC-028 | OPEN | Validate corner pressure structures | Preserve corner patterns only when conserved pressure/composition produces them. |
| MC-029 | OPEN | Atmosphere inspection/tools | Cursor/card show pressure and gas percentages. Gas tools add components; the large Atmosphere tool restores balanced air. |

## Life, ecology, and player interaction

| ID | Status | Mission | Acceptance |
|---|---|---|---|
| MC-030 | REGRESSION | Remove life from fine swaps | Bees, ants, beetles, queens, and player use actor/occupancy state, never exchange material records, stand on hierarchy edges, or count as fine swaps. |
| MC-031 | OPEN | Actor occupancy and medium overlap | Actors overlap air/liquid independently, conservatively displace media, breathe local composition, and can drown or suffocate. |
| MC-032 | PARTIAL | Complete bee lifecycle | Forage, pollen, return, deposit, honey feeding, queen/nest aging, migration, hazards, respiration, replacement, and 100-bee autonomous cap pass multi-cycle runtime testing. |
| MC-033 | REGRESSION | Recurring readable biohazard | Formation is recognizable, dominant, recurring, and free of grid-edge clumping or premature colony death. |
| MC-034 | OPEN | Real ant behavior | Colonies, pheromone trails, forage/carry/home behavior, hazards, flooding, and permitted digging replace generic particle wandering. |
| MC-035 | OPEN | Real beetle behavior | Beetles crawl surfaces/walls, seek food/shelter, respond to light/hazards, and turn at obstacles without flying/jittering as particles. |
| MC-036 | OPEN | Define or remove insect habitat | Give it explicit species, capacity, inputs, lifecycle, and outputs, or remove it; no generic silent spawning. |
| MC-037 | OPEN | Life debug counters | Separate actor moves, species counts, respiration, suffocation, births, deaths, nest returns, and medium displacement. |
| MC-038 | REGRESSION | Exact pre-PR19 suspended hives | Use FastFreddy commit `c8197b4526b74d66e2f04a6e858dd979c63c4eff`, `tools/fix36_hive_swarm.py`. Sandbox, Ecosystem, and buildable Bee Nest share the exact perch, shell, entrance, chamber, queen, and metadata. |
| MC-039 | OPEN | Player-medium impulses | Player collisions minimally disturb every gas/liquid through bounded conserved directional impulse, wake touched sections, and then settle through MC-019 only after the injected impulse and resulting productive movement are exhausted. |

## UI and debug

| ID | Status | Mission | Acceptance |
|---|---|---|---|
| MC-040 | OPEN | One Atmosphere tool | Rename the large `ERASER` button to `ATMOSPHERE`; it writes balanced air, not void or pure oxygen. |
| MC-041 | OPEN | Remove duplicate terrain eraser | Keep only the single Atmosphere control. |
| MC-042 | OPEN | Clarify movement counters | Rename `SWAPS` to `FINE SWAPS`; add `BULK MOVES`, `BULK CELLS`, `FINE REPAIR`, `ACTOR MOVES`, `GAS EXCESS MOVES`, and `PLAYER IMPULSES`. |
| MC-043 | PARTIAL | Preserve lower GPU use | Timestamp overlay, grid, text, stats, bulk, fine, Atmosphere excess, actors, collision impulses, and presentation independently. |
| MC-044 | OPEN | Remove grid-edge coupling | Debug grid is presentation-only and never influences movement/rest. |

## Chemistry and materials

| ID | Status | Mission | Acceptance |
|---|---|---|---|
| MC-050 | OPEN | Correct fertilizer chemistry | Ember never directly becomes fertilizer; use a conserved compost path with ash, organics, silt/dirt, dirty water, air, time, and heat as appropriate. |
| MC-051 | PARTIAL | Wet-material, mud-erosion, and sluicing proof | Wet sand/dirt/silt, mud, and Sluice Box prove full-speed gravity-driven bulk descent, erosion through unsupported material, drying, feed conservation, and gold/silt output without fine-only fallback or damping-induced stalls. |

## World, camera, scheduling, streaming, and concurrency

The canonical active shape is a **17-section starburst** centered on the camera section:

- one center section;
- eight adjacent sections at distance 1: N, NE, E, SE, S, SW, W, NW;
- eight outer sections at distance 2 along the same directions;
- no other section animates.

| ID | Status | Mission | Acceptance |
|---|---|---|---|
| MC-060 | OPEN | Camera-visible state availability | Every section visible at supported zoom is loaded and renderable. Paused sections may show their frozen state but never missing or uninitialized data. |
| MC-061 | OPEN | Camera-centered 17-section starburst | Animate exactly the center plus the eight direction spokes at distance 1 and distance 2, clipped only by world bounds. This supersedes the rejected 7x7 and twelve-nearest plans. |
| MC-062 | OPEN | Hard pause outside starburst | Outside MC-061: no movement, chemistry, ecology, actors, Atmosphere transport, pressure propagation, or simulation-debug work. Wake deterministically upon entry. |
| MC-063 | OPEN | Far-section disk streaming | Serialize clean distant paused sections, free buffers where appropriate, and reload deterministically with versioned corruption-safe saves. |
| MC-064 | OPEN | Automatic section worker scheduler | Reserve the main thread completely for windowing, input, rendering coordination, and everything non-section-related; simulation jobs never run there. Determine hardware concurrency automatically. Use up to 17 simulation workers, one per active starburst section when at least 18 hardware threads exist including the reserved main thread. With fewer than 18, every missing worker causes one worker to take an additional section; assign these doubled loads from the furthest outer-ring sections first, then continue deterministic outer-to-inner round-robin only if hardware is very limited. Never oversubscribe, never schedule paused sections, and preserve deterministic boundary results. |
| MC-065 | OPEN | Coroutine review | Use C++23 coroutines only for asynchronous streaming/I/O where useful, never Vulkan submission or per-cell hot paths. |
| MC-066 | OPEN | Safe pause/wake/boundary transfer | Resolve pending transfers, actors, impulses, pressure, and dependencies before pause. Crossing the starburst edge conserves all state without allowing the paused side to animate. |
| MC-067 | OPEN | Expand world 8x8 dimensions | Width becomes 8 times current width and height becomes 8 times current height: 64 times current cell area. Update generation, buffers, indexing, saves, limits, and overflow checks. |
| MC-068 | OPEN | Camera zero/reset | Explicit reset returns camera pan/offset to world origin zero and documented default zoom without touching simulation state. |
| MC-069 | OPEN | 2x2 maximum zoom-out | Maximum zoom-out displays four pre-expansion world footprints in a 2x2 view; clamp there and preserve input/cursor mapping and culling. |

## Library architecture

| ID | Status | Mission | Acceptance |
|---|---|---|---|
| MC-070 | OPEN | `EpochSimEngine` static library | C++23 static library with ownership-safe public headers; windowing, UI host, and `main` remain outside. |
| MC-071 | OPEN | Thin `EpochSimEngine_Demo` | Small executable links the library and owns native startup/events. |
| MC-072 | OPEN | Optional subsystems | Concurrency, streaming, debug, UI, actors, ecology, and factories disable cleanly without forking the core. |
| MC-073 | DEFERRED | EpochEngine integration | Later migrate/rewrite using canonical `epochengine::` APIs while preserving reusable `EpochSimEngine` boundaries. |

# Permanent invariants

- Material cells, actors, Atmosphere composition, and medium impulses are authoritative; hierarchy metadata only accelerates them.
- Behavior is canonical and provenance-independent.
- Gas/liquid volume, every gas component, pressure, and cross-boundary transfer are conserved.
- Normal air is one Atmosphere mixture.
- Excess-gas transit remains visible until accepted.
- Every gas and liquid can rest and wake deterministically.
- Damping never suppresses a valid gravity, buoyancy, pressure, erosion, reaction, tool, or actor-driven move. It only removes non-productive residual oscillation after local equilibrium.
- Mud and wet bulk elements continue gravity-driven erosion at the same material-defined rate as equivalent fine cells.
- Player/actor disturbance transfers bounded momentum/pressure without erasing or minting medium.
- Only the 17-section camera starburst animates; every other section is paused.
- The main thread never executes section simulation work.
- Worker assignment is deterministic and pairs outer sections first when fewer than 17 workers are available.
- World expansion, zoom, pausing, streaming, and concurrency do not change deterministic reference results.
- The withdrawn camera-clipped debug-view idea is not active.
- 8x8 terrain regions qualify for stability only and never reconstruct missing cells.
- Each terrain pixel takes two laser hits; after more than half are dislodged, the represented remainder collapses rather than vanishes.
- Failed, missed, deferred, and regressed missions remain active until accepted.

# Archived release history

## v2.4.0 — macro hierarchy baseline

Source: `84d435300b5544a36312a6e17404f36a81ee955c`

Shipped cached 8x8 classification, supported structural solids, canonical wet state, Sluice Box baseline, and reduced debug cost. Runtime reopened hierarchy movement and settling missions.

## v2.4.1 — colony and represented-space correction

Source: `c73d0b97804bdefb1552a3f5ce613682d414c3ff`

Windows/Linux C++23 builds, shaders, tests, packages, and checksums passed. Runtime reopened hives, actors, Atmosphere, parity, and settling missions.

## Carry-forward rule

New work receives a stable `MC-###` ID. Failed, missed, deferred, or regressed missions remain active. Completed missions move here with release evidence. Runtime contradiction reopens the same ID.