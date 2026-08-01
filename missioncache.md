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
| MC-017 | REGRESSION | Prevent premature stabilization | Liquid, mud, and wet material sleep only after unchanged volume, support, impulse, boundary height, and erosion state for a bounded confirmation window. Gas rests only after pressure, density, composition, temperature, and incoming/outgoing transfer reach equilibrium. Touching a solid is never by itself a gas-rest condition. No region may sleep while a valid downhill, buoyant, pressure-transfer, erosion, reaction, or actor-driven path remains. |
| MC-018 | OPEN | Bulk/fine parity tests | Deterministic tests prove each eligible 8x8 move matches the equivalent conserved fine-cell result, including Atmosphere displacement, mud erosion, gas boundary flow, and the same rest decision. |
| MC-019 | REGRESSION | Material-specific settling without gas-wall friction | Damping is not a universal per-move slowdown. Gravity, buoyancy, pressure transfer, mud erosion, falling wet material, reactions, boundary changes, tool input, and actor impulses execute at the full material-defined rate. Liquids and mud may use viscosity/internal drag only after no productive move exists, reducing residual lateral oscillation toward rest. Gases have no friction against solids: a wall blocks the normal crossing direction but never adds tangential slowdown or pins gas to the surface. Gas motion ends only through pressure/composition equilibrium, density sorting, or exhausted impulse. Bulk and fine representations make the same active/rest decision. |
| MC-053 | PARTIAL | Transient gas/liquid tile packets | A complete 8x8 gas or liquid region may move as one tile packet. After packet motion ends, gas remains tile-owned only when its complete one-cell perimeter is gas; liquid remains tile-owned only when its complete perimeter contains liquid or non-gas matter. Exposed resting packets become `FINE_ACTIVE` and break back to canonical cells without deleting, filling, snapping, or changing volume. Runtime proof must show moving air/water tiles, exposed breakup, enclosed retention, and stable zero-motion results. |

## Atmosphere and closed-system gas

| ID | Status | Mission | Acceptance |
|---|---|---|---|
| MC-020 | REGRESSION | Composite Atmosphere | Replace standalone normal-air pixels with one conserved state carrying volume, pressure, temperature, and N2/O2/Ar/CO2/H2/He/vapor/contaminants. Fire, lightning, and radiation remain effects. |
| MC-021 | OPEN | Earth-air baseline | Authored air and the Atmosphere tool use approximately 78% N2, 21% O2, 0.9% Ar, trace CO2, and remaining trace gases; packed units sum exactly. |
| MC-022 | OPEN | Absorb all true gases into air | Gas painting and emissions modify local Atmosphere composition and pressure instead of replacing it. |
| MC-023 | OPEN | Visible excess-gas settling | No nested mini-simulation. Excess denser than air visibly moves down then sideways until stable; lighter excess visibly moves up then sideways. It cannot cross solids, liquids, sealed barriers, paused sections, or unloaded boundaries. Solids block crossing but do not create tangential gas friction: gas slides freely along valid open boundary paths at its density/pressure-defined rate. |
| MC-024 | OPEN | Reabsorb excess, hide only later | Stable excess reabsorbs when compatible air has capacity. Transit remains visible until Adam accepts it; debug always exposes transport afterward. |
| MC-025 | PARTIAL | Respiration and combustion | Life and combustion convert available O2 to equal represented CO2 volume. Suffocation uses breathable partial pressure. Runtime rates require proof. |
| MC-026 | OPEN | Closed-box conservation tests | Track total Atmosphere and each component, pressure transfer, separated excess, reabsorption, and conservation error. Include wall-following tests proving that gas contact blocks penetration without slowing valid lateral/upward/downward transport. |
| MC-027 | OPEN | Composition rendering | Balanced air renders smoothly; validation builds show excess transport clearly; accepted builds may later hide normal transit. |
| MC-028 | OPEN | Validate corner pressure structures | Preserve corner patterns only when conserved pressure/composition produces them. Reject structures created by gas-wall friction, sticky corners, or solid-adjacency sleep. |
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
| MC-042 | PARTIAL | Clarify movement counters | Use `FINE SWAPS`, `BULK MOVES`, `BULK CELLS`, `FINE REPAIR`, `ACTOR MOVES`, `GAS EXCESS`, `PLAYER IMP`, `GAS TILES`, `LIQUID TILES`, `ENCLOSED TILES`, and `BREAKUP TILES`. Runtime values must correspond to actual work and ownership transitions. |
| MC-043 | PARTIAL | Preserve lower GPU use | Timestamp overlay, grid, text, stats, bulk, fine, Atmosphere excess, actors, collision impulses, and presentation independently. |
| MC-044 | OPEN | Remove grid-edge coupling | Debug grid is presentation-only and never influences movement/rest. |
| MC-045 | PARTIAL | High-contrast readable debug UI | Preserve the accepted current text size. Categories use distinct high-contrast colors, tile/chunk and active-region boundaries remain readable, and the color key exactly matches damaged, stable, bulk-moved, fine-active, bulk-ready, settled, sleeping, active, enclosed, and breakup overlays. |
| MC-046 | REGRESSION | Debug never blocks the world | On ordinary layouts the complete debug panel occupies the existing sidebar instead of covering the simulation viewport. The world remains visible and interactive; no full-screen stats rectangle is permitted. Very small windows may hide or page nonessential counters, but may not cover the simulation. |
| MC-047 | OPEN | Universal cell-or-tile placement | Every selectable material can be painted as ordinary fine cells or as one aligned 8x8 tile packet through an explicit `CELLS` / `TILES` selector. Placement preserves canonical material behavior, conservation, machine metadata, actor rules, cursor mapping, and dirty-region wakeup. Block-capable material is not forced into tile mode. |

## Chemistry and materials

| ID | Status | Mission | Acceptance |
|---|---|---|---|
| MC-050 | OPEN | Correct fertilizer chemistry | Ember never directly becomes fertilizer; use a conserved compost path with ash, organics, silt/dirt, dirty water, air, time, and heat as appropriate. |
| MC-051 | PARTIAL | Wet-material, mud-erosion, and sluicing proof | Wet sand/dirt/silt, mud, and Sluice Box prove full-speed gravity-driven bulk descent, erosion through unsupported material, drying, feed conservation, and gold/silt output without fine-only fallback or damping-induced stalls. |
| MC-052 | REGRESSION | Settled granular terrain becomes structural | Dry reconstructable dirt, sand, silt, salt, and ice remain loose while sliding. After at least 52 represented cells in an 8x8 region remain unchanged and physically supported for the stabilization window, those existing cells gain structural and supported state without filling or snapping missing cells. Loss of support, fewer than 32 cohesive cells, mining, heat/phase change, player impact, or renewed motion clears structural state and restores crumbling. Side bracing may support terrain but is not required for floor-supported piles to stabilize. Runtime proof must show the pictured brown slopes settle solid and later release into slides when disturbed. |

## World, camera, scheduling, streaming, and concurrency

The canonical active shape is a **17-region starburst** centered on the camera's current map-footprint region. One active region is the complete pre-expansion map size: **640x360 cells**, never a 64x64 chunk and never an 8x8 tile.

- one center 640x360 region;
- eight adjacent map-sized regions at distance 1: N, NE, E, SE, S, SW, W, NW;
- eight outer map-sized regions at distance 2 along the same directions;
- world bounds clip unavailable spokes;
- no other region animates.

If runtime profiling proves the map-sized starburst too expensive, the only permitted fallback is an explicit camera-visible-region mode. It must be visible in the HUD/debug state and may never silently reinterpret the radius in chunk or tile units.

| ID | Status | Mission | Acceptance |
|---|---|---|---|
| MC-060 | OPEN | Camera-visible state availability | Every section visible at supported zoom is loaded and renderable. Paused sections may show their frozen state but never missing or uninitialized data. |
| MC-061 | PARTIAL | Camera-centered 17 map-area starburst | Animate exactly the center 640x360 map-footprint region plus the eight map-sized direction regions at distance 1 and distance 2, clipped only by world bounds. Code and debug must never treat 64x64 chunks or 8x8 tiles as these active regions. Runtime profiling must prove the scope; an explicit camera-visible fallback is allowed only when shown in the HUD. |
| MC-062 | PARTIAL | Hard pause outside active map areas | Outside MC-061: no movement, chemistry, ecology, actors, Atmosphere transport, pressure propagation, or simulation-debug work. Wake deterministically upon entry. Frozen regions remain renderable and cannot corrupt authored maps. |
| MC-063 | OPEN | Far-section disk streaming | Serialize clean distant paused sections, keep only a bounded resident GPU window, free buffers where appropriate, and reload deterministically with versioned corruption-safe saves. This is required before the full logical 8x8 world may be resident without MC-077 risk. |
| MC-064 | PARTIAL | Automatic active-region worker scheduler | Reserve the main thread completely for windowing, input, rendering coordination, and everything non-region-related; simulation jobs never run there. Determine hardware concurrency automatically. Use up to 17 simulation workers, one per active 640x360 map-area region when at least 18 hardware threads exist including the reserved main thread. With fewer than 18, pair additional regions onto workers from the furthest outer spokes first. Never oversubscribe, never schedule paused regions, and preserve deterministic boundary results. Current scheduler assignment exists; actual independent region execution remains runtime-unverified. |
| MC-065 | OPEN | Coroutine review | Use C++23 coroutines only for asynchronous streaming/I/O where useful, never Vulkan submission or per-cell hot paths. |
| MC-066 | OPEN | Safe pause/wake/boundary transfer | Resolve pending transfers, actors, impulses, pressure, and dependencies before pause. Crossing the starburst edge conserves all state without allowing the paused side to animate. |
| MC-067 | PARTIAL | Expand logical world 8x8 dimensions | The logical address space is 8 times the old width and height. Until MC-063 streams distant regions, only a bounded 4x4-map-footprint resident window may occupy GPU memory. Update generation, buffers, indexing, saves, limits, overflow checks, paging, and profiling without restoring a fully resident 64x cell allocation. |
| MC-068 | PARTIAL | Camera home/reset | Explicit reset returns camera to the authored 640x360 map at the bottom center of the expanded world and the documented default zoom without touching simulation state. |
| MC-069 | PARTIAL | 2x2 maximum zoom-out | Maximum zoom-out displays four pre-expansion world footprints in a 2x2 view; clamp there and preserve input/cursor mapping, active-scope clarity, and culling. |
| MC-074 | REGRESSION | Bottom-centered authored maps | Every authored scene occupies exactly one 640x360 footprint centered horizontally at the absolute bottom of the expanded world. Scene generation never repeats, stretches, or fills the remaining 8x8 world; actors, hives, machines, metadata, saves, reset, and camera home use the same offset. |
| MC-075 | PARTIAL | Camera navigation and scope HUD | Middle-mouse drag, mouse-edge scrolling, and camera reset work at every zoom. Keyboard camera panning is permitted only when the current scene has no player; MC-076 owns that routing contract. The sidebar always shows current zoom, active-region mode, and active-region count, while debug draws map-area boundaries clearly. Drag, edge, reset, HUD, and boundary rendering remain runtime-unverified. |
| MC-077 | PARTIAL | Windows GPU memory-manager crash in tile mode | Selecting `TILES` never allocates, resizes, or rebinds GPU memory. The logical world remains 8x8 map footprints, but the resident GPU window is bounded to 4x4 map footprints until MC-063 implements deterministic streaming. Windows/Linux CI must pass; Windows runtime must prove repeated CELLS/TILES switching, painting, reset, scene cycling, save/load, and debug use without device loss, TDR, WDDM reset, allocation growth, or stale descriptors. |

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
- Gas-solid contact is collision only: solids prevent penetration but never apply tangential friction, sticky-wall drag, or adjacency-based sleeping to gas.
- Mud and wet bulk elements continue gravity-driven erosion at the same material-defined rate as equivalent fine cells.
- Player/actor disturbance transfers bounded momentum/pressure without erasing or minting medium.
- Only the 17-position starburst of whole 640x360 map-area regions animates; unavailable spokes are clipped by world bounds and every other region is paused.
- The main thread never executes section simulation work.
- Worker assignment is deterministic and pairs outer map-area regions first when fewer than 17 workers are available.
- World expansion, zoom, pausing, streaming, and concurrency do not change deterministic reference results.
- Directional keyboard input has exactly one owner: the player when present, otherwise the camera. Mouse-edge and middle-mouse camera input remain independent.
- Debug statistics never cover the normal simulation viewport; debug state coloring and active-area boundaries may render in the world.
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

## v2.4.2 — runtime hierarchy, scheduling, and diagnostics pass

Runtime source merge: `2db7a6dad20a88f1ce65f54e3978f7f0daa9736f`. Windows/Linux CI run `30661800841` compiled all 12 shaders, built the C++23 library and demo, passed four contracts, and produced the published `v2.4.2` packages. Runtime observation reopened or retained the debug-layout, map placement, active-scope, settling, actors, Atmosphere, and hierarchy missions above.

## v2.4.3 — universal camera navigation

Release source: `bcd8c5135ab78d02a335329997e18d6b6fa36b1f`. Accepted CI run `30677627697` compiled all 12 shaders, built the C++23 library and demo on Windows 2022 and Ubuntu 24.04, passed all four contracts, installed packages, and uploaded both platform archives. Publication run `30678429804` published tag `v2.4.3`, the Windows archive, Linux archive, and `SHA256SUMS.txt`.

Package checksums:

- Windows: `f37fc8cf4aba001c14a1821223eb13a10f20ee85ac961efd5d32a58509d7bd22`
- Linux: `4d5a8600d99d2d9e133f3cc50e0d9f35ed2d3daae5198215cdf0315dbb52e0b3`

`MC-075` was later corrected: player scenes must not route W/A/S/D to the camera. The incorrect simultaneous-routing behavior from this release is superseded by MC-076 and must not be restored.

## MC-076 — context-sensitive W/A/S/D ownership

Completed by PR #24, merge `e7da78441a1076601764043e0825aecf982daf5d`. Directional input now has exactly one owner: player scenes route W/A/S/D exclusively to the player, while scenes without a player route it exclusively to the camera. Mouse-edge scrolling and middle-mouse drag remain independent camera controls. A shared `constexpr` router is exercised by the C++ behavior contract, and the static source validator rejects simultaneous player/camera routing. Accepted Windows/Linux CI run: `30679657812`.

## Carry-forward rule

New work receives a stable `MC-###` ID. Failed, missed, deferred, or regressed missions remain active. Completed missions move here with release evidence. Runtime contradiction reopens the same ID.
