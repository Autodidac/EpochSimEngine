# SandHybrid v2.5.22

Exact Fix29 hive and liquid-equilibrium recovery.

## Corrected behavior

- Sandbox `(512,234)`, Ecosystem `(512,232)`, Beehive tool placement, and loaded-map normalization now hash the actual 640-wide cell coordinate with seed `0xD17A5EED`. The photographed Fix29 shell, chamber, queen, right exit, and Honey/Pollen/empty payload are compared cell-for-cell instead of accepted from aggregate counts.
- The nine aligned structural Wood perch tiles remain `x=472..543`, `y=216..223`; exact hive-body cells override the overlapping perch while SandHybrid keeps its own bee population and runtime behavior.
- Full Water and Half Water may spend movement only once per fixed tick. Fine movement now runs one seven-phase schedule instead of replaying horizontal pairs ten times against a fixed snapshot.
- Half Water remains a conserved darker one-unit state: fall first, deterministic adjacent merge, clear two-to-four-cell attraction, supplied-ledge split/hang/drip, no macro ownership, and no generic full-Water wandering.
- Full Water selects the nearest reachable outlet with a deterministic tie, preventing dispatch-order surface oscillation. Motionless partial-liquid tiles use liquid age—not surrounding atmosphere age—and sleep without retaining contradictory fine-active scheduling.
- The v2.5.3 complete liquid/gas macro-packet baseline, same-attempt fine fallback, two-tick packet cadence/exposure, fixed 60 Hz simulation, paused editing, cursor mapping, sidebar workspaces, Blueprint transactions, scenes, players, and machinery remain preserved.

## Packaged Vulkan acceptance

Run the installed executable with:

    sandhybrid --world-size compact --runtime-acceptance-report runtime-acceptance.json

The 21-check production Vulkan gate covers exact macro Water/gas transactions and fallback; exact translated, Sandbox, and Ecosystem Fix29 hives; conserved Half Water fall/merge/attraction/split/drip; zero-jitter Water equilibrium; unsupported-ledge flow; and all nine scene foundations. Broader visual, long-duration, machinery, player, bee-cycle, and complete mission-cache scenarios remain active until their own acceptance evidence exists.

## Dependency snapshot

The complete vendored EpochGui dependency remains v0.88.75 at `d8decc9ee2e73e0009f1e8c49d86a52db6748b28`. A release-time fetch was attempted, but GitHub account suspension returned HTTP 403 and anonymous lookup returned 404. The available local `origin/main` is older, so no downgrade or partial copy was made; all three supported upstream Windows tests pass.

## Stable local assets

- SandHybrid-Windows-x64-v2.5.22.zip
- SandHybrid-Windows-x64-v2.5.22.zip.sha256
- SandHybrid-Linux-x64-v2.5.22.tar.gz
- SandHybrid-Linux-x64-v2.5.22.tar.gz.sha256
