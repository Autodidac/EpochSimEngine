#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(path: str, token: str) -> None:
    text = (ROOT / path).read_text(encoding="utf-8")
    if token not in text:
        raise SystemExit(f"{path}: missing {token!r}")

def reject(path: str, token: str) -> None:
    if token in (ROOT / path).read_text(encoding="utf-8"):
        raise SystemExit(f"{path}: obsolete token remains {token!r}")



# Non-modal map overlay and independent input.
for token in (
    "make_map_overlay_viewport",
    "const bool over_map = map_view_enabled",
    "zoom_at_pointer(shared_state, simulation_config, map_overlay_viewport",
    "pan_drag_map = over_map",
):
    require("src/app.cpp", token)
for token in (
    "map_viewport_left",
    "map_grid_view",
    "pointer_over_map",
):
    require("src/vulkan_renderer.cpp", token)
for token in (
    "bool mapSample = mapOverlayPixel()",
    "mapOverlayBorderPixel",
    "mapOverlayPixel() ? mapCells[index] : cells[index]",
):
    require("shaders/fullscreen.frag", token)

# Workspace ownership and blueprint surfaces.
for token in (
    "material_workspace && epochengine::gui_lib::contains(layout.cursor_circle",
    "shared_state.designer_brush_shape : shared_state.brush_shape",
    "const ui::SimulationViewport designer_grid_viewport{layout.designer_grid",
    "paint_designer_grid(shared_state, designer_grid_viewport",
    "layout.inventory_inventory",
    "layout.inventory_blueprints",
    "designer_grid_columns = 64u",
):
    path = "include/sandhybrid/shared_state.hpp" if token == "designer_grid_columns = 64u" else "src/app.cpp"
    require(path, token)
for token in (
    '"BLUEPRINTS"',
    '"STATIC MODEL"',
    '"MAP CHUNK"',
    '"DESIGNER INVENTORY"',
):
    require("tools/generate_ui_text.py", token)
require("shaders/fullscreen.frag", "renderPc.selectedWorkspace == 3u")
require("missioncache.md", "MC-150")
for token in (
    "layout(std430, binding = 9) readonly buffer DesignerCells",
    "vec4 designerGridColor",
    "designerPlacementMode()",
    "designerBrushRadius()",
    "inventoryPane()",
    "x - contentLeft, y - cardTop",
    "renderPc.selectedWorkspace == 1u &&",
    "renderPc.selectedInventorySlot == slot",
):
    require("shaders/fullscreen.frag", token)
reject("shaders/fullscreen.frag", "if (!mapSample && renderPc.selectedWorkspace == 3u)")
require("tests/ui_layout_contract.cpp", "layout.designer_grid.position.x < sidebar_left")
for token in (
    "record_designer_snapshot",
    "VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT",
    ".designer_flags = designer_flags",
):
    require("src/vulkan_renderer.cpp", token)

# Continuous ground through the authored footprint.
require("include/sandhybrid/world_layout.hpp", "The shared surface crosses the authored footprint")
require("shaders/reset.comp", "bool useSubstrate = substrateCell && (!authored || material == MAT_EMPTY)")
require("src/vulkan_renderer.cpp", "inside_authored && existing != Material::empty")
require("tests/world_layout_contract.cpp", "1500u, 1068u")

# Simulation-time lighting and hard pause/reset.
require("include/sandhybrid/simulation_policy.hpp", "day_cycle_steps = 21'600u")
for token in (
    "float daylightFactor()",
    "float sunArc = cos(cycle * 6.28318530718)",
    "applyMaterialAppearance(cell, grid, renderPc.worldTime, base)",
    "DAY NIGHT CYCLE",
):
    path = "shaders/fullscreen.frag" if token != "DAY NIGHT CYCLE" else "tools/generate_ui_text.py"
    require(path, token)
for token in (
    ".render_frame = simulation_step",
    ".world_time = simulation_step",
    "policy::editor_mutation_allowed(paused, reset_this_frame)",
    "vkCmdFillBuffer(command_buffer, sunlight_buffer.handle",
    "vkCmdCopyBuffer(command_buffer, cell_buffers[0].handle",
):
    require("src/vulkan_renderer.cpp", token)
reject("src/vulkan_renderer.cpp", "Fill ignored while paused.")
reject("src/vulkan_renderer.cpp", "Ignite Air ignored while paused.")
reject("src/vulkan_renderer.cpp", "if (!paused && !reset_this_frame) record_paint")
require("tests/behavior_contract.cpp", "editor_mutation_allowed(true, false)")
for token in (
    "!world_paused",
    "shared_state.move_x.store(world_paused ? 0",
    "reset_map_view(shared_state, simulation_config)",
):
    require("src/app.cpp", token)
for mission in ("MC-151", "MC-152", "EV-2512-004", "EV-2512-005"):
    require("missioncache.md", mission)

print("v2.5.12 map overlay, workspace, terrain, lighting, pause, and reset contracts valid.")
