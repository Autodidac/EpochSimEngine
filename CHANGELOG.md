# Changelog

## 2.5.2

- Restored the compact SandHybrid Fix28 Beehive model as the single generated/buildable prefab and renamed material ID 31 to Beehive without aliases.
- Renamed save ID 48 to Iron Ore, retained loose-cell gravity, and enabled structural tile placement alongside refined Iron.
- Replaced hashed scene-image colors with unique paint-friendly RGB values close to rendered cells; Save, Load, and both material keys now share one palette.
- Reorganized the mission cache around explicit P0/P1/P2 priorities and removed embedded release-history bulk.
- Consolidated release history into this file and removed obsolete versioned release-note files/workflows.

## 2.5.1

- Added the common upper-center authored scene, three subterranean geology zones, and stone-wrapped two-brick bottom lava band to generated and loaded scenes.

## 2.5.0

- Established the reusable library architecture and initiated the sparse 64x64 section-grid production rewrite with dirty rectangles, safe phases, halo wakeups, sleeping, and 512x512 stream-page coordinates.
