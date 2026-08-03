from pathlib import Path
import re


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise SystemExit(f"{label} anchor missing")
    return text.replace(old, new, 1)


def regex_once(text: str, pattern: str, replacement: str, label: str) -> str:
    updated, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if count != 1:
        raise SystemExit(f"{label} matched {count} times")
    return updated


# Resident geology is authored ground. Embedded mud and ore must retain the
# same stable structural metadata as their host until directly disturbed.
path = Path("include/sandhybrid/world_layout.hpp")
text = path.read_text(encoding="utf-8")
text = replace_once(
    text,
    "[[nodiscard]] constexpr bool resident_substrate_is_structural(\n"
    "    const Material material) noexcept {\n"
    "    return material == Material::stone || material == Material::dirt ||\n"
    "           material == Material::sand || material == Material::silt;\n"
    "}",
    "[[nodiscard]] constexpr bool resident_substrate_is_structural(\n"
    "    const Material material) noexcept {\n"
    "    return resident_ground_host_material(material) ||\n"
    "           material == Material::iron_ore || material == Material::copper ||\n"
    "           material == Material::aluminum || material == Material::uranium;\n"
    "}",
    "resident substrate structural materials",
)
# The helper is declared before resident_ground_host_material. Keep constexpr
# ordering valid by moving the structural helper below the host helper.
structural = re.search(
    r"\[\[nodiscard\]\] constexpr bool resident_substrate_is_structural\(.*?\n\}\n\n",
    text,
    re.S,
)
if structural is None:
    raise SystemExit("structural helper extraction failed")
structural_text = structural.group(0)
text = text[:structural.start()] + text[structural.end():]
host_end = text.index("}\n\n", text.index("[[nodiscard]] constexpr bool resident_ground_host_material")) + 3
text = text[:host_end] + structural_text + text[host_end:]
path.write_text(text, encoding="utf-8")

path = Path("shaders/reset.comp")
text = path.read_text(encoding="utf-8")
text = replace_once(
    text,
    "bool residentSubstrateStructural(uint material) {\n"
    "    return material == MAT_STONE || material == MAT_DIRT ||\n"
    "           material == MAT_SAND || material == MAT_SILT;\n"
    "}",
    "bool residentSubstrateStructural(uint material) {\n"
    "    return residentGroundHostMaterial(material) ||\n"
    "           material == MAT_IRON_ORE || material == MAT_COPPER ||\n"
    "           material == MAT_ALUMINUM || material == MAT_URANIUM;\n"
    "}",
    "GLSL resident substrate structural materials",
)
path.write_text(text, encoding="utf-8")

# Rebuild the sidebar into explicit Scene, Simulation, View, and Tool rows.
path = Path("include/sandhybrid/ui_layout.hpp")
text = path.read_text(encoding="utf-8")
text = text.replace("inline constexpr std::uint32_t status_height = 126u;",
                    "inline constexpr std::uint32_t status_height = 208u;")
text = text.replace("inline constexpr std::uint32_t group_tabs_height = 112u;",
                    "inline constexpr std::uint32_t group_tabs_height = 96u;")
text = text.replace("inline constexpr std::uint32_t palette_items_height = 136u;",
                    "inline constexpr std::uint32_t palette_items_height = 124u;")
text = text.replace("inline constexpr std::uint32_t eraser_height = 24u;",
                    "inline constexpr std::uint32_t eraser_height = 34u;")
text = text.replace("inline constexpr std::uint32_t keymap_height = 126u;",
                    "inline constexpr std::uint32_t keymap_height = 98u;")
text = text.replace("inline constexpr std::uint32_t cursor_editor_height = 120u;",
                    "inline constexpr std::uint32_t cursor_editor_height = 112u;")
new_layout = r'''[[nodiscard]] inline Layout make_layout(std::uint32_t width, std::uint32_t height) noexcept {
    const auto screen_width = (std::max)(width, 1u);
    const auto screen_height = (std::max)(height, 1u);
    const auto requested = (std::max)(minimum_sidebar_width, screen_width / 3u);
    const auto sidebar = screen_width > minimum_sidebar_width + 160u
        ? (std::min)(preferred_sidebar_width, requested)
        : (std::min)(screen_width, minimum_sidebar_width);
    const auto simulation_width = screen_width > sidebar ? screen_width - sidebar : 1u;
    const float left = float(simulation_width);
    const float side = float(screen_width - simulation_width);
    const float content_left = left + margin;
    const float content_width = (std::max)(1.0f, side - margin * 2.0f);

    Layout layout{
        .status = {{left, 0.0f}, {side, float(status_height)}},
        .simulation = {{0.0f, 0.0f}, {float(simulation_width), float(screen_height)}},
        .group_tabs = {{content_left, float(status_height) + margin},
                       {content_width, float(group_tabs_height)}},
        .palette = {{content_left, float(status_height + group_tabs_height) + margin + gap},
                    {content_width, float(palette_items_height)}},
    };

    constexpr float row_left_padding = 8.0f;
    constexpr float row_right_padding = 8.0f;
    constexpr float row_gap = 4.0f;
    const float row_left = left + row_left_padding;
    const float row_width = (std::max)(1.0f, side - row_left_padding - row_right_padding);

    // Scene files/navigation: PREV, NEXT, SAVE, LOAD.
    const float scene_width = (std::max)(1.0f, (row_width - row_gap * 3.0f) / 4.0f);
    layout.previous_scene = {{row_left, 70.0f}, {scene_width, 28.0f}};
    layout.next_scene = {{row_left + (scene_width + row_gap), 70.0f}, {scene_width, 28.0f}};
    layout.save_scene = {{row_left + (scene_width + row_gap) * 2.0f, 70.0f}, {scene_width, 28.0f}};
    layout.load_scene = {{row_left + (scene_width + row_gap) * 3.0f, 70.0f},
                         {row_width - scene_width * 3.0f - row_gap * 3.0f, 28.0f}};

    // Simulation actions are intentionally paired and equally prominent.
    const float action_width = (std::max)(1.0f, (row_width - row_gap) * 0.5f);
    layout.reset_scene = {{row_left, 102.0f}, {action_width, 30.0f}};
    layout.pause_toggle = {{row_left + action_width + row_gap, 102.0f},
                           {row_width - action_width - row_gap, 30.0f}};

    // View/input modes: MINE/BUILD, PLAYER/WASD PAN, MAP, DEBUG.
    const float top_control_width = (std::max)(1.0f, (row_width - row_gap * 3.0f) / 4.0f);
    layout.mode_toggle = {{row_left, 136.0f}, {top_control_width, 28.0f}};
    layout.camera_controls_toggle = {{row_left + (top_control_width + row_gap), 136.0f},
                                     {top_control_width, 28.0f}};
    layout.map_toggle = {{row_left + (top_control_width + row_gap) * 2.0f, 136.0f},
                         {top_control_width, 28.0f}};
    layout.debug_toggle = {{row_left + (top_control_width + row_gap) * 3.0f, 136.0f},
                           {row_width - top_control_width * 3.0f - row_gap * 3.0f, 28.0f}};

    // Primary tools stay visible above the material browser.
    const float utility_width = (std::max)(1.0f, (row_width - row_gap * 2.0f) / 3.0f);
    layout.atmosphere = {{row_left, 168.0f}, {utility_width, float(eraser_height)}};
    layout.eraser = {{row_left + utility_width + row_gap, 168.0f},
                     {utility_width, float(eraser_height)}};
    layout.fill = {{row_left + (utility_width + row_gap) * 2.0f, 168.0f},
                   {row_width - utility_width * 2.0f - row_gap * 2.0f,
                    float(eraser_height)}};

    const float keymap_top = layout.palette.position.y + layout.palette.size.y + gap;
    layout.keymap = {{content_left, keymap_top}, {content_width, float(keymap_height)}};
    const float cursor_top = keymap_top + float(keymap_height) + gap;
    layout.cursor_editor = {{content_left, cursor_top},
                            {content_width, float(cursor_editor_height)}};

    const float placement_top = cursor_top + 23.0f;
    const float placement_width = content_width / 2.0f;
    layout.placement_cells = {{content_left, placement_top}, {placement_width, 26.0f}};
    layout.placement_tiles = {{content_left + placement_width, placement_top},
                              {content_width - placement_width, 26.0f}};

    const float shape_top = cursor_top + 53.0f;
    const float shape_width = content_width / 4.0f;
    layout.cursor_circle = {{content_left, shape_top}, {shape_width, 24.0f}};
    layout.cursor_square = {{content_left + shape_width, shape_top}, {shape_width, 24.0f}};
    layout.cursor_horizontal = {{content_left + shape_width * 2.0f, shape_top}, {shape_width, 24.0f}};
    layout.cursor_vertical = {{content_left + shape_width * 3.0f, shape_top}, {shape_width, 24.0f}};

    const float control_top = cursor_top + 85.0f;
    const float half = content_width / 2.0f;
    constexpr float control_button_width = 44.0f;
    layout.brush_smaller = {{content_left + 4.0f, control_top}, {control_button_width, 24.0f}};
    layout.brush_larger = {{content_left + half - control_button_width - 4.0f, control_top},
                           {control_button_width, 24.0f}};
    layout.zoom_out = {{content_left + half + 4.0f, control_top}, {control_button_width, 24.0f}};
    layout.zoom_in = {{content_left + content_width - control_button_width - 4.0f, control_top},
                      {control_button_width, 24.0f}};

    const float card_top = cursor_top + float(cursor_editor_height) + gap;
    layout.material_card = {{content_left, card_top},
                            {content_width, (std::max)(1.0f, float(screen_height) - card_top - margin)}};
    return layout;
}

'''
text = regex_once(
    text,
    r"\[\[nodiscard\]\] inline Layout make_layout\(.*?\n\}\n\n(?=\[\[nodiscard\]\] inline epochengine::gui_lib::Rect group_tab_rect)",
    new_layout,
    "sidebar layout",
)
path.write_text(text, encoding="utf-8")

# Render the new grouped controls and preserve material identity in debug/map.
path = Path("shaders/fullscreen.frag")
text = path.read_text(encoding="utf-8")
text = replace_once(
    text,
    "        debugPanelPixel(pixel, x, y, sidebarLeft + 4u, renderPc.statusHeight + 4u,\n"
    "                        renderPc.windowWidth - 4u, renderPc.windowHeight - 4u,\n"
    "                        2, debugColor);",
    "        int debugScale = renderPc.windowHeight > renderPc.statusHeight + 560u ? 2 : 1;\n"
    "        debugPanelPixel(pixel, x, y, sidebarLeft + 4u, renderPc.statusHeight + 4u,\n"
    "                        renderPc.windowWidth - 4u, renderPc.windowHeight - 4u,\n"
    "                        debugScale, debugColor);",
    "adaptive debug panel scale",
)
controls = r'''        // Scene files/navigation.
        uint rowGap = 4u;
        uint rowLeft = sidebarLeft + 8u;
        uint rowWidth = max(sidebarWidth - 16u, 1u);
        uint sceneWidth = max((rowWidth - rowGap * 3u) / 4u, 1u);
        uint sceneIds[4] = uint[4](41u, 42u, 65u, 66u);
        for (uint i = 0u; i < 4u; ++i) {
            uint left = rowLeft + i * (sceneWidth + rowGap);
            uint right = i == 3u ? sidebarLeft + sidebarWidth - 8u : left + sceneWidth;
            if (x >= left && x < right && y >= 70u && y < 98u) {
                color = vec3(0.075, 0.105, 0.145);
                if (borderPixel(x, y, left, 70u, right, 98u)) color *= 0.55;
                uint length = fixedTextLength(sceneIds[i]);
                int scale = int(right - left) >= int(length * 12u + 6u) ? 2 : 1;
                int labelWidth = int(length) * 6 * scale - scale;
                if (fixedPixel(pixel, ivec2(int(left + right) / 2 - labelWidth / 2,
                                             84 - (7 * scale) / 2), scale, sceneIds[i]))
                    color = vec3(0.96);
            }
        }

        // Simulation actions: RESET and PAUSE/RUN are equal-size neighbors.
        uint actionWidth = max((rowWidth - rowGap) / 2u, 1u);
        uint actionIds[2] = uint[2](6u, renderPc.paused != 0u ? 3u : 2u);
        for (uint action = 0u; action < 2u; ++action) {
            uint left = rowLeft + action * (actionWidth + rowGap);
            uint right = action == 1u ? sidebarLeft + sidebarWidth - 8u : left + actionWidth;
            if (x >= left && x < right && y >= 102u && y < 132u) {
                bool enabled = action == 1u && renderPc.paused != 0u;
                color = enabled ? vec3(0.20, 0.38, 0.20) :
                    (action == 0u ? vec3(0.32, 0.16, 0.08) : vec3(0.075, 0.105, 0.145));
                if (borderPixel(x, y, left, 102u, right, 132u)) color *= 0.55;
                uint length = fixedTextLength(actionIds[action]);
                int scale = int(right - left) >= int(length * 12u + 6u) ? 2 : 1;
                int labelWidth = int(length) * 6 * scale - scale;
                if (fixedPixel(pixel, ivec2(int(left + right) / 2 - labelWidth / 2,
                                             117 - (7 * scale) / 2), scale,
                               actionIds[action])) color = vec3(0.97);
            }
        }

        // View/input modes.
        uint viewWidth = max((rowWidth - rowGap * 3u) / 4u, 1u);
        bool playerScene = renderPc.selectedScene == 6u ||
                           renderPc.selectedScene == 7u ||
                           renderPc.selectedScene == 8u;
        uint viewIds[4] = uint[4](
            renderPc.miningMode != 0u ? 8u : 7u,
            playerScene && renderPc.cameraControls == 0u ? 141u : 140u,
            0u,
            9u);
        for (uint control = 0u; control < 4u; ++control) {
            uint left = rowLeft + control * (viewWidth + rowGap);
            uint right = control == 3u ? sidebarLeft + sidebarWidth - 8u : left + viewWidth;
            if (x >= left && x < right && y >= 136u && y < 164u) {
                bool enabled = (control == 1u && renderPc.cameraControls != 0u) ||
                               (control == 2u && renderPc.mapMode != 0u) ||
                               (control == 3u && renderPc.debugMode != 0u);
                color = enabled ? vec3(0.14, 0.31, 0.45) : vec3(0.075, 0.105, 0.145);
                if (borderPixel(x, y, left, 136u, right, 164u)) color *= 0.55;
            }
            bool labelHit = false;
            if (control == 2u) {
                int scale = int(right - left) >= 42 ? 2 : 1;
                int labelWidth = 17 * scale;
                ivec2 origin = ivec2(int(left + right) / 2 - labelWidth / 2,
                                     150 - (7 * scale) / 2);
                labelHit = glyphPixel(pixel, origin, scale, 77u) ||
                           glyphPixel(pixel, origin + ivec2(6 * scale, 0), scale, 65u) ||
                           glyphPixel(pixel, origin + ivec2(12 * scale, 0), scale, 80u);
            } else {
                uint length = fixedTextLength(viewIds[control]);
                int scale = int(right - left) >= int(length * 12u + 6u) ? 2 : 1;
                int labelWidth = int(length) * 6 * scale - scale;
                labelHit = fixedPixel(pixel,
                    ivec2(int(left + right) / 2 - labelWidth / 2,
                          150 - (7 * scale) / 2), scale, viewIds[control]);
            }
            if (labelHit) color = vec3(0.97);
        }

        // Primary tools are always visible and strongly differentiated.
        uint utilityWidth = max((rowWidth - rowGap * 2u) / 3u, 1u);
        uint utilityLabels[3] = uint[3](67u, 159u, 108u);
        for (uint button = 0u; button < 3u; ++button) {
            uint left = rowLeft + button * (utilityWidth + rowGap);
            uint right = button == 2u ? sidebarLeft + sidebarWidth - 8u : left + utilityWidth;
            if (x >= left && x < right && y >= 168u && y < 202u) {
                if (button == 0u) {
                    color = renderPc.selectedMaterial == MAT_ATMOSPHERE
                        ? vec3(0.10, 0.50, 0.76) : vec3(0.07, 0.25, 0.38);
                } else if (button == 1u) {
                    color = renderPc.selectedMaterial == MAT_EMPTY
                        ? vec3(0.72, 0.14, 0.18) : vec3(0.30, 0.055, 0.07);
                } else {
                    color = vec3(0.10, 0.42, 0.20);
                }
                if (borderPixel(x, y, left, 168u, right, 202u)) color *= 0.55;
                uint length = fixedTextLength(utilityLabels[button]);
                int scale = int(right - left) >= int(length * 12u + 6u) ? 2 : 1;
                int labelWidth = int(length) * 6 * scale - scale;
                if (fixedPixel(pixel, ivec2(int(left + right) / 2 - labelWidth / 2,
                                             185 - (7 * scale) / 2), scale,
                               utilityLabels[button])) color = vec3(1.0);
            }
        }

'''
text = regex_once(
    text,
    r"        // A small divider separates scene/navigation controls.*?(?=        uint contentLeft)",
    controls,
    "grouped sidebar controls",
)
text = regex_once(
    text,
    r"        uint eraserTop = paletteTop \+ palettePanelHeight \+ 3u;.*?        uint keymapTop = eraserBottom \+ 3u;",
    "        uint keymapTop = paletteTop + palettePanelHeight + 3u;",
    "remove displaced utility row",
)
old_overlay = r'''    if (renderPc.debugMode != 0u || renderPc.mapMode != 0u) {
        bool activeArea = sectionActiveAt(grid, renderPc.activeAreaX, renderPc.activeAreaY,
                                          renderPc.activeScopeMode);
        if (!activeArea) color.rgb *= renderPc.mapMode != 0u ? 0.38 : 0.28;'''
new_overlay = r'''    if (renderPc.debugMode != 0u || renderPc.mapMode != 0u) {
        bool activeArea = sectionActiveAt(grid, renderPc.activeAreaX, renderPc.activeAreaY,
                                          renderPc.activeScopeMode);
        bool mediumCell = isCellGas(cell) || isCellLiquid(cell) || isHalfWater(cell);
        if (!activeArea) color.rgb *= renderPc.mapMode != 0u ? 0.62 : 0.52;'''
text = replace_once(text, old_overlay, new_overlay, "medium-preserving active scope")
text = replace_once(
    text,
    "        float occupancyAlpha = max(0.28, float(tileOccupancy(tile)) / 64.0);\n"
    "        color.rgb = mix(color.rgb, overlay, alpha * occupancyAlpha);\n\n"
    "        vec3 chunkOverlay = chunkHas(chunk, CHUNK_DIRTY) ? vec3(1.00, 0.10, 0.04) :\n"
    "            (chunkHas(chunk, CHUNK_SLEEPING) ? vec3(0.035, 0.10, 0.30)\n"
    "                                             : vec3(0.05, 0.42, 0.90));\n"
    "        color.rgb = mix(color.rgb, chunkOverlay, chunkHas(chunk, CHUNK_SLEEPING) ? 0.12 : 0.07);",
    "        bool stateEdge = local.x <= 1 || local.y <= 1 || local.x >= 6 || local.y >= 6;\n"
    "        if (renderPc.mapMode != 0u) {\n"
    "            alpha *= mediumCell ? 0.12 : 0.30;\n"
    "            if (!stateEdge) alpha *= 0.16;\n"
    "        } else if (mediumCell) {\n"
    "            alpha *= stateEdge ? 0.28 : 0.06;\n"
    "        }\n"
    "        float occupancyAlpha = max(0.28, float(tileOccupancy(tile)) / 64.0);\n"
    "        color.rgb = mix(color.rgb, overlay, alpha * occupancyAlpha);\n\n"
    "        vec3 chunkOverlay = chunkHas(chunk, CHUNK_DIRTY) ? vec3(1.00, 0.10, 0.04) :\n"
    "            (chunkHas(chunk, CHUNK_SLEEPING) ? vec3(0.035, 0.10, 0.30)\n"
    "                                             : vec3(0.05, 0.42, 0.90));\n"
    "        float chunkAlpha = renderPc.mapMode != 0u\n"
    "            ? (mediumCell ? 0.015 : 0.045)\n"
    "            : (mediumCell ? 0.025 : (chunkHas(chunk, CHUNK_SLEEPING) ? 0.10 : 0.06));\n"
    "        color.rgb = mix(color.rgb, chunkOverlay, chunkAlpha);",
    "medium-preserving debug overlay",
)
path.write_text(text, encoding="utf-8")

Path("tests/ui_layout_contract.cpp").write_text(r'''#include "sandhybrid/material.hpp"
#include "sandhybrid/ui_layout.hpp"

#include <cstdint>

int main() {
    using sandhybrid::Material;
    using sandhybrid::MaterialGroup;

    const auto layout = sandhybrid::ui::make_layout(1280u, 720u);
    const auto industry_tab = sandhybrid::ui::group_tab_rect(
        layout, static_cast<std::uint32_t>(MaterialGroup::industry));
    const auto industry = sandhybrid::ui::group_at(
        layout,
        {industry_tab.position.x + industry_tab.size.x * 0.5f,
         industry_tab.position.y + industry_tab.size.y * 0.5f});
    if (industry != static_cast<std::uint32_t>(MaterialGroup::industry)) return 1;

    const auto bot_slot = sandhybrid::ui::palette_item_rect(layout, MaterialGroup::industry, 3u);
    const auto material = sandhybrid::ui::palette_material_at(
        layout, MaterialGroup::industry,
        {bot_slot.position.x + bot_slot.size.x * 0.5f,
         bot_slot.position.y + bot_slot.size.y * 0.5f});
    if (material != Material::factory_core) return 2;

    if (layout.reset_scene.position.y != layout.pause_toggle.position.y ||
        layout.reset_scene.size.y != layout.pause_toggle.size.y ||
        layout.reset_scene.position.x + layout.reset_scene.size.x >
            layout.pause_toggle.position.x) return 3;
    if (layout.previous_scene.position.x + layout.previous_scene.size.x > layout.next_scene.position.x ||
        layout.next_scene.position.x + layout.next_scene.size.x > layout.save_scene.position.x ||
        layout.save_scene.position.x + layout.save_scene.size.x > layout.load_scene.position.x) return 4;
    if (layout.mode_toggle.position.x + layout.mode_toggle.size.x >
            layout.camera_controls_toggle.position.x ||
        layout.camera_controls_toggle.position.x + layout.camera_controls_toggle.size.x >
            layout.map_toggle.position.x ||
        layout.map_toggle.position.x + layout.map_toggle.size.x >
            layout.debug_toggle.position.x) return 5;
    if (layout.atmosphere.position.x + layout.atmosphere.size.x > layout.eraser.position.x ||
        layout.eraser.position.x + layout.eraser.size.x > layout.fill.position.x ||
        layout.group_tabs.position.y < layout.fill.position.y + layout.fill.size.y) return 6;

    const auto viewport = sandhybrid::ui::make_simulation_viewport(layout, 640u, 360u);
    if (static_cast<std::uint32_t>(viewport.rect.size.x) % 80u != 0u ||
        static_cast<std::uint32_t>(viewport.rect.size.y) % 45u != 0u) return 7;
    if (viewport.rect.size.x / 80.0f != viewport.rect.size.y / 45.0f) return 8;
    if (viewport.rect.position.x < 0.0f ||
        viewport.rect.position.y < layout.simulation.position.y) return 9;

    const auto compact = sandhybrid::ui::make_layout(480u, 320u);
    if (compact.simulation.size.y <= 0.0f || compact.status.size.x < 300.0f) return 10;
    if (compact.reset_scene.size.x <= 0.0f || compact.pause_toggle.size.x <= 0.0f ||
        compact.atmosphere.size.x <= 0.0f || compact.eraser.size.x <= 0.0f ||
        compact.fill.size.x <= 0.0f) return 11;
    if (compact.mode_toggle.position.x < compact.simulation.size.x ||
        compact.pause_toggle.position.x < compact.simulation.size.x ||
        compact.camera_controls_toggle.position.x < compact.simulation.size.x ||
        compact.map_toggle.position.x < compact.simulation.size.x ||
        compact.debug_toggle.position.x < compact.simulation.size.x ||
        compact.material_card.position.x < compact.simulation.size.x) return 12;

    for (std::uint32_t slot = 0u; slot < 4u; ++slot) {
        const auto rect = sandhybrid::ui::inventory_slot_rect(layout, 720u, slot);
        const auto hit = sandhybrid::ui::inventory_slot_at(
            layout, 720u, {rect.position.x + rect.size.x * 0.5f,
                           rect.position.y + rect.size.y * 0.5f});
        if (hit != slot) return 13;
    }
    return 0;
}
''', encoding="utf-8")

path = Path("tests/world_layout_contract.cpp")
text = path.read_text(encoding="utf-8")
text = replace_once(
    text,
    "static_assert(resident_substrate_material(resident_world_width, resident_world_height, 1280u, 1432u) == Material::stone);",
    "static_assert(resident_substrate_material(resident_world_width, resident_world_height, 1280u, 1432u) == Material::stone);\n"
    "static_assert(resident_substrate_is_structural(Material::mud));\n"
    "static_assert(resident_substrate_is_structural(Material::iron_ore));\n"
    "static_assert(resident_substrate_is_structural(Material::copper));\n"
    "static_assert(resident_substrate_is_structural(Material::aluminum));\n"
    "static_assert(resident_substrate_is_structural(Material::uranium));",
    "resident structural assertions",
)
text = replace_once(
    text,
    "            if (deposit_index == 4u) continue;\n            ++deposits[deposit_index];",
    "            if (deposit_index == 4u) continue;\n"
    "            if (!resident_substrate_is_structural(material)) return 7;\n"
    "            ++deposits[deposit_index];",
    "resident deposit structural runtime check",
)
path.write_text(text, encoding="utf-8")

path = Path("tools/validate_shader_contracts.py")
text = path.read_text(encoding="utf-8")
anchor = "    if \"std::atomic_bool camera_controls{false};\" not in shared_state_hpp:\n"
insert = '''    for token in ("action_width", "layout.reset_scene", "layout.pause_toggle",\n                  "layout.atmosphere", "layout.eraser", "layout.fill"):\n        if token not in ui_layout_hpp:\n            errors.append(f"grouped editor layout contract missing {token!r}")\n    fullscreen = (SHADERS / "fullscreen.frag").read_text(encoding="utf-8")\n    for token in ("mediumCell", "stateEdge", "utilityLabels[3] = uint[3](67u, 159u, 108u)",\n                  "debugScale"):\n        if token not in fullscreen:\n            errors.append(f"medium-preserving debug/interface contract missing {token!r}")\n    reset_shader = (SHADERS / "reset.comp").read_text(encoding="utf-8")\n    for token in ("residentGroundHostMaterial(material)", "material == MAT_IRON_ORE",\n                  "material == MAT_COPPER", "material == MAT_ALUMINUM",\n                  "material == MAT_URANIUM"):\n        if token not in reset_shader:\n            errors.append(f"resident structural deposit contract missing {token!r}")\n'''
if insert not in text:
    text = replace_once(text, anchor, insert + anchor, "validator interface insertion")
path.write_text(text, encoding="utf-8")

path = Path("missioncache.md")
text = path.read_text(encoding="utf-8")
text = text.replace(
    "MC-118, MC-119, and MC-120.",
    "MC-118, MC-119, MC-120, and MC-122.",
    1,
)
text = text.replace(
    "current input, camera, map, actor, inventory, scene-origin, and geology regressions.",
    "current input, camera, map, actor, inventory, scene-origin, geology, medium-visibility, and interface regressions.",
    1,
)
text = text.replace(
    "excluding Gold. |",
    "excluding Gold. Resident mud and ore veins now inherit durable structural metadata from their host geology so deposits no longer tear stable tiles apart or create false air/liquid voids. |",
    1,
)
blueprint_row = next((line for line in text.splitlines() if line.startswith("| MC-121 |")), None)
if blueprint_row is None:
    raise SystemExit("MC-121 row missing")
ui_row = "| MC-122 | PARTIAL | Coherent grouped editor interface | Scene navigation/files occupy one row; Reset and Pause/Run are equal-size adjacent simulation actions; Mine/Build, Player/WASD Pan, Map, and Debug form a separate view/input row; Atmosphere, Erase, and Fill are large, permanently visible, strongly differentiated tools above the material browser. Layout contracts pass at 1280x720 and compact sizes. Packaged visual and interaction acceptance remains required. |"
if "| MC-122 |" not in text:
    text = text.replace(blueprint_row, blueprint_row + "\n" + ui_row, 1)
invariant_anchor = "- Mineral deposits are clustered into deterministic contiguous veins with rough edges; isolated cell-scale ore confetti may not fragment otherwise stable terrain tiles."
new_invariant = "- Debug and map overlays must preserve the visible identity of Atmosphere, Water, and Half Water; hierarchy state uses restrained edges/markers rather than replacing whole medium fields with opaque status colors."
if new_invariant not in text:
    if invariant_anchor not in text:
        raise SystemExit("medium invariant anchor missing")
    text = text.replace(invariant_anchor, invariant_anchor + "\n" + new_invariant, 1)
path.write_text(text, encoding="utf-8")

path = Path("CHANGELOG.md")
text = path.read_text(encoding="utf-8")
needle = "## 2.5.6\n\n"
addition = (
    "- Stabilized resident mud and ore veins as authored structural geology so they no longer collapse into false voids that wake Atmosphere and liquids.\n"
    "- Redesigned the sidebar into explicit Scene, Simulation, View, and Tool rows; Reset and Pause are paired, and Atmosphere/Erase/Fill remain large and visible.\n"
    "- Changed debug/map hierarchy visualization to preserve Atmosphere, Water, and Half Water colors while showing state through restrained tile edges and markers.\n"
)
if addition.splitlines()[0] not in text:
    if needle not in text:
        raise SystemExit("v2.5.6 changelog section missing")
    text = text.replace(needle, needle + addition, 1)
path.write_text(text, encoding="utf-8")
