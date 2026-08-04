# SandHybrid v2.5.11

- Replaced gameplay PPM saving with a versioned, exact-state `.shw` world format.
- Saves now include the selected world size, dimensions, scene, full material/age/temperature/aux cell state, chunk table, per-chunk checksums, and a full-payload checksum.
- Added atomic temporary-file publication, automatic previous-save backup, and validated backup recovery.
- Added size- and scene-aware portable folders under `saves/worlds/<size>/<scene>/<slot>/`.
- Added selectable Compact (2560×1440), Standard (5120×1440), and Large (10240×1440) startup sizes through launchers and `--world-size`.
- Added named save slots through `--save-slot`; default UI Save/Load uses the selected slot.
- Kept PPM files as authored 640×360 scene import/export assets only, preventing them from truncating a full resident world.
- Included the v2.5.10 terrain, Volcano, liquid, workspace, fracture, and tool corrections in the release baseline.

Runtime visual acceptance for the terrain/liquid recovery and an in-window world-creation selector remain explicit in `missioncache.md`; they are not silently marked complete.
