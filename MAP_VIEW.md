# SandHybrid Full-World Map

The v2.5.6 map is a presentation/debug view over a separate GPU snapshot. It does not change simulation scheduling, LOD, the main camera, or the active-region origin.

- The resident world is 16 authored-map footprints wide by 4 high: 10240x1440 cells.
- MAP zoom 1 shows the complete world at its true 64:9 aspect. The viewport letterboxes instead of stretching the world.
- MAP owns independent WASD, right-click drag, right-click-held edge pan, center, and zoom state.
- The live main-camera rectangle is drawn separately from restrained 640x360 outlines around the contiguous 4x4 simulation-active window; per-tile/per-chunk checkerboard status paint is disabled in MAP.
- The snapshot refreshes at 4 Hz so full-world inspection does not force full-rate rendering or simulation work.
- Closing MAP returns to the unchanged simulation camera.
