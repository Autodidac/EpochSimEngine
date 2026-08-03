from pathlib import Path
import re


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise SystemExit(f"{label} anchor missing")
    return text.replace(old, new, 1)


def regex_once(text: str, pattern: str, replacement: str, label: str) -> str:
    updated, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if count != 1:
        raise SystemExit(f"{label} pattern matched {count} times")
    return updated


world = Path("include/sandhybrid/world_layout.hpp")
text = world.read_text(encoding="utf-8")
cluster_helpers = r'''[[nodiscard]] constexpr std::int32_t resident_ground_boundary_offset(
    const std::uint32_t x,
    const std::uint32_t salt,
    const std::int32_t amplitude) noexcept {
    const auto coarse = resident_ground_hash(x / 24u, salt) %
        static_cast<std::uint32_t>(amplitude * 2 + 1);
    const auto fine = resident_ground_hash(x / 7u, salt ^ 0x51f2a3b7u) % 7u;
    return static_cast<std::int32_t>(coarse) - amplitude +
           static_cast<std::int32_t>(fine) - 3;
}

[[nodiscard]] constexpr bool resident_ground_rough_edge_cell(
    const std::uint32_t x,
    const std::uint32_t y,
    const std::uint32_t salt) noexcept {
    return (resident_ground_hash(x / 3u, y / 3u + salt) & 15u) == 0u;
}

[[nodiscard]] constexpr Material resident_ground_deposit_material(
    const Material base_material,
    const std::uint32_t x,
    const std::uint32_t y,
    const std::uint32_t depth) noexcept {
    if (!resident_ground_host_material(base_material)) return base_material;

    constexpr std::uint32_t cluster_width = 32u;
    constexpr std::uint32_t cluster_height = 24u;
    const auto cluster_x = x / cluster_width;
    const auto cluster_y = y / cluster_height;
    const auto seed = resident_ground_hash(cluster_x, cluster_y);
    const auto roll = seed & 2047u;

    Material deposit = Material::empty;
    if (depth >= 160u && roll < 8u) deposit = Material::uranium;
    else if (roll < 24u) deposit = Material::aluminum;
    else if (roll < 52u) deposit = Material::copper;
    else if (roll < 132u) deposit = Material::iron_ore;
    if (deposit == Material::empty) return base_material;

    const auto local_x = static_cast<std::int32_t>(x % cluster_width);
    const auto local_y = static_cast<std::int32_t>(y % cluster_height);
    const auto center_x = 10 + static_cast<std::int32_t>((seed >> 11u) % 13u);
    const auto center_y = 7 + static_cast<std::int32_t>((seed >> 16u) % 9u);
    const auto radius_x = 8 + static_cast<std::int32_t>((seed >> 21u) % 6u);
    const auto radius_y = 5 + static_cast<std::int32_t>((seed >> 25u) % 4u);
    const auto dx = local_x - center_x;
    const auto dy = local_y - center_y;
    const auto roughness = static_cast<std::int32_t>(
        resident_ground_hash(x / 4u, y / 4u) & 31u) - 15;
    const auto lhs = dx * dx * radius_y * radius_y +
                     dy * dy * radius_x * radius_x;
    const auto rhs = radius_x * radius_x * radius_y * radius_y +
                     roughness * radius_x;
    return lhs <= rhs ? deposit : base_material;
}'''
text = regex_once(
    text,
    r'\[\[nodiscard\]\] constexpr Material resident_ground_deposit_material\(.*?\n\}\n\n(?=\[\[nodiscard\]\] constexpr Material resident_substrate_material\()',
    cluster_helpers + "\n\n",
    "CPU clustered deposits")

substrate = r'''[[nodiscard]] constexpr Material resident_substrate_material(
    const std::uint32_t world_width,
    const std::uint32_t world_height,
    const std::uint32_t x,
    const std::uint32_t y) noexcept {
    if (x >= world_width || y >= world_height || world_width == 0u || world_height == 0u)
        return Material::empty;

    const auto scene_top = authored_scene_origin_y(world_height);
    const auto scene_bottom = (std::min)(
        world_height, scene_top + pre_expansion_world_height);
    const auto horizontal_shell = (std::min)(world_width, resident_world_shell_cells);
    const bool side_shell = x < horizontal_shell || x >= world_width - horizontal_shell;
    if (y < scene_top) return Material::empty;

    const auto scene_height = scene_bottom - scene_top;
    const auto foundation = (std::min)(scene_height, authored_scene_foundation_cells);
    const auto foundation_start = scene_bottom - foundation;
    if (y < scene_bottom) {
        if (y >= foundation_start || side_shell) return Material::stone;
        return Material::empty;
    }
    if (side_shell) return Material::stone;

    const auto bottom_shell = (std::min)(world_height, resident_world_shell_cells);
    const auto bottom_shell_start = world_height - bottom_shell;
    if (y >= bottom_shell_start) return Material::stone;

    const auto lava_room = bottom_shell_start > scene_bottom
        ? bottom_shell_start - scene_bottom : 0u;
    const auto lava_thickness = (std::min)(lava_room, resident_world_lava_cells);
    const auto lava_start = bottom_shell_start - lava_thickness;
    if (y >= lava_start) return Material::lava;

    const auto cap_room = lava_start > scene_bottom ? lava_start - scene_bottom : 0u;
    const auto cap_thickness = (std::min)(cap_room, resident_world_shell_cells);
    const auto lava_cap_start = lava_start - cap_thickness;
    if (y >= lava_cap_start) return Material::stone;

    const auto depth = y - scene_bottom;
    const auto zone_height = pre_expansion_world_height;
    const auto zone = (std::min)(depth / zone_height, subterranean_zone_count - 1u);
    const auto local_y = static_cast<std::int32_t>(depth % zone_height);
    const auto boundary_a = 106 + resident_ground_boundary_offset(x, zone * 17u + 3u, 18);
    const auto boundary_b = 232 + resident_ground_boundary_offset(x, zone * 19u + 7u, 24);
    const auto boundary_c = 304 + resident_ground_boundary_offset(x, zone * 23u + 11u, 16);

    Material geology = Material::stone;
    if (zone == 0u) {
        geology = local_y < boundary_a ? Material::dirt
            : (local_y < boundary_b ? Material::sand : Material::silt);
    } else if (zone == 1u) {
        geology = local_y < boundary_a ? Material::sand
            : (local_y < boundary_b ? Material::silt : Material::dirt);
    } else {
        geology = local_y < boundary_a ? Material::silt
            : (local_y < boundary_b ? Material::dirt : Material::stone);
    }

    const auto nearest = (std::min)(
        (std::abs)(local_y - boundary_a),
        (std::min)((std::abs)(local_y - boundary_b), (std::abs)(local_y - boundary_c)));
    if (nearest <= 7 && resident_ground_rough_edge_cell(x, y, zone * 31u + 13u)) {
        if (geology == Material::dirt) geology = Material::sand;
        else if (geology == Material::sand) geology = Material::silt;
        else if (geology == Material::silt) geology = Material::dirt;
    }

    const auto mud_seed = resident_ground_hash(x / 24u, y / 18u + zone * 97u);
    const auto mud_local_x = static_cast<std::int32_t>(x % 24u) - 12;
    const auto mud_local_y = static_cast<std::int32_t>(y % 18u) - 9;
    if ((mud_seed & 31u) == 0u &&
        mud_local_x * mud_local_x * 9 + mud_local_y * mud_local_y * 16 < 900) {
        geology = Material::mud;
    }

    return resident_ground_deposit_material(geology, x, y, depth);
}'''
text = regex_once(
    text,
    r'\[\[nodiscard\]\] constexpr Material resident_substrate_material\(.*?\n\}\n\n\} // namespace sandhybrid',
    substrate + "\n\n} // namespace sandhybrid",
    "CPU rough geology")
world.write_text(text, encoding="utf-8")

reset = Path("shaders/reset.comp")
text = reset.read_text(encoding="utf-8")
glsl_helpers = r'''int residentGroundBoundaryOffset(int x, uint salt, int amplitude) {
    uint coarse = residentGroundHash(ivec2(x / 24, int(salt))) % uint(amplitude * 2 + 1);
    uint fine = residentGroundHash(ivec2(x / 7, int(salt ^ 0x51f2a3b7u))) % 7u;
    return int(coarse) - amplitude + int(fine) - 3;
}

bool residentGroundRoughEdgeCell(ivec2 position, uint salt) {
    return (residentGroundHash(ivec2(position.x / 3, position.y / 3 + int(salt))) & 15u) == 0u;
}

uint residentGroundDepositMaterial(uint baseMaterial, ivec2 position, int depth) {
    if (!residentGroundHostMaterial(baseMaterial)) return baseMaterial;
    const ivec2 clusterSize = ivec2(32, 24);
    ivec2 cluster = position / clusterSize;
    uint seed = residentGroundHash(cluster);
    uint roll = seed & 2047u;

    uint deposit = MAT_EMPTY;
    if (depth >= 160 && roll < 8u) deposit = MAT_URANIUM;
    else if (roll < 24u) deposit = MAT_ALUMINUM;
    else if (roll < 52u) deposit = MAT_COPPER;
    else if (roll < 132u) deposit = MAT_IRON_ORE;
    if (deposit == MAT_EMPTY) return baseMaterial;

    ivec2 local = position % clusterSize;
    ivec2 center = ivec2(10 + int((seed >> 11u) % 13u),
                         7 + int((seed >> 16u) % 9u));
    ivec2 radius = ivec2(8 + int((seed >> 21u) % 6u),
                         5 + int((seed >> 25u) % 4u));
    ivec2 delta = local - center;
    int roughness = int(residentGroundHash(position / 4) & 31u) - 15;
    int lhs = delta.x * delta.x * radius.y * radius.y +
              delta.y * delta.y * radius.x * radius.x;
    int rhs = radius.x * radius.x * radius.y * radius.y + roughness * radius.x;
    return lhs <= rhs ? deposit : baseMaterial;
}'''
text = regex_once(
    text,
    r'uint residentGroundDepositMaterial\(.*?\n\}\n\n(?=// Every scene occupies)',
    glsl_helpers + "\n\n",
    "GLSL clustered deposits")

glsl_substrate = r'''uint residentWorldSubstrateMaterial(ivec2 worldPosition) {
    int width = int(pc.width);
    int height = int(pc.height);
    int sceneTop = authoredWorldOrigin().y;
    int sceneBottom = min(height, sceneTop + AUTHORED_WORLD_CELLS.y);
    int horizontalShell = min(width, BRICK_SIZE);
    bool sideShell = worldPosition.x < horizontalShell ||
                     worldPosition.x >= width - horizontalShell;
    if (worldPosition.y < sceneTop) return MAT_EMPTY;

    int foundation = min(sceneBottom - sceneTop, BRICK_SIZE);
    int foundationStart = sceneBottom - foundation;
    if (worldPosition.y < sceneBottom) {
        if (worldPosition.y >= foundationStart || sideShell) return MAT_STONE;
        return MAT_EMPTY;
    }
    if (sideShell) return MAT_STONE;

    int bottomShell = min(height, BRICK_SIZE);
    int bottomShellStart = height - bottomShell;
    if (worldPosition.y >= bottomShellStart) return MAT_STONE;
    int lavaRoom = max(bottomShellStart - sceneBottom, 0);
    int lavaThickness = min(lavaRoom, BRICK_SIZE * 2);
    int lavaStart = bottomShellStart - lavaThickness;
    if (worldPosition.y >= lavaStart) return MAT_LAVA;
    int capRoom = max(lavaStart - sceneBottom, 0);
    int capThickness = min(capRoom, BRICK_SIZE);
    int lavaCapStart = lavaStart - capThickness;
    if (worldPosition.y >= lavaCapStart) return MAT_STONE;

    int depth = worldPosition.y - sceneBottom;
    int zone = min(depth / AUTHORED_WORLD_CELLS.y, 2);
    int localY = depth % AUTHORED_WORLD_CELLS.y;
    int boundaryA = 106 + residentGroundBoundaryOffset(worldPosition.x, uint(zone * 17 + 3), 18);
    int boundaryB = 232 + residentGroundBoundaryOffset(worldPosition.x, uint(zone * 19 + 7), 24);
    int boundaryC = 304 + residentGroundBoundaryOffset(worldPosition.x, uint(zone * 23 + 11), 16);

    uint geology = MAT_STONE;
    if (zone == 0) geology = localY < boundaryA ? MAT_DIRT : (localY < boundaryB ? MAT_SAND : MAT_SILT);
    else if (zone == 1) geology = localY < boundaryA ? MAT_SAND : (localY < boundaryB ? MAT_SILT : MAT_DIRT);
    else geology = localY < boundaryA ? MAT_SILT : (localY < boundaryB ? MAT_DIRT : MAT_STONE);

    int nearest = min(abs(localY - boundaryA), min(abs(localY - boundaryB), abs(localY - boundaryC)));
    if (nearest <= 7 && residentGroundRoughEdgeCell(worldPosition, uint(zone * 31 + 13))) {
        if (geology == MAT_DIRT) geology = MAT_SAND;
        else if (geology == MAT_SAND) geology = MAT_SILT;
        else if (geology == MAT_SILT) geology = MAT_DIRT;
    }

    uint mudSeed = residentGroundHash(ivec2(worldPosition.x / 24, worldPosition.y / 18 + zone * 97));
    ivec2 mudLocal = ivec2(worldPosition.x % 24 - 12, worldPosition.y % 18 - 9);
    if ((mudSeed & 31u) == 0u &&
        mudLocal.x * mudLocal.x * 9 + mudLocal.y * mudLocal.y * 16 < 900) {
        geology = MAT_MUD;
    }
    return residentGroundDepositMaterial(geology, worldPosition, depth);
}'''
text = regex_once(
    text,
    r'uint residentWorldSubstrateMaterial\(.*?\n\}\n\n(?=bool residentSubstrateStructural)',
    glsl_substrate + "\n\n",
    "GLSL rough geology")
reset.write_text(text, encoding="utf-8")

contract = Path("tests/world_layout_contract.cpp")
contract.write_text(r'''#include "sandhybrid/world_layout.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

using namespace sandhybrid;

static_assert(authored_scene_origin_x(resident_world_width) == 960u);
static_assert(authored_scene_origin_y(resident_world_height) == 720u);
static_assert(authored_scene_sky_footprint_rows == 2u);
static_assert(resident_substrate_material(resident_world_width, resident_world_height, 1280u, 0u) == Material::empty);
static_assert(resident_substrate_material(resident_world_width, resident_world_height, 1280u, 1416u) == Material::lava);
static_assert(resident_substrate_material(resident_world_width, resident_world_height, 1280u, 1432u) == Material::stone);

int main() {
    constexpr auto scene_bottom = 1080u;
    constexpr auto geology_end = 1408u;
    std::array<std::size_t, 4> deposits{};
    std::array<std::uint64_t, 2> depth_totals{};
    std::array<std::size_t, 2> depth_counts{};
    std::size_t gold = 0u;
    std::size_t deposit_cells = 0u;
    std::size_t adjacent_deposit_cells = 0u;
    std::array<bool, resident_world_width / 8u> deposit_tiles{};
    std::array<bool, resident_world_width / 8u> mixed_tiles{};
    std::size_t varied_boundaries = 0u;
    int previous_transition = -1;

    for (std::uint32_t x = resident_world_shell_cells;
         x < resident_world_width - resident_world_shell_cells; ++x) {
        int transition = -1;
        for (std::uint32_t y = scene_bottom; y < scene_bottom + 180u; ++y) {
            const auto material = resident_substrate_material(
                resident_world_width, resident_world_height, x, y);
            if (material == Material::dirt) {
                depth_totals[0] += y - scene_bottom;
                ++depth_counts[0];
            } else if (material == Material::sand) {
                depth_totals[1] += y - scene_bottom;
                ++depth_counts[1];
                if (transition < 0) transition = static_cast<int>(y - scene_bottom);
            }
        }
        if (previous_transition >= 0 && transition >= 0 && transition != previous_transition)
            ++varied_boundaries;
        if (transition >= 0) previous_transition = transition;
    }

    for (std::uint32_t y = scene_bottom; y < geology_end; ++y) {
        for (std::uint32_t x = resident_world_shell_cells;
             x < resident_world_width - resident_world_shell_cells; ++x) {
            const auto material = resident_substrate_material(
                resident_world_width, resident_world_height, x, y);
            std::size_t deposit_index = 4u;
            switch (material) {
                case Material::iron_ore: deposit_index = 0u; break;
                case Material::copper: deposit_index = 1u; break;
                case Material::aluminum: deposit_index = 2u; break;
                case Material::uranium: deposit_index = 3u; break;
                case Material::gold: ++gold; break;
                default: break;
            }
            if (deposit_index == 4u) continue;
            ++deposits[deposit_index];
            ++deposit_cells;
            const auto tile_x = x / 8u;
            deposit_tiles[tile_x] = true;
            const auto right = resident_substrate_material(
                resident_world_width, resident_world_height, x + 1u, y);
            const auto down = resident_substrate_material(
                resident_world_width, resident_world_height, x, y + 1u);
            if (right == material || down == material) ++adjacent_deposit_cells;
            if (right != material || down != material) mixed_tiles[tile_x] = true;
        }
    }

    for (const auto count : deposits) if (count == 0u) return 1;
    if (gold != 0u) return 2;
    if (deposits[0] <= deposits[1] || deposits[1] <= deposits[2] ||
        deposits[2] <= deposits[3]) return 3;
    if (depth_counts[0] == 0u || depth_counts[1] == 0u ||
        depth_totals[0] / depth_counts[0] >= depth_totals[1] / depth_counts[1]) return 4;
    if (varied_boundaries < 100u) return 5;
    if (deposit_cells == 0u || adjacent_deposit_cells * 4u < deposit_cells * 3u) return 6;
    return 0;
}
''', encoding="utf-8")

validator = Path("tools/validate_shader_contracts.py")
text = validator.read_text(encoding="utf-8")
text = regex_once(
    text,
    r'    for token in \(\n        "layout\.pause_toggle",.*?errors\.append\(f"camera/pause input contract missing \{token!r\}"\)\n',
    '''    for token in (\n        "layout.pause_toggle",\n        "layout.camera_controls_toggle",\n        "layout.map_toggle",\n        "input.secondary_down",\n        "edge_pan_direction",\n        "input.fill_modifier && primary_pressed",\n        "const auto directional_input = route_directional_input(",\n    ):\n        if token not in app_cpp:\n            errors.append(f"camera/pause input contract missing {token!r}")\n''',
    "validator camera input")
text = text.replace(
    '    if "edge_band_pixels" in app_cpp:\n        errors.append("mouse-edge camera movement remains in app.cpp")\n',
    '    if "edge_pan_direction" not in app_cpp or "input.secondary_down" not in app_cpp:\n        errors.append("right-button edge camera panning is missing")\n')
text = text.replace('for token in ("MMB/RMB PAN", "PLAYER WASD"):',
                    'for token in ("RMB PAN", "PLAYER WASD"):')
text = text.replace('        "2, 60u",\n        "2, 61u",\n',
                    '        "inventoryCount",\n        "renderPc.selectedInventorySlot",\n')
text = text.replace('        ".descriptorCount = 16",', '        ".descriptorCount = 17",')
text = text.replace(
    '             "active_section_y", "active_mode"],',
    '             "active_section_y", "active_mode", "inventory_slot"],', 1)
text = text.replace(
    '             "activeSectionY", "activeMode"],',
    '             "activeSectionY", "activeMode", "inventorySlot"],', 1)
text = text.replace(
    '             "active_scope_mode", "camera_controls"],',
    '             "active_scope_mode", "camera_controls", "map_mode", "camera_origin_x",\n'
    '             "camera_origin_y", "camera_view_width", "camera_view_height",\n'
    '             "selected_inventory_slot"],', 1)
text = text.replace(
    '             "activeScopeMode", "cameraControls"],',
    '             "activeScopeMode", "cameraControls", "mapMode", "cameraOriginX",\n'
    '             "cameraOriginY", "cameraViewWidth", "cameraViewHeight",\n'
    '             "selectedInventorySlot"],', 1)
text = text.replace(
    '    if "if (renderPc.debugMode != 0u)" not in renderer or "local.x == 0 || local.y == 0" not in renderer:\n'
    '        errors.append("tile grid is not isolated behind debug visualization")\n',
    '    if "renderPc.debugMode != 0u || renderPc.mapMode != 0u" not in renderer or "local.x == 0 || local.y == 0" not in renderer:\n'
    '        errors.append("tile grid is not isolated behind debug/map visualization")\n')
text = text.replace('        "if (player_present)",\n',
                    '        "if (map_mode)",\n        "if (player_controls)",\n')
text = text.replace(
    '    for token in ("looseAuthoredTerrain", "material == MAT_DIRT", "material == MAT_GRASS"):',
    '    for token in ("looseAuthoredTerrain", "material == MAT_DIRT", "material == MAT_GRASS", "residentGroundDepositMaterial"):')
text = text.replace(
    '        "roll < 24u) return MAT_IRON_ORE",\n        "roll < 32u) return MAT_COPPER",\n        "roll < 38u) return MAT_ALUMINUM",\n        "depth >= 160 && roll < 41u) return MAT_URANIUM",',
    '        "clusterSize = ivec2(32, 24)",\n        "roll < 132u) deposit = MAT_IRON_ORE",\n        "roll < 52u) deposit = MAT_COPPER",\n        "roll < 24u) deposit = MAT_ALUMINUM",\n        "depth >= 160 && roll < 8u) deposit = MAT_URANIUM",')
text = text.replace(
    '        "roll < 24u) return Material::iron_ore",\n        "roll < 32u) return Material::copper",\n        "roll < 38u) return Material::aluminum",\n        "depth >= 160u && roll < 41u) return Material::uranium",',
    '        "cluster_width = 32u",\n        "roll < 132u) deposit = Material::iron_ore",\n        "roll < 52u) deposit = Material::copper",\n        "roll < 24u) deposit = Material::aluminum",\n        "depth >= 160u && roll < 8u) deposit = Material::uranium",')
validator.write_text(text, encoding="utf-8")

cache = Path("missioncache.md")
text = cache.read_text(encoding="utf-8")
lines = text.splitlines()
for index, line in enumerate(lines):
    if line.startswith("| MC-118 |") and "clustered-vein evidence" not in line:
        lines[index] = line[:-2] + " v2.5.6 clustered-vein evidence: minerals now form deterministic 32x24-host-region elliptical clumps with rough low-frequency edges, sharply reducing mixed/broken terrain tiles while preserving Iron Ore > Copper > Aluminum > deep Uranium abundance and excluding Gold. |"
        break
blueprint = "| MC-121 | OPEN | Editable selection blueprints and copy/paste | A rectangular world selection can be copied to an in-memory clipboard and saved as a versioned, human-editable SandHybrid blueprint. The blueprint preserves canonical material IDs, cell state, wet/half-water state, structural metadata, temperature, atmosphere composition, machine direction/inventory, and optional actor components without copying transient scheduler flags. A movable preview supports paste, rotate, mirror, overwrite/merge policy, and cancel before one atomic commit. Paste wakes only affected sections, conserves represented matter under the selected policy, supports undo, and can be reused across scenes and future releases. |"
if not any(line.startswith("| MC-121 |") for line in lines):
    for index, line in enumerate(lines):
        if line.startswith("| MC-120 |"):
            lines.insert(index + 1, blueprint)
            break
    else:
        raise SystemExit("MC-120 row missing for blueprint insertion")
if not any("Mineral deposits are clustered" in line for line in lines):
    invariant_index = next((i for i, line in enumerate(lines) if line.startswith("- Player/actor disturbance")), None)
    if invariant_index is None:
        raise SystemExit("invariant insertion anchor missing")
    lines.insert(invariant_index, "- Mineral deposits are clustered into deterministic contiguous veins with rough edges; isolated cell-scale ore confetti may not fragment otherwise stable terrain tiles.")
cache.write_text("\n".join(lines) + "\n", encoding="utf-8")

changelog = Path("CHANGELOG.md")
text = changelog.read_text(encoding="utf-8")
needle = "## 2.5.6\n\n"
if needle not in text:
    raise SystemExit("v2.5.6 changelog section missing")
bullet = "- Replaced scattered per-cell mineral noise with dense deterministic ore veins that preserve rough natural edges while greatly reducing mixed/broken terrain tiles.\n- Added MC-121 for a versioned editable selection-blueprint copy/paste system.\n"
if "dense deterministic ore veins" not in text:
    text = text.replace(needle, needle + bullet, 1)
changelog.write_text(text, encoding="utf-8")
