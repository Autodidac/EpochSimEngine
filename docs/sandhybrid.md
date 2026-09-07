# SandHybrid demo for EpochSimEngine

SandHybrid is the bundled Windows/Linux Vulkan demo for EpochSimEngine. The project and simulation library are named EpochSimEngine; SandHybrid names only this demo application. It presents one persistent connected World containing eight west-to-east districts: Sandbox, Ecosystem, Engineering, Frontier, Volcano, Waterworks, Gold Mine, and Demolition.

## v2.5.29 availability and evidence

The September 6 release authorization covers the Windows demo and committed-source archives only. The existing public Linux download remains v2.5.27. Linux v2.5.29 is HELD: its Release CTest passes 79/79, but its final Compact runtime was intentionally stopped without a completed report, and subsequent finite/presentation gates did not start. Linux commands below describe source/development testing, not a certified v2.5.29 Linux download.

Tested native source `716722969d059e8c76791107b3ff9b0e267eb372` passes Windows CTest 90/90, installed Large state 164/164, 12/12 material/save cycles, and idle presentation of 1,920 frames/ticks in 34.5001 seconds. The paired REGION draw p95 increment is 1.2361 ms, not an attributed speedup. All 123 active missions remain; personal visual approval, longer ecology/hive recurrence, Editor 720p clipping, crowded Designer text, legacy KEYMAP entries and broader material/performance work are not declared complete.

## World size and Vulkan device limits

Choose an explicit world preset that fits the selected Vulkan device. Each canonical cell buffer and the full MAP cell snapshot use a storage-buffer descriptor covering the entire resident cell field, at 16 bytes per cell:

| Preset | Resident cells | Bytes per full cell descriptor | MiB |
|---|---|---:|---:|
| Compact | 5120 x 1440 | 117,964,800 | 112.5 |
| Standard | 7680 x 1440 | 176,947,200 | 168.75 |
| Large | 10240 x 1440 | 235,929,600 | 225 |

These are per-descriptor sizes, not total GPU memory consumption. Both cell buffers, MAP, lighting, hierarchy, staging, and driver allocations require additional memory. Every full cell descriptor must fit the device's advertised `maxStorageBufferRange`; free VRAM does not override that limit. An unsupported requested preset must be rejected before allocation/dispatch, never silently resized or admitted through a truncated descriptor. Saves retain their exact world dimensions.

The currently tested stock Mesa 23.2.1 llvmpipe/LLVM 15 device advertises a 128 MiB limit: Compact fits, while Standard and Large do not. From a local Linux development install root, use `./run-compact.sh` for the demo, or `./bin/sandhybrid --world-size compact` with the desired report arguments. Compact retains all eight contiguous districts, but does not cover Standard/Large inter-district gaps or Large's sparse startup-window requirement. Validate those on a device that supports the larger preset; Compact results are not Large parity. Linux v2.5.29 remains held without completed production acceptance; a supported allocation alone is not acceptance. These current-source device checks do not retroactively change the retained public v2.5.27 binary.

For every acceptance or profiling run, retain the selected device/driver, advertised `maxStorageBufferRange`, explicit preset, and resident dimensions with the report. Use the largest supported preset for the native release state gate, and keep preset/device conditions identical for comparisons.

## Simulation

The example runs canonical materials, liquids, gases, weather, ecology, machinery, actors, terrain, and combat on fixed 60 Hz simulation ticks. Presentation caps do not change simulation cadence. Complete aligned `8x8` liquid and gas packets use macro transactions while exposed or incomplete boundaries use fine cells; the same canonical cells remain authoritative.

The required model is a finite closed material system except for explicit user edits and declared boundaries. Reactions, inventory, weather, machines, phase changes, and actor tools must use balanced source/sink transactions. This is a design contract, not a claim that every experimental reaction already passes it; remaining contradictions are tracked in the mission cache.

## Controls and workspaces

- Inventory, Editor, Settings, and Designer are sidebar workspaces.
- Editor owns direct world mutation. Inventory owns player mining/deposit when no Blueprint is active.
- `H` and Camera Home restore the current camera; Reset preserves camera and MAP state.
- Paused mode freezes simulation, actors, clocks, MAP refresh, and effects while authorized editing remains live.
- The player laser damages material and transfers a terminal fragment to a deterministic adjacent real-gas world cell. It never erases, collects inventory, or creates Vacuum; a blocked hit retains its exact source.
- `NUKE FROM SPACE` shows a bright staged flare in the high sky above the continuous Cloud deck, then applies one GPU edit to only that upper Atmosphere region. Breathable Atmosphere below the deck remains intact. The action remains live while paused and performs no synchronous world readback.

The in-app KEYMAP is a quick reference, but its remaining legacy entries are an open documentation/UI issue; use the current controls described above.

## Beehives

Select **BEEHIVE** in Editor and click once per colony. It is a live material tool, not a Blueprint slot: another click places another independent colony instead of replacing the last one. The complete photographed hive and bee footprint must fit inside the World without overlapping another live colony. Authored districts, the high sky, and inter-district gaps use the same placement path. A held mouse button does not spray colonies.

Each colony owns 60 unique bees in the accepted three outward-open crescents. Fifty-four stay in their resting formation; six stagger their foraging trips. When no Flower is available, those six make a bounded search and return without creating Pollen, Honey, or extra bees. A real food target uses the existing material transactions. Paused simulation freezes these trips, and ordinary ecology advances only in its scheduled active window. Independent colonies do not share a district-wide replacement cap.

The accepted photographed body is unchanged. Exact schema-2 saves retain each colony's home and slot owners; old authored-district home encodings remain compatible. Full ecological recurrence and long-duration visual acceptance remain tracked in the mission cache rather than implied by the placement fix.

The queen needs a clear right entrance and a finite exterior Atmosphere supply. The ventilation correction consumes real Oxygen and stores CO2; it does not make a sealed, flooded, overheated or aging queen immortal. Current installed Windows GPU tests pass exact ventilation, edge recovery and frozen-MAP regressions. The stopped Linux run also passed the five actual paused hive/MAP captures, but is not a completed runtime pass. Broader lifecycle and visual mission acceptance remains open.

## Reading Debug

Click **DEBUG** to cycle through REGION, WORLD TOTALS, and off. REGION adds sparse tile-edge markers; WORLD TOTALS and MAP do not add those markers. The material colors remain underneath them. The sidebar MARKER KEY shows the same shapes as the world, not a second color-only coding system.

| Marker label | Shape within an 8x8 tile | Meaning |
| --- | --- | --- |
| DAMAGED | Top and both side edges | Damage or collapse flag is set. |
| ACTIVE | Upper-left corner | Active tile without a higher-priority marker. |
| FINE ACTIVE | Three separated top-edge dots | Fine-cell work owns this tile. |
| BULK MOVED | Short lower-right side | A macro movement flag is set. |
| BULK READY | Upper-right corner | Bulk-ready or macro-movable ownership. Eligibility is not proof of a move. |
| BREAKUP | Short middle bars on both sides | An awake packet is marked for fine fallback. |
| SETTLED | Short central top bar | Settled-medium ownership. |
| ENCLOSED | Upper corners and short lower sides | Enclosed-medium ownership. |
| SLEEPING | Short lower-left side | Sleeping tile. |
| STABLE | No overlay | None of the displayed state markers applies. |

Flags can overlap. The displayed marker uses this priority: damaged, fine active, bulk moved, breakup, bulk ready, settled, enclosed, sleeping, active, stable. Consequently, marker totals need not match each separately counted flag. Markers do not draw a horizontal bottom edge across the material surface.

REGION statistics are a bounded snapshot of **one up-to-640x360 region**, not a census of the whole camera window or World. While REGION is visible and simulation ticks execute, sampling rotates through the 4x4 active window at a 120-tick cadence; values are retained between samples and while paused. WORLD TOTALS or off resets that rotation, and hidden Debug performs no collection. On smaller windows the layout reduces text size; `RESIZE FOR STATS` means some statistic rows do not fit. The tested standard layouts from 960x720 fit the complete key and statistics; exceptionally short windows can also clip the key, so enlarge the window to read the full panel.

- `SAMPLED CELLS/TILES/CHUNKS` describe the inspected region. Boundary chunks may be only partly inside it. `MATERIAL CELLS` counts non-Vacuum cells, not simulation work; `STRUCT`, `LIQUID`, and `GAS` are sampled material classifications.
- `ACTIVE WINDOWS` counts the active region grid, not busy tiles. Fine, bulk, settled, gas, and liquid tile counts are ownership flags and can overlap; do not sum them as separate material volumes. `DIRTY CHUNKS` means pending invalidation/reclassification, not corruption.
- `SLEEP CELLS` is the sampled cell capacity inside sleeping chunks, clipped to the sample. It is **not** the number of instructions or movement attempts skipped. `CELLS MOVED` counts cells carrying the moved/handled flag in that snapshot, not a measured displacement rate.
- `PAIR WORK`, `FINE SWAPS`, `BULK MOVES`, `BULK CELLS`, and `FINE REPAIR` currently show `N/A`: these production event counts are not instrumented. `N/A` does not mean zero activity.
- WORLD TOTALS contains FPS and resident grid dimensions/capacities. `CELL MEMORY MB` is an approximate MiB estimate for the two cell buffers and per-cell light prefix, not total process/GPU memory; hierarchy, column metadata, images, staging, and driver allocations are additional.

## World and weather

Every district shares one aligned grass surface. The high sky contains one continuous conserved Cloud deck. Steam joins Cloud one-for-one; mature Cloud returns staggered equal-temperature Water-family units as rain without waking the whole deck at once. Acid, corrosion, ecology, weather, and phase rules may change a material only through a declared source/sink transaction. Volcano, Waterworks, experiments, and terrain are examples of the same material APIs rather than separate scripted worlds.

## Current corrective boundary

Current local experiment repairs distinguish implementation from acceptance. Conveyors now admit real gas destinations through whole-cell swaps; Habitat input credit and consumption share one deterministic capacity decision. Dense Smoke and tiled life edits use serial column ownership rather than overlapping GPU writers. Half Water keeps liquid presentation until merge, so its saved displaced-medium heat cannot masquerade as gas density. These new production paths still require fresh GPU checks.

The Frontier reset layout opens its catchment inlet and keeps Steel stock clear of the enemy Factory Core without changing initial Water quantities or activating the enemy Core. Existing saves retain their authored cells; reset geometry changes do not silently rewrite them. Wider irrigation, hopper, volcanic-throat, salt, gas-compression and soil-moisture problems remain unfinished.

## Saves and runtime checks

World saves use checksummed schema 2 and retain every canonical cell plus the exact actor owner. Schema 1 cell-only saves remain readable without inventing actor state.

The packaged executable can run the focused production Vulkan gate:

```text
sandhybrid --world-size compact --runtime-acceptance-report runtime-acceptance.json
```

The separate repeated finite-ledger gate reuses the production chemistry, correction, movement, and schema-2 save/load paths for twelve deterministic cycles:

```text
sandhybrid --world-size compact --long-cycle-acceptance-report long-cycle-acceptance.json
```

Each cycle checks Half Water ownership and heat, Water-to-Steam-to-Cloud-to-rain material and temperature ownership, Lava-to-Stone-to-Lava material and heat ownership, exact actor serialization, zero Empty/Vacuum creation, and byte-identical repeatability. Its temporary save fixture is isolated beside the report and removed on both success and failure. Run it once per native installed package; it is intentionally separate from the fast state report.

It can also run a presented-frame acceptance pass from the installed `bin` directory:

```text
sandhybrid --world-size compact --interactive-acceptance-report interactive-acceptance.json
```

The interactive pass measures adjacent normal/REGION Debug presentation, then captures normal World, REGION Debug, WORLD TOTALS, MAP, Inventory, Designer, a close canonical Ecosystem hive, the brightest high-sky Nuke warning, and the resulting bounded high-sky edit. The sibling `<report-name>-frames` directory contains the real Vulkan swapchain BMPs. Hardware Vulkan enforces frame-time and Debug-overhead gates; CPU software Vulkan records the same pages and visual states without pretending its timing is an interactive hardware result.

Focused, repeated-cycle, and interactive checks cover different risks. None substitutes for explicit packaged visual acceptance or broader cross-district ecology and machinery observation.

## Material-tick profiling

The optional profiler diagnoses where the material simulation spends GPU time. From the installed `bin` directory on a Windows device supporting Large:

```powershell
.\sandhybrid.exe --world-size large --simulation-profile-report material-profile.json
```

On the current stock llvmpipe Linux device, select Compact explicitly:

```sh
./sandhybrid --world-size compact --simulation-profile-report=material-profile.json
```

Both argument forms accept a quoted path with spaces. Choose exactly one report mode per process: profiling cannot be combined with focused, long-cycle, or interactive acceptance. `--world-size compact`, `standard`, and `large` select the resident allocation; keep the same size and machine conditions when comparing reports.

Use a fresh output filename for every run. An existing report path is refused with exit code 2 before native/GPU startup, preserving the earlier file. Only consume a report after the process exits with code 0 and its JSON says `completed: true`. A failed process, cancelled run, or partial JSON is never acceptance evidence; the output-path preflight is not an atomic reservation against another process creating that path during the run.

The hidden-window run generates a fresh canonical World and freezes the normal Camera Home 4x4 active window. It performs no player updates, user edits, presentation, live MAP refresh, Debug collection, save/autoload, or world-save writes. It does not profile an existing saved world. Startup, reset, initial sunlight, and initial MAP snapshot are outside the measured intervals.

Hardware Vulkan uses 32 warmup ticks and 240 measured ticks. CPU software Vulkan uses only 2 warmup ticks and 8 measured ticks; its report is diagnostic, not hardware parity or a performance pass. Ticks execute serially as quickly as each submission completes, without presentation pacing; the production material schedule and step-dependent cadence are retained.

The JSON includes device and timestamp identity, resident and active dimensions, actual sample counts, and mean, nearest-rank p50/p95/p99, and maximum milliseconds for the whole material tick and eighteen stages. Eight of these are immutable-source chemistry owners: residual bulk, Bees, industrial inputs, compost donors, machine controllers, harvest donors, Empty/Atmosphere destinations, and phase/gas/fire materials. Stage intervals include their barriers and timestamp overhead. Periodic stages include ticks with no scheduled work; global tracked-rainfall/chunk metadata and the movement snapshot's 16-cell halo keep their production scopes. Bottom-of-pipe timestamps can perturb overlap, and CPU recording, fence waiting, and query readback are excluded from GPU intervals. A cancelled run reports incomplete sample counts; unsupported or ambiguous timestamp clocks fail instead of fabricating measurements.

Startup logs distinguish reset from the first material tick. Software Vulkan also logs when queue submission returns, separating a driver call from the subsequent fence wait. A hardware first-tick fence timeout may log completed GPU marker boundaries; software timeout paths do not call back into query retrieval, because even a non-WAIT query can be serialized behind driver work. A marker identifies completed work, not the cause of an unfinished stage. Later-tick timeouts do not reuse potentially stale marker availability. A stalled run produces no successful timing report. The fence timeout does not bound time spent inside other driver calls, so automated software-driver runs should additionally use an external process deadline. The normal runtime does not collect these markers.

These numbers are **not FPS or interactive frame times**. They do not measure player, rendering, UI, MAP, or Debug overhead, and do not close long-cycle, visual, or performance acceptance missions. Use the separate interactive report for presented-frame behavior.

### Serial stage tracing

For a stalled material tick, opt in to a more intrusive diagnostic using the same report path option:

```powershell
.\sandhybrid.exe --world-size compact --simulation-profile-report material-stage-trace.json --simulation-profile-stage-trace
```

On Linux use `./sandhybrid` with the same arguments. `--simulation-profile-stage-trace` requires `--simulation-profile-report`; it cannot run alone or alongside any acceptance-report mode. The existing fresh-filename refusal and exit-code-0 plus `completed: true` requirements still apply.

Stage tracing starts one fresh material tick with zero warmup ticks and one sample. It submits the existing eighteen production stages separately, in their original order and scope, using a separate bounded submission for each stage. Chemistry's eight owners still read the same immutable source and write disjoint scratch cells before the single correction/copyback. The output is labeled `serial-stage-diagnostic-not-performance`. These forced submission boundaries change scheduling and synchronization: results are not comparable to normal material profiling, interactive timing, or FPS, and cannot establish a performance improvement or acceptance pass. This option is off by default and retains the profiler's hidden window and exclusions for rendering, actors, input, save/autoload, and live MAP/Debug work.

"Cold" in its log means a freshly reset simulation with no warmup, not an emptied driver shader cache; the diagnostic never clears that cache. Cancellation observed after a completed stage prevents the next stage submission and produces no partial-tick report. A timed-out stage may still be compiling or executing inside the driver: its name narrows the investigation but does not identify the internal cause.
