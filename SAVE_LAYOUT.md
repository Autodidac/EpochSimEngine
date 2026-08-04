# SandHybrid world saves

Gameplay saves are no longer PPM scene images. PPM files remain an authored-scene import/export format only.

## Folder layout

Each executable keeps its own portable save tree:

```
saves/
  worlds/
    compact|standard|large/
      sandbox|blank|volcano|waterworks|ecosystem|engineering_lab|platformer|demolition|frontier_base/
        <slot>/
          world.shw
          world.bak
          manifest.txt
```

The default slot is `quick`. Use `--save-slot NAME` to select another slot. Slot names are sanitized to 32 alphanumeric, dash, or underscore characters, so paths cannot escape the save root.

## World sizes

- Compact: 2560×1440 cells (4×4 authored-camera footprints)
- Standard: 5120×1440 cells (8×4 footprints)
- Large: 10240×1440 cells (16×4 footprints)

Use the supplied size launchers or pass `--world-size compact|standard|large`. A save is stored under its size and cannot be loaded into a differently sized simulation. This prevents silent cropping, stretching, and buffer overruns.

## File integrity

`world.shw` stores exact 16-byte cell state, including material, age, temperature, and auxiliary state. The world is split into deterministic 64×64 chunks. Each chunk chooses raw or run-length encoding and has its own checksum; the complete payload also has a checksum.

Saving writes `world.tmp`, rotates the previous valid file to `world.bak`, then atomically publishes `world.shw`. Loading validates the complete file before modifying the live simulation. If the primary file is damaged, the loader attempts `world.bak`.
