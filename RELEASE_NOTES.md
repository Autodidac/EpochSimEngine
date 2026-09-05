# SandHybrid v2.5.28

Hive placement, colony lifecycle, and save recovery correction for the SandHybrid example/runtime bundled with the EpochSimEngine library.

## Corrected behavior

- Each Beehive press creates one complete photographed saturated-golden, no-perch Fix29 hive. Holding the button or moving the pointer no longer stamps duplicate colonies. Cells/Tiles mode and line-shaped brushes use the same complete prefab.
- A recognizable obsolete body/perch is cleaned within the placement footprint, and a subsequent placement replaces the prior tool colony. Unrelated materials and authored colonies outside the overlap remain intact.
- Cleanup uses an immutable bounded snapshot and parallel single-cell ownership, including far-ranging old-colony foragers. This replaces a serial scan that failed to reach the old body on Linux software Vulkan.
- Exactly 60 live district-home SandHybrid bees retain unique home and formation slots. Six foragers depart independently while 54 resting Bees retain the accepted three outward-open crescents; movement claims prevent repeated movement within one dispatch.
- Exact schema-2 loads no longer run the retired single-scene normalizer. The uniquely recognizable v2.5.27 phantom body and stale Bee remnants are repaired in memory with a log entry; ambiguous saves and all Empty/Atmosphere opening payloads are preserved.
- Schema-1 migration reconstructs the correct district origins, live timers, and all 60 Bee owners. Retired random circular-nest growth is rejected while current queen migration and replacement lifecycle paths remain available.

The accepted hive/crescent artwork and high-sky Nuke presentation are unchanged.

Windows presentation now uses a private high-resolution one-shot timer, avoiding dependence on other applications' ordinary-sleep timer settings. Fixed simulation ticks and presentation caps are unchanged; no global timer setting or busy wait is used.

## Verification and remaining work

Final Release builds pass 44/44 Windows and 34/34 Linux CTests, including execution of both installed API-4 library consumers and the ten supported Windows EpochGui suites. Installed RTX 5080 and Linux Xvfb/llvmpipe production reports each pass 84/84 with identical results; both installed finite-ledger/save-load reports pass 12/12. Repeat placement and the new far-edge cleanup case retain one Queen, 60 Bees, and 193 shell cells. The autonomous fixture observes six independent foragers and 54 resting owners; three chained hazard/save-load cycles recover distinct missing slots without exceeding 60.

Two final installed Windows presentation passes each capture 1,920 frames and all nine visual states under unchanged acceptance gates. Normal/REGION cadence p95 is 17.0189/16.9651 ms and 17.0428/16.9811 ms; draw p95 deltas are 1.4828 and 1.3834 ms. Linux captures all nine states in 15 presented frames; its software-renderer performance is explicitly ungated. Agent-reviewed hive captures preserve the accepted artwork. These bounded results do not establish that every workload is jitter-free or replace personal user visual acceptance.

The Linux production run exits zero in 57:49.33, peaks at 9,773,760 KiB RSS, and uses zero swap (elapsed time includes one brief intentional pause for Windows timing isolation). Linux finite/save cycles take 0:50.06 with 1,417,384 KiB maximum RSS; presentation takes 7:45.47 with 1,530,868 KiB maximum RSS, both with zero swap. Earlier failed cleanup and timing reports are retained; no acceptance threshold was relaxed.

The mission cache retains 123 active missions (70 PARTIAL, 36 REGRESSION, 16 OPEN, one DEFERRED). Broader bee aging/migration, long visual recurrence, cross-district acceptance, and the architectural backlog remain active. The user has authorized release after the package gates, but has not personally visually checked this correction.

## Dependency

The complete canonical EpochGui snapshot remains v0.89.30 at b97167423373b9a7af3f821dcf91d8a71613dbf2, confirmed against the Site mirror on 2026-09-04. Its CMake/GNU compatibility boundary remains documented in third_party/EpochGui/SNAPSHOT.md.

## Stable release assets

- SandHybrid-Windows-x64-v2.5.28.zip
- SandHybrid-Linux-x64-v2.5.28.tar.gz
- SandHybrid-v2.5.28-source.tar.gz
- EpochSimEngine-v2.5.28-source.zip (the same platform-neutral committed source in Windows-friendly ZIP form)

Each archive has a sibling SHA-256 record. Current aliases retain their existing names and are updated only after matching immutable objects verify. Site publication is a normal visible release; GitHub publication is out of scope.
