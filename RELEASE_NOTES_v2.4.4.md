# SandHybrid v2.4.4

## Critical Windows GPU stability correction

- Tile selection remains state-only and cannot allocate, resize, or rebind GPU buffers.
- The logical world remains 8x8 original map footprints.
- Until MC-063 implements deterministic far-section streaming, the resident GPU window is bounded to 4x4 map footprints.
- This reduces persistent cell, snapshot, staging, sunlight, hierarchy, and full-pass dispatch load to 25% of the crashing fully resident configuration.
- MC-077 records the required Windows runtime switching and device-loss acceptance. MC-063 and MC-067 retain the complete streamed logical-world work.

## Input correction retained

W/A/S/D controls the player exclusively when a player exists and controls the camera exclusively otherwise.
