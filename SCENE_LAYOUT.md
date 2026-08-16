# SandHybrid World Layout

SandHybrid exposes one persistent `World`. The normal runtime does not swap or reset separate scenes.

## District composition

Eight 640x360 authored districts remain ordered west-to-east but are distributed across the resident width:

1. Sandbox
2. Ecosystem
3. Engineering
4. Frontier Base
5. Volcano
6. Waterworks
7. Gold Mine
8. Demolition

Compact is exactly 5120x1440, so its districts are contiguous at X `0, 640, 1280, 1920, 2560, 3200, 3840, 4480`. Standard is 7680x1440 and starts them at X `0, 1000, 2000, 3000, 4000, 5000, 6000, 7000`. Large is 10240x1440 and starts them at X `0, 1368, 2736, 4104, 5472, 6840, 8208, 9576`.

The scenes do not share a blindly fixed Y origin. Each district translates its intended authored surface row onto world Y `1040`, so the main grass line is continuous while each authored structure retains its intended height relative to terrain. They share canonical cells, actor state, atmosphere, heat, weather, hierarchy scheduling, reset epoch, and one save. Travelling between them never selects another simulation instance.

`Blank` and the former scene enum values remain internal migration identifiers only. They are not selectable runtime worlds and there are no Prev/Next controls.

## Structural rules

- Generated terrain and authored structural or liquid starting volumes use complete aligned 8x8 tiles.
- Fine actors, vegetation, loose cargo, smoke, rubble, and the cell-resolution hive are explicit exceptions.
- Each district keeps its authored empty interior; resident substrate never backfills it.
- Every district has a complete supported Stone foundation and a deterministic breathable player recovery point.
- Structural containment exists only at the outer resident-world boundary.
- Large startup activates no more than three authored districts inside the initial 4x4 scheduler window; other districts remain canonical resident state without paying active chemistry/movement cost.
- Runtime screenshot contradictions and unfinished cross-district behavior remain release-blocking evidence in `missioncache.md`.