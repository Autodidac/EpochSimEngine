#!/usr/bin/env python3
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
errors = []
swarm = (root / "shaders/bee_swarm.glsl").read_text(encoding="utf-8")
move = (root / "shaders/move.comp").read_text(encoding="utf-8")
chemistry = (root / "shaders/chemistry.comp").read_text(encoding="utf-8")
bee_move = (root / "shaders/bee_move.comp").read_text(encoding="utf-8")
legacy_swarm = (root / "shaders/bee_swarm_chemistry_legacy.glsl").read_text(encoding="utf-8")
reset = (root / "shaders/reset.comp").read_text(encoding="utf-8")
scene_image = (root / "src/scene_image.cpp").read_text(encoding="utf-8")
renderer = (root / "src/vulkan_renderer.cpp").read_text(encoding="utf-8")
materials = (root / "shaders/materials.glsl").read_text(encoding="utf-8")
fullscreen = (root / "shaders/fullscreen.frag").read_text(encoding="utf-8")
test = (root / "tests/scene_image_contract.cpp").read_text(encoding="utf-8")
actor_header = (root / "include/sandhybrid/actor_medium.hpp").read_text(encoding="utf-8")

match = re.search(r"BEE_INITIAL_PACKED\[BEE_FORMATION_COUNT\].*?\((.*?)\);", swarm, re.S)
values = [int(value) for value in re.findall(r"(\d+)u", match.group(1))] if match else []
cpu_match = re.search(r"fix29_bee_formation_packed\{\{(.*?)\}\};", actor_header, re.S)
cpu_values = [int(value) for value in re.findall(r"(\d+)u", cpu_match.group(1))] if cpu_match else []
if len(values) != 60 or len(set(values)) != 60 or values != sorted(values):
    errors.append("formation anchor table must contain exactly 60 unique sorted anchors")
if cpu_values != values:
    errors.append("CPU and GPU formation anchor tables must be byte-identical")
points = [((value & 127) - 64, (value >> 7) - 64) for value in values]
if points:
    if min(x*x + y*y for x, y in points) < 144:
        errors.append("biohazard swarm overlaps the hive")
    if max(x for x, y in points) - min(x for x, y in points) > 52 or max(y for x, y in points) - min(y for x, y in points) > 52:
        errors.append("biohazard swarm is no longer compact")
    upper = sum(y < 0 for x, y in points)
    lower_left = sum(x < 0 and y > 0 for x, y in points)
    lower_right = sum(x > 0 and y > 0 for x, y in points)
    if (upper, lower_left, lower_right) != (20, 20, 20):
        errors.append("swarm must contain exactly 20 bees in each of three lobes")
    point_set = set(points)
    if any(not any(
            (x + dx, y + dy) in point_set
            for dy in (-1, 0, 1) for dx in (-1, 0, 1)
            if dx != 0 or dy != 0)
           for x, y in points):
        errors.append("biohazard outline contains isolated scattered bees")
    remaining = set(points)
    component_sizes = []
    while remaining:
        stack = [remaining.pop()]
        size = 0
        while stack:
            x, y = stack.pop()
            size += 1
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    neighbor = (x + dx, y + dy)
                    if (dx != 0 or dy != 0) and neighbor in remaining:
                        remaining.remove(neighbor)
                        stack.append(neighbor)
        component_sizes.append(size)
    if sorted(component_sizes) != [20, 20, 20]:
        errors.append("biohazard must be three separate 20-bee canonical lobes")
    fingerprint = 2166136261
    for value in values:
        for byte in (value & 0xff, value >> 8):
            fingerprint = ((fingerprint ^ byte) * 16777619) & 0xffffffff
    if fingerprint != 0xbeb650bd:
        errors.append("canonical biohazard formation fingerprint changed")
    for absent, present, label in (
            ((0, -22), (0, -15), "upper"),
            ((-20, 8), (-10, 8), "left"),
            ((20, 8), (10, 8), "right")):
        if absent in point_set or present not in point_set:
            errors.append(f"{label} lobe opening does not face away from the hive")
for token in ("beeBiohazardTargetOffset", "return beeFormationOffset(slot)",
              "if (sourceDistance == 0) return false;",
              "if (boundedSidestep) return true;", "preserveAgentAge", "activeAgentPair"):
    if token not in swarm + move:
        errors.append(f"bee movement contract missing {token!r}")
for token in ("beeUsesPersistentWorldHome(uint width, uint height)", "district << 20u"):
    if token not in swarm:
        errors.append(f"persistent bee-home contract missing {token!r}")
for token in ("BEE_TIMER_BITS = 14u", "BEE_TARGET_NONE = 0x3ffffu",
              "BEE_TARGET_NEWBORN = 0x3fffeu",
              "age >> BEE_TIMER_BITS", "min(timer, BEE_TIMER_MAX)"):
    if token not in swarm:
        errors.append(f"large-World bee target packing missing {token!r}")
corrections = (root / "shaders" / "conservation_corrections.comp").read_text()
for token in ("CORRECTION_BEE_TIMER_BITS = 14u",
              "CORRECTION_BEE_TARGET_NONE = 0x3ffffu",
              "CORRECTION_BEE_TARGET_NEWBORN = 0x3fffeu",
              "correctionNearestBeeTile", "correctionBeeIsForager",
              "timer >= 1200u", "CORRECTION_BEE_AUX_POLLEN",
              "TILE_HAS_FLOWER", "TILE_HAS_HONEY",
              "correctionBeeBesideTarget", "corrected.age = correctionBeePackAge",
              '#include "bee_swarm.glsl"',
              "target == CORRECTION_BEE_TARGET_NEWBORN",
              "correctionBeeExactHome"):
    if token not in corrections:
        errors.append(f"current authored-bee lifecycle correction missing {token!r}")
for source_name, source in (("chemistry", chemistry), ("movement", move)):
    if '#include "bee_swarm_chemistry_legacy.glsl"' not in source:
        errors.append(f"known-good general {source_name} kernel is no longer pinned to the legacy swarm boundary")
for token in ("BEE_FORMATION_COUNT = 100u", "BEE_TARGET_NONE = 0xffffu"):
    if token not in legacy_swarm:
        errors.append(f"frozen general-kernel swarm boundary missing {token!r}")
for token in ('#include "bee_swarm.glsl"', "beeMoveExactHome", "tileQueenPosition",
              "beeBiohazardTargetOffset", "home + ivec2(11, 0)",
              "targetTile == BEE_TARGET_NEWBORN",
              "beeNewbornTransitTarget", "const int outsideLaneX = 23",
              "approachY = settled.y < 0 ? -24 : 16",
              "local.y == approachY && local.x != settled.x",
              "beeMoveDistrictPopulation",
              "beeMoveStationaryPopulation",
              "storedHome / int(TILE_SIZE)",
              "beeMoveBirthOwner",
              "districtBees >= BEE_COLONY_MAX",
              "stationaryBees != districtBees",
              "missingSlot >= BEE_FORMATION_COUNT",
              "beeNewbornHomeTimer(queenPosition)",
              "nextCells[indexOf(position)] = newborn",
              "nextCells[indexOf(position)] = cells[birthOwner]",
              "!newbornTransit && ((slot + pc.step) & 3u) != 0u",
              "tryBeeMove", "atomicCompSwap",
              "atomicAnd(cells[index].aux, ~AUX_MOVED)"):
    if token not in bee_move:
        errors.append(f"dedicated current bee movement missing {token!r}")
for token in ("cells[indexOf(candidate)].material != MAT_QUEEN_BEE",
              "for (int dy = -2; dy <= 7; ++dy)"):
    if token not in bee_move:
        errors.append(f"canonical-cell Queen-home scan missing {token!r}")
for token in ("bee_movement_pipeline", 'create_compute_pipeline("bee_move.comp.spv")',
              "bind_compute(command_buffer, bee_movement_pipeline, current_set)",
              "record_bee_birth_pass", "bee_birth_push.material = 1u"):
    if token not in renderer:
        errors.append(f"dedicated bee movement dispatch missing {token!r}")
if "if ((bee.aux & BEE_AUX_SWARM) != 0u) return false;" not in move:
    errors.append("generic fine movement still contends with dedicated authored-bee movement")
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
for token in ("fixedHiveContent", "Historical Fix29 shell",
              "vec4(comb ? 0.62 : 0.66"):
    if token not in materials:
        errors.append(f"Fix29 visible hive palette missing {token!r}")
for token in ("fixedHiveComposite", "illumination = max(illumination, 0.90)",
              "FIX29_REFERENCE_BODY_ROWS[25]", "FIX29_REFERENCE_TONE_BIT0_ROWS[25]",
              "FIX29_REFERENCE_TONE_BIT1_ROWS[25]", "FIX29_REFERENCE_TONE_BIT2_ROWS[25]",
              "FIX29_REFERENCE_PALETTE[8]",
              "fixedHiveReferenceBody",
              "fixedHiveReferenceAtQueen", "renderPc.selectedScene",
              "Classify it before rejecting", "if (!mapSample &&"):
    if token not in fullscreen:
        errors.append(f"Fix29 shadow-readable presentation missing {token!r}")
for token in ("fix29HiveFibre", "hiveFibre", "hiveFibreColor"):
    if token in fullscreen:
        errors.append(f"Fix29 presentation must not repaint adjacent medium via {token!r}")
for token in (".selected_scene = tool_hive_anchor", "tool_hive_anchor =",
              "material == static_cast<std::uint32_t>(Material::beehive)"):
    if token not in renderer:
        errors.append(f"Fix29 tool/MAP presentation anchor missing {token!r}")
if reset.count("SCENE_") < 18:
    errors.append("not all nine scenes are represented in the brick rebuild")

for token in ("aux_bee_swarm", "pack_bee_metadata", "queen_indices", "bee_indices",
              "fix29_bee_pack_age", "fix29_bee_target_none"):
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

for token in ("aux_bee_swarm", "home_x != queen_x",
              "fix29_bee_target_from_age", "fix29_bee_target_none"):
    if token not in test:
        errors.append(f"scene-image regression test missing {token!r}")
for token in ("beehive_button_tool_map_payload_exact", "download_map_snapshot_cells",
              "record_map_snapshot(command_buffer)", "map_payload_exact",
              "bee_lifecycle_gpu_transitions", "flower_target_acquired",
              "pollen_picked_up", "pollen_deposited", "honey_fed"):
    if token not in renderer:
        errors.append(f"Beehive button/MAP production contract missing {token!r}")
for token in ("MaterialGroup::colony, 3u", "beehive_button_material != Material::beehive"):
    if token not in (root / "tests/ui_layout_contract.cpp").read_text(encoding="utf-8"):
        errors.append(f"Beehive sidebar button hit contract missing {token!r}")

if errors:
    raise SystemExit("Fix34 audit failed:\n  - " + "\n  - ".join(errors))
print("Fix34 audit passed: historical golden no-perch hive plus 60 moving bees in three exact outward-open 20-bee biohazard lobes, and nine 8x8 brick-aligned scenes.")
