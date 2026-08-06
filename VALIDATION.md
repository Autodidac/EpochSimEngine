# SandHybrid Validation Matrix

The project separates three validation levels:

- **Contract:** deterministic C++23 tests for IDs, phase thresholds, terrain stability policy, local water conservation, UI hit testing, and source-independent canonical state.
- **Static shader/interface:** generated-file reproducibility, include resolution, delimiter checks, reserved identifiers, material/card mappings, required rule tokens, and exact C++/GLSL push-constant layouts.
- **Windows Vulkan runtime:** actual MSVC compilation, `glslc` compilation, Vulkan execution, visual behavior, conservation logging, and GPU-load observation. Run `validate_windows.bat Release`, then execute the listed runtime scene checks.

| # | Requirement | Automated coverage | Runtime check |
|---:|---|---|---|
| 1 | Copper melts in sufficiently hot lava | `phase_at(copper, 1300) == molten`; generated copper threshold | Place copper beside a hot/pressurized vent or hotter lava source and inspect with `Alt`. |
| 2 | Steel survives lava below melting point | Steel at 1300 C is softened, not molten or vapor | Place steel in ordinary lava; confirm mass remains and card phase is solid/softened. |
| 3 | Lower-melting metals melt before steel | Gold/copper thresholds asserted below steel | Heat gold, copper, and steel together. |
| 4 | Plastic softens, melts, burns, or decomposes | Plastic threshold ordering and conversion text asserted; chemistry rules statically required | Heat plastic gradually, then expose it to fire. |
| 5 | Plastic reacts with lava and produces configured byproducts | Canonical chemistry includes plastic ignition/decomposition outputs | Drop both plastic types into lava and inspect products/counters. |
| 6 | Blocked thermal vent builds toward eruption | `update_vent_pressure` and eruption threshold contracts | Seal the Volcano vent and watch pressure/gas/magma escalation. |
| 7 | Open vent releases pressure without automatic major eruption | Open-pressure decay contract | Open the vent path and confirm pressure falls through gas/lava release. |
| 8 | Water fills/equalizes a basin quickly without volume loss | Bounded local equalization test preserves 64/64 units | Use Waterworks/Blank, alter a basin, and compare conservation counters. |
| 9 | `Alt` shows the exact material under cursor | Direct cursor-to-cell render path and input suppression statically checked | Hold `Alt` and move across cell boundaries, gases, liquids, and damaged terrain. |
| 10 | Dense settled cells qualify for stability without reconstruction | 52/64 occupancy and 120-tick stability contracts | Fill one 8x8 region above threshold; verify existing cells stop falling and empty positions stay empty. |
| 11 | Incomplete regions remain loose | 51/64 stability rejection asserted | Leave a region below 52 cells and confirm it remains simulated. |
| 12 | Stability/break cycles conserve mass | Representation conservation test | Repeatedly break and settle terrain while watching counters. |
| 13 | Pre-placed metal survives partial destruction | Creation paths canonical; structural damage releases same material | Damage pre-placed metal without heating it past vaporization. |
| 14 | Cursor-painted metal survives after losing more than half | Creation paths canonical; no provenance destruction | Paint a metal block, remove over half, and inspect all remaining fragments. |
| 15 | Stabilized metal matches other placement paths | Seven creation paths resolve to identical canonical state | Compare card phase/thresholds for map, painted, and broken metal. |
| 16 | Damaged terrain collapses without disappearing | 32-cell threshold and same-pass release are statically checked | Shoot a hanging block until 31 pixels remain; verify the remainder drops together. |
| 17 | Stable regions sleep and reduce GPU load | Tile sleeping flags and chemistry/movement early-outs required by validator | Enable `F3`, wait for green sleeping regions, compare GPU load against active water/fire. |
| 18 | Stability does not oscillate | Restabilization cooldown exceeds qualification time | Repeatedly disturb a candidate region and confirm cooldown prevents flicker. |
| 19 | Normal rendering hides raw square grid | Grid rendering is required to remain inside debug branch | Run with `F3` off. |
| 20 | Debug reveals structure/simulation state | Tile boundary, candidate, stable, sleep, active, damage tokens statically required | Toggle `F3` and inspect each overlay state. |
| 21 | CO2 is visually distinct | Static validator requires the violet CO2 presentation | Compare CO2 against smoke, darkness, stone, and water. |
| 22 | UI is aligned, responsive, unobtrusive | Wide/compact EpochGui hit-box contracts | Resize through compact and wide layouts; verify no overlaps. |
| 23 | Colors remain distinct during reactions | One generated palette/card catalog | Inspect common water/fire/smoke/steam/CO2 and acid/material combinations. |
| 24 | Gas rendering supports future shader presentation | Static validator requires `gasPresentation` boundary | Confirm current gas opacity does not obscure terrain; later shader work stays isolated. |

## Conservation runtime procedure

1. Start Sandbox or Blank.
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

- With `F3` counters visible, complete moving Water and gas tiles increment macro attempts/moves as in v2.5.3; blocked packets continue visibly through fine fallback instead of freezing.
- In Ecosystem, compare the suspended hive body, chamber, exit, queen center, and Honey/Pollen/Empty contents against the Fix36 pre-PR19 Ecosystem reference, including the wide loose-Wood perch and exact queen centers without changing SandHybrid bee behavior.
- While `PAUSED`, paint, erase, fill, and Ignite Air, confirm the edit appears immediately, and confirm clocks, actors, reactions, lighting, MAP refresh, and effects do not advance.
- Verify Inventory and Designer remain inside the sidebar at wide and compact sizes; each exposes `INVENTORY` and `BLUEPRINTS`, and Designer never replaces the world viewport.

## v2.5.16 stable release gates

- `tools/validate_v2513_contract.py` preserves the first-28 P0 recovery contracts without hard-coding an obsolete release tag.
- `tools/validate_v2515_contract.py` retains the macro, Half Water, hive, cursor, Fill, paused-editing, and sidebar recovery baselines.
- `tools/validate_v2516_contract.py` requires real Blueprint slots, exact transactional placement, paused character-scene editing, honest sidebar rendering, and a normal stable publication path.
- `tools/validate_release_tree.py` rejects tracked packages, executables, compiled shaders, payload chunks, one-shot workflows, and versioned release-note fragments.
- Windows and Linux full Release CI compile every shader, build with warnings as errors, run CTest, install the package, archive it, generate SHA-256 files, and publish only from the stable `v2.5.16` tag.
- Local release-candidate evidence: native Windows and Linux Release builds each passed 26/26 tests; fresh package audits found the executable, 12 SPIR-V shaders, installed Blueprint header, library/development files, launchers, and canonical docs. Packaged Windows automation completed a transactional Blueprint paste while PAUSED and exited cleanly.
- Runtime and visual acceptance stays active in `missioncache.md`; deterministic contracts are evidence, not a substitute for eye testing.
