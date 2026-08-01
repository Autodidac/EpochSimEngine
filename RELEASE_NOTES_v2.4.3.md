# EpochSimEngine v2.4.3

This patch release completes the requested universal keyboard camera-control implementation and preserves the mission ledger's runtime-evidence rules.

## Camera controls

- `W`, `A`, `S`, and `D` now pan the camera in every scene.
- Character scenes continue forwarding the same keys to the player, so player and camera movement remain available simultaneously.
- Middle-mouse dragging and mouse-edge scrolling remain unchanged.

## Regression protection

- The source contract now requires universal camera input.
- Validation fails if the old player-scene camera gate is reintroduced.
- README controls and `MC-075` in `missioncache.md` describe the same behavior.

## Validation policy

Windows 2022 and Ubuntu 24.04 Release builds must compile all 12 entry shaders, build the C++23 static library and demo, pass all four contracts, install packages, and upload platform archives before publication.

Runtime-only scene acceptance is not inferred from compilation. Missions that still require visual, performance, conservation, or gameplay verification remain active and carry forward under their existing `MC-###` IDs.
