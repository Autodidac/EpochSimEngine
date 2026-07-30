#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
files = {
    "materials": (root / "shaders/materials.glsl").read_text(encoding="utf-8"),
    "tiles": (root / "shaders/tiles.comp").read_text(encoding="utf-8"),
    "move": (root / "shaders/move.comp").read_text(encoding="utf-8"),
    "swarm": (root / "shaders/bee_swarm.glsl").read_text(encoding="utf-8"),
    "chemistry": (root / "shaders/chemistry.comp").read_text(encoding="utf-8"),
}
errors = []
required = {
    "materials": ["MAT_PLANT_STEM) temperature = 20", "AUX_PLANT_STEM | 1u"],
    "tiles": ["activeContent", "!activeContent", "activeAgent", "activeLoose"],
    "move": ["sleepSafe", "beeMovementTarget", "return beeOrbitTarget(bee.aux, movePc.step);",
             "if (targetDistance < sourceDistance) return true;", "boundedSidestep",
             "Dense agents circulate around the moving hidden-mask target", "insectMoveAllowed"],
    "swarm": ["beeOrbitTarget", "beeSwarmTarget", "beeBiohazardTargetOffset",
              "BEE_SWARM_BIOHAZARD_TICKS", "BEE_SWARM_ALTERNATE_TICKS"],
    "chemistry": ["flowerDropsSeed", "grassFrontier", "stemMoisture",
                  "source.material == MAT_PLANT_STEM"],
}
for name, tokens in required.items():
    for token in tokens:
        if token not in files[name]:
            errors.append(f"{name}: missing {token}")
forbidden = {
    "materials": ["MAT_PLANT_STEM) temperature = 900", "AUX_CHARGED | 72u"],
    "move": ["return tileHas(tileA, TILE_SLEEPING) && tileHas(tileB, TILE_SLEEPING);",
             "targetDistance <= 49"],
}
for name, tokens in forbidden.items():
    for token in tokens:
        if token in files[name]:
            errors.append(f"{name}: forbidden legacy contract remains: {token}")
if errors:
    raise SystemExit("\n".join(errors))
print("Ecology and motion contracts passed: dynamic tiles, hidden swarm masks, insects, grass, seeds, stems, and flowers.")
