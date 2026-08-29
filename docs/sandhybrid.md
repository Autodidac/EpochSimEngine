# SandHybrid example runtime

SandHybrid is the bundled Windows/Linux Vulkan example for EpochSimEngine. It presents one persistent connected World containing eight west-to-east districts: Sandbox, Ecosystem, Engineering, Frontier, Volcano, Waterworks, Gold Mine, and Demolition.

## Simulation

The example runs canonical materials, liquids, gases, weather, ecology, machinery, actors, terrain, and combat on fixed 60 Hz simulation ticks. Presentation caps do not change simulation cadence. Complete aligned `8x8` liquid and gas packets use macro transactions while exposed or incomplete boundaries use fine cells; the same canonical cells remain authoritative.

The World is a finite closed material system except for explicit user edits and declared boundaries. Reactions, inventory, weather, machines, phase changes, and actor tools must use balanced source/sink transactions.

## Controls and workspaces

- Inventory, Editor, Settings, and Designer are sidebar workspaces.
- Editor owns direct world mutation. Inventory owns player mining/deposit when no Blueprint is active.
- `H` and Camera Home restore the current camera; Reset preserves camera and MAP state.
- Paused mode freezes simulation, actors, clocks, MAP refresh, and effects while authorized editing remains live.
- The player laser damages and transfers material; it is never an eraser.

The in-app KEYMAP is authoritative for the complete current bindings.

## World and weather

Every district shares one aligned grass surface. The high sky contains one continuous conserved Cloud deck. Steam joins Cloud one-for-one; mature Cloud may return equal Water-family units as rain. Volcano, Waterworks, experiments, and terrain are examples of the same material APIs rather than separate scripted worlds.

## Saves and runtime checks

World saves use checksummed schema 2 and retain every canonical cell plus the exact actor owner. Schema 1 cell-only saves remain readable without inventing actor state.

The packaged executable can run the focused production Vulkan gate:

```text
sandhybrid --world-size compact --runtime-acceptance-report runtime-acceptance.json
```

Focused checks never substitute for long-cycle conservation, interactive performance, or packaged visual acceptance.
