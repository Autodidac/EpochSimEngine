# SandHybrid v2.5.14-test.1

Phase 2 testing prerelease for atmosphere transport and actor-owned ecology contracts.

## Included

- Exact packed-atmosphere inspection for N2, O2, Ar, CO2, Ne, H2, He, vapor, and contaminants.
- Conserved pressure equalization, enrichment, excess-gas reabsorption, respiration, and combustion.
- Deterministic gas density and buoyancy routing with sealed, liquid, paused, and unloaded boundary blocking.
- Tangential gas motion along solid walls without artificial wall friction.
- Connected balanced-Air fill, upper-left connected-Air ignition, corner-pressure validation, and zero-pressure Half Water ambient bookkeeping.
- Actor-owned player, bee, queen, ant, and beetle records outside material identity.
- Bee forage, pollen return and deposit, honey feeding, migration, hazard response, readable recurring biohazard formation, and a hard 100-bee cap.
- Ant forage, pheromone, home, hazard, flood, and permitted-dig intent.
- Beetle surface crawl, obstacle turns, shelter seeking, light avoidance, and hazard response.
- Explicit ant and beetle habitat birth transactions with capacity, food, water, waste, and cadence.
- Canonical Fix28 suspended-hive classification and scene-origin home mapping.
- Expanded life, respiration, drowning, suffocation, birth, death, hive, pollen, honey, displacement, and species counters.
- Public core API version 4 capability flags.

## Validation

- Windows and Linux headless library suites.
- Windows and Linux full Release/Vulkan package matrix.
- GCC 14.2 full repository build: 20/20 tests passed locally.
- Direct GCC and Clang 17 C++23 contracts passed with warnings as errors.
- Core hygiene, branding, generated/shader, persistence, terrain/liquid, mission-cache, and downstream package contracts.

## Testing focus

Observe gas settling and wall motion, Air fill and ignition, Half Water ambient behavior, actor-medium overlap, bee lifecycle/cap/formation, ant and beetle navigation intent, habitat transactions, and Fix28 hive placement.

Visual, Vulkan-integration, navigation, save/load, and multi-cycle ecology missions remain active until runtime observation passes. This prerelease does not claim that the earlier broken Phase 1 transport was merged.
