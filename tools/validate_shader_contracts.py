#!/usr/bin/env python3
"""Static cross-contract checks for EpochSand's C++ and GLSL material model.

This intentionally complements, rather than replaces, glslc. It catches ID drift,
generated-UI drift, missing includes, delimiter damage, reserved identifiers that
previously broke builds, and accidental duplicate expression fragments.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SHADERS = ROOT / "shaders"
ENTRY_SHADERS = (
    "reset.comp",
    "paint.comp",
    "sunlight.comp",
    "tiles.comp",
    "chemistry.comp",
    "move.comp",
    "actor.comp",
    "fullscreen.vert",
    "fullscreen.frag",
)


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//.*", "", text)


def resolve_includes(path: Path, stack: tuple[Path, ...] = ()) -> str:
    if path in stack:
        raise RuntimeError(f"include cycle: {' -> '.join(map(str, stack + (path,)))}")
    output: list[str] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        match = re.match(r'\s*#include\s+"([^"]+)"', line)
        if match:
            include = SHADERS / match.group(1)
            if not include.is_file():
                raise RuntimeError(f"{path}: missing include {include}")
            output.append(resolve_includes(include, stack + (path,)))
        elif stack and line.lstrip().startswith("#version"):
            continue
        else:
            output.append(line)
    return "\n".join(output)


def extract_uint_function(text: str, name: str) -> str:
    match = re.search(rf"\buint\s+{name}\s*\([^)]*\)\s*\{{", text)
    if not match:
        raise RuntimeError(f"missing generated function {name}")
    opening = match.end() - 1
    depth = 0
    for index in range(opening, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1 : index]
    raise RuntimeError(f"unclosed generated function {name}")




def parse_generated_storage() -> list[int]:
    header = (ROOT / "include/epoch/sand/ui_text_data.hpp").read_text(encoding="utf-8")
    match = re.search(r"text_storage\s*\{(.*?)\};", header, re.S)
    if not match:
        raise RuntimeError("generated UI text storage array not found")
    return [int(value) for value in re.findall(r"(\d+)u", match.group(1))]


def parse_glsl_uint_constant(text: str, name: str) -> int:
    match = re.search(rf"const\s+uint\s+{name}\s*=\s*(\d+)u", text)
    if not match:
        raise RuntimeError(f"missing generated constant {name}")
    return int(match.group(1))


def parse_material_ids() -> dict[str, int]:
    header = (ROOT / "include/epoch/sand/material.hpp").read_text(encoding="utf-8")
    match = re.search(r"enum class Material[^\{]*\{(.*?)\n\};", header, re.S)
    if not match:
        raise RuntimeError("Material enum not found")
    result: dict[str, int] = {}
    value = -1
    for raw_item in match.group(1).split(","):
        item = raw_item.strip()
        if not item:
            continue
        if "=" in item:
            name, explicit = map(str.strip, item.split("=", 1))
            value = int(explicit)
        else:
            name = item
            value += 1
        result[name] = value
    return result


def check_delimiters(name: str, text: str, errors: list[str]) -> None:
    clean = strip_comments(text)
    clean = "\n".join("" if line.lstrip().startswith("#") else line for line in clean.splitlines())
    pairs = {")": "(", "]": "[", "}": "{"}
    stack: list[tuple[str, int]] = []
    line_number = 1
    for character in clean:
        if character == "\n":
            line_number += 1
        if character in "([{":
            stack.append((character, line_number))
        elif character in ")]}":
            if not stack or stack[-1][0] != pairs[character]:
                errors.append(f"{name}:{line_number}: unmatched {character}")
                return
            stack.pop()
    if stack:
        errors.append(f"{name}: unclosed delimiter {stack[-1]}")
    if not re.search(r"\bvoid\s+main\s*\(", clean):
        errors.append(f"{name}: missing main")
    if re.search(r"\breturn\b[^;{}]*\|\|\s*\breturn\b", clean, re.S):
        errors.append(f"{name}: duplicated return expression")
    for reserved in ("resource", "active", "uniform"):
        declaration = rf"\b(?:uint|int|float|bool|Cell|ActorState|ivec\d|uvec\d|vec\d)\s+{reserved}\b"
        if re.search(declaration, clean):
            errors.append(f"{name}: reserved identifier declaration '{reserved}'")


def main() -> int:
    errors: list[str] = []
    for shader_name in ENTRY_SHADERS:
        check_delimiters(shader_name, resolve_includes(SHADERS / shader_name), errors)

    cpp_ids = parse_material_ids()
    material_count = cpp_ids["count"]
    materials_glsl = (SHADERS / "material_ids.glsl").read_text(encoding="utf-8")
    glsl_ids = {
        match.group(1).lower(): int(match.group(2))
        for match in re.finditer(r"const uint MAT_([A-Z0-9_]+)\s*=\s*(\d+)u;", materials_glsl)
    }
    for name, value in cpp_ids.items():
        if name != "count" and glsl_ids.get(name) != value:
            errors.append(f"material ID mismatch {name}: C++={value}, GLSL={glsl_ids.get(name)}")
    count_match = re.search(r"const uint MATERIAL_COUNT\s*=\s*(\d+)u", materials_glsl)
    if not count_match or int(count_match.group(1)) != material_count:
        errors.append("MATERIAL_COUNT does not match Material::count")

    ui_text = (SHADERS / "ui_text.glsl").read_text(encoding="utf-8")
    try:
        ui_storage = parse_generated_storage()
        group_base = parse_glsl_uint_constant(ui_text, "GROUP_MATERIAL_BASE")
        group_counts_base = parse_glsl_uint_constant(ui_text, "GROUP_MATERIAL_COUNTS_BASE")
        group_count = parse_glsl_uint_constant(ui_text, "GROUP_COUNT")
        group_slots = parse_glsl_uint_constant(ui_text, "GROUP_MATERIAL_SLOTS")
        card_offsets_base = parse_glsl_uint_constant(ui_text, "CARD_TEXT_OFFSETS_BASE")
        card_words_base = parse_glsl_uint_constant(ui_text, "CARD_TEXT_WORDS_BASE")
        card_material_count = parse_glsl_uint_constant(ui_text, "CARD_MATERIAL_COUNT")
        card_line_count = parse_glsl_uint_constant(ui_text, "CARD_LINE_COUNT")
    except RuntimeError as error:
        errors.append(str(error))
        ui_storage = []
        group_base = group_counts_base = group_count = group_slots = 0
        card_offsets_base = card_words_base = card_material_count = card_line_count = 0

    group_counts = ui_storage[group_counts_base:group_counts_base + group_count]
    group_storage = ui_storage[group_base:group_base + group_count * group_slots]
    group_values: list[int] = []
    if len(group_counts) != group_count or any(count > group_slots for count in group_counts):
        errors.append("generated group slot counts are invalid")
    if len(group_storage) != group_count * group_slots:
        errors.append("generated padded group map has the wrong storage size")
    else:
        for group, count in enumerate(group_counts):
            row = group_storage[group * group_slots:(group + 1) * group_slots]
            group_values.extend(row[:count])
            if any(value != material_count for value in row[count:]):
                errors.append(f"generated group {group} has non-sentinel padding")
    if any(value >= material_count for value in group_values):
        errors.append("generated group map contains an invalid material ID")
    if len(group_values) != len(set(group_values)):
        errors.append("generated group map contains duplicate material IDs")
    for hidden in (cpp_ids.get("gold_ore"), cpp_ids.get("iron_ore")):
        if hidden in group_values:
            errors.append("legacy ore/concentrate IDs must not appear in the palette")

    group_count_function = extract_uint_function(ui_text, "groupMaterialCount")
    for token in ("uiTextStorage", "GROUP_MATERIAL_COUNTS_BASE"):
        if token not in group_count_function:
            errors.append(f"GPU group count accessor missing {token!r}")

    group_function = extract_uint_function(ui_text, "groupMaterial")
    for token in ("uiTextStorage", "GROUP_MATERIAL_BASE", "GROUP_MATERIAL_SLOTS", "+ slot"):
        if token not in group_function:
            errors.append(f"GPU group map accessor missing {token!r}")

    expected_card_offsets = material_count * 10 + 1
    card_offsets = ui_storage[card_offsets_base:card_offsets_base + expected_card_offsets]
    if card_material_count != material_count or card_line_count != 10:
        errors.append(
            f"card table dimensions are {card_material_count}x{card_line_count}, expected {material_count}x10"
        )
    if len(card_offsets) != expected_card_offsets:
        errors.append(f"card offset table has {len(card_offsets)} entries, expected {expected_card_offsets}")
    elif card_offsets[0] != 0 or any(a > b for a, b in zip(card_offsets, card_offsets[1:])):
        errors.append("card offset table is not monotonic and zero-based")
    elif card_words_base + ((card_offsets[-1] + 3) // 4) > len(ui_storage):
        errors.append("card text words exceed generated UI storage")

    for function_name in ("cardTextLength", "cardTextChar"):
        body = extract_uint_function(ui_text, function_name)
        for token in ("uiTextStorage", "CARD_TEXT_OFFSETS_BASE", "CARD_LINE_COUNT"):
            if token not in body:
                errors.append(f"{function_name} GPU accessor missing {token!r}")
    if "CARD_TEXT_WORDS_BASE + (byteIndex >> 2u)" not in extract_uint_function(ui_text, "cardTextChar"):
        errors.append("cardTextChar does not use packed storage-buffer lookup")

    if "layout(std430, binding = 6) readonly buffer UiTextStorageBuffer" not in ui_text:
        errors.append("UI text is not backed by the read-only binding-6 storage buffer")
    if len(ui_text.splitlines()) > 300:
        errors.append("generated UI text shader regressed to oversized embedded data")

    actor_comp = (SHADERS / "actor.comp").read_text(encoding="utf-8")
    reset_comp = (SHADERS / "reset.comp").read_text(encoding="utf-8")
    app_cpp = (ROOT / "src/app.cpp").read_text(encoding="utf-8")
    window_hpp = (ROOT / "include/epoch/sand/window.hpp").read_text(encoding="utf-8")
    win32_cpp = (ROOT / "src/window_win32.cpp").read_text(encoding="utf-8")
    xcb_cpp = (ROOT / "src/window_xcb.cpp").read_text(encoding="utf-8")
    ui_layout = (ROOT / "include/epoch/sand/ui_layout.hpp").read_text(encoding="utf-8")
    fullscreen = (SHADERS / "fullscreen.frag").read_text(encoding="utf-8")

    if actor_comp.count("ivec2 center = ivec2(state.x, state.y - 4);") != 1:
        errors.append("actor breathing center declaration must be unique")
    for token in (
        "hitMaterial == MAT_ENEMY_BOT",
        "state.ammo > 0u",
        "Ammo never blocks ordinary mining",
        "state.shotTimer = plasma ? 14u : 7u",
    ):
        if token not in actor_comp:
            errors.append(f"context-sensitive tool contract missing {token!r}")
    for token in ("ambientAir", "Atmosphere affects the oxygen meter only"):
        if token not in actor_comp:
            errors.append(f"nonlethal atmosphere contract missing {token!r}")
    if "state.health -=" in actor_comp:
        errors.append("passive atmosphere still drains player health")
    for token in ("bool primary_pressed{}", "bool secondary_pressed{}"):
        if token not in window_hpp:
            errors.append(f"window press-edge contract missing {token!r}")
    for source_name, source in (("Win32", win32_cpp), ("XCB", xcb_cpp)):
        for token in ("primary_pressed = true", "secondary_pressed = true"):
            if token not in source:
                errors.append(f"{source_name} press-edge latch missing {token!r}")
    if "input.primary_pressed" not in app_cpp or "input.secondary_pressed" not in app_cpp:
        errors.append("app does not consume native press-edge latches")
    for token in ("looseAuthoredTerrain", "material == MAT_DIRT", "material == MAT_GRASS"):
        if token not in reset_comp:
            errors.append(f"authored terrain stability contract missing {token!r}")
    for token in ("status_height = 72u", "group_tabs_height = 48u", "palette_items_height = 76u"):
        if token not in ui_layout:
            errors.append(f"large UI layout contract missing {token!r}")
    for token in ("int[5](78, 78, 104, 136, 104)", "ivec2(12, 12), 4, 0u", "hudTop + 78u"):
        if token not in fullscreen:
            errors.append(f"large UI shader contract missing {token!r}")

    renderer_cpp = (ROOT / "src/vulkan_renderer.cpp").read_text(encoding="utf-8")
    cmake_text = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    for token in (
        "Buffer ui_text_buffer{}",
        ".binding = 6",
        ".stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT",
        ".descriptorCount = 14",
        ".dstBinding = 6",
        "ui::text_storage.data()",
    ):
        if token not in renderer_cpp:
            errors.append(f"renderer UI text descriptor contract missing {token!r}")

    shader_suffixes = {".glsl", ".comp", ".frag", ".vert"}
    for path in SHADERS.iterdir():
        if path.suffix not in shader_suffixes:
            continue
        lines = strip_comments(path.read_text(encoding="utf-8")).splitlines()
        for index, line in enumerate(lines[:-1]):
            if line.rstrip().endswith(";") and re.match(
                r"\s*(material|source|result|cell|target|moving)\s*==", lines[index + 1]
            ):
                errors.append(f"{path.name}:{index + 2}: stray expression after semicolon")

    chemistry = (SHADERS / "chemistry.comp").read_text(encoding="utf-8")
    movement = (SHADERS / "move.comp").read_text(encoding="utf-8")
    if "VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT" in renderer_cpp:
        errors.append("movement pipeline still disables driver optimization")
    if 'if(SHADER_FILE STREQUAL "move.comp")' in cmake_text:
        errors.append("move.comp still bypasses offline optimization")
    if "set(SHADER_OPTIMIZATION -O)" not in cmake_text:
        errors.append("shader pipeline no longer performs offline optimization")
    if "adjacentContains" not in movement:
        errors.append("movement shader lost its bounded local signal implementation")
    if re.search(r"\bfor\s*\(", strip_comments(movement)):
        errors.append("movement shader reintroduced driver-expensive loops")
    for forbidden_movement_pattern in (
        "nearestMaterialDistanceSquared",
        "nearestMaterialOffset",
        "atan(",
    ):
        if forbidden_movement_pattern in movement:
            errors.append(
                f"movement shader startup regression: {forbidden_movement_pattern!r} remains"
            )

    tiles = (SHADERS / "tiles.comp").read_text(encoding="utf-8")
    renderer = (SHADERS / "fullscreen.frag").read_text(encoding="utf-8")
    materials = (SHADERS / "materials.glsl").read_text(encoding="utf-8")
    required_rules = (
        "dirtyWaterSeparationReady",
        "harvestConsumesWater",
        "MAT_SILT",
        "MAT_FERTILIZER",
        "MAT_FOOD",
        "MAT_WASTE",
        "applyStructuralHazards",
    )
    for rule in required_rules:
        if rule not in chemistry:
            errors.append(f"chemistry contract missing {rule}")

    architecture_contracts = {
        "movement": (movement, ("isStructural(moving)", "liquidColumnPressure", "TILE_SLEEPING")),
        "tiles": (tiles, ("TILE_STABILITY_OCCUPANCY", "TILE_STABILIZE_TICKS", "TILE_RESTABILIZE_COOLDOWN", "TILE_SLEEPING")),
        "renderer": (renderer, ("renderPc.inspectMode", "renderPc.debugMode", "gasPresentation", "TILE_CANDIDATE")),
        "materials": (materials, ("cellPhase", "materialMeltingPoint", "materialVaporizationPoint", "isReconstructableMaterial", "STRUCTURAL_COLLAPSE_CELLS")),
        "chemistry": (chemistry, ("materialThermalConductivity", "MAT_MAGMA_VENT", "AUX_STRUCTURAL", "recordConservation")),
    }
    for contract, (text, tokens) in architecture_contracts.items():
        for token in tokens:
            if token not in text:
                errors.append(f"{contract} contract missing {token}")

    combined = "\n".join(path.read_text(encoding="utf-8") for path in SHADERS.iterdir()
                             if path.suffix in shader_suffixes)
    for forbidden in ("provenance", "creationSource", "spawnSource", "tileOrigin"):
        if re.search(rf"\b{forbidden}\b", strip_comments(combined), re.I):
            errors.append(f"placement-source physics identifier remains: {forbidden}")

    if "setStateValue(result, 255u)" in chemistry.split("TILE_STABLE", 1)[1].split("if (isStructural(source)", 1)[0]:
        errors.append("stability qualification resets represented damage instead of preserving it")

    # CPU and GLSL push constants must remain byte-for-byte field compatible.
    push_contracts = {
        "SimulationPush": (
            ["width", "height", "step", "seed", "brush_x", "brush_y", "radius", "material"],
            ["width", "height", "step", "seed", "brushX", "brushY", "radius", "material"],
            (SHADERS / "materials.glsl").read_text(encoding="utf-8"),
            "pc",
        ),
        "MovementPush": (
            ["width", "height", "step", "seed", "phase", "parity", "reserved0", "reserved1"],
            ["width", "height", "step", "seed", "phase", "parity", "reserved0", "reserved1"],
            movement,
            "movePc",
        ),
        "ActorPush": (
            ["width", "height", "step", "seed", "move_x", "move_y", "aim_x", "aim_y",
             "fire", "reset", "scene", "deposit", "simulate", "reserved0", "reserved1", "reserved2"],
            ["width", "height", "step", "seed", "moveX", "moveY", "aimX", "aimY",
             "fire", "reset", "scene", "deposit", "simulate", "reserved0", "reserved1", "reserved2"],
            (SHADERS / "actor.comp").read_text(encoding="utf-8"),
            "actorPc",
        ),
        "RenderPush": (
            ["grid_width", "grid_height", "window_width", "window_height", "selected_material",
             "material_count", "cursor_x", "cursor_y", "brush_radius", "status_height",
             "palette_height", "group_tabs_height", "material_slots", "frames_per_second", "paused",
             "steps_per_frame", "selected_group", "hovered_group", "hovered_material", "selected_scene",
             "group_count", "scene_count", "mining_mode", "inspect_mode", "debug_mode", "tile_columns",
             "tile_rows"],
            ["gridWidth", "gridHeight", "windowWidth", "windowHeight", "selectedMaterial",
             "materialCount", "cursorX", "cursorY", "brushRadius", "statusHeight", "paletteHeight",
             "groupTabsHeight", "materialSlots", "framesPerSecond", "paused", "stepsPerFrame",
             "selectedGroup", "hoveredGroup", "hoveredMaterial", "selectedScene", "groupCount",
             "sceneCount", "miningMode", "inspectMode", "debugMode", "tileColumns", "tileRows"],
            renderer,
            "renderPc",
        ),
    }
    for name, (cpp_expected, glsl_expected, glsl_text, instance) in push_contracts.items():
        cpp_match = re.search(rf"struct\s+{name}\s+final\s*\{{(.*?)\n\}};", renderer_cpp, re.S)
        glsl_match = re.search(rf"uniform\s+{name}\s*\{{(.*?)\}}\s*{instance}", glsl_text, re.S)
        if not cpp_match or not glsl_match:
            errors.append(f"missing {name} push layout")
            continue
        cpp_fields = re.findall(r"std::(?:u?int32_t)\s+(\w+)\s*\{", cpp_match.group(1))
        glsl_fields = re.findall(r"(?:uint|int)\s+(\w+)\s*;", glsl_match.group(1))
        if cpp_fields != cpp_expected:
            errors.append(f"{name} C++ fields drifted: {cpp_fields}")
        if glsl_fields != glsl_expected:
            errors.append(f"{name} GLSL fields drifted: {glsl_fields}")

    co2_case = re.search(r"case\s+MAT_CARBON_DIOXIDE:(.*?break;)", materials, re.S)
    if not co2_case or "0.015" not in co2_case.group(1) or "0.030" not in co2_case.group(1):
        errors.append("CO2 no longer uses the requested near-black charcoal presentation")
    hydrogen_case = re.search(r"case\s+MAT_HYDROGEN:(.*?break;)", materials, re.S)
    if not hydrogen_case or "1.00" not in hydrogen_case.group(1) or "0.68" not in hydrogen_case.group(1):
        errors.append("hydrogen no longer uses the requested pink presentation")
    if "if (renderPc.debugMode != 0u)" not in renderer or "local.x == 0 || local.y == 0" not in renderer:
        errors.append("tile grid is not isolated behind debug visualization")
    actor = (SHADERS / "actor.comp").read_text(encoding="utf-8")
    reset = (SHADERS / "reset.comp").read_text(encoding="utf-8")
    app_cpp = (ROOT / "src/app.cpp").read_text(encoding="utf-8")
    if "recordConservation(cells[oxygenIndex], carbonDioxide)" not in actor:
        errors.append("actor respiration silently deletes oxygen instead of converting it")
    if "state.y < 112" in actor:
        errors.append("actor breathing regressed to a hard-coded world-height suffocation rule")
    if "state.health -=" in actor:
        errors.append("actor health is still reduced by passive atmosphere classification")
    for token in ("fire_tool_pressed", "deposit_resource_pressed"):
        if token not in app_cpp or token not in renderer_cpp:
            errors.append(f"latched player action contract missing {token!r}")
    if "segmentDistance" not in renderer or "actor.hitX" not in renderer or "actor.hitY" not in renderer:
        errors.append("tool beam/impact feedback is missing from the renderer")
    for token in ("authoredStructuralCell", "looseAuthoredCargo", "Large upper reservoir", "real sediment sifter"):
        if token not in reset:
            errors.append(f"authored scene contract missing {token!r}")
    if "MAT_GOLD_ORE" in reset or "MAT_IRON_ORE" in reset or "MAT_GOLD_ORE" in actor or "MAT_IRON_ORE" in actor:
        errors.append("ore blocks remain in authored scenes or player mining")
    for token in ("previouslyDense", "previous.occupancy >= TILE_STABILITY_OCCUPANCY",
                  "structuralTile && previouslyDense && structural < TILE_COLLAPSE_OCCUPANCY"):
        if token not in tiles:
            errors.append(f"bounded structural collapse contract missing {token!r}")
    if "sameNeighbors" not in renderer or "sameNeighbors == 0u" not in renderer:
        errors.append("gas renderer no longer suppresses isolated particle halos")

    if errors:
        print("Shader contract validation failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1

    print(
        f"Shader contracts valid: {material_count} materials, "
        f"{len(group_values)} palette slots, {len(ENTRY_SHADERS)} entry shaders."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
