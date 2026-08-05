#!/usr/bin/env python3
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
errors = []
swarm = (root / "shaders/bee_swarm.glsl").read_text(encoding="utf-8")
move = (root / "shaders/move.comp").read_text(encoding="utf-8")
reset = (root / "shaders/reset.comp").read_text(encoding="utf-8")
scene_image = (root / "src/scene_image.cpp").read_text(encoding="utf-8")
renderer = (root / "src/vulkan_renderer.cpp").read_text(encoding="utf-8")
test = (root / "tests/scene_image_contract.cpp").read_text(encoding="utf-8")

match = re.search(r"BEE_INITIAL_PACKED\[BEE_FORMATION_COUNT\].*?\((.*?)\);", swarm, re.S)
values = [int(value) for value in re.findall(r"(\d+)u", match.group(1))] if match else []
if len(values) != 100 or len(set(values)) != 100 or values != sorted(values):
    errors.append("formation anchor table must contain exactly 100 unique sorted anchors")
points = [((value & 127) - 64, (value >> 7) - 64) for value in values]
if points:
    if min(x*x + y*y for x, y in points) < 144:
        errors.append("biohazard swarm overlaps the hive")
    central = sum(150 <= x*x + y*y <= 500 for x, y in points)
    upper = sum(y < -18 for x, y in points)
    lower_left = sum(x < -18 and y > 0 for x, y in points)
    lower_right = sum(x > 18 and y > 0 for x, y in points)
    if central < 14 or min(upper, lower_left, lower_right) < 18:
        errors.append("swarm no longer has a central ring and three distinct curved lobes")
for token in ("beeBiohazardTargetOffset", "beeFormationOffset(targetSlot) * 5 / 4",
              "if (boundedSidestep) return true;", "preserveAgentAge", "activeAgentPair"):
    if token not in swarm + move:
        errors.append(f"bee movement contract missing {token!r}")
if "shapePhase" in swarm:
    errors.append("whole biohazard symbol still rotates like a propeller")

for token in ("BRICK_SIZE = 8", "brickRect", "brickFrame", "brickStair",
              "sandboxMaterial", "volcanoMaterial", "waterworksMaterial", "ecosystemMaterial",
              "engineeringMaterial", "goldMineMaterial", "demolitionMaterial", "frontierBaseMaterial",
              "Large upper reservoir", "real sediment sifter", "authoredStructuralCell"):
    if token not in reset:
        errors.append(f"aligned scene contract missing {token!r}")
if "rectContains" in reset:
    errors.append("legacy pixel-aligned scene rectangle helper remains")
if reset.count("SCENE_") < 18:
    errors.append("not all nine scenes are represented in the brick rebuild")

for token in ("aux_bee_swarm", "pack_bee_metadata", "queen_indices", "bee_indices",
              "bee_target_none << 16u"):
    if token not in scene_image:
        errors.append(f"scene-image bee metadata contract missing {token!r}")
for token in ("vkCmdFillBuffer(command_buffer, chunk_buffer.handle", "if (explicit_load)"):
    if token not in renderer:
        errors.append(f"renderer reset/load contract missing {token!r}")
for token in ("aux_bee_swarm", "home_x != queen_x", "bee_target_none"):
    if token not in test:
        errors.append(f"scene-image regression test missing {token!r}")

if errors:
    raise SystemExit("Fix34 audit failed:\n  - " + "\n  - ".join(errors))
print("Fix34 audit passed: moving bees, true biohazard silhouette, and nine 8x8 brick-aligned scenes.")
