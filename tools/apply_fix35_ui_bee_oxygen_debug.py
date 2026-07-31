from __future__ import annotations

from pathlib import Path


def read(path: str) -> str:
    return Path(path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    Path(path).write_text(text, encoding="utf-8")


def replace_once(path: str, old: str, new: str) -> None:
    text = read(path)
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"Expected one marker in {path}, found {count}: {old[:160]!r}")
    write(path, text.replace(old, new, 1))


def replace_between(path: str, start: str, end: str, replacement: str) -> None:
    text = read(path)
    start_index = text.find(start)
    if start_index < 0:
        raise RuntimeError(f"Missing start marker in {path}: {start[:160]!r}")
    end_index = text.find(end, start_index + len(start))
    if end_index < 0:
        raise RuntimeError(f"Missing end marker in {path}: {end[:160]!r}")
    if text.find(start, start_index + len(start)) >= 0:
        raise RuntimeError(f"Start marker is not unique in {path}: {start[:160]!r}")
    write(path, text[:start_index] + replacement + text[end_index:])


# The cursor editor was rendered after a 126-pixel keymap but hit-tested after
# a 108-pixel keymap. Align the input layout and make the +/- targets wider.
replace_once(
    "include/epoch/sand/ui_layout.hpp",
    "inline constexpr std::uint32_t keymap_height = 108u;\n",
    "inline constexpr std::uint32_t keymap_height = 126u;\n",
)
replace_once(
    "include/epoch/sand/ui_layout.hpp",
    "    const float control_top = cursor_top + 60.0f;\n"
    "    const float half = content_width / 2.0f;\n"
    "    layout.brush_smaller = {{content_left + 4.0f, control_top}, {30.0f, 26.0f}};\n"
    "    layout.brush_larger = {{content_left + half - 34.0f, control_top}, {30.0f, 26.0f}};\n"
    "    layout.zoom_out = {{content_left + half + 4.0f, control_top}, {30.0f, 26.0f}};\n"
    "    layout.zoom_in = {{content_left + content_width - 34.0f, control_top}, {30.0f, 26.0f}};\n",
    "    const float control_top = cursor_top + 60.0f;\n"
    "    const float half = content_width / 2.0f;\n"
    "    constexpr float control_button_width = 44.0f;\n"
    "    layout.brush_smaller = {{content_left + 4.0f, control_top}, {control_button_width, 26.0f}};\n"
    "    layout.brush_larger = {{content_left + half - control_button_width - 4.0f, control_top},\n"
    "                           {control_button_width, 26.0f}};\n"
    "    layout.zoom_out = {{content_left + half + 4.0f, control_top}, {control_button_width, 26.0f}};\n"
    "    layout.zoom_in = {{content_left + content_width - control_button_width - 4.0f, control_top},\n"
    "                      {control_button_width, 26.0f}};\n",
)

# The visible eraser is an atmosphere/vacuum replacement tool, not a silent
# material deletion path. Selecting it paints oxygen, and right-click erasing
# uses the same canonical oxygen state.
replace_once(
    "src/app.cpp",
    "            } else if (epochengine::gui_lib::contains(layout.eraser, pointer)) {\n"
    "                shared_state.selected_material.store(static_cast<std::uint32_t>(Material::empty),\n"
    "                                                     std::memory_order_relaxed);\n",
    "            } else if (epochengine::gui_lib::contains(layout.eraser, pointer)) {\n"
    "                shared_state.selected_material.store(static_cast<std::uint32_t>(Material::oxygen),\n"
    "                                                     std::memory_order_relaxed);\n",
)
replace_once(
    "src/vulkan_renderer.cpp",
    "        const auto material = erase ? static_cast<std::uint32_t>(Material::empty)\n"
    "                                    : state.selected_material.load(std::memory_order_relaxed);\n",
    "        const auto material = erase ? static_cast<std::uint32_t>(Material::oxygen)\n"
    "                                    : state.selected_material.load(std::memory_order_relaxed);\n",
)

# Win32 button messages carry the authoritative click coordinates. Updating
# only on WM_MOUSEMOVE made fast clicks and captured clicks hit stale UI cells.
for message in ("WM_LBUTTONDOWN", "WM_MBUTTONDOWN", "WM_RBUTTONDOWN"):
    replace_once(
        "src/window_win32.cpp",
        f"        case {message}:\n",
        f"        case {message}:\n"
        "            self->mouse_x = GET_X_LPARAM(lparam);\n"
        "            self->mouse_y = GET_Y_LPARAM(lparam);\n",
    )
for message in ("WM_LBUTTONUP", "WM_MBUTTONUP", "WM_RBUTTONUP"):
    replace_once(
        "src/window_win32.cpp",
        f"        case {message}:\n",
        f"        case {message}:\n"
        "            self->mouse_x = GET_X_LPARAM(lparam);\n"
        "            self->mouse_y = GET_Y_LPARAM(lparam);\n",
    )
replace_once(
    "src/window_win32.cpp",
    "        case WM_SYSKEYDOWN:\n"
    "        case WM_KEYDOWN:\n",
    "        case WM_CAPTURECHANGED:\n"
    "            if (reinterpret_cast<HWND>(lparam) != hwnd) {\n"
    "                self->primary_down = false;\n"
    "                self->secondary_down = false;\n"
    "                self->middle_down = false;\n"
    "            }\n"
    "            return 0;\n"
    "        case WM_KILLFOCUS:\n"
    "            self->primary_down = false;\n"
    "            self->secondary_down = false;\n"
    "            self->middle_down = false;\n"
    "            self->move_left = false;\n"
    "            self->move_right = false;\n"
    "            self->move_up = false;\n"
    "            self->move_down = false;\n"
    "            self->jump = false;\n"
    "            self->inspect_material = false;\n"
    "            return 0;\n"
    "        case WM_SYSKEYDOWN:\n"
    "        case WM_KEYDOWN:\n",
)

# Mirror release-coordinate and focus-loss cleanup on XCB.
replace_once(
    "src/window_xcb.cpp",
    "        XCB_EVENT_MASK_BUTTON_RELEASE |\n"
    "        XCB_EVENT_MASK_KEY_PRESS |\n",
    "        XCB_EVENT_MASK_BUTTON_RELEASE |\n"
    "        XCB_EVENT_MASK_FOCUS_CHANGE |\n"
    "        XCB_EVENT_MASK_KEY_PRESS |\n",
)
replace_once(
    "src/window_xcb.cpp",
    "        case XCB_BUTTON_RELEASE: {\n"
    "            const auto* button = reinterpret_cast<xcb_button_release_event_t*>(event);\n",
    "        case XCB_BUTTON_RELEASE: {\n"
    "            const auto* button = reinterpret_cast<xcb_button_release_event_t*>(event);\n"
    "            impl_->mouse_x = button->event_x;\n"
    "            impl_->mouse_y = button->event_y;\n",
)
replace_once(
    "src/window_xcb.cpp",
    "        case XCB_KEY_PRESS: {\n",
    "        case XCB_FOCUS_OUT:\n"
    "            impl_->primary_down = false;\n"
    "            impl_->secondary_down = false;\n"
    "            impl_->middle_down = false;\n"
    "            impl_->move_left = false;\n"
    "            impl_->move_right = false;\n"
    "            impl_->move_up = false;\n"
    "            impl_->move_down = false;\n"
    "            impl_->jump = false;\n"
    "            impl_->inspect_material = false;\n"
    "            break;\n"
    "        case XCB_KEY_PRESS: {\n",
)

# Explicitly return to biohazard between the two short alternate formations.
replace_once(
    "shaders/bee_swarm.glsl",
    "const uint BEE_SWARM_CYCLE_TICKS =\n"
    "    BEE_SWARM_BIOHAZARD_TICKS + BEE_SWARM_ALTERNATE_TICKS * 2u;\n",
    "const uint BEE_SWARM_PHASE_TICKS =\n"
    "    BEE_SWARM_BIOHAZARD_TICKS + BEE_SWARM_ALTERNATE_TICKS;\n"
    "const uint BEE_SWARM_CYCLE_TICKS = BEE_SWARM_PHASE_TICKS * 2u;\n",
)
replace_once(
    "shaders/bee_swarm.glsl",
    "uint beeSwarmState(uint aux, uint step) {\n"
    "    uint local = step % BEE_SWARM_CYCLE_TICKS;\n"
    "    if (local < BEE_SWARM_BIOHAZARD_TICKS) return 0u;\n"
    "    ivec2 home = beeHomeCenterFromAux(aux);\n"
    "    uint cycle = step / BEE_SWARM_CYCLE_TICKS;\n"
    "    bool reverse = (beeHash32(uint(home.x) * 73856093u ^ uint(home.y) * 19349663u ^ cycle) & 1u) != 0u;\n"
    "    uint alternate = (local - BEE_SWARM_BIOHAZARD_TICKS) / BEE_SWARM_ALTERNATE_TICKS;\n"
    "    return reverse ? 2u - alternate : 1u + alternate;\n"
    "}\n",
    "uint beeSwarmState(uint aux, uint step) {\n"
    "    uint local = step % BEE_SWARM_CYCLE_TICKS;\n"
    "    uint phase = local / BEE_SWARM_PHASE_TICKS;\n"
    "    uint phaseLocal = local % BEE_SWARM_PHASE_TICKS;\n"
    "    if (phaseLocal < BEE_SWARM_BIOHAZARD_TICKS) return 0u;\n"
    "    ivec2 home = beeHomeCenterFromAux(aux);\n"
    "    uint cycle = step / BEE_SWARM_CYCLE_TICKS;\n"
    "    bool reverse = (beeHash32(uint(home.x) * 73856093u ^ uint(home.y) * 19349663u ^ cycle) & 1u) != 0u;\n"
    "    return reverse ? 2u - phase : 1u + phase;\n"
    "}\n",
)

# Every authored scene starts pressurized. Empty remains available internally
# for explicit simulation products and loaded/modded scene images.
replace_once(
    "shaders/reset.comp",
    "    uint scene = pc.material % SCENE_COUNT;\n"
    "    uint material = materialForScene(scene, p);\n"
    "    bool structural = authoredStructuralCell(scene, p, material);\n",
    "    uint scene = pc.material % SCENE_COUNT;\n"
    "    uint material = materialForScene(scene, p);\n"
    "    if (material == MAT_EMPTY) material = MAT_OXYGEN;\n"
    "    bool structural = authoredStructuralCell(scene, p, material);\n",
)

# Debug collection is a sampled diagnostic, not a full-grid pass every frame.
replace_once(
    "src/vulkan_renderer.cpp",
    "    bool first_present_logged{false};\n"
    "    std::optional<std::uint32_t> pending_scene_export{};\n",
    "    bool first_present_logged{false};\n"
    "    bool debug_was_visible{};\n"
    "    std::uint32_t debug_sample_frame{};\n"
    "    std::optional<std::uint32_t> pending_scene_export{};\n",
)
replace_once(
    "src/vulkan_renderer.cpp",
    "        const bool debug_stats = state.debug_visualization.load(std::memory_order_relaxed);\n"
    "        if (debug_stats) reset_debug_stats(frame.command_buffer);\n"
    "        const bool step_once = state.single_step.exchange(false, std::memory_order_acq_rel);\n"
    "        const bool run_simulation = !reset_this_frame && (step_once ||\n"
    "            (simulation_tick && !state.paused.load(std::memory_order_relaxed)));\n"
    "        if (run_simulation) record_simulation_step(frame.command_buffer, debug_stats);\n",
    "        const bool debug_visible = state.debug_visualization.load(std::memory_order_relaxed);\n"
    "        bool collect_debug_stats = false;\n"
    "        if (debug_visible) {\n"
    "            collect_debug_stats = !debug_was_visible || (debug_sample_frame % 8u) == 0u;\n"
    "            ++debug_sample_frame;\n"
    "        } else {\n"
    "            debug_sample_frame = 0u;\n"
    "        }\n"
    "        debug_was_visible = debug_visible;\n"
    "        if (collect_debug_stats) reset_debug_stats(frame.command_buffer);\n"
    "        const bool step_once = state.single_step.exchange(false, std::memory_order_acq_rel);\n"
    "        const bool run_simulation = !reset_this_frame && (step_once ||\n"
    "            (simulation_tick && !state.paused.load(std::memory_order_relaxed)));\n"
    "        if (run_simulation) record_simulation_step(frame.command_buffer, collect_debug_stats);\n",
)
replace_once(
    "src/vulkan_renderer.cpp",
    "        if (debug_stats) {\n"
    "            const auto movement_pair_tests = run_simulation\n"
    "                ? config.grid_width * config.grid_height * 9u / 2u\n"
    "                : 0u;\n"
    "            record_debug_stats(frame.command_buffer, state, movement_pair_tests);\n"
    "        }\n",
    "        if (collect_debug_stats) {\n"
    "            const auto movement_pair_tests = run_simulation\n"
    "                ? config.grid_width * config.grid_height * 13u / 2u\n"
    "                : 0u;\n"
    "            record_debug_stats(frame.command_buffer, state, movement_pair_tests);\n"
    "        }\n",
)

# Avoid tile/chunk buffer reads outside debug mode.
replace_once(
    "shaders/fullscreen.frag",
    "    Cell cell = cellAt(grid);\n"
    "    TileState tile = tileAt(grid);\n"
    "    ChunkState chunk = chunkAt(grid);\n"
    "    vec4 color = worldColor(cell, grid);\n\n"
    "    if (renderPc.debugMode != 0u) {\n",
    "    Cell cell = cellAt(grid);\n"
    "    vec4 color = worldColor(cell, grid);\n\n"
    "    if (renderPc.debugMode != 0u) {\n"
    "        TileState tile = tileAt(grid);\n"
    "        ChunkState chunk = chunkAt(grid);\n",
)

# Render actual wide control buttons matching the corrected hitboxes.
replace_once(
    "shaders/fullscreen.frag",
    "            uint controlTop = cursorTop + 60u;\n"
    "            uint halfWidth = contentWidth / 2u;\n"
    "            cursorText = cursorText || fixedPixel(pixel, ivec2(int(contentLeft + 39u), int(controlTop + 2u)), 1, 104u) ||\n",
    "            uint controlTop = cursorTop + 60u;\n"
    "            uint halfWidth = contentWidth / 2u;\n"
    "            const uint controlButtonWidth = 44u;\n"
    "            uint brushMinusLeft = contentLeft + 4u;\n"
    "            uint brushPlusLeft = contentLeft + halfWidth - controlButtonWidth - 4u;\n"
    "            uint zoomMinusLeft = contentLeft + halfWidth + 4u;\n"
    "            uint zoomPlusLeft = contentLeft + contentWidth - controlButtonWidth - 4u;\n"
    "            uint buttonLefts[4] = uint[4](brushMinusLeft, brushPlusLeft, zoomMinusLeft, zoomPlusLeft);\n"
    "            for (uint button = 0u; button < 4u; ++button) {\n"
    "                uint left = buttonLefts[button];\n"
    "                uint right = left + controlButtonWidth;\n"
    "                if (x >= left && x < right && y >= controlTop && y < controlTop + 26u) {\n"
    "                    color = vec3(0.075, 0.105, 0.145);\n"
    "                    if (borderPixel(x, y, left, controlTop, right, controlTop + 26u)) color *= 0.55;\n"
    "                }\n"
    "            }\n"
    "            cursorText = cursorText || fixedPixel(pixel, ivec2(int(contentLeft + 53u), int(controlTop + 2u)), 1, 104u) ||\n",
)
replace_once(
    "shaders/fullscreen.frag",
    "                fixedPixel(pixel, ivec2(int(contentLeft + halfWidth + 39u), int(controlTop + 2u)), 1, 105u) ||\n",
    "                fixedPixel(pixel, ivec2(int(contentLeft + halfWidth + 53u), int(controlTop + 2u)), 1, 105u) ||\n",
)
replace_once(
    "shaders/fullscreen.frag",
    "            bool minusLeft = glyphPixel(pixel, ivec2(int(contentLeft + 13u), int(controlTop + 9u)), 2, 45u);\n"
    "            bool plusLeft = glyphPixel(pixel, ivec2(int(contentLeft + halfWidth - 25u), int(controlTop + 9u)), 2, 43u);\n"
    "            bool minusRight = glyphPixel(pixel, ivec2(int(contentLeft + halfWidth + 13u), int(controlTop + 9u)), 2, 45u);\n"
    "            bool plusRight = glyphPixel(pixel, ivec2(int(contentLeft + contentWidth - 25u), int(controlTop + 9u)), 2, 43u);\n",
    "            bool minusLeft = glyphPixel(pixel, ivec2(int(brushMinusLeft + 17u), int(controlTop + 6u)), 2, 45u);\n"
    "            bool plusLeft = glyphPixel(pixel, ivec2(int(brushPlusLeft + 17u), int(controlTop + 6u)), 2, 43u);\n"
    "            bool minusRight = glyphPixel(pixel, ivec2(int(zoomMinusLeft + 17u), int(controlTop + 6u)), 2, 45u);\n"
    "            bool plusRight = glyphPixel(pixel, ivec2(int(zoomPlusLeft + 17u), int(controlTop + 6u)), 2, 43u);\n",
)

# Replace the six-column full-width diagnostic slab with an adaptive compact
# 4x3 panel (2x6 on narrow viewports) containing the useful live counters.
debug_start = (
    "    if (renderPc.debugMode != 0u) {\n"
    "        uint panelLeft = renderPc.viewportLeft + 4u;\n"
)
debug_end = (
    "    if (actor.enabled != 0u && actor.health != 0u && actor.shotTimer > 0u) {\n"
)
compact_debug = """    if (renderPc.debugMode != 0u) {
        uint availableWidth = max(renderPc.viewportWidth > 8u ? renderPc.viewportWidth - 8u
                                                              : renderPc.viewportWidth, 1u);
        uint panelWidth = min(360u, availableWidth);
        uint panelLeft = renderPc.viewportLeft + 4u;
        uint panelRight = panelLeft + panelWidth;
        uint panelTop = renderPc.viewportTop + 4u;
        bool narrowPanel = panelWidth < 280u;
        uint columns = narrowPanel ? 2u : 4u;
        uint rows = narrowPanel ? 6u : 3u;
        uint panelBottom = min(viewportBottom, panelTop + 23u + rows * 14u);
        if (x >= panelLeft && x < panelRight && y >= panelTop && y < panelBottom) {
            color.rgb = mix(color.rgb, vec3(0.018, 0.027, 0.040), 0.88);
            if (borderPixel(x, y, panelLeft, panelTop, panelRight, panelBottom))
                color.rgb = vec3(0.16, 0.30, 0.42);

            bool statsText = fixedPixel(pixel, ivec2(int(panelLeft + 7u), int(panelTop + 4u)), 1, 75u);
            uint columnWidth = max((panelWidth - 12u) / columns, 1u);
            uint fixedLabels[8] = uint[8](1u, 76u, 80u, 79u, 82u, 83u, 98u, 81u);
            uint fixedValues[8] = uint[8](
                renderPc.framesPerSecond,
                debugStats[STAT_SIMULATION_STEP],
                debugStats[STAT_ACTIVE_CELLS],
                debugStats[STAT_MOVED_CELLS],
                debugStats[STAT_BEE_COUNT],
                debugStats[STAT_BEE_MOVES],
                debugStats[STAT_ACTIVE_TILES],
                debugStats[STAT_SLEEPING_TILES]);
            for (uint stat = 0u; stat < 8u; ++stat) {
                uint column = stat % columns;
                uint row = stat / columns;
                statsText = statsText || statPixel(
                    pixel,
                    ivec2(int(panelLeft + 7u + column * columnWidth),
                          int(panelTop + 19u + row * 14u)),
                    fixedLabels[stat], fixedValues[stat]);
            }
            uint hierarchyValues[4] = uint[4](
                debugStats[STAT_SLEEPING_CHUNKS],
                debugStats[STAT_ACTIVE_CHUNKS],
                debugStats[STAT_MACRO_TILE_MOVES],
                debugStats[STAT_MACRO_CELL_MOVES]);
            for (uint stat = 0u; stat < 4u; ++stat) {
                uint slot = stat + 8u;
                uint column = slot % columns;
                uint row = slot / columns;
                statsText = statsText || hierarchyStatPixel(
                    pixel,
                    ivec2(int(panelLeft + 7u + column * columnWidth),
                          int(panelTop + 19u + row * 14u)),
                    stat, hierarchyValues[stat]);
            }
            if (statsText) color.rgb = vec3(0.94, 0.98, 1.0);
            outColor = vec4(color.rgb, 1.0);
            return;
        }
    }

"""
replace_between("shaders/fullscreen.frag", debug_start, debug_end, compact_debug)

print("Applied Fix35 UI, bee-cycle, oxygen-atmosphere, and compact-debug fixes.")
