# SandHybrid Mission Ledger

`missioncache.md` is the authoritative mission registry. This ledger records the current release handoff and the non-negotiable publication gate; it never replaces or closes active mission acceptance criteria.

## Current release target

- Target: `v2.5.14` as a normal public GitHub release.
- State: Windows/Linux Release builds, 24/24 tests, install, archives, checksums, and content audits passed; stable publication and packaged visual observation remain pending.
- Public prerelease tags and `-test` release names are forbidden.

## Recovery baselines

- Macro tiles: v2.5.3 moving full-liquid/full-gas eligibility, with same-tick fine fallback when a macro transaction cannot commit.
- Beehive: SimpleSandSim Fix29 shell, chamber, exit, and deterministic chamber contents only; SandHybrid actor/ecology behavior remains owned by SandHybrid.
- Pause: simulation and presentation clocks freeze while direct editor mutations remain live and do not advance the simulation.
- Inventory: sidebar-only with `INVENTORY` and `BLUEPRINTS` subtabs.
- Designer: sidebar-only authoring grid with `INVENTORY` and `BLUEPRINTS` subtabs; the main viewport continues to show the world.

## Publication gate

Before publication, the exact release source must pass Windows and Linux C++23 Release builds, all shaders and deterministic tests, install/package/archive steps, and SHA-256 generation. Runtime and visual missions remain non-COMPLETE until the packaged executable is observed. Publication evidence must include the stable release URL, tag, commit, asset names, checksums, and packaged runtime observations.
