from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    file = Path(path)
    text = file.read_text(encoding="utf-8")
    if old in text:
        file.write_text(text.replace(old, new, 1), encoding="utf-8")
    elif new not in text:
        raise SystemExit(f"{path}: expected source block not found")


replace_once(
    "CMakeLists.txt",
    "project(SandHybrid VERSION 2.5.0 LANGUAGES CXX)",
    "project(SandHybrid VERSION 2.5.1 LANGUAGES CXX)",
)
replace_once(
    "CMakeLists.txt",
    """    include/sandhybrid/simulation_policy.hpp
)""",
    """    include/sandhybrid/simulation_policy.hpp
    include/sandhybrid/world_layout.hpp
)""",
)
replace_once(
    "CMakeLists.txt",
    """    add_executable(sandhybrid_public_api_contract tests/public_api_contract.cpp)
    target_link_libraries(sandhybrid_public_api_contract PRIVATE SandHybrid::SandHybrid)
    target_compile_features(sandhybrid_public_api_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_public_api_contract)
    add_test(NAME sandhybrid_public_api_contract COMMAND sandhybrid_public_api_contract)

    if(SANDHYBRID_BUILD_VULKAN_RUNTIME)""",
    """    add_executable(sandhybrid_public_api_contract tests/public_api_contract.cpp)
    target_link_libraries(sandhybrid_public_api_contract PRIVATE SandHybrid::SandHybrid)
    target_compile_features(sandhybrid_public_api_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_public_api_contract)
    add_test(NAME sandhybrid_public_api_contract COMMAND sandhybrid_public_api_contract)

    add_executable(sandhybrid_world_layout_contract tests/world_layout_contract.cpp)
    target_link_libraries(sandhybrid_world_layout_contract PRIVATE SandHybrid::SandHybrid)
    target_compile_features(sandhybrid_world_layout_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_world_layout_contract)
    add_test(NAME sandhybrid_world_layout_contract COMMAND sandhybrid_world_layout_contract)

    if(SANDHYBRID_BUILD_VULKAN_RUNTIME)""",
)

replace_once(
    "include/sandhybrid/library.hpp",
    """#include \"sandhybrid/section_scheduler.hpp\"
""",
    """#include \"sandhybrid/section_scheduler.hpp\"
#include \"sandhybrid/world_layout.hpp\"
""",
)

replace_once(
    "src/app.cpp",
    """#include \"sandhybrid/app.hpp\"

#include \"sandhybrid/input_routing.hpp\"""",
    """#include \"sandhybrid/app.hpp\"

#include \"sandhybrid/input_routing.hpp\"
#include \"sandhybrid/world_layout.hpp\"""",
)
replace_once(
    "src/app.cpp",
    """    const auto map_origin_x = config.grid_width > pre_expansion_world_width
        ? (config.grid_width - pre_expansion_world_width) / 2u : 0u;
    const auto map_origin_y = config.grid_height > pre_expansion_world_height
        ? config.grid_height - pre_expansion_world_height : 0u;""",
    """    const auto map_origin_x = authored_scene_origin_x(config.grid_width);
    const auto map_origin_y = authored_scene_origin_y(config.grid_height);""",
)

replace_once(
    "src/vulkan_renderer.cpp",
    """#include \"sandhybrid/scene_image.hpp\"
#include \"sandhybrid/ui_layout.hpp\"""",
    """#include \"sandhybrid/scene_image.hpp\"
#include \"sandhybrid/ui_layout.hpp\"
#include \"sandhybrid/world_layout.hpp\"""",
)
replace_once(
    "src/vulkan_renderer.cpp",
    """    return cell;
}

std::filesystem::path executable_directory()""",
    """    return cell;
}

SceneCell make_resident_substrate_cell(
    const Material material,
    const std::uint32_t index) {
    auto cell = make_fill_cell(static_cast<std::uint32_t>(material), index);
    if (resident_substrate_is_structural(material)) {
        cell.aux |= fill_aux_structural | fill_aux_supported;
        cell.aux = (cell.aux & ~fill_aux_state_mask) | 255u;
    }
    return cell;
}

std::filesystem::path executable_directory()""",
)
replace_once(
    "src/vulkan_renderer.cpp",
    """    [[nodiscard]] std::uint32_t authored_map_origin_x() const noexcept {
        return config.grid_width > pre_expansion_world_width
            ? (config.grid_width - pre_expansion_world_width) / 2u : 0u;
    }

    [[nodiscard]] std::uint32_t authored_map_origin_y() const noexcept {
        return config.grid_height > pre_expansion_world_height
            ? config.grid_height - pre_expansion_world_height : 0u;
    }""",
    """    [[nodiscard]] std::uint32_t authored_map_origin_x() const noexcept {
        return authored_scene_origin_x(config.grid_width);
    }

    [[nodiscard]] std::uint32_t authored_map_origin_y() const noexcept {
        return authored_scene_origin_y(config.grid_height);
    }""",
)
replace_once(
    "src/vulkan_renderer.cpp",
    """                static_cast<std::uint32_t>(Material::oxygen),
                static_cast<std::uint32_t>(index));""",
    """                static_cast<std::uint32_t>(Material::atmosphere),
                static_cast<std::uint32_t>(index));""",
)
replace_once(
    "src/vulkan_renderer.cpp",
    """                world_cells[world_index] = cell;
            }
        }
        upload_scene_cells(world_cells);""",
    """                world_cells[world_index] = cell;
            }
        }

        for (std::uint32_t y = 0u; y < config.grid_height; ++y) {
            for (std::uint32_t x = 0u; x < config.grid_width; ++x) {
                const auto material = resident_substrate_material(
                    config.grid_width, config.grid_height, x, y);
                if (material == Material::empty) continue;
                const auto index = static_cast<std::size_t>(y) * config.grid_width + x;
                world_cells[index] = make_resident_substrate_cell(
                    material, static_cast<std::uint32_t>(index));
            }
        }
        upload_scene_cells(world_cells);""",
)
replace_once(
    "src/vulkan_renderer.cpp",
    "Loaded bottom-centered 640x360 scene image: ",
    "Loaded upper-center 640x360 scene image with common subterranean geology: ",
)
replace_once(
    "src/vulkan_renderer.cpp",
    "Saved bottom-centered 640x360 scene image: ",
    "Saved upper-center 640x360 authored scene image: ",
)

replace_once(
    "shaders/reset.comp",
    """ivec2 authoredWorldOrigin() {
    int x = max((int(pc.width) - AUTHORED_WORLD_CELLS.x) / 2, 0);
    int y = max(int(pc.height) - AUTHORED_WORLD_CELLS.y, 0);
    return ivec2(x, y);
}""",
    """ivec2 authoredWorldOrigin() {
    int x = max((int(pc.width) - AUTHORED_WORLD_CELLS.x) / 2, 0);
    return ivec2(x, 0);
}""",
)
replace_once(
    "shaders/reset.comp",
    """// Keep every authored object at its original bottom-centered 640x360 coordinate.
// Only a sparse one-brick floor and one-brick side walls extend to the resident
// horizontal bounds, upgrading old scenes without stretching or duplicating them.
uint residentSceneEnvelopeMaterial(uint scene, ivec2 worldPosition) {
    int sceneTop = max(int(pc.height) - AUTHORED_WORLD_CELLS.y, 0);
    bool floor = worldPosition.y >= int(pc.height) - BRICK_SIZE;
    bool sideWall = worldPosition.y >= sceneTop &&
        (worldPosition.x < BRICK_SIZE || worldPosition.x >= int(pc.width) - BRICK_SIZE);
    return floor || sideWall ? sceneBoundaryMaterial(scene) : MAT_EMPTY;
}""",
    """// Every scene occupies the upper-center 640x360 camera footprint. The three
// resident footprints below it are real geology. A one-brick stone foundation
// separates the scene from the geology; a two-brick lava band spans the bottom
// and is enclosed by one-brick stone cap, bottom shell, and side walls.
uint residentWorldSubstrateMaterial(ivec2 worldPosition) {
    int width = int(pc.width);
    int height = int(pc.height);
    int sceneBottom = min(height, AUTHORED_WORLD_CELLS.y);
    int horizontalShell = min(width, BRICK_SIZE);
    bool sideShell = worldPosition.x < horizontalShell ||
                     worldPosition.x >= width - horizontalShell;

    int foundation = min(sceneBottom, BRICK_SIZE);
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
    ivec2 patch = worldPosition / 64;

    if (zone == 0) {
        if (localY < 96) return MAT_SAND;
        if (localY < 232) return MAT_DIRT;
        if (((patch.x + patch.y * 3) % 11) == 0) return MAT_MUD;
        return MAT_SILT;
    }
    if (zone == 1) {
        if (localY < 104) return MAT_DIRT;
        if (localY < 248) {
            if (((patch.x * 5 + patch.y) % 13) == 0) return MAT_MUD;
            return MAT_SILT;
        }
        return ((patch.x + patch.y) % 7) == 0 ? MAT_STONE : MAT_SAND;
    }
    if (localY < 80)
        return ((patch.x * 3 + patch.y) % 9) == 0 ? MAT_MUD : MAT_SILT;
    if (localY < 176) return MAT_DIRT;
    if (localY < 224 && ((patch.x + patch.y * 2) % 5) == 0) return MAT_SAND;
    return MAT_STONE;
}

bool residentSubstrateStructural(uint material) {
    return material == MAT_STONE || material == MAT_DIRT ||
           material == MAT_SAND || material == MAT_SILT;
}""",
)
replace_once(
    "shaders/reset.comp",
    """    uint material = authored ? materialForScene(scene, local) : MAT_EMPTY;
    uint envelopeMaterial = residentSceneEnvelopeMaterial(scene, p);
    bool envelopeCell = material == MAT_EMPTY && envelopeMaterial != MAT_EMPTY;
    if (envelopeCell) material = envelopeMaterial;
    if (material == MAT_EMPTY) material = MAT_ATMOSPHERE;
    bool structural = envelopeCell || (authored && authoredStructuralCell(scene, local, material));
    bool anchored = envelopeCell || (authored && brickCoordinate(local).y >= worldBrickSize().y - 2);
    Cell cell = structural ? makeStructuralCell(material, true) : makeCell(material);""",
    """    uint material = authored ? materialForScene(scene, local) : MAT_EMPTY;
    uint substrateMaterial = residentWorldSubstrateMaterial(p);
    bool substrateCell = substrateMaterial != MAT_EMPTY;
    if (substrateCell) material = substrateMaterial;
    if (material == MAT_EMPTY) material = MAT_ATMOSPHERE;
    bool structural = substrateCell
        ? residentSubstrateStructural(material)
        : (authored && authoredStructuralCell(scene, local, material));
    Cell cell = structural ? makeStructuralCell(material, true) : makeCell(material);""",
)

replace_once(
    "tools/validate_shader_contracts.py",
    """    camera_policy = (ROOT / \"include/sandhybrid/camera_policy.hpp\").read_text(encoding=\"utf-8\")
    debug_stats_contract""",
    """    camera_policy = (ROOT / \"include/sandhybrid/camera_policy.hpp\").read_text(encoding=\"utf-8\")
    world_layout = (ROOT / \"include/sandhybrid/world_layout.hpp\").read_text(encoding=\"utf-8\")
    debug_stats_contract""",
)
replace_once(
    "tools/validate_shader_contracts.py",
    """    for token in (
        \"int x = max((int(pc.width) - AUTHORED_WORLD_CELLS.x) / 2, 0);\",
        \"int y = max(int(pc.height) - AUTHORED_WORLD_CELLS.y, 0);\",
    ):
        if token not in reset:
            errors.append(f\"bottom-centered authored map contract missing {token!r}\")""",
    """    for token in (
        \"int x = max((int(pc.width) - AUTHORED_WORLD_CELLS.x) / 2, 0);\",
        \"return ivec2(x, 0);\",
        \"residentWorldSubstrateMaterial\",
        \"int lavaThickness = min(lavaRoom, BRICK_SIZE * 2);\",
        \"residentSubstrateStructural\",
        \"bool substrateCell = substrateMaterial != MAT_EMPTY;\",
    ):
        if token not in reset:
            errors.append(f\"upper-center scene/geology shader contract missing {token!r}\")
    for token in (
        \"subterranean_zone_count =\",
        \"resident_world_dimension_scale - 1u\",
        \"resident_world_lava_cells = 16u\",
        \"authored_scene_origin_y\",
        \"return 0u;\",
        \"resident_substrate_material\",
    ):
        if token not in world_layout:
            errors.append(f\"shared resident world-layout contract missing {token!r}\")""",
)
replace_once(
    "tools/validate_shader_contracts.py",
    """    for token in (\"residentSceneEnvelopeMaterial\", \"sceneBoundaryMaterial\", \"Paired compost experiment\", \"aperture so diffusion\", \"Scientific wet-separation station\"):
        if token not in reset:
            errors.append(f\"resident/scientific scene contract missing {token!r}\")""",
    """    for token in (\"residentWorldSubstrateMaterial\", \"residentSubstrateStructural\", \"Paired compost experiment\", \"aperture so diffusion\", \"Scientific wet-separation station\"):
        if token not in reset:
            errors.append(f\"resident/scientific scene contract missing {token!r}\")""",
)
replace_once(
    "tools/validate_shader_contracts.py",
    """        \"layout.placement_tiles\",
    ):
        if token not in app_cpp:""",
    """        \"layout.placement_tiles\",
        \"authored_scene_origin_x(config.grid_width)\",
        \"authored_scene_origin_y(config.grid_height)\",
    ):
        if token not in app_cpp:""",
)
replace_once(
    "tools/validate_shader_contracts.py",
    """    if \"material == Material::atmosphere) cell.aux |= 54u\" not in renderer_cpp:
        errors.append(\"CPU Fill path does not preserve Atmosphere oxygen composition\")""",
    """    if \"material == Material::atmosphere) cell.aux |= 54u\" not in renderer_cpp:
        errors.append(\"CPU Fill path does not preserve Atmosphere oxygen composition\")
    for token in (
        \"authored_scene_origin_y(config.grid_height)\",
        \"resident_substrate_material(\",
        \"make_resident_substrate_cell(\",
        \"Material::atmosphere\",
        \"Loaded upper-center 640x360 scene image with common subterranean geology\",
    ):
        if token not in renderer_cpp:
            errors.append(f\"loaded-scene geology parity contract missing {token!r}\")""",
)

mission = Path("missioncache.md")
mission_text = mission.read_text(encoding="utf-8")
mission_replacements = {
    "| MC-068 | PARTIAL | Camera home/reset | Every scene starts centered on its authored 640x360 footprint at the absolute bottom center. Reset returns to exactly that center and one-map scale without touching simulation state. The resident-window reduction must not halve or offset the start view. Runtime verification remains required. |":
    "| MC-068 | PARTIAL | Camera home/reset | Every scene starts centered on its authored upper-center 640x360 footprint at world Y=0. Reset returns to exactly that footprint and one-map scale without touching simulation state. The three resident map footprints below remain available for geology and exploration. Runtime verification remains required. |",
    "| MC-074 | PARTIAL | Bottom-centered authored objects with resident-width bounds | Every authored object retains its exact coordinate inside the original bottom-centered 640x360 footprint. Scenes never repeat, stretch, or reposition objects. Only a sparse one-brick floor across the resident width and one-brick side walls around the bottom scene band extend to the resident horizontal edges. Actors, hives, machines, metadata, saves, reset, and camera home keep the same offset. Runtime scene cycling remains required. |":
    "| MC-074 | PARTIAL | Upper-center authored objects with common world stack | Every authored object retains its exact local coordinate inside the original 640x360 footprint, horizontally centered at resident-world Y=0. Scenes never repeat or stretch. A common one-brick stone foundation spans the footprint bottom, and three filled subterranean footprints continue below. Actors, hives, machines, metadata, generated scenes, saved PPM loads, reset, and camera home use the same offset. Runtime scene cycling/load acceptance remains required. |",
    "| MC-089 | PARTIAL | Upgrade every scene to resident horizontal bounds | Sandbox, Blank, Volcano, Waterworks, Ecosystem, Engineering, Gold Mine, Demolition, and Frontier Base receive a continuous sparse floor to both resident horizontal edges and side boundary walls without moving any existing object or densely filling unused world space. Scene-specific boundary materials remain structural and stable. |":
    "| MC-089 | PARTIAL | Upgrade every scene to the shared resident geology stack | Sandbox, Blank, Volcano, Waterworks, Ecosystem, Engineering, Gold Mine, Demolition, and Frontier Base use the same resident-width stone foundation, side shell, three sand/soil/silt/mud/stone subterranean zones, stone lava cap, two-brick lava band, and stone bottom shell without moving authored objects. Generated scenes and loaded saved counterparts are identical outside the authored footprint. Runtime acceptance remains required. |",
}
for old, new in mission_replacements.items():
    if old in mission_text:
        mission_text = mission_text.replace(old, new, 1)
    elif new not in mission_text:
        raise SystemExit(f"missioncache.md: expected mission row not found: {old[:80]}")

anchor = "| MC-089 | PARTIAL | Upgrade every scene to the shared resident geology stack | Sandbox, Blank, Volcano, Waterworks, Ecosystem, Engineering, Gold Mine, Demolition, and Frontier Base use the same resident-width stone foundation, side shell, three sand/soil/silt/mud/stone subterranean zones, stone lava cap, two-brick lava band, and stone bottom shell without moving authored objects. Generated scenes and loaded saved counterparts are identical outside the authored footprint. Runtime acceptance remains required. |\n"
new_mission = "| MC-114 | PARTIAL | Common four-footprint vertical world layout | The authored scene and default camera occupy the upper-center 640x360 footprint. Exactly three 640x360 resident footprints below are filled with deterministic layered sand, soil, silt, mud, and stone. A one-brick stone separator sits under the scene; the world bottom contains a continuous two-brick lava band enclosed by one-brick stone cap, bottom, and side shells. Reset and Load produce the same substrate. Static contracts pass; packaged visual and save/load acceptance remain required. |\n"
if "| MC-114 |" not in mission_text:
    if anchor not in mission_text:
        raise SystemExit("missioncache.md: MC-089 insertion anchor not found")
    mission_text = mission_text.replace(anchor, anchor + new_mission, 1)

invariant = "- Every generated scene and every loaded saved counterpart uses the same upper-center authored footprint, three-zone subterranean geology stack, and stone-wrapped two-brick bottom lava band.\n"
permanent_anchor = "# Permanent invariants\n\n"
if invariant not in mission_text:
    if permanent_anchor not in mission_text:
        raise SystemExit("missioncache.md: permanent invariant anchor not found")
    mission_text = mission_text.replace(permanent_anchor, permanent_anchor + invariant, 1)
mission.write_text(mission_text, encoding="utf-8")

Path("tools/apply_scene_geology_stack.py").unlink(missing_ok=True)
