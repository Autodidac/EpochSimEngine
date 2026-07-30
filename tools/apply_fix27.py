#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

def read(path):
    return (ROOT / path).read_text(encoding="utf-8")

def write(path, text):
    (ROOT / path).write_text(text, encoding="utf-8", newline="\n")

def one(text, old, new, label):
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)

def rx(text, pattern, new, label):
    result, count = re.subn(pattern, new, text, count=1, flags=re.S)
    if count != 1:
        raise RuntimeError(f"{label}: expected one regex match, found {count}")
    return result

# Shared one-shot scene image requests.
shared = read("include/epoch/sand/shared_state.hpp")
shared = one(shared,
    "    std::atomic_bool reset{false};\n",
    "    std::atomic_bool reset{false};\n    std::atomic_bool save_scene_image{false};\n    std::atomic_bool load_scene_image{false};\n",
    "shared scene requests")
write("include/epoch/sand/shared_state.hpp", shared)

window = read("include/epoch/sand/window.hpp")
window = one(window,
    "    bool reset{};\n    bool next_scene{};\n",
    "    bool reset{};\n    bool save_scene{};\n    bool load_scene{};\n    bool next_scene{};\n",
    "window scene requests")
write("include/epoch/sand/window.hpp", window)

# Windows: F5 saves the selected scene PPM; F9 reloads it.
win = read("src/window_win32.cpp")
win = one(win,
    "    bool reset{};\n    bool next_scene{};\n",
    "    bool reset{};\n    bool save_scene{};\n    bool load_scene{};\n    bool next_scene{};\n",
    "win state")
win = one(win,
    "            case VK_F3:\n                self->toggle_debug = true;\n                return 0;\n",
    "            case VK_F3:\n                self->toggle_debug = true;\n                return 0;\n            case VK_F5:\n                self->save_scene = true;\n                return 0;\n            case VK_F9:\n                self->load_scene = true;\n                return 0;\n",
    "win key bindings")
win = one(win,
    "    impl_->reset = false;\n    impl_->next_scene = false;\n",
    "    impl_->reset = false;\n    impl_->save_scene = false;\n    impl_->load_scene = false;\n    impl_->next_scene = false;\n",
    "win one shot reset")
win = one(win,
    "        .reset = impl_->reset,\n        .next_scene = impl_->next_scene,\n",
    "        .reset = impl_->reset,\n        .save_scene = impl_->save_scene,\n        .load_scene = impl_->load_scene,\n        .next_scene = impl_->next_scene,\n",
    "win input output")
write("src/window_win32.cpp", win)

# Linux/XCB equivalent keys.
xcb = read("src/window_xcb.cpp")
xcb = one(xcb,
    "constexpr std::uint32_t keysym_f3 = 0xFFC0u;\n",
    "constexpr std::uint32_t keysym_f3 = 0xFFC0u;\nconstexpr std::uint32_t keysym_f5 = 0xFFC2u;\nconstexpr std::uint32_t keysym_f9 = 0xFFC6u;\n",
    "xcb function keys")
xcb = one(xcb,
    "    bool reset{};\n    bool next_scene{};\n",
    "    bool reset{};\n    bool save_scene{};\n    bool load_scene{};\n    bool next_scene{};\n",
    "xcb state")
xcb = one(xcb,
    "    impl_->reset = false;\n    impl_->next_scene = false;\n",
    "    impl_->reset = false;\n    impl_->save_scene = false;\n    impl_->load_scene = false;\n    impl_->next_scene = false;\n",
    "xcb one shot reset")
xcb = one(xcb,
    "            } else if (keysym == keysym_f3) {\n                impl_->toggle_debug = true;\n            } else if (keysym == keysym_escape) {\n",
    "            } else if (keysym == keysym_f3) {\n                impl_->toggle_debug = true;\n            } else if (keysym == keysym_f5) {\n                impl_->save_scene = true;\n            } else if (keysym == keysym_f9) {\n                impl_->load_scene = true;\n            } else if (keysym == keysym_escape) {\n",
    "xcb key bindings")
xcb = one(xcb,
    "        .reset = impl_->reset,\n        .next_scene = impl_->next_scene,\n",
    "        .reset = impl_->reset,\n        .save_scene = impl_->save_scene,\n        .load_scene = impl_->load_scene,\n        .next_scene = impl_->next_scene,\n",
    "xcb input output")
write("src/window_xcb.cpp", xcb)

# Sidebar geometry: scene I/O buttons, permanent eraser, then keymap, then card.
write("include/epoch/sand/ui_layout.hpp", r'''#pragma once
#include "epoch/sand/material.hpp"
#include <gui/floating_window.hpp>
#include <gui/font.hpp>
#include <algorithm>
#include <cstdint>

namespace epoch::sand::ui {
inline constexpr std::uint32_t preferred_sidebar_width = 384u;
inline constexpr std::uint32_t minimum_sidebar_width = 300u;
inline constexpr std::uint32_t status_height = 126u;
inline constexpr std::uint32_t group_tabs_height = 112u;
inline constexpr std::uint32_t palette_items_height = 136u;
inline constexpr std::uint32_t eraser_height = 24u;
inline constexpr std::uint32_t keymap_height = 100u;
inline constexpr std::uint32_t palette_height = 0u;
inline constexpr float margin = 5.0f;
inline constexpr float gap = 3.0f;

struct Layout final {
    epochengine::gui_lib::Rect status{}, simulation{}, group_tabs{}, palette{};
    epochengine::gui_lib::Rect previous_scene{}, next_scene{}, reset_scene{}, save_scene{}, load_scene{};
    epochengine::gui_lib::Rect mode_toggle{}, debug_toggle{}, eraser{}, keymap{}, material_card{};
};
struct SimulationViewport final { epochengine::gui_lib::Rect rect{}; std::uint32_t tile_pixel_size{}; };

[[nodiscard]] inline SimulationViewport make_simulation_viewport(
    const Layout& layout, std::uint32_t grid_width, std::uint32_t grid_height) noexcept {
    constexpr std::uint32_t cells_per_tile = 8u;
    const auto tile_columns = (std::max)(1u, (grid_width + cells_per_tile - 1u) / cells_per_tile);
    const auto tile_rows = (std::max)(1u, (grid_height + cells_per_tile - 1u) / cells_per_tile);
    const auto panel_width = (std::max)(1u, static_cast<std::uint32_t>(layout.simulation.size.x));
    const auto panel_height = (std::max)(1u, static_cast<std::uint32_t>(layout.simulation.size.y));
    if (panel_width < tile_columns || panel_height < tile_rows) return {layout.simulation, 0u};
    const auto tile_pixels = (std::max)(1u, (std::min)(panel_width / tile_columns, panel_height / tile_rows));
    const auto viewport_width = tile_columns * tile_pixels;
    const auto viewport_height = tile_rows * tile_pixels;
    const auto left = layout.simulation.position.x + float((panel_width - viewport_width) / 2u);
    const auto top = layout.simulation.position.y + float((panel_height - viewport_height) / 2u);
    return {{{left, top}, {float(viewport_width), float(viewport_height)}}, tile_pixels};
}

[[nodiscard]] inline Layout make_layout(std::uint32_t width, std::uint32_t height) noexcept {
    const auto screen_width = (std::max)(width, 1u);
    const auto screen_height = (std::max)(height, 1u);
    const auto requested = (std::max)(minimum_sidebar_width, screen_width / 3u);
    const auto sidebar = screen_width > minimum_sidebar_width + 160u
        ? (std::min)(preferred_sidebar_width, requested)
        : (std::min)(screen_width, minimum_sidebar_width);
    const auto simulation_width = screen_width > sidebar ? screen_width - sidebar : 1u;
    const float left = float(simulation_width);
    const float side = float(screen_width - simulation_width);

    Layout layout{
        .status = {{left, 0.0f}, {side, float(status_height)}},
        .simulation = {{0.0f, 0.0f}, {float(simulation_width), float(screen_height)}},
        .group_tabs = {{left + margin, float(status_height) + margin},
                       {(std::max)(1.0f, side - margin * 2.0f), float(group_tabs_height)}},
        .palette = {{left + margin, float(status_height + group_tabs_height) + margin + gap},
                    {(std::max)(1.0f, side - margin * 2.0f), float(palette_items_height)}},
    };

    const float scene_left = left + 8.0f;
    const float scene_gap = 3.0f;
    const float scene_width = (std::max)(1.0f, (side - 16.0f - scene_gap * 4.0f) / 5.0f);
    layout.previous_scene = {{scene_left, 70.0f}, {scene_width, 26.0f}};
    layout.next_scene = {{scene_left + (scene_width + scene_gap), 70.0f}, {scene_width, 26.0f}};
    layout.reset_scene = {{scene_left + (scene_width + scene_gap) * 2.0f, 70.0f}, {scene_width, 26.0f}};
    layout.save_scene = {{scene_left + (scene_width + scene_gap) * 3.0f, 70.0f}, {scene_width, 26.0f}};
    layout.load_scene = {{scene_left + (scene_width + scene_gap) * 4.0f, 70.0f}, {scene_width, 26.0f}};

    layout.mode_toggle = {{left + 8.0f, 100.0f}, {(std::max)(112.0f, side * 0.46f), 22.0f}};
    layout.debug_toggle = {{layout.mode_toggle.position.x + layout.mode_toggle.size.x + 4.0f, 100.0f},
                           {(std::max)(1.0f, side - layout.mode_toggle.size.x - 24.0f), 22.0f}};

    const float eraser_top = layout.palette.position.y + layout.palette.size.y + gap;
    layout.eraser = {{left + margin, eraser_top}, {(std::max)(1.0f, side - margin * 2.0f), float(eraser_height)}};
    const float keymap_top = eraser_top + float(eraser_height) + gap;
    layout.keymap = {{left + margin, keymap_top}, {(std::max)(1.0f, side - margin * 2.0f), float(keymap_height)}};
    const float card_top = keymap_top + float(keymap_height) + gap;
    layout.material_card = {{left + margin, card_top},
                            {(std::max)(1.0f, side - margin * 2.0f),
                             (std::max)(1.0f, float(screen_height) - card_top - margin)}};
    return layout;
}

[[nodiscard]] inline epochengine::gui_lib::Rect group_tab_rect(
    const Layout& layout, std::uint32_t index) noexcept {
    constexpr std::uint32_t columns = 2u;
    const auto rows = (material_group_count + columns - 1u) / columns;
    const auto column = index % columns;
    const auto row = index / columns;
    const float cell_width = layout.group_tabs.size.x / float(columns);
    const float cell_height = layout.group_tabs.size.y / float((std::max)(rows, 1u));
    return {{layout.group_tabs.position.x + float(column) * cell_width + gap * 0.5f,
             layout.group_tabs.position.y + float(row) * cell_height + gap * 0.5f},
            {(std::max)(1.0f, cell_width - gap), (std::max)(1.0f, cell_height - gap)}};
}

[[nodiscard]] inline epochengine::gui_lib::Rect palette_item_rect(
    const Layout& layout, MaterialGroup group, std::uint32_t index) noexcept {
    constexpr std::uint32_t columns = 2u;
    const auto count = (std::max)(material_group_size(group), 1u);
    const auto rows = (count + columns - 1u) / columns;
    const auto column = index % columns;
    const auto row = index / columns;
    const float cell_width = layout.palette.size.x / float(columns);
    const float cell_height = layout.palette.size.y / float((std::max)(rows, 1u));
    return {{layout.palette.position.x + float(column) * cell_width + gap * 0.5f,
             layout.palette.position.y + float(row) * cell_height + gap * 0.5f},
            {(std::max)(1.0f, cell_width - gap), (std::max)(1.0f, cell_height - gap)}};
}

[[nodiscard]] inline std::uint32_t group_at(const Layout& layout, epochengine::gui_lib::Vec2 point) noexcept {
    for (std::uint32_t index = 0u; index < material_group_count; ++index)
        if (epochengine::gui_lib::contains(group_tab_rect(layout, index), point)) return index;
    return material_group_count;
}

[[nodiscard]] inline std::uint32_t palette_slot_at(
    const Layout& layout, MaterialGroup group, epochengine::gui_lib::Vec2 point) noexcept {
    const auto count = material_group_size(group);
    for (std::uint32_t index = 0u; index < count; ++index)
        if (epochengine::gui_lib::contains(palette_item_rect(layout, group, index), point)) return index;
    return count;
}

[[nodiscard]] inline Material palette_material_at(
    const Layout& layout, MaterialGroup group, epochengine::gui_lib::Vec2 point) noexcept {
    const auto slot = palette_slot_at(layout, group, point);
    return slot < material_group_size(group) ? grouped_material(group, slot) : Material::count;
}
} // namespace epoch::sand::ui
''')

# App routes keyboard/buttons and keeps the eraser independently visible.
app = read("src/app.cpp")
app = one(app,
    "        } else if (input.reset) {\n            shared_state.reset.store(true, std::memory_order_release);\n        }\n\n        const auto layout",
    "        } else if (input.reset) {\n            shared_state.reset.store(true, std::memory_order_release);\n        }\n        if (input.save_scene) shared_state.save_scene_image.store(true, std::memory_order_release);\n        if (input.load_scene) shared_state.load_scene_image.store(true, std::memory_order_release);\n\n        const auto layout",
    "app keyboard scene I/O")
app = one(app,
    "            } else if (epochengine::gui_lib::contains(layout.reset_scene, pointer)) {\n                shared_state.reset.store(true, std::memory_order_release);\n            } else if (epochengine::gui_lib::contains(layout.mode_toggle, pointer)) {\n",
    "            } else if (epochengine::gui_lib::contains(layout.reset_scene, pointer)) {\n                shared_state.reset.store(true, std::memory_order_release);\n            } else if (epochengine::gui_lib::contains(layout.save_scene, pointer)) {\n                shared_state.save_scene_image.store(true, std::memory_order_release);\n            } else if (epochengine::gui_lib::contains(layout.load_scene, pointer)) {\n                shared_state.load_scene_image.store(true, std::memory_order_release);\n            } else if (epochengine::gui_lib::contains(layout.mode_toggle, pointer)) {\n",
    "app scene buttons")
app = one(app,
    "            } else if (epochengine::gui_lib::contains(layout.debug_toggle, pointer)) {\n                const bool debug = shared_state.debug_visualization.load(std::memory_order_relaxed);\n                shared_state.debug_visualization.store(!debug, std::memory_order_release);\n            } else if (hovered_group < material_group_count) {\n",
    "            } else if (epochengine::gui_lib::contains(layout.debug_toggle, pointer)) {\n                const bool debug = shared_state.debug_visualization.load(std::memory_order_relaxed);\n                shared_state.debug_visualization.store(!debug, std::memory_order_release);\n            } else if (epochengine::gui_lib::contains(layout.eraser, pointer)) {\n                shared_state.selected_material.store(static_cast<std::uint32_t>(Material::empty),\n                                                     std::memory_order_relaxed);\n            } else if (hovered_group < material_group_count) {\n",
    "app permanent eraser")
write("src/app.cpp", app)

# Text IDs used by the new buttons and keymap.
generator = read("tools/generate_ui_text.py")
generator = one(generator,
    '    "SPACE JUMP", "P PAUSE", "LMB USE", "RMB DROP ERASE", "ALT CELL CARD",\n',
    '    "SPACE JUMP", "P PAUSE", "LMB USE", "RMB DROP ERASE", "ALT CELL CARD",\n'
    '    "SAVE", "LOAD", "ERASER", "KEYMAP", "WHEEL BRUSH", "N STEP", "R RESET",\n'
    '    "BRACKETS SCENE", "F5 SAVE PPM", "F9 LOAD PPM",\n',
    "UI text IDs")
write("tools/generate_ui_text.py", generator)

# Enforce per-material cohesive-region rules. Minority one-pixel glass cannot
# piggyback on a different structural material in the same aligned region.
tiles_glsl = read("shaders/tiles.glsl")
tiles_glsl = one(tiles_glsl,
    "const uint TILE_STABILITY_OCCUPANCY = 52u;\nconst uint TILE_COLLAPSE_OCCUPANCY = 32u;\n",
    "const uint TILE_STABILITY_OCCUPANCY = 52u;\nconst uint TILE_MIN_COHESIVE_CELLS = 32u;\nconst uint TILE_COLLAPSE_OCCUPANCY = TILE_MIN_COHESIVE_CELLS;\n",
    "tile minimum")
write("shaders/tiles.glsl", tiles_glsl)

tiles = read("shaders/tiles.comp")
tiles = one(tiles,
    "    bool previouslyDense = tileHas(previous, TILE_STRUCTURAL) &&\n                            previous.occupancy >= TILE_STABILITY_OCCUPANCY;\n    bool denseStructural = structural >= TILE_STABILITY_OCCUPANCY || previouslyDense;\n    bool damaged = structuralTile && denseStructural &&\n                   (structural < TILE_CELL_COUNT ||\n                    (structural > 0u && healthSum / structural < 240u));\n",
    "    bool previouslyDense = tileHas(previous, TILE_STRUCTURAL) &&\n                            previous.occupancy >= TILE_STABILITY_OCCUPANCY;\n    bool reducedDurability = structuralTile && dominantCount < TILE_STABILITY_OCCUPANCY;\n    bool damaged = structuralTile &&\n                   (reducedDurability || dominantCount < TILE_CELL_COUNT ||\n                    (structural > 0u && healthSum / structural < 240u));\n",
    "tile damage rule")
tiles = one(tiles,
    "    bool collapsing = structuralTile && previouslyDense && structural < TILE_COLLAPSE_OCCUPANCY;\n",
    "    bool collapsing = structuralTile && dominantCount < TILE_MIN_COHESIVE_CELLS;\n",
    "tile collapse rule")
tiles = one(tiles,
    "    uint stableCells = structuralTile ? structural : occupied;\n",
    "    uint stableCells = structuralTile ? dominantCount : occupied;\n",
    "tile stable cells")
tiles = one(tiles,
    "    tiles[index] = TileState(dominant, structuralTile ? structural : occupied,\n",
    "    tiles[index] = TileState(dominant, structuralTile ? dominantCount : occupied,\n",
    "tile represented occupancy")
write("shaders/tiles.comp", tiles)

chemistry = read("shaders/chemistry.comp")
chemistry = one(chemistry,
    "    TileState tile = tiles[tileIndex(p, pc.width)];\n    if (tileHas(tile, TILE_STABLE) && !isStructural(source) &&\n",
    "    TileState tile = tiles[tileIndex(p, pc.width)];\n    bool minorityStructural = isStructural(source) && tile.material != source.material;\n    if (isStructural(source) && !minorityStructural && tileHas(tile, TILE_DAMAGED) &&\n        tile.occupancy >= TILE_MIN_COHESIVE_CELLS &&\n        tile.occupancy < TILE_STABILITY_OCCUPANCY) {\n        uint health = stateValue(result);\n        if (health == 0u) health = 255u;\n        uint limitedHealth = max(64u, tile.occupancy * 255u / TILE_STABILITY_OCCUPANCY);\n        setStateValue(result, min(health, limitedHealth));\n    }\n    if (tileHas(tile, TILE_STABLE) && !isStructural(source) &&\n",
    "underbuilt durability")
chemistry = one(chemistry,
    "    } else if (isStructural(source) && tileHas(tile, TILE_COLLAPSING)) {\n",
    "    } else if (isStructural(source) && (minorityStructural || tileHas(tile, TILE_COLLAPSING))) {\n",
    "minority structure release")
write("shaders/chemistry.comp", chemistry)

# Compact sidebar renderer: permanent eraser + keymap exactly between palette/card.
frag = read("shaders/fullscreen.frag")
sidebar = r'''    uint sidebarWidth = min(renderPc.paletteHeight, renderPc.windowWidth);
    uint sidebarLeft = renderPc.windowWidth - sidebarWidth;
    if (x >= sidebarLeft) {
        vec3 color = vec3(0.025, 0.034, 0.048);
        uint localX = x - sidebarLeft;
        if (localX < 2u) color = vec3(0.14, 0.23, 0.32);
        bool text = fixedPixel(pixel, ivec2(int(sidebarLeft + 10u), 8), 2, 0u);
        uint sceneId = renderPc.selectedScene % max(renderPc.sceneCount, 1u);
        text = text || fixedPixel(pixel, ivec2(int(sidebarLeft + 10u), 31), 1, 5u) ||
               scenePixel(pixel, ivec2(int(sidebarLeft + 58u), 27), 2, sceneId) ||
               fixedPixel(pixel, ivec2(int(sidebarLeft + 10u), 51), 2, 1u) ||
               numberPixel(pixel, ivec2(int(sidebarLeft + 58u), 51), 2, renderPc.framesPerSecond) ||
               fixedPixel(pixel, ivec2(int(sidebarLeft + 136u), 51), 1,
                          renderPc.paused != 0u ? 3u : 2u);

        uint sceneGap = 3u;
        uint sceneLeft = sidebarLeft + 8u;
        uint sceneWidth = max(1u, (sidebarWidth - 16u - sceneGap * 4u) / 5u);
        uint sceneIds[5] = uint[5](41u, 42u, 6u, 65u, 66u);
        for (uint i = 0u; i < 5u; ++i) {
            uint left = sceneLeft + i * (sceneWidth + sceneGap);
            uint right = left + sceneWidth;
            if (x >= left && x < right && y >= 70u && y < 96u) {
                color = vec3(0.075, 0.105, 0.145);
                if (borderPixel(x, y, left, 70u, right, 96u)) color *= 0.55;
                uint length = fixedTextLength(sceneIds[i]);
                int scale = int(sceneWidth) >= int(length * 12u + 6u) ? 2 : 1;
                int width = int(length) * 6 * scale - scale;
                if (fixedPixel(pixel, ivec2(int(left + right) / 2 - width / 2,
                                             83 - (7 * scale) / 2), scale, sceneIds[i]))
                    color = vec3(0.95);
            }
        }

        uint modeWidth = max(112u, sidebarWidth * 46u / 100u);
        uint modeLeft = sidebarLeft + 8u;
        uint debugLeft = modeLeft + modeWidth + 4u;
        uint debugWidth = max(1u, sidebarWidth - modeWidth - 24u);
        if (x >= modeLeft && x < modeLeft + modeWidth && y >= 100u && y < 122u) {
            color = vec3(0.075, 0.105, 0.145);
            if (borderPixel(x, y, modeLeft, 100u, modeLeft + modeWidth, 122u)) color *= 0.55;
        }
        if (x >= debugLeft && x < debugLeft + debugWidth && y >= 100u && y < 122u) {
            color = renderPc.debugMode != 0u ? vec3(0.20, 0.38, 0.20) : vec3(0.075, 0.105, 0.145);
            if (borderPixel(x, y, debugLeft, 100u, debugLeft + debugWidth, 122u)) color *= 0.55;
        }
        text = text || fixedPixel(pixel, ivec2(int(modeLeft + 14u), 106), 1,
                                  renderPc.miningMode != 0u ? 8u : 7u) ||
               fixedPixel(pixel, ivec2(int(debugLeft + 10u), 106), 1, 9u);

        uint contentLeft = sidebarLeft + 5u;
        uint contentWidth = max(sidebarWidth - 10u, 1u);
        uint groupTop = renderPc.statusHeight + 5u;
        uint groupRows = max((renderPc.groupCount + 1u) / 2u, 1u);
        uint groupCellWidth = max(contentWidth / 2u, 1u);
        uint groupCellHeight = max(renderPc.groupTabsHeight / groupRows, 1u);
        if (y >= groupTop && y < groupTop + renderPc.groupTabsHeight &&
            x >= contentLeft && x < contentLeft + contentWidth) {
            uint column = min((x - contentLeft) / groupCellWidth, 1u);
            uint row = min((y - groupTop) / groupCellHeight, groupRows - 1u);
            uint group = row * 2u + column;
            if (group < renderPc.groupCount) {
                uint left = contentLeft + column * groupCellWidth;
                uint right = column == 1u ? contentLeft + contentWidth : left + groupCellWidth;
                uint top = groupTop + row * groupCellHeight;
                uint bottom = min(groupTop + renderPc.groupTabsHeight, top + groupCellHeight);
                color = group == renderPc.selectedGroup ? vec3(0.14, 0.30, 0.45) : vec3(0.04, 0.052, 0.07);
                if (group == renderPc.hoveredGroup) color += vec3(0.055);
                if (borderPixel(x, y, left, top, right, bottom)) color *= 0.55;
                int scale = int(right - left) >= int(groupTextLength(group)) * 12 + 8 ? 2 : 1;
                int width = int(groupTextLength(group)) * 6 * scale - scale;
                if (groupPixel(pixel, ivec2(int(left + right) / 2 - width / 2,
                                            int(top + bottom) / 2 - (7 * scale) / 2), scale, group))
                    color = vec3(0.95);
            }
            outColor = vec4(color, 1.0);
            return;
        }

        uint paletteTop = groupTop + renderPc.groupTabsHeight + 3u;
        const uint palettePanelHeight = 136u;
        uint slotCount = max(groupMaterialCount(renderPc.selectedGroup), 1u);
        uint slotRows = max((slotCount + 1u) / 2u, 1u);
        uint cellWidth = max(contentWidth / 2u, 1u);
        uint cellHeight = max(palettePanelHeight / slotRows, 1u);
        if (y >= paletteTop && y < paletteTop + palettePanelHeight &&
            x >= contentLeft && x < contentLeft + contentWidth) {
            uint column = min((x - contentLeft) / cellWidth, 1u);
            uint row = min((y - paletteTop) / cellHeight, slotRows - 1u);
            uint slot = row * 2u + column;
            if (slot < slotCount) {
                uint material = groupMaterial(renderPc.selectedGroup, slot);
                uint left = contentLeft + column * cellWidth;
                uint right = column == 1u ? contentLeft + contentWidth : left + cellWidth;
                uint top = paletteTop + row * cellHeight;
                uint bottom = min(paletteTop + palettePanelHeight, top + cellHeight);
                color = materialColor(material, 0u, material * 1299721u,
                                      ivec2(int(slot), int(renderPc.selectedGroup))).rgb * 0.62;
                if (material == renderPc.selectedMaterial) color = min(color * 1.10 + vec3(0.13), vec3(1.0));
                if (material == renderPc.hoveredMaterial) color = min(color + vec3(0.09), vec3(1.0));
                if (borderPixel(x, y, left, top, right, bottom)) color *= 0.5;
                int scale = int(right - left) >= int(materialTextLength(material)) * 12 + 8 ? 2 : 1;
                int width = int(materialTextLength(material)) * 6 * scale - scale;
                if (materialPixel(pixel, ivec2(int(left + right) / 2 - width / 2,
                                               int(top + bottom) / 2 - (7 * scale) / 2), scale, material))
                    color = dot(color, vec3(0.299, 0.587, 0.114)) > 0.55 ? vec3(0.02) : vec3(0.97);
            }
            outColor = vec4(color, 1.0);
            return;
        }

        uint eraserTop = paletteTop + palettePanelHeight + 3u;
        uint eraserBottom = eraserTop + 24u;
        if (y >= eraserTop && y < eraserBottom && x >= contentLeft && x < contentLeft + contentWidth) {
            color = renderPc.selectedMaterial == MAT_EMPTY ? vec3(0.44, 0.12, 0.14) : vec3(0.20, 0.065, 0.075);
            if (borderPixel(x, y, contentLeft, eraserTop, contentLeft + contentWidth, eraserBottom)) color *= 0.55;
            uint length = fixedTextLength(67u);
            int width = int(length) * 12 - 2;
            if (fixedPixel(pixel, ivec2(int(contentLeft + contentWidth / 2u) - width / 2,
                                        int(eraserTop + 5u)), 2, 67u)) color = vec3(1.0, 0.90, 0.90);
            outColor = vec4(color, 1.0);
            return;
        }

        uint keymapTop = eraserBottom + 3u;
        uint keymapBottom = keymapTop + 100u;
        if (y >= keymapTop && y < keymapBottom && x >= contentLeft && x < contentLeft + contentWidth) {
            color = vec3(0.035, 0.047, 0.064);
            if (borderPixel(x, y, contentLeft, keymapTop, contentLeft + contentWidth, keymapBottom))
                color = vec3(0.12, 0.20, 0.28);
            bool keyText = fixedPixel(pixel, ivec2(int(contentLeft + 8u), int(keymapTop + 6u)), 2, 68u);
            uint leftIds[6] = uint[6](62u, 63u, 64u, 69u, 60u, 61u);
            uint rightIds[5] = uint[5](70u, 71u, 72u, 73u, 74u);
            for (uint i = 0u; i < 6u; ++i)
                keyText = keyText || fixedPixel(pixel, ivec2(int(contentLeft + 8u), int(keymapTop + 25u + i * 12u)), 1, leftIds[i]);
            for (uint i = 0u; i < 5u; ++i)
                keyText = keyText || fixedPixel(pixel, ivec2(int(contentLeft + contentWidth / 2u), int(keymapTop + 25u + i * 12u)), 1, rightIds[i]);
            if (keyText) color = vec3(0.93, 0.96, 0.99);
            outColor = vec4(color, 1.0);
            return;
        }

        uint cardTop = keymapBottom + 3u;
        uint actorPanel = actor.enabled != 0u ? 102u : 5u;
        uint cardBottom = renderPc.windowHeight > actorPanel + 5u
            ? renderPc.windowHeight - actorPanel - 5u : renderPc.windowHeight;
        ivec2 cursor = clamp(ivec2(renderPc.cursorX, renderPc.cursorY), ivec2(0),
                              ivec2(int(renderPc.gridWidth) - 1, int(renderPc.gridHeight) - 1));
        Cell inspected = cellAt(cursor);
        uint cardMaterial = renderPc.inspectMode != 0u ? inspected.material :
            (renderPc.hoveredMaterial < renderPc.materialCount ? renderPc.hoveredMaterial : renderPc.selectedMaterial);
        cardMaterial = min(cardMaterial, renderPc.materialCount - 1u);
        if (y >= cardTop && y < cardBottom) {
            if (borderPixel(x, y, contentLeft, cardTop, contentLeft + contentWidth, cardBottom))
                color = vec3(0.13, 0.29, 0.43);
            text = text || materialPixel(pixel, ivec2(int(contentLeft + 10u), int(cardTop + 9u)), 3, cardMaterial);
            if (renderPc.inspectMode != 0u) {
                uint phase = cellPhase(inspected);
                text = text || fixedPixel(pixel, ivec2(int(contentLeft + 10u), int(cardTop + 36u)), 2, 12u) ||
                       phasePixel(pixel, ivec2(int(contentLeft + 70u), int(cardTop + 36u)), 2, phase) ||
                       fixedPixel(pixel, ivec2(int(contentLeft + 190u), int(cardTop + 36u)), 2, 13u) ||
                       signedNumberPixel(pixel, ivec2(int(contentLeft + 238u), int(cardTop + 36u)), 2,
                                         inspected.temperature);
            }
            uint first = cardTop + (renderPc.inspectMode != 0u ? 58u : 38u);
            for (uint line = 0u; line < 10u; ++line) {
                uint lineY = first + line * 18u;
                if (lineY + 14u < cardBottom &&
                    cardPixel(pixel, ivec2(int(contentLeft + 10u), int(lineY)), 2, cardMaterial, line)) text = true;
            }
            if (text) color = vec3(0.93, 0.96, 0.99);
            outColor = vec4(color, 1.0);
            return;
        }

        if (actor.enabled != 0u) {
            uint top = cardBottom + 3u;
            if (y >= top) {
                color = vec3(0.032, 0.043, 0.058);
                bool actorText = fixedPixel(pixel, ivec2(int(contentLeft + 8u), int(top + 8u)), 2, 45u) ||
                    numberPixel(pixel, ivec2(int(contentLeft + 48u), int(top + 8u)), 2, actor.health) ||
                    fixedPixel(pixel, ivec2(int(contentLeft + 116u), int(top + 8u)), 2, 46u) ||
                    numberPixel(pixel, ivec2(int(contentLeft + 156u), int(top + 8u)), 2, actor.oxygen) ||
                    fixedPixel(pixel, ivec2(int(contentLeft + 224u), int(top + 8u)), 2, 47u) ||
                    numberPixel(pixel, ivec2(int(contentLeft + 286u), int(top + 8u)), 2, actor.ammo) ||
                    fixedPixel(pixel, ivec2(int(contentLeft + 8u), int(top + 34u)), 2, 60u) ||
                    fixedPixel(pixel, ivec2(int(contentLeft + 174u), int(top + 34u)), 2, 61u);
                if (actorText) color = vec3(0.94, 0.97, 1.0);
                outColor = vec4(color, 1.0);
                return;
            }
        }
        if (text) color = vec3(0.94, 0.97, 1.0);
        outColor = vec4(color, 1.0);
        return;
    }

    uint viewportRight ='''
frag = rx(frag,
    r"    uint sidebarWidth\s*=.*?\n\s*uint viewportRight =",
    sidebar,
    "sidebar replacement")
write("shaders/fullscreen.frag", frag)

# Vulkan CPU/GPU staging for image-backed scene I/O.
renderer = read("src/vulkan_renderer.cpp")
renderer = one(renderer,
    '#include "epoch/sand/scene.hpp"\n',
    '#include "epoch/sand/scene.hpp"\n#include "epoch/sand/scene_image.hpp"\n',
    "renderer scene include")
renderer = one(renderer,
    "    Buffer conservation_buffer{};\n    Buffer ui_text_buffer{};\n",
    "    Buffer conservation_buffer{};\n    Buffer ui_text_buffer{};\n    Buffer scene_staging_buffer{};\n",
    "renderer staging member")
renderer = one(renderer,
    "    bool first_submission_logged{false};\n    bool first_present_logged{false};\n",
    "    bool first_submission_logged{false};\n    bool first_present_logged{false};\n    std::optional<std::uint32_t> pending_scene_export{};\n",
    "renderer pending export")
renderer = one(renderer,
    "            destroy_buffer(ui_text_buffer);\n            destroy_buffer(conservation_buffer);\n",
    "            destroy_buffer(scene_staging_buffer);\n            destroy_buffer(ui_text_buffer);\n            destroy_buffer(conservation_buffer);\n",
    "renderer staging cleanup")
renderer = one(renderer,
    "        const auto light_size = cell_count * sizeof(std::uint32_t);\n",
    "        const auto light_size = cell_count * sizeof(std::uint32_t);\n",
    "renderer buffer marker")
renderer = one(renderer,
    "        for (auto& buffer : cell_buffers) {\n            buffer = create_buffer(cells_size, storage_usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);\n        }\n",
    "        for (auto& buffer : cell_buffers) {\n            buffer = create_buffer(cells_size, storage_usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);\n        }\n        scene_staging_buffer = create_buffer(cells_size,\n            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,\n            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);\n",
    "renderer staging allocation")

scene_methods = r'''    template <typename Recorder>
    void immediate_submit(Recorder&& recorder) {
        const VkCommandBufferAllocateInfo allocate_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        VkCommandBuffer command_buffer{};
        check_vk(vkAllocateCommandBuffers(device, &allocate_info, &command_buffer),
                 "vkAllocateCommandBuffers(scene I/O)");
        try {
            const VkCommandBufferBeginInfo begin_info{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            };
            check_vk(vkBeginCommandBuffer(command_buffer, &begin_info),
                     "vkBeginCommandBuffer(scene I/O)");
            recorder(command_buffer);
            check_vk(vkEndCommandBuffer(command_buffer), "vkEndCommandBuffer(scene I/O)");
            const VkSubmitInfo submit_info{
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .commandBufferCount = 1,
                .pCommandBuffers = &command_buffer,
            };
            check_vk(vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE),
                     "vkQueueSubmit(scene I/O)");
            check_vk(vkQueueWaitIdle(graphics_queue), "vkQueueWaitIdle(scene I/O)");
        } catch (...) {
            vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
            throw;
        }
        vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
    }

    [[nodiscard]] std::filesystem::path scene_directory() const {
        return executable_directory() / "scenes";
    }

    void upload_scene_cells(const std::span<const SceneCell> cells) {
        if (cells.size_bytes() != scene_staging_buffer.size)
            throw std::runtime_error("Scene image produced an unexpected cell count.");
        void* mapped = nullptr;
        check_vk(vkMapMemory(device, scene_staging_buffer.memory, 0,
                             scene_staging_buffer.size, 0, &mapped),
                 "vkMapMemory(scene upload)");
        std::memcpy(mapped, cells.data(), cells.size_bytes());
        vkUnmapMemory(device, scene_staging_buffer.memory);

        immediate_submit([&](const VkCommandBuffer command_buffer) {
            const VkBufferCopy copy{.size = scene_staging_buffer.size};
            for (const auto& destination : cell_buffers) {
                vkCmdCopyBuffer(command_buffer, scene_staging_buffer.handle,
                                destination.handle, 1, &copy);
                buffer_barrier(command_buffer, destination, VK_ACCESS_TRANSFER_WRITE_BIT,
                               VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                               VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            }
            vkCmdFillBuffer(command_buffer, tile_buffer.handle, 0, tile_buffer.size, 0u);
            buffer_barrier(command_buffer, tile_buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            vkCmdFillBuffer(command_buffer, conservation_buffer.handle, 0,
                            conservation_buffer.size, 0u);
            buffer_barrier(command_buffer, conservation_buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        });
        current_set = 0u;
        simulation_step = 0u;
        needs_reset = false;
    }

    [[nodiscard]] bool load_scene_image(const std::uint32_t scene_index) {
        const auto scene = static_cast<Scene>(scene_index % scene_count);
        const auto path = scene_image_path(scene_directory(), scene);
        std::vector<SceneCell> cells(
            static_cast<std::size_t>(config.grid_width) * config.grid_height);
        std::string error;
        if (!load_scene_ppm(path, config.grid_width, config.grid_height, cells, error)) {
            startup_log("Scene image load skipped: " + error);
            return false;
        }
        upload_scene_cells(cells);
        std::string key_error;
        if (!write_scene_material_key(scene_directory(), key_error))
            startup_log("Scene material-key warning: " + key_error);
        startup_log("Loaded moddable scene image: " + path.string());
        return true;
    }

    void save_scene_image(const std::uint32_t scene_index) {
        std::vector<SceneCell> cells(
            static_cast<std::size_t>(config.grid_width) * config.grid_height);
        immediate_submit([&](const VkCommandBuffer command_buffer) {
            buffer_barrier(command_buffer, cell_buffers[current_set],
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_TRANSFER_READ_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT);
            const VkBufferCopy copy{.size = scene_staging_buffer.size};
            vkCmdCopyBuffer(command_buffer, cell_buffers[current_set].handle,
                            scene_staging_buffer.handle, 1, &copy);
            buffer_barrier(command_buffer, scene_staging_buffer,
                           VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT);
        });
        void* mapped = nullptr;
        check_vk(vkMapMemory(device, scene_staging_buffer.memory, 0,
                             scene_staging_buffer.size, 0, &mapped),
                 "vkMapMemory(scene save)");
        std::memcpy(cells.data(), mapped, cells.size() * sizeof(SceneCell));
        vkUnmapMemory(device, scene_staging_buffer.memory);

        const auto scene = static_cast<Scene>(scene_index % scene_count);
        const auto path = scene_image_path(scene_directory(), scene);
        std::string error;
        if (!save_scene_ppm(path, config.grid_width, config.grid_height, cells, error))
            throw std::runtime_error("Unable to save scene image: " + error);
        if (!write_scene_material_key(scene_directory(), error))
            throw std::runtime_error("Unable to save scene material key: " + error);
        startup_log("Saved moddable scene image: " + path.string());
    }

'''
renderer = one(renderer,
    "    void record_reset(const VkCommandBuffer command_buffer, const std::uint32_t scene_index) {\n",
    scene_methods + "    void record_reset(const VkCommandBuffer command_buffer, const std::uint32_t scene_index) {\n",
    "renderer scene methods")
renderer = one(renderer,
    "        check_vk(fence_result, \"vkWaitForFences\");\n\n        std::uint32_t image_index{};\n",
    "        check_vk(fence_result, \"vkWaitForFences\");\n\n"
    "        const auto selected_scene = state.selected_scene.load(std::memory_order_relaxed) % scene_count;\n"
    "        if (state.save_scene_image.exchange(false, std::memory_order_acq_rel)) {\n"
    "            if (!needs_reset) save_scene_image(selected_scene);\n"
    "            else startup_log(\"Scene save skipped until the initial scene exists.\");\n"
    "        }\n"
    "        const bool explicit_load = state.load_scene_image.exchange(false, std::memory_order_acq_rel);\n"
    "        const bool reset_requested = needs_reset || state.reset.exchange(false, std::memory_order_acq_rel);\n"
    "        bool image_loaded = false;\n"
    "        if (explicit_load || reset_requested) {\n"
    "            const auto selected = static_cast<Scene>(selected_scene);\n"
    "            if (scene_image_exists(scene_directory(), selected)) image_loaded = load_scene_image(selected_scene);\n"
    "            else if (explicit_load) startup_log(\"No saved PPM exists for the selected scene.\");\n"
    "        }\n\n"
    "        std::uint32_t image_index{};\n",
    "renderer scene request handling")
renderer = one(renderer,
    "        bool reset_actor = false;\n        bool reset_this_frame = false;\n        if (needs_reset || state.reset.exchange(false, std::memory_order_acq_rel)) {\n            record_reset(frame.command_buffer, state.selected_scene.load(std::memory_order_relaxed));\n            reset_actor = true;\n            reset_this_frame = true;\n        }\n",
    "        bool reset_actor = image_loaded;\n        bool reset_this_frame = image_loaded;\n        if (reset_requested && !image_loaded) {\n            record_reset(frame.command_buffer, selected_scene);\n            pending_scene_export = selected_scene;\n            reset_actor = true;\n            reset_this_frame = true;\n        }\n",
    "renderer reset selection")
renderer = one(renderer,
    "        if (!first_present_logged) {\n            startup_log(\"First frame presented.\");\n            first_present_logged = true;\n        }\n\n        frame_index =",
    "        if (!first_present_logged) {\n            startup_log(\"First frame presented.\");\n            first_present_logged = true;\n        }\n        if (pending_scene_export.has_value()) {\n            const auto scene_to_export = *pending_scene_export;\n            pending_scene_export.reset();\n            save_scene_image(scene_to_export);\n        }\n\n        frame_index =",
    "renderer procedural scene export")
write("src/vulkan_renderer.cpp", renderer)

# Build the new scene codec and ship a writable scene directory.
cmake = read("CMakeLists.txt")
cmake = one(cmake,
    "        src/app.cpp\n        src/vulkan_renderer.cpp\n",
    "        src/app.cpp\n        src/scene_image.cpp\n        src/vulkan_renderer.cpp\n",
    "CMake scene source")
cmake = one(cmake,
    "        COMMAND ${CMAKE_COMMAND} -E copy_if_different\n                ${EPOCH_SAND_SHADER_BINARIES}\n                \"$<TARGET_FILE_DIR:epoch_sand>/shaders\"\n    )\n",
    "        COMMAND ${CMAKE_COMMAND} -E copy_if_different\n                ${EPOCH_SAND_SHADER_BINARIES}\n                \"$<TARGET_FILE_DIR:epoch_sand>/shaders\"\n        COMMAND ${CMAKE_COMMAND} -E make_directory \"$<TARGET_FILE_DIR:epoch_sand>/scenes\"\n        COMMAND ${CMAKE_COMMAND} -E copy_if_different\n                \"${CMAKE_CURRENT_SOURCE_DIR}/scenes/README.txt\"\n                \"$<TARGET_FILE_DIR:epoch_sand>/scenes/README.txt\"\n    )\n",
    "CMake scene assets")
cmake = one(cmake,
    "    install(FILES ${EPOCH_SAND_SHADER_BINARIES} DESTINATION bin/shaders)\n",
    "    install(FILES ${EPOCH_SAND_SHADER_BINARIES} DESTINATION bin/shaders)\n    install(FILES scenes/README.txt DESTINATION bin/scenes)\n",
    "CMake scene install")
cmake = one(cmake,
    "    add_test(NAME epoch_sand_behavior_contract COMMAND epoch_sand_behavior_contract)\nendif()\n",
    "    add_test(NAME epoch_sand_behavior_contract COMMAND epoch_sand_behavior_contract)\n\n"
    "    add_executable(epoch_sand_scene_image_contract tests/scene_image_contract.cpp src/scene_image.cpp)\n"
    "    target_include_directories(epoch_sand_scene_image_contract PRIVATE include)\n"
    "    target_compile_features(epoch_sand_scene_image_contract PRIVATE cxx_std_23)\n"
    "    epoch_sand_configure_warnings(epoch_sand_scene_image_contract)\n"
    "    add_test(NAME epoch_sand_scene_image_contract COMMAND epoch_sand_scene_image_contract)\n"
    "endif()\n",
    "CMake scene test")
write("CMakeLists.txt", cmake)

# Harden malformed image parsing and avoid dead constants under /WX.
scene_cpp = read("src/scene_image.cpp")
scene_cpp = scene_cpp.replace("constexpr std::uint32_t aux_wet = 0x80000000u;\n", "", 1)
scene_cpp = one(scene_cpp,
    "    if (!read_token(stream, token)) { error = \"missing scene width\"; return false; }\n    const auto file_width = static_cast<std::uint32_t>(std::stoul(token));\n    if (!read_token(stream, token)) { error = \"missing scene height\"; return false; }\n    const auto file_height = static_cast<std::uint32_t>(std::stoul(token));\n",
    "    std::uint32_t file_width{};\n    std::uint32_t file_height{};\n    try {\n        if (!read_token(stream, token)) { error = \"missing scene width\"; return false; }\n        file_width = static_cast<std::uint32_t>(std::stoul(token));\n        if (!read_token(stream, token)) { error = \"missing scene height\"; return false; }\n        file_height = static_cast<std::uint32_t>(std::stoul(token));\n    } catch (const std::exception&) {\n        error = \"scene image dimensions are invalid\";\n        return false;\n    }\n",
    "scene dimension parsing")
scene_cpp = one(scene_cpp,
    "#include <limits>\n",
    "#include <limits>\n#include <exception>\n",
    "scene exception include")
write("src/scene_image.cpp", scene_cpp)

print("SandHybrid Fix27 scene images, structural minimum, eraser, and keymap applied.")
