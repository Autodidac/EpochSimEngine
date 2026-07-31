# EpochSimEngine Release Ledger

This file is the durable release ledger. A mission remains open until its acceptance criteria are verified in Windows and Linux Release builds. Missed, deferred, avoided, and runtime-regressed work must remain listed rather than disappearing between releases.

## v2.4.0 macro hierarchy release — corrected by runtime evidence

| ID | Status | Mission | Acceptance criteria |
|---|---|---|---|
| M01 | REGRESSION | Preserve v2.3.3 life/oxygen behavior | Bees retained the explicit model but consumed oxygen too quickly and died from local CO2 accumulation. Reopened as MC-020 through MC-022. |
| M02 | COMPLETE | Chunk-first work rejection | A clean sleeping 64x64 chunk skips its 8x8 tile scans; dirty/boundary chunks wake deterministically. Runtime off-camera profiling remains MC-011. |
| M03 | COMPLETE | Cached 8x8 macro classification | Tile metadata records uniform, macro-movable, macro-solid, macro-powder, macro-liquid, macro-gas, fine-active, wet, and settled-medium state. |
| M04 | REGRESSION | Full-block macro movement | Runtime showed `MACRO 0 / MCELL 0`. Horizontal bulk movement rejected oxygen-filled open space by requiring literal `MAT_EMPTY`. Reopened as MC-012. |
| M05 | COMPLETE | Structural integrity for solid blocks | Cohesive full solid regions stabilize only with physical support and remain represented by their original cells; no reconstruction or synthesized pixels. |
| M06 | COMPLETE | Wet material model | Wet sand, wet dirt, wet silt, and mud use canonical AUX_WET state; full regions are eligible for macro movement and mixed edges receive bounded fine repair. |
| M07 | REGRESSION | Settled liquid behavior | Equal-level random fallback was removed, but gameplay still showed a moving slope and hopping. Reopened as MC-013. |
| M08 | PARTIAL | Half-water coalescence and presentation | Conservative attraction and presentation exist; isolated halves and settling still require runtime proof. |
| M09 | COMPLETE | Sluice-box processing | A buildable Sluice Box accepts wet sand with falling water and conserves eight feed cells as one gold plus seven silt outputs. |
| M10 | COMPLETE | Debug regression and GPU cost | SWAPS and hierarchy skip counts are visible; debug sampling is reduced and dense 8x8 grid lines are omitted. |
| M11 | PARTIAL | Rendering review | Normal rendering blends half-cell edges, but macro utilization and visible pressure/corner artifacts still require correction and measurement. |
| M12 | PARTIAL | Threading/concurrency review | Native events and Vulkan simulation remain on separate explicit threads. Optional section workers, frozen rings, disk streaming, and coroutine I/O remain open. |
| M13 | COMPLETE | Cross-platform release gate | The v2.4.0 source and packages passed their automated Windows/Linux gates. Visual/runtime acceptance was not implied by those builds. |

## v2.4.1 colony, atmosphere, and represented-space macro correction

| ID | Status | Mission | Acceptance criteria |
|---|---|---|---|
| C01 | ACTIVE | Restore approved suspended hive | Use the exact earlier FastFreddy wood support, nest shell, entrance, chamber, honey/pollen, and queen geometry. |
| C02 | ACTIVE | Cap autonomous colonies at 100 bees | Initial authored colony uses 100 formation slots and autonomous births stop at 100 local bees without deleting user-painted life. |
| C03 | ACTIVE | Slow respiration and bound fire CO2 | Bee O2 exchange is reduced 128x, other-life exchange 32x, fire/ember conversion is probabilistic, and every exchange remains one O2 volume to one CO2 volume. |
| C04 | ACTIVE | Make biohazard readable | The 100-point mask is slightly enlarged, remaps more slowly, has minimal flutter, remains dominant, and visibly recurs. |
| C05 | ACTIVE | Use represented atmosphere in horizontal macro movement | Full liquid/gas regions use validated density displacement into oxygen/CO2/etc.; no literal-empty requirement remains. |
| C06 | PARTIAL | Preserve full bee lifecycle | Forage, pollen pickup, return, deposit, honey feeding, queen migration, nest aging, hazard death, respiration, and suffocation remain wired. Multi-cycle gameplay evidence is still required. |
| C07 | OPEN | Runtime acceptance | Verify non-zero macro counters, longer-lived 100-bee colony, repeated readable biohazard cycles, bounded CO2, and stable liquid behavior in Windows and Linux builds. |

## Active follow-up

`missioncache.md` is the canonical cross-release backlog. It contains hierarchy, liquid, atmosphere, ecology, section streaming, optional concurrency, naming, static-library/demo, and EpochEngine migration missions.

## Carry-forward rule

Any future regression or incomplete acceptance criterion reopens the same mission ID. New work is appended; prior missions are never silently removed.
