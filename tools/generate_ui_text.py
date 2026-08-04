#!/usr/bin/env python3
from pathlib import Path

from material_catalog import (
    BLOCK_MATERIALS,
    GROUPS,
    MATERIALS,
    NO_TEMPERATURE,
    material_group_labels,
    physics_for,
)

ROOT = Path(__file__).resolve().parents[1]
FIXED = [
    "SANDHYBRID", "FPS", "RUNNING", "PAUSED", "BRUSH", "SCENE", "RESET", "BUILD", "MINE",
    "DEBUG", "ALT INSPECT", "MATERIAL", "PHASE", "TEMP", "MASS", "INTEGRITY", "DENSITY",
    "SOFT", "MELT", "BOIL", "VAPOR", "IGNITE", "STRENGTH", "EROSION", "ACID", "ALERT",
    "STRUCTURAL", "LOOSE", "SLEEPING", "ACTIVE", "CANDIDATE", "DAMAGE", "OCCUPANCY",
    "PRESSURE", "CONSERVATION", "CREATED", "DESTROYED", "CONVERTED", "BOUNDARY",
    "F3 DEBUG", "HOLD ALT", "PREV", "NEXT", "TOOLS", "MATERIALS",
    "HP", "O2", "AMMO", "GOLD", "IRON", "LMB TOOL", "RMB DROP", "AL", "CU", "DRILL", "RANGE", "JUMP", "PLASMA", "LOCKED", "READY",
    "SPACE JUMP", "P PAUSE", "LMB USE", "RMB DROP ERASE", "ALT CELL CARD",
    "SAVE", "LOAD", "AIR", "KEYMAP", "WHEEL ZOOM", "N STEP", "R RESET",
    "BRACKETS SCENE", "F5 SAVE PPM", "F9 LOAD PPM",
    "DEBUG STATS", "STEP", "PAIRS", "FINE SWAPS", "MOVED", "CELLS", "SLEEP TILES",
    "BEES", "BEE MOVES", "QUEENS", "HIVE CELLS", "FLOWERS", "HONEY", "ANTS",
    "ANT MOVES", "BEETLES", "BEETLE MOVES", "HABITATS", "SELECTED",
    "STRUCT", "LIQUID", "GAS", "POLLEN", "ACTIVE TILES",
    "CURSOR", "CIRCLE", "SQUARE", "H LINE", "V LINE", "SIZE", "ZOOM", "BEE ACTIVE",
    "MMB/RMB PAN", "F FILL", "0 CAM ZERO",
    "ACTOR MOVES", "PLAYER IMP", "FINE REPAIR", "GAS EXCESS",
    "SLEEP CHUNKS", "ACTIVE CHUNKS", "BULK MOVES", "BULK CELLS", "SKIPPED CELLS",
    "FINE TILES", "BULK TILES", "SETTLED TILES", "DIRTY CHUNKS", "GAS TILES",
    "LIQUID TILES", "ENCLOSED TILES", "BREAKUP TILES", "COLOR KEY", "DAMAGED",
    "STABLE", "BULK MOVED", "FINE ACTIVE", "BULK READY", "SETTLED", "ENCLOSED", "BREAKUP",
    "TILES", "ACTIVE AREAS", "MAP STAR", "CAMERA VIEW", "WASD PAN", "PLAYER WASD", "PLACEMENT",
    "RESIDENT MB", "PAIR TESTS", "SKIPPED", "MOVING", "GAS FLOW", "LIQUID FLOW",
    "STRUCT FAIL", "CONVEYOR", "MACHINE IN", "MACHINE OUT", "VOLCANO LAVA",
    "VOLCANO GAS", "GAS EDGE", "REACTIONS", "HALF WATER", "WET", "ERASER",
    "SCOPE CELLS", "NONEMPTY CELLS", "TOTAL TILES", "UNCLASSIFIED", "TOTAL CHUNKS",
    "IGNITE AIR",
    "INVENTORY", "EDITOR", "SETTINGS", "DESIGNER",
    "SCENE FILES", "SIMULATION", "VIEW INPUT", "PRIMARY TOOLS",
    "CELL WIDTH", "CELL HEIGHT", "CELL TOTAL",
    "TILE WIDTH", "TILE HEIGHT", "TILE TOTAL", "NONE",
]
SCENES = [
    "Sandbox", "Blank", "Volcano", "Waterworks", "Ecosystem", "Engineering lab",
    "Platformer", "Demolition", "Frontier base",
]
PHASES = ["EMPTY", "SOLID", "POWDER", "LIQUID", "GAS", "PLASMA", "SOFT", "MOLTEN", "VAPOR"]


def _packed_words(text: str) -> list[int]:
    encoded = text.upper().encode("ascii", "replace")
    words: list[int] = []
    for offset in range(0, len(encoded), 4):
        word = 0
        for lane, value in enumerate(encoded[offset:offset + 4]):
            word |= value << (lane * 8)
        words.append(word)
    return words


def _append_text_table(storage: list[int], values: list[str]) -> tuple[int, int, int]:
    offsets = [0]
    flattened = ""
    for value in values:
        flattened += value.upper()
        offsets.append(len(flattened))

    offsets_base = len(storage)
    storage.extend(offsets)
    words_base = len(storage)
    storage.extend(_packed_words(flattened))
    return offsets_base, words_base, len(values)


def _append_card_table(storage: list[int]) -> tuple[int, int, int, int]:
    all_lines: list[list[str]] = []
    groups = material_group_labels()
    for index, material in enumerate(MATERIALS):
        name, _, strength, erosion, _, acid, strong, weak, conversions, role, danger = material
        physics = physics_for(name, strength)
        threshold = lambda value: "NONE" if value == NO_TEMPERATURE else f"{value}C"
        all_lines.append([
            f"CATEGORY: {groups[index]}",
            f"STR {strength:03d} ERO {erosion:03d} ACID {acid:03d}",
            f"SOFT {threshold(physics['softening'])} MELT {threshold(physics['melting'])}",
            f"BOIL {threshold(physics['boiling'])} VAP {threshold(physics['vaporization'])}",
            f"IGNITE {threshold(physics['ignition'])}",
            strong,
            weak,
            conversions,
            role,
            danger,
        ])

    offsets = [0]
    flattened = ""
    for lines in all_lines:
        for line in lines:
            flattened += line.upper()
            offsets.append(len(flattened))

    offsets_base = len(storage)
    storage.extend(offsets)
    words_base = len(storage)
    storage.extend(_packed_words(flattened))
    line_count = len(all_lines[0]) if all_lines else 0
    return offsets_base, words_base, len(all_lines), line_count


def _append_group_map(storage: list[int]) -> tuple[int, int, int, int]:
    slots = max(len(group[2]) for group in GROUPS)
    counts_base = len(storage)
    storage.extend(len(group[2]) for group in GROUPS)
    base = len(storage)
    for _, _, material_ids in GROUPS:
        storage.extend(material_ids)
        storage.extend([len(MATERIALS)] * (slots - len(material_ids)))
    return base, counts_base, len(GROUPS), slots


def _cpp_uint_array(values: list[int], columns: int = 12) -> str:
    lines = [
        "#pragma once",
        "",
        "#include <array>",
        "#include <cstdint>",
        "",
        "namespace sandhybrid::ui {",
        "",
        f"inline constexpr std::array<std::uint32_t, {len(values)}> text_storage{{",
    ]
    for offset in range(0, len(values), columns):
        rendered = ", ".join(f"{value}u" for value in values[offset:offset + columns])
        suffix = "," if offset + columns < len(values) else ""
        lines.append(f"    {rendered}{suffix}")
    lines.extend(["};", "", "} // namespace sandhybrid::ui", ""])
    return "\n".join(lines)


def _glsl_table_accessors(prefix: str, offsets_base: int, words_base: int, count: int) -> str:
    upper = prefix.upper()
    return "\n".join([
        f"const uint {upper}_TEXT_OFFSETS_BASE = {offsets_base}u;",
        f"const uint {upper}_TEXT_WORDS_BASE = {words_base}u;",
        f"const uint {upper}_TEXT_COUNT = {count}u;",
        "",
        f"uint {prefix}TextLength(uint id) {{",
        f"    if (id >= {upper}_TEXT_COUNT) return 0u;",
        f"    uint begin = uiTextStorage[{upper}_TEXT_OFFSETS_BASE + id];",
        f"    uint end = uiTextStorage[{upper}_TEXT_OFFSETS_BASE + id + 1u];",
        "    return end - begin;",
        "}",
        "",
        f"uint {prefix}TextChar(uint id, uint index) {{",
        f"    if (id >= {upper}_TEXT_COUNT) return 32u;",
        f"    uint begin = uiTextStorage[{upper}_TEXT_OFFSETS_BASE + id];",
        f"    uint end = uiTextStorage[{upper}_TEXT_OFFSETS_BASE + id + 1u];",
        "    if (index >= end - begin) return 32u;",
        "    uint byteIndex = begin + index;",
        f"    uint word = uiTextStorage[{upper}_TEXT_WORDS_BASE + (byteIndex >> 2u)];",
        "    return (word >> ((byteIndex & 3u) * 8u)) & 255u;",
        "}",
        "",
    ])


def cpp_string(value: str) -> str:
    return '"' + value.replace('\\', '\\\\').replace('"', '\\"') + '"'


def generate_material_header():
    groups = material_group_labels()
    lines = [
        "#pragma once", "", "#include <array>", "#include <cstdint>", "#include <limits>",
        "#include <string_view>", "", "namespace sandhybrid {", "",
        "inline constexpr std::int16_t no_temperature = std::numeric_limits<std::int16_t>::max();", "",
        "enum class Material : std::uint32_t {",
    ]
    for material_id, material in enumerate(MATERIALS):
        lines.append(f"    {material[0]} = {material_id},")
    lines += [
        "    count", "};", "",
        "inline constexpr auto material_count = static_cast<std::uint32_t>(Material::count);", "",
        "enum class MaterialPhase : std::uint8_t {",
        "    empty = 0, solid, powder, liquid, gas, plasma, softened, molten, vapor", "};", "",
        "struct MaterialProfile final {",
        "    std::uint16_t strength{};", "    std::uint16_t erosion_resistance{};",
        "    std::uint16_t density{};", "    std::uint16_t acid_resistance{};",
        "    std::int16_t service_temperature{};", "    std::int16_t softening_point{};",
        "    std::int16_t melting_point{};", "    std::int16_t boiling_point{};",
        "    std::int16_t vaporization_point{};", "    std::int16_t ignition_point{};",
        "    std::uint16_t thermal_conductivity{};", "    MaterialPhase base_phase{};",
        "    std::string_view category{};", "    std::string_view strengths{};",
        "    std::string_view weaknesses{};", "    std::string_view conversions{};",
        "    std::string_view ecological_role{};", "    std::string_view danger{};", "};", "",
        "inline constexpr std::array<std::string_view, material_count> material_names{",
    ]
    for material in MATERIALS:
        lines.append(f"    {cpp_string(material[1])},")
    lines += ["};", "", "inline constexpr std::array<MaterialProfile, material_count> material_profiles{{"]
    phase_names = ["empty", "solid", "powder", "liquid", "gas", "plasma"]
    for index, material in enumerate(MATERIALS):
        name, _, strength, erosion, service, acid, strong, weak, conversions, role, danger = material
        physics = physics_for(name, strength)
        phase = phase_names[physics['base_phase']]
        lines.append(
            f"    {{{strength}u, {erosion}u, {physics['density']}u, {acid}u, {service}, "
            f"{physics['softening']}, {physics['melting']}, {physics['boiling']}, "
            f"{physics['vaporization']}, {physics['ignition']}, {physics['conductivity']}u, "
            f"MaterialPhase::{phase}, {cpp_string(groups[index])}, {cpp_string(strong)}, "
            f"{cpp_string(weak)}, {cpp_string(conversions)}, {cpp_string(role)}, {cpp_string(danger)}}},"
        )
    lines += [
        "}};", "",
        "[[nodiscard]] constexpr const MaterialProfile& material_profile(const Material material) noexcept {",
        "    const auto index = static_cast<std::uint32_t>(material);",
        "    return material_profiles[index < material_profiles.size() ? index : 0u];",
        "}", "",
        "[[nodiscard]] constexpr MaterialPhase phase_at(const Material material, const std::int32_t temperature) noexcept {",
        "    const auto& profile = material_profile(material);",
        "    if (profile.vaporization_point != no_temperature && temperature >= profile.vaporization_point)",
        "        return MaterialPhase::vapor;",
        "    if (profile.melting_point != no_temperature && temperature >= profile.melting_point &&",
        "        (profile.base_phase == MaterialPhase::solid || profile.base_phase == MaterialPhase::powder))",
        "        return MaterialPhase::molten;",
        "    if (profile.softening_point != no_temperature && temperature >= profile.softening_point &&",
        "        (profile.base_phase == MaterialPhase::solid || profile.base_phase == MaterialPhase::powder))",
        "        return MaterialPhase::softened;",
        "    return profile.base_phase;",
        "}", "",
        "enum class MaterialGroup : std::uint32_t {",
    ]
    for group_id, (name, _, _) in enumerate(GROUPS):
        lines.append(f"    {name} = {group_id},")
    lines += [
        "    count", "};", "",
        "inline constexpr auto material_group_count = static_cast<std::uint32_t>(MaterialGroup::count);",
        f"inline constexpr std::uint32_t material_slots_per_group = {max(len(group[2]) for group in GROUPS)}u;", "",
        "inline constexpr std::array<std::uint32_t, material_group_count> material_group_slot_counts{",
    ]
    for _, _, material_ids in GROUPS:
        lines.append(f"    {len(material_ids)}u,")
    lines += [
        "};", "",
        "inline constexpr std::array<std::string_view, material_group_count> material_group_names{",
    ]
    for _, label, _ in GROUPS:
        lines.append(f"    {cpp_string(label)},")
    lines += [
        "};", "",
        "inline constexpr std::array<std::array<Material, material_slots_per_group>, material_group_count> material_groups{{",
    ]
    material_names = [material[0] for material in MATERIALS]
    slots = max(len(group[2]) for group in GROUPS)
    for _, _, material_ids in GROUPS:
        values = [f"Material::{material_names[material_id]}" for material_id in material_ids]
        values.extend(["Material::count"] * (slots - len(values)))
        lines.append(f"    {{{', '.join(values)}}},")
    lines += [
        "}};", "",
        "[[nodiscard]] constexpr std::string_view material_group_name(const MaterialGroup group) noexcept {",
        "    const auto index = static_cast<std::uint32_t>(group);",
        "    return index < material_group_names.size() ? material_group_names[index] : \"Unknown\";",
        "}", "",
        "[[nodiscard]] constexpr std::uint32_t material_group_size(const MaterialGroup group) noexcept {",
        "    const auto index = static_cast<std::uint32_t>(group);",
        "    return index < material_group_slot_counts.size() ? material_group_slot_counts[index] : 0u;",
        "}", "",
        "[[nodiscard]] constexpr Material grouped_material(const MaterialGroup group, const std::uint32_t slot) noexcept {",
        "    const auto group_index = static_cast<std::uint32_t>(group);",
        "    if (group_index >= material_groups.size() || slot >= material_group_size(group)) return Material::count;",
        "    return material_groups[group_index][slot];",
        "}", "",
        "[[nodiscard]] constexpr bool is_block_material(const Material material) noexcept {",
        "    switch (material) {",
    ]
    for name in BLOCK_MATERIALS:
        lines.append(f"    case Material::{name}:")
    lines += [
        "        return true;", "    default:", "        return false;", "    }", "}", "",
        "} // namespace sandhybrid", "",
    ]
    (ROOT / "include/sandhybrid/material.hpp").write_text("\n".join(lines), encoding="utf-8", newline="\n")


def generate_material_ids():
    lines = ["#ifndef SANDHYBRID_MATERIAL_IDS_GLSL", "#define SANDHYBRID_MATERIAL_IDS_GLSL", ""]
    for material_id, material in enumerate(MATERIALS):
        lines.append(f"const uint MAT_{material[0].upper()} = {material_id}u;")
    lines += [f"const uint MATERIAL_COUNT = {len(MATERIALS)}u;", "", "#endif", ""]
    (ROOT / "shaders/material_ids.glsl").write_text("\n".join(lines), encoding="utf-8", newline="\n")


def generate_material_physics():
    phase_constants = ["PHASE_EMPTY", "PHASE_SOLID", "PHASE_POWDER", "PHASE_LIQUID", "PHASE_GAS", "PHASE_PLASMA"]
    fields = [
        ("materialDensity", "density", "uint", "0u"),
        ("materialBasePhase", "base_phase", "uint", "PHASE_SOLID"),
        ("materialSofteningPoint", "softening", "int", "NO_TEMPERATURE"),
        ("materialMeltingPoint", "melting", "int", "NO_TEMPERATURE"),
        ("materialBoilingPoint", "boiling", "int", "NO_TEMPERATURE"),
        ("materialVaporizationPoint", "vaporization", "int", "NO_TEMPERATURE"),
        ("materialIgnitionPoint", "ignition", "int", "NO_TEMPERATURE"),
        ("materialThermalConductivity", "conductivity", "uint", "32u"),
    ]
    lines = [
        "#ifndef SANDHYBRID_MATERIAL_PHYSICS_GLSL", "#define SANDHYBRID_MATERIAL_PHYSICS_GLSL", "",
        "const int NO_TEMPERATURE = 32767;", "const uint PHASE_EMPTY = 0u;", "const uint PHASE_SOLID = 1u;",
        "const uint PHASE_POWDER = 2u;", "const uint PHASE_LIQUID = 3u;", "const uint PHASE_GAS = 4u;",
        "const uint PHASE_PLASMA = 5u;", "const uint PHASE_SOFTENED = 6u;", "const uint PHASE_MOLTEN = 7u;",
        "const uint PHASE_VAPOR = 8u;", "",
    ]
    for function_name, key, return_type, default in fields:
        lines += [f"{return_type} {function_name}(uint material) {{", "    switch (material) {"]
        for material_id, material in enumerate(MATERIALS):
            physics = physics_for(material[0], material[2])
            value = physics[key]
            if key == "base_phase":
                rendered = phase_constants[value]
            elif return_type == "uint":
                rendered = f"{value}u"
            else:
                rendered = str(value)
            lines.append(f"    case {material_id}u: return {rendered};")
        lines += [f"    default: return {default};", "    }", "}", ""]
    lines += ["#endif", ""]
    (ROOT / "shaders/material_physics.glsl").write_text("\n".join(lines), encoding="utf-8", newline="\n")


def generate_ui_text():
    materials = [material[1] for material in MATERIALS]
    groups = [group[1] for group in GROUPS]
    storage: list[int] = []

    tables = {
        "fixed": _append_text_table(storage, FIXED),
        "material": _append_text_table(storage, materials),
        "group": _append_text_table(storage, groups),
        "scene": _append_text_table(storage, SCENES),
        "phase": _append_text_table(storage, PHASES),
    }
    group_base, group_counts_base, group_count, group_slots = _append_group_map(storage)
    card_offsets_base, card_words_base, card_material_count, card_line_count = _append_card_table(storage)

    output = [
        "#ifndef SANDHYBRID_UI_TEXT_GLSL",
        "#define SANDHYBRID_UI_TEXT_GLSL",
        "",
        "layout(std430, binding = 6) readonly buffer UiTextStorageBuffer {",
        "    uint uiTextStorage[];",
        "};",
        "",
    ]
    for prefix, (offsets_base, words_base, count) in tables.items():
        output.append(_glsl_table_accessors(prefix, offsets_base, words_base, count))

    output.extend([
        f"const uint GROUP_MATERIAL_BASE = {group_base}u;",
        f"const uint GROUP_MATERIAL_COUNTS_BASE = {group_counts_base}u;",
        f"const uint GROUP_COUNT = {group_count}u;",
        f"const uint GROUP_MATERIAL_SLOTS = {group_slots}u;",
        "",
        "uint groupMaterialCount(uint group) {",
        "    return group < GROUP_COUNT ? uiTextStorage[GROUP_MATERIAL_COUNTS_BASE + group] : 0u;",
        "}",
        "",
        "uint groupMaterial(uint group, uint slot) {",
        "    if (group >= GROUP_COUNT || slot >= GROUP_MATERIAL_SLOTS) return MATERIAL_COUNT;",
        "    return uiTextStorage[GROUP_MATERIAL_BASE + group * GROUP_MATERIAL_SLOTS + slot];",
        "}",
        "",
        f"const uint CARD_TEXT_OFFSETS_BASE = {card_offsets_base}u;",
        f"const uint CARD_TEXT_WORDS_BASE = {card_words_base}u;",
        f"const uint CARD_MATERIAL_COUNT = {card_material_count}u;",
        f"const uint CARD_LINE_COUNT = {card_line_count}u;",
        "",
        "uint cardTextLength(uint id, uint line) {",
        "    if (id >= CARD_MATERIAL_COUNT || line >= CARD_LINE_COUNT) return 0u;",
        "    uint key = id * CARD_LINE_COUNT + line;",
        "    uint begin = uiTextStorage[CARD_TEXT_OFFSETS_BASE + key];",
        "    uint end = uiTextStorage[CARD_TEXT_OFFSETS_BASE + key + 1u];",
        "    return end - begin;",
        "}",
        "",
        "uint cardTextChar(uint id, uint line, uint index) {",
        "    if (id >= CARD_MATERIAL_COUNT || line >= CARD_LINE_COUNT) return 32u;",
        "    uint key = id * CARD_LINE_COUNT + line;",
        "    uint begin = uiTextStorage[CARD_TEXT_OFFSETS_BASE + key];",
        "    uint end = uiTextStorage[CARD_TEXT_OFFSETS_BASE + key + 1u];",
        "    if (index >= end - begin) return 32u;",
        "    uint byteIndex = begin + index;",
        "    uint word = uiTextStorage[CARD_TEXT_WORDS_BASE + (byteIndex >> 2u)];",
        "    return (word >> ((byteIndex & 3u) * 8u)) & 255u;",
        "}",
        "",
        "#endif",
        "",
    ])

    (ROOT / "shaders/ui_text.glsl").write_text("\n".join(output), encoding="utf-8", newline="\n")
    (ROOT / "include/sandhybrid/ui_text_data.hpp").write_text(
        _cpp_uint_array(storage), encoding="utf-8", newline="\n"
    )


def main():
    generate_material_header()
    generate_material_ids()
    generate_material_physics()
    generate_ui_text()


if __name__ == "__main__":
    main()
