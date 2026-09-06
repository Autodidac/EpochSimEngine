#!/usr/bin/env python3
from pathlib import Path
import ast
import re

root = Path(__file__).resolve().parents[1]
errors = []
swarm = (root / "shaders/bee_swarm.glsl").read_text(encoding="utf-8")
move = (root / "shaders/move.comp").read_text(encoding="utf-8")
chemistry = (root / "shaders/chemistry.comp").read_text(encoding="utf-8")
bee_move = (root / "shaders/bee_move.comp").read_text(encoding="utf-8")
legacy_swarm = (root / "shaders/bee_swarm_chemistry_legacy.glsl").read_text(encoding="utf-8")
reset = (root / "shaders/reset.comp").read_text(encoding="utf-8")
paint = (root / "shaders/paint.comp").read_text(encoding="utf-8")
scene_image = (root / "src/scene_image.cpp").read_text(encoding="utf-8")
renderer = (root / "src/vulkan_renderer.cpp").read_text(encoding="utf-8")
materials = (root / "shaders/materials.glsl").read_text(encoding="utf-8")
fullscreen = (root / "shaders/fullscreen.frag").read_text(encoding="utf-8")
test = (root / "tests/scene_image_contract.cpp").read_text(encoding="utf-8")
actor_header = (root / "include/sandhybrid/actor_medium.hpp").read_text(encoding="utf-8")
colony_header = (root / "include/sandhybrid/bee_colony.hpp").read_text(encoding="utf-8")
colony_test = (root / "tests/bee_colony_contract.cpp").read_text(encoding="utf-8")
colony_acceptance = (root / "src/acceptance_bee_colonies.inl").read_text(encoding="utf-8")
tile_shader = (root / "shaders/tiles.comp").read_text(encoding="utf-8")
tile_defs = (root / "shaders/tiles.glsl").read_text(encoding="utf-8")


def function_body(source, signature):
    """Extract one real function body, not a later matching token/comment."""
    source = re.sub(r"/\*.*?\*/|//[^\n]*", "", source, flags=re.S)
    start = source.find(signature)
    if start < 0:
        raise ValueError(f"missing function {signature!r}")
    start = source.index("{", start) + 1
    depth = 1
    for end in range(start, len(source)):
        depth += (source[end] == "{") - (source[end] == "}")
        if depth == 0:
            return source[start:end]
    raise ValueError(f"unterminated function {signature!r}")


def uint_expression(expression):
    """Compile only the source's tiny uint arithmetic, never arbitrary GLSL."""
    expression = re.sub(r"\b(0x[0-9a-fA-F]+|[0-9]+)u\b", r"\1", expression)
    expression = expression.replace("state.occupancy", "old_occupancy")
    expression = expression.replace("previous.occupancy", "old_occupancy")
    expression = expression.replace("previous.flags", "old_flags")
    tree = ast.parse("(" + expression.strip() + ")", mode="eval")
    permitted = (ast.Expression, ast.Name, ast.Load, ast.Constant, ast.BinOp,
                 ast.UnaryOp, ast.BitAnd, ast.BitOr, ast.LShift, ast.RShift,
                 ast.Invert, ast.Call)
    for node in ast.walk(tree):
        if not isinstance(node, permitted):
            raise ValueError("unsupported colony-index arithmetic")
        if isinstance(node, ast.Call) and (
                not isinstance(node.func, ast.Name) or node.func.id != "min" or
                node.keywords or len(node.args) != 2):
            raise ValueError("unexpected colony-index arithmetic call")
    return compile(tree, "<production colony-index uint expression>", "eval")


# Source-bound pure arithmetic oracle. Mode3 may rebuild ecology locations and
# counts after an edit/load, but it must not pre-classify macro packets, consume
# a failed attempt, advance stability, or touch canonical material/actor cells.
# This validates the actual parsed helper arithmetic and field-write boundary;
# it is not runtime evidence and does not replace the GPU eight-step tests.
try:
    index_body = function_body(tile_shader, "void indexColonyTile(")
    compact_index = re.sub(r"\s+", "", index_body)
    if not compact_index.endswith(
            "TileStateprevious=tiles[index];"
            "previous.flags=(previous.flags&~featureMask)|features;"
            "previous.occupancy=(previous.occupancy&TILE_OCCUPANCY_MASK)|"
            "locations|(bees<<TILE_BEE_COUNT_SHIFT);"
            "tiles[index]=previous;"):
        raise ValueError("colony index must preserve physical TileState fields exactly")
    previous_writes = re.findall(r"previous\.(\w+)\s*(?:[+\-*/|&^]?=|\+\+|--)", index_body)
    if previous_writes != ["flags", "occupancy"] or compact_index.count("tiles[") != 2 or re.search(
            r"\b(?:atomic\w*|pc|debugStats|chunks)\b", index_body):
        raise ValueError("colony index changed its physical write/cadence boundary")
    if "readonly buffer CurrentCells" not in tile_shader or \
            "readonly buffer Chunks" not in tile_shader:
        raise ValueError("tile index inputs must remain read-only")
    allowed_calls = {"tileInside", "ivec2", "indexOf"}
    calls = set(re.findall(r"\b([A-Za-z_]\w*)\s*\(", index_body)) - {"for", "if"}
    if not calls <= allowed_calls:
        raise ValueError(f"unexpected colony-index helper calls: {sorted(calls - allowed_calls)}")
    main_body = function_body(tile_shader, "void main()")
    if not re.search(r"bool editRectangle\s*=\s*pc.activeMode\s*==\s*3u;", main_body):
        raise ValueError("mode3 no longer selects the ecology-only index")
    if not re.search(r"if\s*\(editRectangle\)\s*\{\s*"
                     r"indexColonyTile\(index, origin\);\s*return;\s*\}\s*"
                     r"TileState previous = tiles\[index\];", main_body):
        raise ValueError("mode3 no longer exits before physical tile classification")

    constants = {name: int(value, 0) for name, value in re.findall(
        r"const uint (TILE_\w+) = (0x[0-9a-fA-F]+|[0-9]+)u;", tile_defs)}
    mask_match = re.search(r"const uint featureMask\s*=\s*(.*?);", index_body, re.S)
    required_features = {"TILE_HAS_QUEEN", "TILE_HAS_HIVE", "TILE_HAS_FLOWER",
                         "TILE_HAS_HONEY", "TILE_HAS_BEES",
                         "TILE_HAS_MIGRATING_QUEEN", "TILE_BEE_HAZARD"}
    mask_names = set(re.findall(r"TILE_\w+", mask_match.group(1))) if mask_match else set()
    if mask_names != required_features:
        raise ValueError("colony feature mask includes physical flags or omits ecology")
    env = {"__builtins__": {}, "min": min, **constants}
    feature_mask = eval(uint_expression(mask_match.group(1)), env)
    if feature_mask != 0x7f00 or constants["TILE_OCCUPANCY_MASK"] != 0x7f:
        raise ValueError("colony feature/occupancy bit allocation changed")
    fields = ["TILE_QUEEN_X_SHIFT", "TILE_QUEEN_Y_SHIFT", "TILE_FLOWER_X_SHIFT",
              "TILE_FLOWER_Y_SHIFT", "TILE_HONEY_X_SHIFT", "TILE_HONEY_Y_SHIFT"]
    if [constants[name] for name in fields] != [7, 10, 13, 16, 19, 22] or \
            constants["TILE_BEE_COUNT_SHIFT"] != 25:
        raise ValueError("colony location/count fields overlap physical occupancy")
    occupancy_code = uint_expression(re.search(
        r"previous.occupancy\s*=\s*(.*?);", index_body, re.S).group(1))
    flags_code = uint_expression(re.search(
        r"previous.flags\s*=\s*(.*?);", index_body).group(1))
    for sample in range(128):
        old_flags = (0xffffffff, 0, 1 << (sample % 32), 0xa5a55a5a)[sample % 4]
        features = sample << 8
        locations = sum(((sample + field * 3) & 7) << constants[name]
                        for field, name in enumerate(fields))
        actual_flags = eval(flags_code, env, {
            "old_flags": old_flags, "featureMask": feature_mask, "features": features})
        if actual_flags & ~feature_mask != old_flags & ~feature_mask or \
                actual_flags & feature_mask != features:
            raise ValueError("ecology indexing changes macro/physical flags")
        for occupied in range(128):
            old_occupancy = occupied | 0xffffff80  # Retired ecology bits must be replaced.
            for bees in (0, 1, 32, 60, 64):
                packed = eval(occupancy_code, env, {"old_occupancy": old_occupancy,
                    "locations": locations, "bees": bees})
                if packed != occupied | locations | (bees << 25):
                    raise ValueError("ecology indexing changes occupancy or retains stale fields")
except (ValueError, SyntaxError, KeyError, NameError) as error:
    errors.append(f"ecology-only tile index contract failed: {error}")

match = re.search(r"BEE_INITIAL_PACKED\[BEE_FORMATION_COUNT\].*?\((.*?)\);", swarm, re.S)
values = [int(value) for value in re.findall(r"(\d+)u", match.group(1))] if match else []
cpu_match = re.search(r"fix29_bee_formation_packed\{\{(.*?)\}\};", actor_header, re.S)
cpu_values = [int(value) for value in re.findall(r"(\d+)u", cpu_match.group(1))] if cpu_match else []
if len(values) != 60 or len(set(values)) != 60 or values != sorted(values):
    errors.append("formation anchor table must contain exactly 60 unique sorted anchors")
if cpu_values != values:
    errors.append("CPU and GPU formation anchor tables must be byte-identical")
for token in ("fix29_bee_departure_threshold", "fix29_bee_initial_timer",
              "fix29_bee_forager_activation_window_ticks"):
    if token not in actor_header:
        errors.append(f"CPU plural-forager timing contract missing {token!r}")
for token in ("beeForagerSlot", "beeForagerOrdinal",
              "beeDepartureThreshold", "beeInitialTimer",
              "beeForagerApproachPosition"):
    if token not in swarm:
        errors.append(f"GPU plural-forager contract missing {token!r}")
if "beeInitialTimer(uint(slot))" not in reset:
    errors.append("reset path does not use the shared bee initial timer")
if "beeInitialTimer(uint(colonySlot))" not in paint or \
   "beeInitialTimer(slot)" not in paint:
    errors.append("paint paths do not use the shared bee initial timer")
if "fix29_bee_initial_timer(slot)" not in scene_image:
    errors.append("scene-import path does not use the shared bee initial timer")
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
# District constructors remain byte-compatible. Only formerly unencodable
# persistent sky/gap homes use bit23 plus global16 X10/Y7 and a six-bit slot.
# Require the same discriminator/decoders in both shader include boundaries;
# keeping the legacy general-kernel formation table does not permit stale home
# decoding when it observes a newly placed colony.
for source_name, source in (("current", swarm), ("general-kernel", legacy_swarm)):
    for token in ("BEE_AUX_GLOBAL_HOME = 0x00800000u",
                  "(aux >> 17u) & 63u", "(aux >> 13u) & 127u",
                  "int(aux & 1023u) * 16", "int((aux >> 10u) & 127u) * 16",
                  "homeX | (homeY << 7u) | ((slot & 127u) << 13u)",
                  "(district << 20u)", "uint metadata = BEE_AUX_GLOBAL_HOME",
                  "homeCenter.x / 16, 0, 1023", "homeCenter.y / 16, 0, 127",
                  "((slot & 63u) << 17u)",
                  "BEE_AUTHORED_HOME_SLOT_BIT = 0x80u",
                  "(aux & ~BEE_METADATA_MASK) | metadata"):
        if token not in source:
            errors.append(f"{source_name} compatible/global bee-home contract missing {token!r}")
for token in ("bee_global_home_bit = 0x00800000u", "pack_bee_home_metadata",
              "((slot & 127u) << 13u)", "(address.district << 20u)",
              "((slot & 63u) << 17u)", "((slot & 127u) | 128u) << 15u",
              "bee_home_encodable", "bee_home_from_metadata", "bee_slot_from_metadata",
              "beehive_placement_footprint", "fix29_bee_formation_offset(slot)"):
    if token not in colony_header:
        errors.append(f"shared CPU colony ownership contract missing {token!r}")
for token in ("old_district_payload", "legacy_district_contract",
              "global_home_contract", "legacy_small_world_contract",
              "packed == old_district_payload", "packed == expected",
              "global slot decoded incorrectly", "lifecycle flag altered"):
    if token not in colony_test:
        errors.append(f"CPU colony compatibility regression missing {token!r}")
for token in ("beeNewbornHomeTimer(ivec2 homeCenter, uint metadata)",
              "256u | uint(homeCenter.x & 15) | (uint(homeCenter.y & 15) << 4u)",
              "uint(homeCenter.x & 7) | (uint(homeCenter.y & 7) << 3u)",
              "(packed & 256u) != 0u", "int(packed & 15u)",
              "int((packed >> 4u) & 15u)", "int(packed & 7u)",
              "int((packed >> 3u) & 7u)",
              "beeHomeSearchExtent(uint aux, uint width, uint height)",
              "(aux & BEE_AUX_GLOBAL_HOME) != 0u ? 15 : 7",
              "beeOwnsHome(uint aux, ivec2 queen, uint width, uint height)",
              "uint expected = beePackMetadata(0u, queen, 0u, width, height)",
              "beeHomeCenterFromAux(expected, width, height)"):
    if token not in swarm:
        errors.append(f"exact global/legacy newborn and home search contract missing {token!r}")
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
              "beeOwnsHome(bee.aux, homeCenter, pc.width, pc.height)",
              "if (tileBeeCount(tiles[tile]) == 0u &&",
              # One global worker per tile now returns at the same candidate
              # guard; it no longer continues an over-limit resident scan.
              "!tileHas(tiles[tile], TILE_HAS_BEES | TILE_HAS_HIVE | TILE_HAS_QUEEN)) return;",
              "void runBeePopulation()",
              "atomicAdd(beeHomePopulation[homeIndex], 1u)",
              "beeMoveBirthOwner",
              "districtBees >= BEE_COLONY_MAX",
              "stationaryBees != districtBees",
              "missingSlot >= BEE_FORMATION_COUNT",
              "beeNewbornHomeTimer(queenPosition, newborn.aux)",
              "nextCells[indexOf(position)] = newborn",
              "nextCells[indexOf(position)] = cells[birthOwner]",
              "!newbornTransit && ((slot + pc.step) & 3u) != 0u",
              "beeForagerApproachPosition(flower, slot)",
              "(bee.aux & AUX_MOVED) == 0u",
              "tryBeeMove", "atomicCompSwap",
              "atomicAnd(cells[index].aux, ~AUX_MOVED)"):
    if token not in bee_move:
        errors.append(f"dedicated current bee movement missing {token!r}")
for token in ("cells[indexOf(candidate)].material != MAT_QUEEN_BEE",
              "int extent = beeHomeSearchExtent(bee.aux, pc.width, pc.height)",
              "for (int dy = -2; dy <= extent; ++dy)"):
    if token not in bee_move:
        errors.append(f"canonical-cell Queen-home scan missing {token!r}")
for token in ("int extent = beeHomeSearchExtent(bee.aux, pc.width, pc.height)",
              "for (int dy = -2; dy <= extent; ++dy)"):
    if token not in corrections:
        errors.append(f"current lifecycle global Queen-home scan missing {token!r}")
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
for token in ("beehive_second_colony_preserves_far_foragers", "far_edges_preserved",
              "beehive_repeat_creates_independent_colony", "beehive_legacy_overlap_cleanup",
              "beehive_button_tool_map_payload_exact", "download_map_snapshot_cells",
              "record_map_snapshot(command_buffer)", "map_payload_exact",
              "bee_lifecycle_gpu_transitions", "flower_target_acquired",
              "pollen_picked_up", "pollen_deposited", "honey_fed"):
    if token not in renderer:
        errors.append(f"Beehive button/MAP production contract missing {token!r}")
for token in ('#include "acceptance_bee_colonies.inl"',
              "beehive_placement_fits", "bee_home_encodable",
              "record_loaded_tile_metadata", "cleanup_push.active_mode = 5u"):
    if token not in renderer:
        errors.append(f"independent colony production admission contract missing {token!r}")
if "cleanup(previous_x, previous_y, 4u)" in renderer or \
   "beehive_repeat_replaces_prior_tool_colony" in renderer:
    errors.append("new Beehive placement still enforces retired singleton replacement")
for token in ("paintLegacyColonySignature", "pc.activeMode == 5u",
              "!paintLegacyColonySignature(queen)",
              "paintSnapshot[indexOf(center)].age = admitted",
              "paintSnapshot[indexOf(center)].age == 0u"):
    if token not in paint:
        errors.append(f"atomic non-overlapping colony placement missing {token!r}")
for token in ("beehive_two_independent_editor_colonies",
              "beehive_blocked_overlap_preserves_all_canonical_cells",
              "beehive_no_flower_six_searchers_per_colony",
              "beehive_no_flower_search_returns_without_food_creation",
              "beehive_gap_high_sky_edges_and_off_focus_are_placeable",
              "beehive_multiple_global_homes_schema2_exact_round_trip",
              "route_world_primary_action", "request_beehive_placement",
              "consume_beehive_placement", "record_paint_at_grid",
              "counts[owner] == 60u", "fixed_nonforagers[owner] == 54u",
              "moved_foragers[owner] == 6u", "!slots[owner][slot]",
              "run_acceptance_focused_tick", "std::memcmp",
              "saved && load_world", "upload_actor_state(loaded_owners.actor)"):
    if token not in colony_acceptance:
        errors.append(f"independent colony production regression missing {token!r}")
for token in ("queenVentTransaction", "queenVentInsideDispatch(queen)",
              "queenVentInsideDispatch(mouth)", "respirePackedMedium(breathed)",
              "queenVentCanonicalBody(queen)", "isStructural(donor)",
              "recordConservation(source, corrected)"):
    if token not in corrections:
        errors.append(f"finite canonical Queen ventilation guard missing {token!r}")
for token in (
    "beehive_queen_ventilation_is_one_exact_oxygen_carbon_transaction",
    "beehive_queen_ventilation_refuses_blocked_depleted_hazard_old_and_clipped_owners",
    "beehive_queen_ventilation_exhausts_its_real_oxygen_owner",
    "beehive_queen_ventilation_retains_live_thermal_phase_accounting",
):
    if token not in colony_acceptance:
        errors.append(f"Queen ventilation production regression missing {token!r}")
for token in ("MaterialGroup::colony, 3u", "beehive_button_material != Material::beehive"):
    if token not in (root / "tests/ui_layout_contract.cpp").read_text(encoding="utf-8"):
        errors.append(f"Beehive sidebar button hit contract missing {token!r}")

if errors:
    raise SystemExit("Fix34 audit failed:\n  - " + "\n  - ".join(errors))
print("Fix34 audit passed: historical golden no-perch hive plus 60 moving bees in three exact outward-open 20-bee biohazard lobes, and nine 8x8 brick-aligned scenes.")
