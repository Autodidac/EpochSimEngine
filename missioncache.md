# EpochSimEngine Mission Cache

**Mandatory workflow:** read this file before changing code. Update it in the same commit as mission work. Never delete an OPEN, PARTIAL, BLOCKED, DEFERRED, or REGRESSION mission. A mission becomes COMPLETE only after its acceptance criteria pass on Windows and Linux and, for visual/runtime behavior, after gameplay evidence confirms the result.

Status meanings: `OPEN` not implemented; `PARTIAL` code exists but acceptance is unmet; `REGRESSION` previously attempted behavior is visibly broken; `COMPLETE` verified; `DEFERRED` intentionally scheduled later with the reason retained.

## Release v2.4.0 scope

| ID | Status | Mission | Acceptance criteria / evidence |
|---|---|---|---|
| MC-001 | COMPLETE | Preserve the v2.3.3 UI input repairs | Cursor-size controls and buttons use matching rendered/input layout, current event coordinates, and responsive hit areas. |
| MC-002 | COMPLETE | Preserve bee/bug movement through represented atmosphere | Painted bees, ants, and beetles move through gas/liquid by conserved displacement; oxygen remains breathable rather than a blocking solid. |
| MC-003 | COMPLETE | Preserve explicit oxygen/CO2 life model | Living agents consume oxygen, exchange it for CO2, and suffocate in zero oxygen, fully non-breathable gas, or liquid enclosure. Empty cells are not silently breathable. |
| MC-004 | COMPLETE | Preserve oxygen-filled eraser/vacuum scene initialization | Erased and authored atmosphere starts as canonical oxygen rather than unrepresented void. |
| MC-005 | REGRESSION | Preserve bee formation cycle | Code returns to biohazard, but the runtime symbol is difficult to read and the colony dies before repeated lifecycle cycles can be observed. |
| MC-006 | COMPLETE | Restore useful debug counters | SWAPS, macro moves, macro cells, fine repair, sleeping chunks, and skipped cells remain available. |
| MC-007 | COMPLETE | Reduce debug overlay cost | Statistics are sampled every 16 frames and unreadable dense 8x8 grid lines are omitted. |
| MC-008 | COMPLETE | Canonical wet-state materials | Wet sand, wet dirt, wet silt, and mud use material state rather than provenance; drying restores the same base material. |
| MC-009 | COMPLETE | Add Sluice Box processing | With falling water, eight wet-sand feed cells produce one gold and seven silt cells without creating or deleting represented mass. |
| MC-010 | COMPLETE | Structural integrity for cohesive solids | Full cohesive regions require physical support to stabilize; stability never reconstructs, fills, snaps, or synthesizes pixels. |
| MC-011 | PARTIAL | 64x64 chunk-first work rejection | Code caches active/sleeping/dirty/boundary chunks and skips sleeping neighborhoods. Runtime profiling must still prove that clean off-camera regions avoid fine work. |
| MC-012 | REGRESSION | 8x8 macro-element movement | v2.4.0 classified full regions, but horizontal bulk movement required literal empty cells even though open space is represented by oxygen. Runtime showed MACRO 0 / MCELL 0. |
| MC-013 | REGRESSION | True liquid settling without hopping | Equal-level random fallback was removed, but current gameplay still shows hopping and excessive movement. Water must reach a stable rest rapidly and remain still until pressure, support, or boundaries change. |
| MC-014 | PARTIAL | Half-water coalescence and presentation | Short-range attraction and stronger blended coverage exist. Isolated halves must still be eliminated without jitter, mass creation, or trapped single-half pockets. |
| MC-015 | PARTIAL | Normal rendering hides hierarchy artifacts | Normal presentation blends fine edges and half-water. Visible macro squares, block popping, and corner pressure artifacts require gameplay verification and repair. |
| MC-016 | OPEN | Rename hierarchy terminology | Choose durable names for pixel cells, 8x8 bulk elements, 64x64 simulation sections, loaded/frozen rings, and disk-streamed regions; update code, debug UI, and docs together. |

## Atmosphere and ecology follow-up

| ID | Status | Mission | Acceptance criteria / evidence |
|---|---|---|---|
| MC-020 | ACTIVE | Reduce bee oxygen consumption | v2.4.1 code target is 128x slower bee respiration and 32x slower other-life respiration. Gameplay must show long-lived colonies while sealed oxygen-free spaces still eventually kill them. |
| MC-021 | ACTIVE | Bound fire CO2 production | v2.4.1 code target converts adjacent oxygen probabilistically from fire/ember instead of every tick. Output remains one conserved O2 volume to one CO2 volume. Runtime stress test remains required. |
| MC-022 | PARTIAL | Prevent lethal local CO2 piles through pressure transport | Horizontal macro displacement is being corrected to swap full represented gas/liquid regions by density. Partial-edge pressure transport still needs runtime proof. |
| MC-023 | OPEN | Validate closed-system volume and pressure | Gases/liquids carry conserved represented volume; displacement raises pressure, transfers existing volume, and never creates or silently deletes material. Add counters and closed-box tests. |
| MC-024 | OPEN | Explain and validate oxygen corner structures | Determine whether the observed oxygen corner pattern is legitimate pressure packing or a movement artifact; preserve the attractive look only when physically consistent. |
| MC-025 | OPEN | Correct fertilizer chemistry | Ember-to-fertilizer is not accepted as a direct reaction. Define a plausible ash/organic waste/silt/dirty-water compost path and conserve all inputs and products. |
| MC-026 | ACTIVE | Restore approved suspended hive | Use the exact earlier FastFreddy suspended wood beam, nest shell, entrance, honey/pollen chamber, and queen geometry in the ecosystem scene. |
| MC-027 | ACTIVE | Cap autonomous hive population at 100 bees | Initial authored formation contains 100 bees; queen/nest reproduction refuses births at the local 100-bee cap. Explicit user-painted bees are not silently deleted. |
| MC-028 | PARTIAL | Complete bee lifecycle | Preserve queen, nest, forage, pollen pickup, return, deposit, honey feeding, migration, aging, hazard death, oxygen use, CO2 exchange, and colony replacement. Runtime multi-cycle evidence remains required. |
| MC-029 | ACTIVE | Make biohazard formation readable | Use a stable, slightly enlarged 100-point mask with slower slot remapping and minimal flutter; biohazard remains the dominant phase and must visibly recur. |

## Performance, sections, and optional concurrency

| ID | Status | Mission | Acceptance criteria / evidence |
|---|---|---|---|
| MC-030 | OPEN | Camera-visible simulation guarantee | Every section intersecting the camera is loaded and fully animated before presentation; the camera never shows stale or estimated cells. |
| MC-031 | OPEN | Twelve-nearest active sections | Activate the 12 sections nearest the player/camera first, with deterministic priority and boundary halos. The count must be configurable. |
| MC-032 | OPEN | Loaded frozen ring | Sections outside the active radius remain memory-resident but frozen. Entering the preload radius restores them before they become visible. |
| MC-033 | OPEN | Far-section disk streaming | Serialize the farthest clean sections to disk, free their live buffers, and reload them deterministically as the player approaches. Saves must be versioned and corruption-safe. |
| MC-034 | OPEN | Optional section concurrency | Default/reference mode is deterministic single-thread scheduling. An optional worker pool may process independent sections; results must match reference mode at section boundaries. |
| MC-035 | OPEN | Coroutine review | Use C++23 coroutines only for asynchronous streaming/I/O where they reduce blocking. Do not insert coroutines into ordered Vulkan submission or per-cell hot paths. |
| MC-036 | OPEN | Freeze unseen simulation safely | Off-camera sections freeze only after pending reactions, cross-boundary transfers, actors, pressure, and streaming dependencies are resolved. |
| MC-037 | OPEN | Rendering and debug GPU benchmark | Measure overlay, grid, text, stats collection, macro pass, fine pass, and presentation costs separately. Debug visualization must remain a small minority of frame GPU time. |

## Library architecture and EpochEngine migration path

| ID | Status | Mission | Acceptance criteria / evidence |
|---|---|---|---|
| MC-040 | OPEN | Build a static simulation library | Produce a C++23 static library target with public ownership-safe headers; platform windowing and `main` are not part of the library API. |
| MC-041 | OPEN | Thin demo executable | Build `EpochSimEngine_Demo` as a small executable that links the static library and owns native window/event startup. |
| MC-042 | OPEN | Optional subsystems | Concurrency, disk streaming, debug visualization, UI, actors, ecology, and factories can be disabled without forking the simulation core. |
| MC-043 | DEFERRED | EpochEngine integration | Later migrate/rewrite the library into EpochEngine using repository-canonical `epochengine::` APIs and architecture. Keep current boundaries migration-friendly. |

## Permanent invariants

- The cell/material state is authoritative; hierarchy metadata accelerates it but never replaces or invents represented material.
- Material behavior is canonical and provenance-independent.
- No silent deletion, vaporization, reconstruction, or gas creation is permitted.
- 8x8 terrain regions qualify for stability only; they never reconstruct missing cells.
- Each terrain pixel takes two laser hits to dislodge; after more than half are dislodged, the represented remainder collapses rather than vanishing.
- Missed, avoided, failed, and deferred missions remain visible in this cache.
