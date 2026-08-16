# SandHybrid World Layout

SandHybrid exposes one persistent `World`. The normal runtime does not swap or reset separate scenes.

## District composition

Eight 640x360 authored districts occupy one centered 4x2 envelope in the resident `SceneCell` buffers:

| Row | Column 0 | Column 1 | Column 2 | Column 3 |
|---|---|---|---|---|
| North | Sandbox | Ecosystem | Engineering | Frontier Base |
| South | Volcano | Waterworks | Gold Mine | Demolition |

The compact resident world is 2560x1440, so the district envelope begins at `(0,360)`. Standard centers it at `(1280,360)` inside 5120x1440; Large centers it at `(3840,360)` inside 10240x1440. Districts share the same canonical cells, actor state, atmosphere, heat, weather, hierarchy scheduling, reset epoch, and save file. Travelling between them never selects a new simulation instance.

`Blank` and the former scene enum values remain internal migration identifiers only. They are not selectable runtime worlds and there are no Prev/Next controls.

## Structural rules

- Generated terrain and authored structural or liquid starting volumes use complete aligned 8x8 tiles.
- Fine actors, vegetation, loose cargo, smoke, rubble, and the cell-resolution hive are explicit exceptions.
- Each district keeps its authored empty interior; resident substrate never backfills it.
- Every district has a complete supported Stone foundation and a deterministic breathable player recovery point.
- Structural containment exists only at the outer resident-world boundary.
- Runtime screenshot contradictions and unfinished cross-district behavior remain release-blocking evidence in `missioncache.md`.