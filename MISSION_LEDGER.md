# SandHybrid Mission Ledger

`missioncache.md` is the authoritative mission registry. This ledger records the current release handoff and the non-negotiable publication gate; it never replaces or closes active mission acceptance criteria.

## Current release target

- Target: `v2.5.15` as a normal public GitHub release after recovery validation.
- State: v2.5.14 was published normally, but packaged user observation contradicted its macro-tile and Half Water behavior. v2.5.15 native Windows and Linux Release builds now pass 24/24 tests and fresh packages are staged; packaged Windows observation accepts sidebar presentation and corrected logical cursor ownership, while macro-tile, Half Water, hive-content, pause-editing, and full scene-cycle runtime acceptance remain active.
- Public prerelease tags and `-test` release names are forbidden.

## v2.5.14 publication record

- Release: https://github.com/Autodidac/EpochSimEngine/releases/tag/v2.5.14
- Commit: `71d6098ce6ec99bc89475af6859382a2b31ce9a4`
- Windows SHA-256: `cc80807b3afd1560c973b17753aea0792450026f4b8f169e99798a86d0329d5c`
- Linux SHA-256: `cf8ec6c96480a974230b77e81242e067cf2fbaa4fa21f01db4cb0aae86092e81`
- Acceptance result: stable publication passed; macro-tile and Half Water packaged runtime acceptance failed and the affected missions were reopened.

## Recovery baselines

- Macro tiles: v2.5.3 moving full-liquid/full-gas eligibility, exact displacement into full uniform compatible targets even when their scheduling metadata is fine-active, intact-packet exclusion from non-fallback fine passes, and same-tick fine fallback when a packet cannot commit.
- Half Water: v2.4.1 fine-owned conserved fall, merge, dedicated passes, and bounded two-to-four-cell attraction. A half cannot sleep with fall/merge/attraction work pending and cannot wander through generic diagonal Water flow.
- Beehive: exact hard-coded pre-PR19 Ecosystem model restored by SimpleSandSim Fix36: queen centers (512,234)/(512,232), loose Wood perch x=-37..29 and y=-16..-13, shell 25 <= radius² < 92, chamber radius² < 25, queen center, exit x=1..10 with |y|<=1, and deterministic Empty/Honey/Pollen chamber contents. SandHybrid actor/ecology behavior remains SandHybrid-owned.
- Pause: simulation and presentation clocks freeze while direct editor mutations remain live and do not advance the simulation.
- Inventory: sidebar-only with `INVENTORY` and `BLUEPRINTS` subtabs.
- Designer: sidebar-only authoring grid with INVENTORY and BLUEPRINTS subtabs; the main viewport continues to show the world. Editor and Designer cursor controls are isolated, and the visible world cursor shares the committed edit mapping/radius/shape without a sidebar-edge ghost.

## Publication gate

Before publication, the exact release source must pass Windows and Linux C++23 Release builds, all shaders and deterministic tests, install/package/archive steps, and SHA-256 generation. Runtime and visual missions remain non-COMPLETE until the packaged executable is observed. Publication evidence must include the stable release URL, tag, commit, asset names, checksums, and packaged runtime observations.