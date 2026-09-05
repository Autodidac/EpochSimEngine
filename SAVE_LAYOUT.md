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

- Compact: 5120x1440 cells
- Standard: 7680x1440 cells
- Large: 10240x1440 cells

Every size contains the same eight west-to-east 640x360 districts and one common terrain surface at world Y `1040`. Compact has no horizontal gaps; Standard and Large distribute aligned gaps between districts. A save is stored under its size and cannot be loaded into a differently sized resident world, preventing silent cropping, stretching, or buffer overruns.

## File integrity

`world.shw` stores every exact 16-byte canonical cell, including material, age, temperature, and auxiliary state, across the combined World. Cloud deck units, Steam/Smoke composition, Water/Half Water state, Volcano ejecta, and district-gap atmosphere are saved as ordinary canonical ownership, never regenerated decoration. A Half Water cell therefore retains its reserved Half flag, displaced-medium material/volume, encoded displaced-medium temperature, own Water temperature, and age through both primary and backup recovery. Actor inventory is a separate persisted owner in the same closed material ledger, so a laser transfer changes ownership without changing the world-plus-inventory total. The payload is split into deterministic 64x64 chunks. Each chunk chooses raw or run-length encoding and carries its own checksum; the complete payload also has a checksum.

## Format compatibility

Schema 2 keeps the schema-1 header and deterministic cell-chunk layout, using the formerly reserved header word to declare a 96-byte owner record inside the complete payload checksum. That record has an 8-byte owner magic, a 4-byte actor-record version, a 4-byte actor byte count, and the exact endian-stable 20-word/80-byte GPU actor payload: position, vertical velocity, enabled state, Gold/Iron/Ammo, shot and movement timers, grounded state, health, Oxygen, last-hit position, scene, exposure ticks, Aluminum/Copper, unlock bits, and drill level.

The loader accepts schema 1 and schema 2. Schema 1 must have no owner payload and loads the exact cells while reporting that no actor owner was stored, allowing deterministic migration to retain the current actor rather than fabricate inventory. Schema 2 validates owner magic/version/size, booleans, inventory limits, health/Oxygen, scene, progression, enabled-player bounds, chunks, and the full payload hash before committing either the cell buffer or actor state. Primary corruption may recover both from `world.bak`; failure of both files changes neither caller-owned buffer.

Saving rejects invalid cells/metadata before touching a slot, writes `world.tmp`, and validates the previous primary completely before admitting it as a backup. A healthy primary is copied to `world.bak.tmp` and atomically replaces `world.bak`; the prepared new file then atomically replaces `world.shw`. A corrupt or missing primary never displaces an existing good backup, including after repeated recovery/save cycles. Publication never deletes a healthy generation before its replacement is ready. The text manifest is derived metadata; the checksummed world file is authoritative. These file-replacement guarantees are not a claim of power-loss durability on every filesystem.

Backup, primary, and manifest publication are independent atomic replacements, not one atomic slot transaction. A primary-publication failure can occur after the backup has already changed. A manifest failure can make `save_world` return `false` after the valid new `world.shw` has already committed; that primary remains authoritative even if `manifest.txt` is stale or absent. Therefore a failed save does not generally imply that every slot file remains byte-identical. Invalid input and particular early failures have the narrower no-mutation guarantees described above.

Loading validates the complete file before changing cells, actor owners, or returned metadata. If the primary file is damaged, the loader attempts `world.bak`; failure of both files leaves all caller outputs unchanged. Schema and material payloads are unchanged by the backup fix.
