# SandHybrid v2.5.14-test.2

Testing prerelease combining the validated Phase 1 runtime/UI recovery with Phase 2 atmosphere and ecology contracts.

## Runtime and UI recovery

- Half Water restores canonical balanced Air, attracts only visible partners two to four cells away, remains fine-owned, and sleeps only after bounded rest conditions.
- Complete Water/gas regions attempt macro movement first and immediately retain fine fallback when the packet cannot commit.
- Pause freezes simulation clocks, MAP refresh, lighting/day-night, particles, actors, tools, and presentation effects; reset returns state and queued input to step zero.
- MAP remains independently navigable, Designer owns a separate 64x32 authoring grid, and Editor retains world-placement controls.
- Authored terrain, lighting, scene origin, hive placement, and workspace hit boxes are revalidated by deterministic contracts.

## Atmosphere and ecology

- Packed N2/O2/Ar/CO2/Ne/H2/He/vapor/contaminant inspection and conserved pressure transfer.
- Density/buoyancy routing, sealed/paused/unloaded boundaries, wall-tangential gas motion, reabsorption, respiration, and combustion policies.
- Connected balanced-Air fill, upper-left connected-Air ignition, corner-pressure contracts, and zero-pressure Half Water ambient bookkeeping.
- Actor-owned player, bee, queen, ant, and beetle records outside material identity.
- Bee forage/return/deposit/feed/migration/hazard policy with a hard 100-bee cap and recurring readable biohazard formation.
- Ant and beetle behavior intent, explicit habitat transactions, Fix28 hive classification/home mapping, and expanded life telemetry.

## Repository cleanup

- Removed obsolete checked-in Fix33 Windows/Linux package archives and checksums.
- Removed versioned release-note fragments; release history is consolidated in `CHANGELOG.md`.
- Added a release-tree validator that rejects binary packages, generated SPIR-V, payload transports, one-shot workflows, and stale versioned notes.
- Audited every remaining branch: validated source was integrated; transport-only, fully merged, and obsolete branches are deleted after publication.

## Validation and testing focus

Windows and Linux full C++23 Release/Vulkan builds, shaders, deterministic tests, installation, archive creation, and package uploads passed for the release candidate. Runtime/visual missions remain active until the packaged build is observed, especially liquid settling, Half Water curves, pause/reset presentation, gas transport/rendering, hive/ecology cycles, Designer behavior, and save/load persistence.

Release source integration: PR #66, merge commit `2ef2e9b2daf83e453ed7f6dcc43bce9a8434597c`.

Validated GitHub Actions artifacts:

- Windows artifact digest: `sha256:f21baae8c7dd305a1781ff728f007b9ae7ba04b848473821bec963af9c4e36d3`
- Linux artifact digest: `sha256:0dd7b1d17c83d10e96095ba3e08ce9d2bb028e2183ad552e66f4123089d9582d`
