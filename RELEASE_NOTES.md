# EpochSimEngine v2.5.28

EpochSimEngine is the project and reusable simulation library. SandHybrid names only its bundled Windows/Linux demo.

## Current candidate status

The updated private Windows eye-test candidate includes the subsequent September 5 save, weather, ownership, debug, and submission changes. The latest behavior candidate passes Windows CTest 74/74, installed RTX 5080 checks 126/126, finite/save cycles 12/12 and the 1,920-frame presentation gate. Current-source Linux parity, personal user visual approval, and final versioned packages remain pending. The naming-only follow-up is documented in the mission cache with its own validation; no new simulation or public release is implied.

The evidence below records the older frozen September 4 candidate, not completed Linux validation of the updated source. Its immutable local archives and tag remain unchanged. Final Site handoff must identify the accepted exact source and new artifacts; it may not reuse historical hashes for rebuilt files.

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

The mission cache retains 123 active missions (70 PARTIAL, 36 REGRESSION, 16 OPEN, one DEFERRED). Broader bee aging/migration, long visual recurrence, cross-district acceptance, and the architectural backlog remain active. The current user instruction requires personal eye-test approval before final Release packages and Site handoff.

## Dependency

The complete canonical EpochGui snapshot remains v0.89.30 at b97167423373b9a7af3f821dcf91d8a71613dbf2, confirmed against the Site mirror on 2026-09-04. Its CMake/GNU compatibility boundary remains documented in third_party/EpochGui/SNAPSHOT.md.

## Final release naming (packages pending)

- `SandHybrid-Windows-x64-v2.5.28.zip` — Windows demo and bundled library install.
- `SandHybrid-Linux-x64-v2.5.28.tar.gz` — Linux demo and bundled library install.
- `EpochSimEngine-v2.5.28-source.tar.gz` — platform-neutral committed project/library source.
- `EpochSimEngine-v2.5.28-source.zip` — the same platform-neutral committed source in Windows-friendly ZIP form.

Final archives require sibling SHA-256 records. The older frozen `SandHybrid-v2.5.28-source.tar.gz` is a legacy-named artifact, not the new canonical source package; it is not renamed or overwritten by this naming correction. Existing public clone/download/updater URLs retain compatibility, and current aliases update only after matching immutable objects verify under final release authorization. Site publication is a normal visible EpochSimEngine release with SandHybrid demo downloads; GitHub publication is out of scope.
