# SandHybrid Scene and World Layout

- The resident world contains 16 by 4 complete 640x360 camera regions.
- The authored 640x360 scene begins at aligned region origin `(1280,720)` and occupies one complete camera region without crossing a region boundary.
- Camera reset, character spawns, actor reset, bee homes, generated content, loaded-scene offsets, saves, and scene metadata use the same authored origin.
- Generic left/right walls around the authored scene are forbidden.
- The authored foundation continues horizontally across the full resident world.
- Structural containment exists only on the outer resident-world perimeter.
- Runtime screenshot contradictions and propagation requirements are release-blocking evidence in `missioncache.md`.
- The evidence ledger is authoritative for the v2.5.6 release gate.
