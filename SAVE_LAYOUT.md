# SandHybrid world saves

Gameplay uses one exact persistent-World save. PPM remains an authored district import/export format, not a second gameplay world format.

## Folder layout

Each executable keeps its own portable save tree:

```
saves/
  worlds/
    compact|standard|large/
      world/
        <slot>/
          world.shw
          world.bak
          manifest.txt
```

The default slot is `quick`. Use `--save-slot NAME` to select another slot. Slot names are sanitized to 32 alphanumeric, dash, or underscore characters, so paths cannot escape the save root.

Legacy scene IDs remain decoder-compatible for deterministic migration tooling, but the normal UI never exposes separate scene saves and does not merge old folders implicitly.

## World sizes

- Compact: 2560x1440 cells
- Standard: 5120x1440 cells
- Large: 10240x1440 cells

Every size contains the same centered 2560x720 eight-district envelope. A save is stored under its size and cannot be loaded into a differently sized resident world, preventing silent cropping, stretching, or buffer overruns.

## File integrity

`world.shw` stores every exact 16-byte canonical cell, including material, age, temperature, and auxiliary state, across the combined World. The payload is split into deterministic 64x64 chunks. Each chunk chooses raw or run-length encoding and carries its own checksum; the complete payload also has a checksum.

Saving writes `world.tmp`, rotates the previous valid file to `world.bak`, then atomically publishes `world.shw`. Loading validates the complete file before changing either resident buffer. If the primary file is damaged, the loader attempts `world.bak`.