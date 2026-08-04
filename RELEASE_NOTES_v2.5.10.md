# SandHybrid v2.5.10

- Ground now continues across the entire resident world outside the authored 640x360 scene footprint.
- Biome boundaries use broad deterministic transition bands instead of hard seams.
- Mineral generation now produces complete structural 8x8 vein cores with sparse, localized rubble pockets and curved sand-trap cavities.
- The Volcano lake uses authored-local coordinates again, and the cone contains a visible crater lava lake, larger chamber, throat, and pressure vents.
- Water, saltwater, dirty water, acid, oil, honey, and molten materials equalize faster. The old 18-frame surface cutoff is gone, pressure checks reach eight cells, ledge searches are material-bounded, and six unrestricted horizontal passes run per simulation tick.
- Complete structural tiles collapse after the 31st destroyed cell and release all survivors into ordinary movement.
- Inventory, Editor, Settings, and Designer use distinct bodies, and the tool beam is rendered as compact sparse pixel bursts.

Unfinished visual and simulation acceptance remains explicit in `missioncache.md`; nothing is silently closed.
