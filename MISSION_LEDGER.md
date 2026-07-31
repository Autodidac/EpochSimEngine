# EpochSimEngine Release Ledger

This file is the durable release ledger. A mission remains open until its acceptance criteria are verified in Windows and Linux Release builds. Missed, deferred, or avoided work must remain listed rather than disappearing between releases.

## v2.4.0 macro hierarchy release

| ID | Status | Mission | Acceptance criteria |
|---|---|---|---|
| M01 | COMPLETE | Preserve v2.3.3 life/oxygen behavior | Bees, ants, and beetles retain conserved gas/liquid displacement, respiration, CO2 exchange, and suffocation behavior. |
| M02 | COMPLETE | Chunk-first work rejection | A clean sleeping 64x64 chunk skips its 8x8 tile scans; dirty/boundary chunks wake deterministically. |
| M03 | COMPLETE | Cached 8x8 macro classification | Tile metadata records uniform, macro-movable, macro-solid, macro-powder, macro-liquid, macro-gas, fine-active, wet, and settled-medium state. |
| M04 | COMPLETE | Full-block macro movement | Full aligned loose solids, powders, liquids, and gases decide movement from cached tile state before any fine pair logic; macro-moved tiles are excluded from the same frame's pixel pass. |
| M05 | COMPLETE | Structural integrity for solid blocks | Cohesive full solid regions stabilize only with physical support and remain represented by their original cells; no reconstruction or synthesized pixels. |
| M06 | COMPLETE | Wet material model | Wet sand, wet dirt, wet silt, and mud use canonical AUX_WET state; full regions move in macro passes and mixed edges receive bounded periodic fine repair. |
| M07 | COMPLETE | Settled liquid behavior | Equal-level random liquid hopping is removed; water spreads only for a drop, cover difference, or pressure gradient and can reach a true settled state. |
| M08 | COMPLETE | Half-water coalescence and presentation | Half-water receives deterministic short-range attraction toward another half, keeps conserved displaced gas, and renders with stronger smoothed coverage without changing volume. |
| M09 | COMPLETE | Sluice-box processing | A buildable Sluice Box accepts only wet sand while supplied by flowing water and conserves eight feed cells as one gold plus seven silt outputs. |
| M10 | COMPLETE | Debug regression and GPU cost | SWAPS and hierarchy skip counts are visible again; debug sampling is reduced to every 16 frames and the 8x8 grid is omitted when too dense to read. |
| M11 | COMPLETE | Rendering review | Macro/fine/settled states have distinct debug visualization while normal rendering blends half-cell edges and never exposes raw macro blocks. |
| M12 | PARTIAL | Threading/concurrency review | Native events and Vulkan simulation remain on separate explicit threads. Optional section workers, frozen rings, and disk streaming remain OPEN in `missioncache.md`. |
| M13 | COMPLETE | Cross-platform release gate | All 12 Vulkan shaders, C++23 targets, material/ecology/shader audits, three contract tests, packages, and SHA-256 checks pass on Windows and Linux before release. |

## Active follow-up

`missioncache.md` is the canonical cross-release backlog and contains the observed liquid, atmosphere, section-streaming, optional-concurrency, naming, and static-library missions.

## Carry-forward rule

Any future regression or incomplete acceptance criterion reopens the same mission ID. New work is appended; prior missions are never silently removed.
