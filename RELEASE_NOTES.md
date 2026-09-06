# EpochSimEngine v2.5.29

EpochSimEngine is the project and reusable simulation library. SandHybrid names only its bundled Windows/Linux demo.

## Release preparation status

This is a fresh corrective release candidate, not an overwrite of the frozen local v2.5.28 tag or packages. Public v2.5.27 remains unchanged until the Site owner receives the exact accepted committed source and final artifact handoff. The user has requested publication; native installed-package checks remain required. Personal visual acceptance is not claimed.

The focused source checkpoint passes Windows Release CTest 82/82, native GNU11.4 Linux Release CTest 71/71 and RTX 5080 production Vulkan 163/163. These results precede the final versioned package rebuild. Current installed Windows/Linux runtime, finite/save-cycle and presentation reports will be recorded separately; historical v2.5.28 reports do not validate these bytes.

## Corrected behavior

- Each Beehive press creates one complete photographed saturated-golden, no-perch Fix29 hive. Holding the button or moving the pointer no longer stamps duplicate colonies. Cells/Tiles mode and line-shaped brushes use the same complete prefab.
- Beehive is an independent one-colony-per-press Editor tool, not a singleton Blueprint. Later placement preserves earlier colonies and supports clear sky/gap homes using backward-compatible tagged ownership. Physical footprint/overlap checks still apply. Recognizable obsolete bodies are cleaned only within the admitted placement footprint.
- Cleanup uses an immutable bounded snapshot and parallel single-cell ownership, including far-ranging old-colony foragers. This replaces a serial scan that failed to reach the old body on Linux software Vulkan.
- Exactly 60 live district-home SandHybrid bees retain unique home and formation slots; independently placed global-home colonies also retain exactly 60 slots. Six foragers can search, return and rejoin without a Flower while 54 resting Bees retain the accepted three outward-open crescents. Searching alone creates no food. Movement claims prevent repeated movement within one dispatch.
- A newborn passing through the entrance no longer incorrectly blocks finite Queen ventilation. A simultaneous Bee breath consumes its own Oxygen first; the Queen requires a further real Oxygen-to-CO2 exchange. Foreign life, occupied donors, flooding, heat and exhausted oxygen remain genuine failure conditions.
- Map reset now gives canonical hive shell cells the same supported structural ownership and health as the Beehive tool. This repairs the smooth fallback body without changing the accepted ragged artwork. Immediate/delayed state checks and actual rendered-hive witnesses now reject the formerly missed constructor mismatch.
- Smoke and tiled direct-life brushes serialize overlapping shafts in three disjoint column phases. Conveyor cargo swaps exact displaced gas. Food/Waste/Fertilizer input credit requires matching admitted source debit, capacity, active dispatch and awake endpoints. Unpaired machine outputs and general packed-gas compression remain separate open work.
- Half Water stays visually liquid instead of interpreting its displaced-medium byte as Steam opacity. Off-label pixels reject glyph work early without changing the authored label raster; work-count reductions are not a measured whole-simulation speedup.
- Frontier reset opens its blocked catchment inlet and restores the overwritten Factory Core with the same Water and Steel quantities. Existing saves are not rewritten; other unfinished district experiments remain in the mission cache.
- Acceptance fixtures no longer exceed the default Windows thread stack. GNU Release retains warnings-as-errors while constructing local push payloads inside submission lambdas. Finite-oxygen fixtures use exact rare-event inputs instead of probabilistic searches.
- Exact schema-2 loads no longer run the retired single-scene normalizer. The uniquely recognizable v2.5.27 phantom body and stale Bee remnants are repaired in memory with a log entry; ambiguous saves and all Empty/Atmosphere opening payloads are preserved.
- Schema-1 migration reconstructs the correct district origins, live timers, and all 60 Bee owners. Retired random circular-nest growth is rejected while current queen migration and replacement lifecycle paths remain available.

The accepted hive/crescent artwork and high-sky Nuke presentation are unchanged.

Windows presentation now uses a private high-resolution one-shot timer, avoiding dependence on other applications' ordinary-sleep timer settings. Fixed simulation ticks and presentation caps are unchanged; no global timer setting or busy wait is used.

## Verification and remaining work

The fresh source-stage 163-check GPU report includes the original five-edge 96-tick newborn-route regression, simultaneous two-breath and scarce-donor controls, real paint/conveyor/Habitat input checks, laser transfers, exact saves and delayed authored hives. Windows tests include all eleven supported upstream EpochGui suites. GNU compatibility builds do not claim unsupported module-suite execution.

Frozen v2.5.28 at `805039d1fa4b9a03b94a89225e48f316dc6821a5` remains historical local evidence with its exact manifests, tag and bytes. Its 44/34 CPU and 84/84 GPU results, singleton placement policy and legacy source filename must not be presented as current acceptance or overwritten. Older public releases and rollback history remain protected.

Historical v2.5.28 finite/save and presentation measurements remain in the mission cache and frozen release manifest, not evidence for this candidate. Failed and interrupted runs are retained alongside successful reports. Current installed-package gates are pending; no acceptance threshold is relaxed.

The mission cache retains 123 active missions (70 PARTIAL, 36 REGRESSION, 16 OPEN, one DEFERRED). Broad bee aging/migration, salt/brine/soil phase ownership, unfinished experiments, long visual recurrence and architectural/performance acceptance are not declared complete by this release. The latest user publication request supersedes waiting for a separate personal eye-test reply without asserting personal visual acceptance or waiving native package gates.

## Dependency

The complete 53-file canonical EpochGui snapshot remains v0.89.30 at b97167423373b9a7af3f821dcf91d8a71613dbf2, freshly confirmed against the Site mirror on 2026-09-06. Its CMake/GNU compatibility boundary remains documented in third_party/EpochGui/SNAPSHOT.md.

## Final release naming (packages pending)

- `SandHybrid-Windows-x64-v2.5.29.zip` — Windows demo and bundled library install.
- `SandHybrid-Linux-x64-v2.5.29.tar.gz` — Linux demo and bundled library install.
- `EpochSimEngine-v2.5.29-source.tar.gz` — platform-neutral committed project/library source, not Linux-only.
- `EpochSimEngine-v2.5.29-source.zip` — the same platform-neutral committed source in Windows-friendly ZIP form.

Final archives require lowercase SHA-256 records with two spaces, the exact filename and one final LF/no CR. Source archives use exact committed-tree Git archive semantics, excluding generated/build/cache/untracked material. The older frozen `SandHybrid-v2.5.28-source.tar.gz` remains a legacy artifact and is not renamed or overwritten. Existing clone/download/updater URLs retain compatibility; current aliases update only after matching immutable objects and mirror verify. Publication is a normal visible EpochSimEngine release with SandHybrid demo downloads; GitHub is out of scope. The Site task owns hosted cleanup and preserves supported aliases, history and rollback records.
