#!/usr/bin/env python3
"""Static cross-contract checks for SandHybrid's C++ and GLSL material model.

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
    "chunks.comp",
    "chemistry.comp",
    "macro_move.comp",
    "move.comp",
    "actor.comp",
    "debug_stats.comp",
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
    header = (ROOT / "include/sandhybrid/ui_text_data.hpp").read_text(encoding="utf-8")
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
    header = (ROOT / "include/sandhybrid/material.hpp").read_text(encoding="utf-8")
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
    for forbidden in ("gold_ore", "iron_ore", "metal", "ally_bot", "enemy_bot", "bot_fabricator"):
        if forbidden in cpp_ids:
            errors.append(f"retired material identifier remains: {forbidden}")

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
    paint_comp = (SHADERS / "paint.comp").read_text(encoding="utf-8")
    move_comp = (SHADERS / "move.comp").read_text(encoding="utf-8")
    chemistry_comp = (SHADERS / "chemistry.comp").read_text(encoding="utf-8")
    reset_comp = (SHADERS / "reset.comp").read_text(encoding="utf-8")
    app_cpp = (ROOT / "src/app.cpp").read_text(encoding="utf-8")
    input_routing_hpp = (ROOT / "include/sandhybrid/input_routing.hpp").read_text(encoding="utf-8")
    window_hpp = (ROOT / "include/sandhybrid/window.hpp").read_text(encoding="utf-8")
    win32_cpp = (ROOT / "src/window_win32.cpp").read_text(encoding="utf-8")
    xcb_cpp = (ROOT / "src/window_xcb.cpp").read_text(encoding="utf-8")
    ui_layout = (ROOT / "include/sandhybrid/ui_layout.hpp").read_text(encoding="utf-8")
    fullscreen = (SHADERS / "fullscreen.frag").read_text(encoding="utf-8")
    tiles_comp = (SHADERS / "tiles.comp").read_text(encoding="utf-8")
    debug_stats_comp = (SHADERS / "debug_stats.comp").read_text(encoding="utf-8")

    if actor_comp.count("ivec2 center = ivec2(state.x, state.y - 4);") != 1:
        errors.append("actor breathing center declaration must be unique")
    for token in (
        "hitMaterial == MAT_BEETLE",
        "state.ammo > 0u",
        "Ammo never blocks ordinary mining",
        "state.shotTimer = plasma ? 14u : 7u",
        "state.drillLevel",
        "state.aluminum",
        "state.copper",
    ):
        if token not in actor_comp:
            errors.append(f"context-sensitive tool contract missing {token!r}")
    for token in ("oxygenVolume > 0u", "fullyChoked", "state.health -= 1u"):
        if token not in actor_comp:
            errors.append(f"closed-system atmosphere contract missing {token!r}")
    for token in ("isDirectPaintLife", "displacePaintGas",
                  "BEE_AUX_SWARM | BEE_AUX_FED"):
        if token not in paint_comp:
            errors.append(f"painted life/oxygen displacement contract missing {token!r}")
    for token in ("isPassableLifeMedium", "insectMediumMoveAllowed",
                  "target.material == MAT_BEE"):
        if token not in move_comp:
            errors.append(f"passable life movement contract missing {token!r}")
    if "Painted and loaded orphan bees self-seed" not in chemistry_comp:
        errors.append("painted bee activation contract missing")
    if "std::jthread" in app_cpp or "stop_token" in app_cpp or "request_stop" in app_cpp:
        errors.append("obsolete implicit jthread ownership remains")
    if "SandHybrid" not in app_cpp:
        errors.append("SandHybrid branding is missing from the application")
    for token in ("make_simulation_viewport", "viewport_left", "viewport_width"):
        if token not in app_cpp + ui_layout + (ROOT / "src/vulkan_renderer.cpp").read_text(encoding="utf-8"):
            errors.append(f"tile-aligned viewport contract missing {token!r}")
    for token in ("viewportLeft", "viewportWidth", "Deliberate letterbox"):
        if token not in fullscreen:
            errors.append(f"fullscreen tile-aligned viewport missing {token!r}")
    for token in (
        "mediumBoundaryEnclosed",
        "fullLiquid && (moving || liquidEnclosed)",
        "fullGas && (moving || gasEnclosed)",
        "mediumBreakup",
        "TILE_MEDIUM_ENCLOSED",
        "TILE_MEDIUM_BREAKUP",
    ):
        if token not in tiles_comp:
            errors.append(f"transient medium-tile contract missing {token!r}")
    for token in (
        "STAT_MACRO_GAS_TILES",
        "STAT_MACRO_LIQUID_TILES",
        "STAT_MEDIUM_ENCLOSED_TILES",
        "STAT_MEDIUM_BREAKUP_TILES",
    ):
        if token not in debug_stats_comp:
            errors.append(f"medium debug counter missing {token!r}")
    for token in ("COLOR KEY", "TILE_MEDIUM_BREAKUP", "debugKeyColor", "textScale"):
        if token not in fullscreen and token != "COLOR KEY":
            errors.append(f"high-contrast debug contract missing {token!r}")
    if "COLOR KEY" not in (ROOT / "tools/generate_ui_text.py").read_text(encoding="utf-8"):
        errors.append("debug color-key text is missing")
    for retired in ("MAT_METAL", "MAT_GOLD_ORE", "MAT_IRON_ORE", "MAT_ALLY_BOT", "MAT_ENEMY_BOT", "MAT_BOT_FABRICATOR"):
        for shader_name in ENTRY_SHADERS:
            if retired in (SHADERS / shader_name).read_text(encoding="utf-8"):
                errors.append(f"{shader_name}: retired identifier remains: {retired}")
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
    for token in (
        "preferred_sidebar_width = 384u",
        "status_height = 126u",
        "group_tabs_height = 112u",
        "palette_items_height = 136u",
        "material_card",
    ):
        if token not in ui_layout:
            errors.append(f"compact sidebar layout contract missing {token!r}")
    for token in (
        "sidebarWidth",
        "groupMaterialCount(renderPc.selectedGroup)",
        "materialPixel(pixel",
        "3, cardMaterial",
        "cardPixel(pixel",
        "2, cardMaterial",
        "2, 60u",
        "2, 61u",
    ):
        if token not in fullscreen:
            errors.append(f"compact sidebar shader contract missing {token!r}")

    renderer_cpp = (ROOT / "src/vulkan_renderer.cpp").read_text(encoding="utf-8")
    macro_move = (SHADERS / "macro_move.comp").read_text(encoding="utf-8")
    tiles_comp = (SHADERS / "tiles.comp").read_text(encoding="utf-8")
    chemistry = (SHADERS / "chemistry.comp").read_text(encoding="utf-8")
    for token in ("TILE_MACRO_MOVABLE", "TILE_FINE_ACTIVE", "TILE_SETTLED_MEDIUM"):
        if token not in macro_move + tiles_comp:
            errors.append(f"macro hierarchy contract missing {token!r}")
    for token in ("MAT_SLUICE_BOX", "inventory.x -= 8u", "inventory.y += 7u", "inventory.z += 1u", "return inventory.z > 0u ? MAT_GOLD : MAT_WATER"):
        if token not in chemistry:
            errors.append(f"sluice conservation contract missing {token!r}")
    cmake_text = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    for token in (
        "Buffer ui_text_buffer{}",
        ".binding = 6",
        ".stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT",
        ".descriptorCount = 16",
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
        "renderer": (renderer, ("renderPc.inspectMode", "renderPc.debugMode", "gasPresentation", "sidebarWidth", "cardPixel")),
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

    # Stable/bulk-ready tile state is metadata only. It must never contain the
    # removed stable-region promotion branch. Explicit machine/habitat structural
    # assignments elsewhere remain valid.
    if "tileHas(tile, TILE_STABLE) && !isStructural(source)" in chemistry:
        errors.append("stable tile metadata still reconstructs loose cells")
    for token in (
        "bool unsupportedStructural = structuralTile && !physicallySupported;",
        "dominantCount < TILE_MIN_COHESIVE_CELLS || unsupportedStructural",
        "bool supported = terrainStable && physicallySupported;",
    ):
        if token not in tiles:
            errors.append(f"settled-terrain release contract missing {token!r}")

    # CPU and GLSL push constants must remain byte-for-byte field compatible.
    push_contracts = {
        "SimulationPush": (
            ["width", "height", "step", "seed", "brush_x", "brush_y", "radius", "material",
             "active_section_x", "active_section_y", "active_mode", "reserved"],
            ["width", "height", "step", "seed", "brushX", "brushY", "radius", "material",
             "activeSectionX", "activeSectionY", "activeMode", "reserved"],
            (SHADERS / "materials.glsl").read_text(encoding="utf-8"),
            "pc",
        ),
        "MovementPush": (
            ["width", "height", "step", "seed", "phase", "parity", "reserved0", "reserved1",
             "active_section_x", "active_section_y", "active_mode", "worker_count"],
            ["width", "height", "step", "seed", "phase", "parity", "reserved0", "reserved1",
             "activeSectionX", "activeSectionY", "activeMode", "workerCount"],
            movement,
            "movePc",
        ),
        "ActorPush": (
            ["width", "height", "step", "seed", "move_x", "move_y", "aim_x", "aim_y",
             "fire", "reset", "scene", "deposit", "simulate", "active_section_x",
             "active_section_y", "active_mode"],
            ["width", "height", "step", "seed", "moveX", "moveY", "aimX", "aimY",
             "fire", "reset", "scene", "deposit", "simulate", "activeSectionX",
             "activeSectionY", "activeMode"],
            (SHADERS / "actor.comp").read_text(encoding="utf-8"),
            "actorPc",
        ),
        "RenderPush": (
            ["grid_width", "grid_height", "window_width", "window_height", "selected_material",
             "material_count", "cursor_x", "cursor_y", "brush_radius", "status_height",
             "palette_height", "group_tabs_height", "material_slots", "frames_per_second", "paused",
             "steps_per_frame", "selected_group", "hovered_group", "hovered_material", "selected_scene",
             "group_count", "scene_count", "mining_mode", "inspect_mode", "debug_mode", "tile_columns",
             "tile_rows", "viewport_left", "viewport_top", "viewport_width", "viewport_height",
             "view_origin_x", "view_origin_y", "view_width", "view_height", "brush_shape",
             "placement_mode", "active_area_count", "active_area_x", "active_area_y",
             "active_scope_mode"],
            ["gridWidth", "gridHeight", "windowWidth", "windowHeight", "selectedMaterial",
             "materialCount", "cursorX", "cursorY", "brushRadius", "statusHeight", "paletteHeight",
             "groupTabsHeight", "materialSlots", "framesPerSecond", "paused", "stepsPerFrame",
             "selectedGroup", "hoveredGroup", "hoveredMaterial", "selectedScene", "groupCount",
             "sceneCount", "miningMode", "inspectMode", "debugMode", "tileColumns", "tileRows", "viewportLeft", "viewportTop", "viewportWidth", "viewportHeight",
             "viewOriginX", "viewOriginY", "viewWidth", "viewHeight", "brushShape",
             "placementMode", "activeAreaCount", "activeAreaX", "activeAreaY",
             "activeScopeMode"],
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
    paint = (SHADERS / "paint.comp").read_text(encoding="utf-8")
    chunks_contract = (SHADERS / "chunks.glsl").read_text(encoding="utf-8")
    app_cpp = (ROOT / "src/app.cpp").read_text(encoding="utf-8")
    section_header = (ROOT / "include/sandhybrid/section_scheduler.hpp").read_text(encoding="utf-8")
    camera_policy = (ROOT / "include/sandhybrid/camera_policy.hpp").read_text(encoding="utf-8")
    debug_stats_contract = (SHADERS / "debug_stats.glsl").read_text(encoding="utf-8")

    for token in (
        "resident_world_dimension_scale = 4u", "logical_world_dimension_scale = 8u",
        "camera_zoom_min = resident_world_dimension_scale / 2u",
        "camera_zoom_default = resident_world_dimension_scale",
        "camera_zoom_max = resident_world_dimension_scale * 8u",
        "camera_view_width(camera_zoom_min) == 1280u", "camera_view_height(camera_zoom_min) == 720u",
        "camera_view_width(camera_zoom_default) == 640u", "camera_view_height(camera_zoom_default) == 360u",
    ):
        if token not in camera_policy: errors.append(f"camera footprint scale contract missing {token!r}")
    stat_word_match = re.search(r"DEBUG_STAT_WORD_COUNT\s*=\s*(\d+)u", debug_stats_contract)
    stat_values = {name: int(value) for name, value in re.findall(r"const uint (STAT_[A-Z0-9_]+)\s*=\s*(\d+)u", debug_stats_contract)}
    if not stat_word_match: errors.append("debug stat word count is missing")
    elif any(value >= int(stat_word_match.group(1)) for value in stat_values.values()): errors.append("debug stat index exceeds the allocated buffer")
    if len(stat_values.values()) != len(set(stat_values.values())): errors.append("debug stat indices overlap")
    if stat_values.get("STAT_MATERIAL_BASE") != 32 or stat_values.get("STAT_MACRO_TILE_MOVES", 0) < 32 + material_count: errors.append("material counters overlap custom activity counters")
    for token in (
        "active_region_width_cells = 640",
        "active_region_height_cells = 360",
    ):
        if token not in section_header:
            errors.append(f"map-area active scope missing C++ contract {token!r}")
    for token in (
        "ACTIVE_REGION_WIDTH_CELLS = 640",
        "ACTIVE_REGION_HEIGHT_CELLS = 360",
    ):
        if token not in chunks_contract:
            errors.append(f"map-area active scope missing GLSL contract {token!r}")
    for token in (
        "bool tileMode = ((pc.material >> 18u) & 1u) != 0u;",
        "Cell cell = isReconstructableMaterial(material) ? makeStructuralCell(material, anchored)",
    ):
        if token not in paint:
            errors.append(f"universal cell/tile placement contract missing {token!r}")
    for token in (
        "int x = max((int(pc.width) - AUTHORED_WORLD_CELLS.x) / 2, 0);",
        "int y = max(int(pc.height) - AUTHORED_WORLD_CELLS.y, 0);",
    ):
        if token not in reset:
            errors.append(f"bottom-centered authored map contract missing {token!r}")
    for token in (
        "edge_band_pixels = 28",
        "route_directional_input(",
        "camera_direction_x = directional_input.camera_x",
        "camera_direction_y = directional_input.camera_y",
        "shared_state.move_x.store(directional_input.player_x",
        "shared_state.move_y.store(directional_input.player_y",
        "layout.placement_cells",
        "layout.placement_tiles",
    ):
        if token not in app_cpp:
            errors.append(f"camera/placement input contract missing {token!r}")
    for token in (
        "struct DirectionalInputRouting final",
        "if (player_present)",
        "return {0, 0, horizontal, vertical};",
        "return {horizontal, vertical, 0, 0};",
    ):
        if token not in input_routing_hpp:
            errors.append(f"context-sensitive directional routing contract missing {token!r}")
    for forbidden in (
        "Camera navigation is universal",
        "W/A/S/D moves both the player and view",
        "camera_direction_x += (input.move_right ? 1 : 0)",
        "camera_direction_y += (input.move_down ? 1 : 0)",
    ):
        if forbidden in app_cpp:
            errors.append(f"player-scene camera duplication remains: {forbidden!r}")
    if "recordConservation(oxygen, carbonDioxide)" not in actor:
        errors.append("actor respiration does not exchange oxygen for equal-volume CO2")
    if "state.y < 112" in actor:
        errors.append("actor breathing regressed to a hard-coded world-height suffocation rule")
    if "state.health -= 1u" not in actor:
        errors.append("actor no longer takes damage after conserved oxygen reaches zero")
    if "ambientAir" in actor:
        errors.append("vacuum is still treated as implicit breathable atmosphere")
    for token in ("fire_tool_pressed", "deposit_resource_pressed"):
        if token not in app_cpp or token not in renderer_cpp:
            errors.append(f"latched player action contract missing {token!r}")
    if "segmentDistance" not in renderer or "actor.hitX" not in renderer or "actor.hitY" not in renderer:
        errors.append("tool beam/impact feedback is missing from the renderer")
    for token in ("authoredStructuralCell", "looseAuthoredCargo", "Large upper reservoir", "real sediment sifter"):
        if token not in reset:
            errors.append(f"authored scene contract missing {token!r}")
    for token in ("residentSceneEnvelopeMaterial", "sceneBoundaryMaterial", "Paired compost experiment", "aperture so diffusion", "Scientific wet-separation station"):
        if token not in reset:
            errors.append(f"resident/scientific scene contract missing {token!r}")
    for token in ("compostFeedReady", "compostWaterReady", "compostPairEvent", "compostIngredientsPresent", "MAT_DIRTY_WATER", "result = makeCell(MAT_FERTILIZER)", "result = makeCell(MAT_WATER)"):
        if token not in chemistry:
            errors.append(f"paired compost contract missing {token!r}")
    if "source.material == MAT_ASH && hasAnyWater" in chemistry:
        errors.append("ash still converts directly to fertilizer from arbitrary water contact")
    if "MAT_GOLD_ORE" in reset or "MAT_IRON_ORE" in reset or "MAT_GOLD_ORE" in actor or "MAT_IRON_ORE" in actor:
        errors.append("ore blocks remain in authored scenes or player mining")
    for token in ("previouslyDense", "tileOccupancy(previous) >= TILE_STABILITY_OCCUPANCY",
                  "dominantCount < TILE_MIN_COHESIVE_CELLS",
                   "structuralTile ? dominantCount"):
        if token not in tiles:
            errors.append(f"bounded structural collapse contract missing {token!r}")
    if "sameNeighbors" not in renderer or "sameNeighbors == 0u" not in renderer:
        errors.append("gas renderer no longer suppresses isolated particle halos")

    for token in ("if (!tileInside(p)) continue;", "supportedStructural > 0u", "STAT_STRUCTURAL_COLLAPSES", "STAT_GAS_EDGE_ACTIVE_TILES"):
        if token not in tiles: errors.append(f"structure/atmosphere regression contract missing {token!r}")
    if re.search(r"for \(int x = 0; x < int\(TILE_SIZE\); \+\+x\) \{\s*for \(int x = 0;", tiles): errors.append("tile support sampling contains a duplicated nested x loop")
    for token in ("activeStructuralProcess(source.material)", "machineAcceptsResource(resourcePosition, controller, resourceCell)", "machineInputRank", "currentInventory", "machineWaterFlowNear", "MAT_SLUICE_BOX", "MAT_SMELTER", "MAT_ASSEMBLER", "ventEmissionKind", "pressure > 96u ? pressure - 96u : 0u", "pressure > 24u ? pressure - 24u : 0u", "STAT_MACHINE_INPUTS", "STAT_MACHINE_OUTPUTS", "STAT_VOLCANO_LAVA_OUTPUTS", "STAT_VOLCANO_GAS_OUTPUTS"):
        if token not in chemistry: errors.append(f"industry/volcano regression contract missing {token!r}")
    for token in ("Functional industrial line", "material = MAT_CONVEYOR", "material = MAT_SLUICE_BOX", "material = MAT_WATER"):
        if token not in reset: errors.append(f"engineering industry scene contract missing {token!r}")
    labels = (ROOT / "tools/generate_ui_text.py").read_text(encoding="utf-8")
    for token in ("RESIDENT MB", "STRUCT FAIL", "CONVEYOR", "MACHINE IN", "MACHINE OUT", "VOLCANO LAVA", "VOLCANO GAS", "GAS EDGE", "REACTIONS"):
        if token not in labels: errors.append(f"activity debug label missing {token!r}")
    for token in ("vec3(1.00, 0.10, 0.72)", "vec3(0.035, 0.10, 0.30)", "debugStats[STAT_STRUCTURAL_COLLAPSES]", "debugStats[STAT_CONVEYOR_MOVES]", "debugStats[STAT_MACHINE_INPUTS]", "debugStats[STAT_MACHINE_OUTPUTS]", "debugStats[STAT_VOLCANO_LAVA_OUTPUTS]", "debugStats[STAT_VOLCANO_GAS_OUTPUTS]"):
        if token not in renderer: errors.append(f"resource-first debug contract missing {token!r}")
    for token in ("vec3(0.58, 0.20, 1.00)", "vec3(0.08, 0.96, 0.28)",
                  "value = min(value, 99999999u)", "separatorYs", "keyColorMap"):
        if token not in renderer: errors.append(f"v2.4.8 debug readability contract missing {token!r}")
    for token in ("layout.atmosphere", "layout.fill", "layout.eraser",
                  "Material::atmosphere", "Material::empty"):
        if token not in app_cpp: errors.append(f"distinct atmosphere/fill/eraser input contract missing {token!r}")
    if "contains(layout.atmosphere" in app_cpp and "contains(layout.fill" in app_cpp:
        atmosphere_handler = app_cpp.split("contains(layout.atmosphere", 1)[1].split(
            "contains(layout.fill", 1)[0]
        if "fill_region" in atmosphere_handler:
            errors.append("Atmosphere control must select balanced air without triggering Fill")
    else:
        errors.append("Atmosphere and Fill handlers are not both present")
    if "contains(layout.fill" in app_cpp and "contains(layout.eraser" in app_cpp:
        fill_handler = app_cpp.split("contains(layout.fill", 1)[1].split(
            "contains(layout.eraser", 1)[0]
        if "fill_region.store(true" not in fill_handler:
            errors.append("Fill control does not trigger the region-fill command")
    if "material == Material::atmosphere) cell.aux |= 54u" not in renderer_cpp:
        errors.append("CPU Fill path does not preserve Atmosphere oxygen composition")
    material_header = (ROOT / 'include/sandhybrid/material.hpp').read_text(encoding='utf-8')
    if '"Soil"' not in material_header:
        errors.append("player-facing Soil material name is missing")
    run_bat = (ROOT / "run.bat").read_text(encoding="utf-8")
    for token in ("bin\\sandhybrid.exe", "%*", "pause"):
        if token not in run_bat: errors.append(f"Windows launcher contract missing {token!r}")
    for token in ("(cell.aux & AUX_WET) != 0u", "return base + 32u",
                  "effectiveDensity(moving) > effectiveDensity(target)"):
        if token not in movement: errors.append(f"wet-material density contract missing {token!r}")
    if "WET" not in labels:
        errors.append("derived wet material card label is missing")
    tile_defs = (SHADERS / "tiles.glsl").read_text(encoding="utf-8")
    macro_move_comp = (SHADERS / "macro_move.comp").read_text(encoding="utf-8")
    for token in (
        "TILE_DESTROYED_CELLS_TO_CRUMBLE = 31u",
        "TILE_MIN_COHESIVE_CELLS =\n    TILE_CELL_COUNT - TILE_DESTROYED_CELLS_TO_CRUMBLE + 1u",
        "TILE_BULK_READY = 0x08000000u",
    ):
        if token not in tile_defs: errors.append(f"48-percent solid-collapse contract missing {token!r}")
    for token in (
        "bool bulkReadySolid = fullRegion && isBlockCapable(dominant)",
        "bool macroMovable = (macroLiquid || macroGas || macroPowder)",
        "if (bulkReadySolid) flags |= TILE_MACRO_SOLID | TILE_BULK_READY;",
    ):
        if token not in tiles_comp: errors.append(f"solid tile classification contract missing {token!r}")
    for token in (
        "tileHas(state, TILE_MACRO_SOLID) || tileHas(state, TILE_BULK_READY)",
        "if (isCellPowder(source))",
    ):
        if token not in macro_move_comp: errors.append(f"solid macro-movement rejection contract missing {token!r}")
    if "isCellPowder(source) || isBlockCapable(source.material)" in macro_move_comp:
        errors.append("block-capable solids can still displace as whole macro tiles")
    for token in ("bool looseSolid = isLooseSolid(moving);",
                  "(!looseSolid && isCellImmovable(moving))"):
        if token not in move_comp: errors.append(f"fine-cell solid crumble contract missing {token!r}")
    if "tileHas(tile, TILE_STABLE) && !isStructural(source)" in chemistry_comp:
        errors.append("stable tile metadata still reconstructs damaged cells")
    if "TILE_BULK_READY) || tileHas(tile, TILE_MACRO_MOVABLE)" not in fullscreen:
        errors.append("bulk-ready debug state is not decoupled from macro movement")
    project_owned_files = [ROOT / "CMakeLists.txt", ROOT / "README.md", ROOT / "AGENTS.md", ROOT / "src/app.cpp", ROOT / "src/main.cpp", ROOT / "src/vulkan_renderer.cpp", ROOT / ".github/workflows/source-export.yml", ROOT / ".github/workflows/v249-ci.yml"]
    forbidden_branding = ("Epoch" + "SimEngine", "Epoch" + "Sand", "epoch" + "_sand", "namespace epoch" + "::sand", "include/epoch" + "/sand")
    for project_file in project_owned_files:
        source_text = project_file.read_text(encoding="utf-8")
        for forbidden in forbidden_branding:
            if forbidden in source_text: errors.append(f"legacy project branding remains in {project_file.relative_to(ROOT)}: {forbidden!r}")

    motion_ecology_contracts = {
        "tiles": (tiles, ("activeContent", "!activeContent", "activeAgent", "activeLoose")),
        "movement": (movement, ("sleepSafe", "beeOrbitTarget", "beeMovementTarget",
                                 "if (targetDistance < sourceDistance) return true;", "boundedSidestep",
                                 "insectMoveAllowed", "MAT_PLANT_STEM")),
        "chemistry": (chemistry, ("flowerDropsSeed", "stemMoisture", "grassFrontier",
                                  "source.material == MAT_PLANT_STEM")),
        "materials": (materials, ("MAT_PLANT_STEM) temperature = 20",
                                  "AUX_PLANT_STEM | 1u")),
    }
    for contract, (text, tokens) in motion_ecology_contracts.items():
        for token in tokens:
            if token not in text:
                errors.append(f"{contract} motion/ecology contract missing {token!r}")
    for forbidden in ("MAT_PLANT_STEM) temperature = 900", "AUX_CHARGED | 72u"):
        if forbidden in materials:
            errors.append(f"plant stem still aliases obsolete projectile state: {forbidden!r}")
    if "bool sleeping = terrainStable" in tiles and "!activeContent" not in tiles:
        errors.append("tile sleeping still ignores dynamic biology and fluids")
    if "if ((bee.aux & AUX_MOVED) != 0u) return false;" not in movement:
        errors.append("bees can move repeatedly in one simulation tick")
    if "if ((insect.aux & AUX_MOVED) != 0u) return false;" not in movement:
        errors.append("insects can move repeatedly in one simulation tick")

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
