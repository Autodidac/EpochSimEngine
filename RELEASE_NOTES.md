# SandHybrid v2.5.23

One persistent World and eight-opportunity macro scheduling recovery.

## Corrected behavior

- The normal runtime now exposes one `WORLD`, not a scene carousel. Sandbox, Ecosystem, Engineering, Frontier Base, Volcano, Waterworks, Gold Mine, and Demolition reset together as connected districts in one resident cell field. Prev/Next controls are removed; Save and Load own the full control row.
- The player is always enabled and recovers at the supported breathable Frontier district spawn. Reset and camera home use the same district-relative coordinates.
- Normal saves use `saves/worlds/<size>/world/<slot>/` and preserve the complete combined resident buffers. Legacy scene IDs remain migration-only.
- Complete aligned 8x8 liquid and gas packets remain macro-owned scheduling views over canonical cells and attempt movement every two fixed 60 Hz ticks. Exposed or misaligned packets receive eight due opportunities; only a fresh open/incompatible perimeter at that point permits fine fallback. Enclosed Air bordered by solids or compatible gas tiles stays tiled.
- Fine cells continually requalify for exact complete-tile ownership. Macro movement never replaces, synthesizes, or deletes a canonical material cell.
- Full Water keeps the Half Water flag masked, renders with stable full-Water color, and uses deterministic outlet selection. Half Water stays darker, falls first, merges deterministically, attracts only over a clear two-to-four-cell gap, and retains supplied ledge hang/drip behavior.
- The Volcano district uses the broad left lake and far-right Stone cone layout with a complete Stone foundation. Debug presentation uses restrained square 8x8 edges and bounded sampling rather than filled oblong region tint.
- Editor paint, erase, Fill, Ignite Air, selection, and Blueprint placement remain live in both RUNNING and PAUSED while fixed simulation, actors, clocks, effects, and MAP refresh freeze when paused.

## Focused acceptance

The Windows Release suite passes all deterministic, source, save, layout, shader-interface, downstream-package, and supported EpochGui tests. The cached packaged Vulkan report covers exact macro Water/Hydrogen movement, eighth-opportunity blocked fallback, enclosed-Air retention, conserved Half Water fall/merge/drip cases, zero-jitter Water equilibrium, all eight in-place district foundations, and the live Frontier player at `(2088,567)` with full health and Oxygen.

This focused gate does not close the active long-duration ecology/weather loop, cross-district traversal and machinery observation, debug overhead benchmark, broad visual review, or the user's photographed irregular earliest-repository hive. The current invariant hive is still checked consistently across generation/tool/load paths, but this release does not claim that unresolved photograph match is complete.

Run the installed executable manually when a fresh driver shader cache is acceptable:

    sandhybrid --world-size compact --runtime-acceptance-report runtime-acceptance.json

## Dependency snapshot

The complete vendored EpochGui dependency remains v0.88.75 at `d8decc9ee2e73e0009f1e8c49d86a52db6748b28`. The required release-time upstream query was attempted on 2026-08-16, but GitHub returned account-suspension HTTP 403 and no public repository result. The available snapshot was retained intact rather than downgraded or partially copied; all supported vendored tests pass.

## Stable local assets

- SandHybrid-Windows-x64-v2.5.23.zip
- SandHybrid-Windows-x64-v2.5.23.zip.sha256
- SandHybrid-Linux-x64-v2.5.23.tar.gz
- SandHybrid-Linux-x64-v2.5.23.tar.gz.sha256

This pass is authorized for local packages only. It does not push, tag, publish, or modify a public GitHub release.