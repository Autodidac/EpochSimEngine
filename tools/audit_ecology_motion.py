#!/usr/bin/env python3
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
files = {
    "materials": (root / "shaders/materials.glsl").read_text(encoding="utf-8"),
    "tiles": (root / "shaders/tiles.comp").read_text(encoding="utf-8"),
    "move": (root / "shaders/move.comp").read_text(encoding="utf-8"),
    "paint": (root / "shaders/paint.comp").read_text(encoding="utf-8"),
    "swarm": (root / "shaders/bee_swarm.glsl").read_text(encoding="utf-8"),
    "chemistry": (root / "shaders/chemistry.comp").read_text(encoding="utf-8"),
    "reset": (root / "shaders/reset.comp").read_text(encoding="utf-8"),
}
errors = []
required = {
    "materials": ["MAT_PLANT_STEM) temperature = 20", "AUX_PLANT_STEM | 1u"],
    "tiles": ["activeContent", "!activeContent", "activeAgent", "activeLoose"],
    "move": ["sleepSafe", "beeMovementTarget", "return beeOrbitTarget(bee.aux, movePc.step, movePc.width, movePc.height);",
             "if (targetDistance < sourceDistance) return true;", "boundedSidestep",
             "Dense agents circulate around the moving hidden-mask target", "insectMoveAllowed",
             "isPassableLifeMedium", "insectMediumMoveAllowed",
             "target.material == MAT_BEE"],
    "paint": ["isDirectPaintLife", "displacePaintGas",
              "BEE_AUX_SWARM | BEE_AUX_FED"],
    "swarm": ["beeOrbitTarget", "beeSwarmTarget", "beeBiohazardTargetOffset",
              "BEE_SWARM_BIOHAZARD_TICKS", "BEE_SWARM_ALTERNATE_TICKS",
              "BEE_FORMATION_COUNT = 100u", "BEE_COLONY_MAX = 100u",
              "step / 360u", "beeFormationOffset(targetSlot) * 5 / 4"],
    "chemistry": ["flowerDropsSeed", "grassFrontier", "stemMoisture",
                  "source.material == MAT_PLANT_STEM",
                  "Painted and loaded orphan bees self-seed",
                  "respiringNeighborDemand", "beeRoll < demand.x",
                  "fireRespiration", "BEE_COLONY_MAX"],
    "reset": ["Canonical Fix29 Ecosystem-map hive body/content", "beehiveSupportForScene",
              "beehivePrefabMaterial", "includeFormation"],
}
for name, tokens in required.items():
    for token in tokens:
        if token not in files[name]:
            errors.append(f"{name}: missing {token}")
forbidden = {
    "materials": ["MAT_PLANT_STEM) temperature = 900", "AUX_CHARGED | 72u"],
    "move": ["return tileHas(tileA, TILE_SLEEPING) && tileHas(tileB, TILE_SLEEPING);",
             "targetDistance <= 49"],
    "swarm": ["BEE_FORMATION_COUNT = 200u", "step / 90u"],
    "chemistry": ["respiringNeighborCount",
                  "if (nearFire || hasNeighbor(p, MAT_EMBER)"],
}
for name, tokens in forbidden.items():
    for token in tokens:
        if token in files[name]:
            errors.append(f"{name}: forbidden legacy contract remains: {token}")
if errors:
    raise SystemExit("\n".join(errors))
packed = re.search(r"BEE_INITIAL_PACKED\[BEE_FORMATION_COUNT\] = uint\[\]\((.*?)\n\);", files["swarm"], re.S)
if packed is None or len(re.findall(r"\d+u", packed.group(1))) != 100:
    errors.append("swarm: expected exactly 100 authored biohazard points")
macro = (root / "shaders/macro_move.comp").read_text(encoding="utf-8")
if "regionSupported(sourceOrigin, source) && target.material == MAT_EMPTY" in macro:
    errors.append("macro: horizontal bulk movement still rejects represented atmosphere")
if "regionSupported(sourceOrigin, source);" not in macro:
    errors.append("macro: horizontal density/displacement contract missing")
if errors:
    raise SystemExit("\n".join(errors))
print("Ecology and motion contracts passed: 100-bee colony cap, slower respiration, stable biohazard mask, approved hive, represented-atmosphere macro displacement, mobile life, plants, and flowers.")
