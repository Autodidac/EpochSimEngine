# SandHybrid Validation Matrix

The project separates three validation levels:

- **Contract:** deterministic C++23 tests for IDs, phase thresholds, terrain stability policy, local water conservation, UI hit testing, and source-independent canonical state.
- **Static shader/interface:** generated-file reproducibility, include resolution, delimiter checks, reserved identifiers, material/card mappings, required rule tokens, and exact C++/GLSL push-constant layouts.
- **Windows Vulkan runtime:** actual MSVC compilation, `glslc` compilation, Vulkan execution, visual behavior, conservation logging, and GPU-load observation. Run `validate_windows.bat Release`, then execute the listed runtime World checks when a fresh driver shader-cache warmup is acceptable.

| # | Requirement | Automated coverage | Runtime check |
|---:|---|---|---|
| 1 | Copper melts in sufficiently hot lava | `phase_at(copper, 1300) == molten`; generated copper threshold | Place copper beside a hot/pressurized vent or hotter lava source and inspect with `Alt`. |
| 2 | Steel survives lava below melting point | Steel at 1300 C is softened, not molten or vapor | Place steel in ordinary lava; confirm mass remains and card phase is solid/softened. |
| 3 | Lower-melting metals melt before steel | Gold/copper thresholds asserted below steel | Heat gold, copper, and steel together. |
| 4 | Plastic softens, melts, burns, or decomposes | Plastic threshold ordering and conversion text asserted; chemistry rules statically required | Heat plastic gradually, then expose it to fire. |
| 5 | Plastic reacts with lava and produces configured byproducts | Canonical chemistry includes plastic ignition/decomposition outputs | Drop both plastic types into lava and inspect products/counters. |
| 6 | Blocked thermal vent builds toward eruption | `update_vent_pressure` and eruption threshold contracts | Seal the Volcano-district vent and watch pressure/gas/magma escalation. |
| 7 | Open vent releases pressure without automatic major eruption | Open-pressure decay contract | Open the vent path and confirm pressure falls through gas/lava release. |
| 8 | Water fills/equalizes a basin quickly without volume loss | Bounded local equalization test preserves 64/64 units | Use the Waterworks district, alter a basin, and compare conservation counters. |
| 9 | `Alt` shows the exact material under cursor | Direct cursor-to-cell render path and input suppression statically checked | Hold `Alt` and move across cell boundaries, gases, liquids, and damaged terrain. |
| 10 | Dense settled cells qualify for stability without reconstruction | 52/64 occupancy and 120-tick stability contracts | Fill one 8x8 region above threshold; verify existing cells stop falling and empty positions stay empty. |
| 11 | Incomplete regions remain loose | 51/64 stability rejection asserted | Leave a region below 52 cells and confirm it remains simulated. |
| 12 | Stability/break cycles conserve mass | Representation conservation test | Repeatedly break and settle terrain while watching counters. |
| 13 | Pre-placed metal survives partial destruction | Creation paths canonical; structural damage releases same material | Damage pre-placed metal without heating it past vaporization. |
| 14 | Cursor-painted metal survives after losing more than half | Creation paths canonical; no provenance destruction | Paint a metal block, remove over half, and inspect all remaining fragments. |
| 15 | Stabilized metal matches other placement paths | Seven creation paths resolve to identical canonical state | Compare card phase/thresholds for map, painted, and broken metal. |
| 16 | Damaged terrain collapses without disappearing | 32-cell threshold and same-pass release are statically checked | Shoot a hanging block until 31 pixels remain; verify the remainder drops together. |
| 17 | Stable regions sleep and reduce GPU load | Tile sleeping flags and chemistry/movement early-outs required by validator | Enable `F3`, inspect restrained square sleeping markers, compare GPU load against active water/fire. |
| 18 | Stability does not oscillate | Restabilization cooldown exceeds qualification time | Repeatedly disturb a candidate region and confirm cooldown prevents flicker. |
| 19 | Normal rendering hides raw square grid | Grid rendering is required to remain inside debug branch | Run with `F3` off. |
| 20 | Debug reveals structure/simulation state without filled oblong tint | Square tile-edge/marker glyphs and bounded telemetry cadence are contract checked | Toggle `F3`, inspect truthful states, and benchmark enabled overhead below 3%. |
| 21 | CO2 is visually distinct | Catalog contracts require near-black translucent charcoal CO2 | Compare CO2 against smoke, darkness, stone, and water. |
| 22 | UI is aligned, responsive, unobtrusive | Wide/compact EpochGui hit-box contracts | Resize through compact and wide layouts; verify no overlaps. |
| 23 | Colors remain distinct during reactions | One generated palette/card catalog | Inspect common water/fire/smoke/steam/CO2 and acid/material combinations. |
| 24 | Gas rendering supports future shader presentation | Static validator requires `gasPresentation` boundary | Confirm current gas opacity does not obscure terrain; later shader work stays isolated. |

## Conservation runtime procedure

1. Start the persistent World and travel to a clear district area.
2. Press `F3` to enable periodic conservation diagnostics.
3. Create a closed experiment away from map boundaries.
4. Run phase changes, reactions, structural breakup, and stability qualification.
5. Treat `stabilized` and `broken` as represented state transfers, not mass loss.
6. Investigate any non-zero conservation-error counter. Boundary-lost counters are reserved for explicit transient or map-boundary exits.

## Windows command

```bat
validate_windows.bat Release
```

This command builds the real application and all GLSL shaders, runs the static shader/interface validator, rebuilds the two C++23 contracts with warnings-as-errors, and runs CTest. Runtime visual and GPU checks still require launching the produced executable because they depend on the installed Vulkan driver and GPU.

## Core regression checks

- A complete mouse down/up pair received within one native poll still produces exactly one `primary_pressed` or `secondary_pressed` edge.
- Character primary action drills ordinary terrain even while plasma ammunition is carried. Plasma is consumed only when the first ray hit is a hostile target.
- Every stable terrain pixel requires two ordinary laser hits: 255 integrity with 144 damage per hit.
- At 32 remaining pixels the region stays coherent; at 31 remaining pixels all survivors release in the same simulation pass.
- Ambient empty cells restore oxygen and never cause passive health loss. Health damage requires prolonged zero-oxygen exposure inside a concentrated toxic pocket.
- Authored terrain remains stable, while deliberate sand/silt/cargo samples remain loose and simulated.
- CO2 renders near-black, hydrogen renders pink, and the enlarged UI hit rectangles match the fragment-shader controls.

- With `F3` counters visible, complete moving Water and gas tiles attempt exact packet movement every two fixed ticks; an exposed blocked packet receives eight due opportunities before a fresh perimeter check permits fine fallback, while enclosed Air remains tiled.
- In the Ecosystem district, compare the suspended hive against the supplied irregular historical photograph. The current formula/perch contract must not be reported as that photograph match; reset, placement, load normalization, queen/home metadata, and bee cycles must agree only after the actual historical payload is recovered.
- While `PAUSED`, paint, erase, fill, and Ignite Air, confirm each edit appears immediately while both RUNNING and PAUSED, and confirm clocks, actors, reactions, lighting, MAP refresh, and effects do not advance.
- Verify Inventory and Designer remain inside the sidebar at wide and compact sizes; each exposes `INVENTORY` and `BLUEPRINTS`, and Designer never replaces the world viewport.

## Packaged Vulkan state-readback command

Run this from a fresh native package on a system with a working Vulkan presentation device:

```text
sandhybrid --world-size large --runtime-acceptance-report runtime-acceptance.json
```

The executable allocates the selected resident World (Large is 10240x1440), runs the production reset, actor, paint, tile, macro-movement, fine-movement, and chemistry pipelines, writes a schema-1 JSON report, and exits 0 only when every focused check passes. Seeded movement dispatch remains bounded to the first 192 columns and rows because those micro-scenarios live there, while reset, district, player, startup-footprint, and hard-coded hive checks cover the complete selected resident buffers. Unrelated long-running chemistry, effects, sunlight, weather, and cross-district travel are intentionally omitted; deterministic contracts separately enforce their source policies. Exit 3 means observed GPU state contradicted the accepted behavior. The report covers:

- Editor paint commits exactly once while RUNNING and while PAUSED;
- exact one-packet 8x8 Water and Hydrogen displacement;
- blocked Water retains macro ownership for seven due opportunities and enters fine fallback on the eighth only after an open-boundary check;
- enclosed Air remains tiled after the same classifier budget;
- Half Water fall, clear-gap two-to-four-cell attraction, merge, supplied-ledge split/hang/drip, and full-Water zero-jitter equilibrium;
- isolated full Water crossing an unsupported ledge;
- complete supported structural Stone foundations and common aligned grass Y `1040` in all eight distributed World districts;
- live persistent player state at the distributed Frontier recovery spawn with full health and Oxygen;
- no more than three authored districts intersecting the initial Large 4x4 active window;
- cell-exact photographed Fix29 support, shell, queen, exit, and Empty/Honey/Pollen payload in placed, Sandbox, and Ecosystem hives; the placed body is checked immediately and again after 120 focused ticks, while each hard-coded colony retains 100 unique district-correct bee homes.

This focused gate does not close broader cross-district traversal, weather/ecology cycles, machinery, save migration, full bee lifecycle, debug overhead, or final user visual review of the photographed hive and distributed scenery.

## v2.5.25 stable local release gates

The post-v2.5.25 camera-preservation follow-up adds three source/runtime gates without changing the immutable v2.5.25 archives:

The packet/performance follow-up adds successful-distance and bounded-dispatch gates without claiming unmeasured FPS:

- `sandhybrid_behavior_contract` proves eight-step travel saturation, independent failed-attempt accumulation, success reset, and perimeter-conditional breakup.
- `sandhybrid_section_scheduler_contract` proves clipped 4x4 dispatch dimensions and a fourfold Large cell-domain reduction.
- Packaged Windows Vulkan acceptance proves `macro_bubble_eight_step_breakup` with Hydrogen `64`, Water `512`, progress `7` retained and progress `8` broken to fine; `macro_blocked_fine_fallback` reports exactly eight blocked attempts.
- Serial native verification passes Windows Release `35/35` CTests and WSL Linux Release `32/32`. Interactive timestamp/FPS evidence remains required for performance mission closure.

- `sandhybrid_input_routing_contract` proves `request_world_reset` raises only the reset epoch request and preserves simulation-camera and MAP center/zoom state.
- `sandhybrid_ui_layout_contract` proves the new sidebar Camera Home hit region remains inside the sidebar and does not overlap camera-mode, MAP, or DEBUG controls.
- `tools/validate_v2525_contract.py` requires Win32 and XCB `H` routing, the shader-visible `H CAM HOME` label, the five-button VIEW/INPUT row, active-view home dispatch, and camera-preserving reset routing.

- `tools/validate_v2513_contract.py` preserves the first-28 P0 recovery contracts.
- `tools/validate_v2515_contract.py` retains the macro, Half Water, hive, cursor, Fill, paused-editing, and sidebar recovery baselines.
- `tools/validate_v2516_contract.py` retains real Blueprint slots, exact transactional placement, paused persistent-World editing, and honest sidebar rendering.
- `tools/validate_v2517_contract.py` requires reserved Half Water state, one canonical Fix29 prefab entropy, the packaged Vulkan readback harness, high-DPI logical rendering, and a normal stable publication path.
- `tools/validate_v2519_contract.py` requires district-specific full-tile surfaces and authored-air ownership, Designer Clear, bounded Fill writes, supported breathable player recovery, two-tick macro cadence/cohesion, and one-submit fixed-step debt shedding.
- `tools/validate_v2521_contract.py` requires the photographed compact hive across CPU/GPU/load contracts, persistent-World player policy, Volcano/geology recovery, Actions and presentation-limit layout, fixed simulation pacing, static normal presentation, state-driven gas mixing, bounded debug sampling, conservative Sluice output, and the stable v2.5.25 publication path.
- `tools/validate_v2522_contract.py` requires exact scene-local Fix29 hive payloads, single-spend liquid movement, conserved Half Water behavior, zero-jitter settled surfaces, and result-based packaged Vulkan evidence.
- `tools/validate_v2525_contract.py` requires aligned distributed district origins, common grass Y, sparse Large startup, durable Fix29 contents across every constructor/runtime path, delayed exact hive readback, and stable v2.5.25 package names.
- `tools/validate_release_tree.py` rejects tracked packages, executables, compiled shaders, payload chunks, one-shot workflows, and versioned release-note fragments.
- Windows and Linux full Release builds compile every shader, build with warnings as errors, run all CTests, install the package, archive it, audit its contents, and generate SHA-256 files.
- Before any public publication, both fresh native packages must execute the Vulkan state-readback command successfully; this pass is local-only. Windows high-DPI capture must also show a paused committed edit beneath its cursor and no world ghost in the sidebar.
- Runtime and visual acceptance stays active in `missioncache.md`; deterministic contracts and focused readback are evidence, not substitutes for every remaining mission scenario.
