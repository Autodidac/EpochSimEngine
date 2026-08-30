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
materials = (root / "shaders/materials.glsl").read_text(encoding="utf-8")
fullscreen = (root / "shaders/fullscreen.frag").read_text(encoding="utf-8")
test = (root / "tests/scene_image_contract.cpp").read_text(encoding="utf-8")

match = re.search(r"BEE_INITIAL_PACKED\[BEE_FORMATION_COUNT\].*?\((.*?)\);", swarm, re.S)
values = [int(value) for value in re.findall(r"(\d+)u", match.group(1))] if match else []
if len(values) != 100 or len(set(values)) != 100 or values != sorted(values):
    errors.append("formation anchor table must contain exactly 100 unique sorted anchors")
points = [((value & 127) - 64, (value >> 7) - 64) for value in values]
if points:
    if min(x*x + y*y for x, y in points) < 144:
        errors.append("biohazard swarm overlaps the hive")
    if max(x for x, y in points) - min(x for x, y in points) > 52 or max(y for x, y in points) - min(y for x, y in points) > 52:
        errors.append("biohazard swarm is no longer compact")
    if any(-40 <= x <= 31 and -18 <= y <= -9 for x, y in points):
        errors.append("biohazard swarm overwrites the complete Wood support tiles")
    central = sum(150 <= x*x + y*y <= 500 for x, y in points)
    upper = sum(y < -18 for x, y in points)
    lower_left = sum(x < -10 and y > 0 for x, y in points)
    lower_right = sum(x > 10 and y > 0 for x, y in points)
    if central < 14 or min(upper, lower_left, lower_right) < 18:
        errors.append("swarm no longer has a central ring and three distinct curved lobes")
for token in ("beeBiohazardTargetOffset", "return beeFormationOffset(slot)",
              "if (sourceDistance == 0) return false;",
              "if (boundedSidestep) return true;", "preserveAgentAge", "activeAgentPair"):
    if token not in swarm + move:
        errors.append(f"bee movement contract missing {token!r}")
for token in ("beeUsesPersistentWorldHome(uint width, uint height)", "district << 20u"):
    if token not in swarm:
        errors.append(f"persistent bee-home contract missing {token!r}")
if "BEE_PERSISTENT_HOME_BIT" in swarm:
    errors.append("persistent bee metadata collides with reserved Half Water state")
for token in ("shapePhase", "beeHaloTargetOffset", "beeCloudTargetOffset",
              "BEE_SWARM_ALTERNATE_TICKS", "formationCycle"):
    if token in swarm:
        errors.append(f"whole-colony presentation motion remains: {token}")


for token in ("BRICK_SIZE = 8", "brickRect", "brickFrame", "brickStair",
              "sandboxMaterial", "volcanoMaterial", "waterworksMaterial", "ecosystemMaterial",
              "engineeringMaterial", "goldMineMaterial", "demolitionMaterial", "frontierBaseMaterial",
              "Large upper reservoir", "real sediment sifter", "authoredStructuralCell"):
    if token not in reset:
        errors.append(f"aligned scene contract missing {token!r}")
if "rectContains" in reset:
    errors.append("legacy pixel-aligned scene rectangle helper remains")
for token in ("Three complete authored Hydrogen packets", "bubbleRow = maximumBrick.y - 5",
              "b.x == bubbleQuarter", "material = MAT_HYDROGEN"):
    if token not in reset:
        errors.append(f"normal-World Waterworks packet fixture missing {token!r}")
for token in ("fixedHiveContent", "vec4(comb ? 0.78 : 0.82"):
    if token not in materials:
        errors.append(f"Fix29 visible hive palette missing {token!r}")
for token in ("fixedHiveComposite", "illumination = max(illumination, 0.90)",
              "Classify it before rejecting", "if (!mapSample &&"):
    if token not in fullscreen:
        errors.append(f"Fix29 shadow-readable presentation missing {token!r}")
if reset.count("SCENE_") < 18:
    errors.append("not all nine scenes are represented in the brick rebuild")

for token in ("aux_bee_swarm", "pack_bee_metadata", "queen_indices", "bee_indices",
              "bee_target_none << 16u"):
    if token not in scene_image:
        errors.append(f"scene-image bee metadata contract missing {token!r}")
for token in ("vkCmdFillBuffer(command_buffer, chunk_buffer.handle", "if (explicit_load)",
              "world_waterworks_authored_bubble_packets",
              "world_waterworks_bubbles_eight_step_breakup"):
    if token not in renderer:
        errors.append(f"renderer reset/load contract missing {token!r}")
download_block = re.search(
    r"download_scene_cells\(\).*?buffer_barrier\(command_buffer, scene_staging_buffer,"
    r".*?VK_ACCESS_HOST_READ_BIT \| VK_ACCESS_HOST_WRITE_BIT \|"
    r".*?VK_ACCESS_TRANSFER_READ_BIT,.*?VK_ACCESS_TRANSFER_WRITE_BIT,",
    renderer, re.S)
upload_block = re.search(
    r"upload_scene_cells\(.*?buffer_barrier\(command_buffer, scene_staging_buffer,"
    r".*?VK_ACCESS_HOST_READ_BIT \| VK_ACCESS_HOST_WRITE_BIT \|"
    r".*?VK_ACCESS_TRANSFER_WRITE_BIT,.*?VK_ACCESS_TRANSFER_READ_BIT,",
    renderer, re.S)
if not download_block or not upload_block:
    errors.append("cold-start scene upload/readback lacks explicit host/transfer staging barriers")

for token in ("aux_bee_swarm", "home_x != queen_x", "bee_target_none"):
    if token not in test:
        errors.append(f"scene-image regression test missing {token!r}")

if errors:
    raise SystemExit("Fix34 audit failed:\n  - " + "\n  - ".join(errors))
print("Fix34 audit passed: moving bees, true biohazard silhouette, and nine 8x8 brick-aligned scenes.")
