// Included after deterministic actor setup, before long ecology cycles.
// These are real presented
// frame captures for human visual review, not an automatic visual-acceptance
// claim. Three paused frames must leave canonical cells, actor and clocks alone.
{
    const auto visual_saved_step = simulation_step;
    const auto visual_saved_anchor = tool_hive_anchor;
    const auto visual_saved_capture = pending_frame_capture;
    const auto visual_saved_debug_visible = debug_was_visible;
    const auto visual_saved_debug_frame = debug_sample_frame;
    const auto visual_saved_map_step = map_snapshot_step;
    const auto visual_saved_map_slice = map_snapshot_slice;
    const auto visual_saved_map_visible = map_was_visible;
    const auto visual_saved_flash = nuke_flash_frames_remaining;
    const auto visual_saved_nuke = nuke_dispatch_pending;
    const auto ui_snapshot = [&]() {
        return std::array<std::int64_t, 15u>{
            state.selected_workspace.load(std::memory_order_acquire),
            state.camera_controls.load(std::memory_order_acquire),
            state.camera_zoom.load(std::memory_order_acquire),
            state.camera_center_x.load(std::memory_order_acquire),
            state.camera_center_y.load(std::memory_order_acquire),
            state.map_view.load(std::memory_order_acquire),
            state.map_zoom.load(std::memory_order_acquire),
            state.map_center_x.load(std::memory_order_acquire),
            state.map_center_y.load(std::memory_order_acquire),
            state.debug_visualization.load(std::memory_order_acquire),
            state.debug_page.load(std::memory_order_acquire),
            state.selected_material.load(std::memory_order_acquire),
            state.selected_group.load(std::memory_order_acquire),
            state.paused.load(std::memory_order_acquire),
            state.blueprint_placement_active.load(std::memory_order_acquire)};
    };
    const auto visual_user_ui = ui_snapshot();
    // Isolated input avoids consuming any queued user edit/save/load/actor
    // action. The user's camera, MAP and Debug fields are never overwritten.
    const auto visual_owner = std::make_unique<SharedState>();
    auto& visual_state = *visual_owner;
    visual_state.window_width.store(state.window_width.load(std::memory_order_acquire));
    visual_state.window_height.store(state.window_height.load(std::memory_order_acquire));
    visual_state.selected_scene.store(static_cast<std::uint32_t>(world_scene));
    visual_state.selected_workspace.store(1u);
    visual_state.selected_material.store(material_id(Material::beehive));
    visual_state.selected_group.store(static_cast<std::uint32_t>(MaterialGroup::colony));
    visual_state.paused.store(true);
    visual_state.camera_controls.store(true);
    visual_state.camera_zoom.store(8u);
    visual_state.camera_center_x.store(672);
    visual_state.camera_center_y.store(740);
    visual_state.map_zoom.store(16u);
    visual_state.map_center_x.store(672);
    visual_state.map_center_y.store(740);
    visual_state.mouse_x.store(-1);
    visual_state.mouse_y.store(-1);
    visual_state.designer_dirty.store(false);
    visual_state.debug_visualization.store(false);
    visual_state.map_view.store(false);
    nuke_flash_frames_remaining = 0u;
    nuke_dispatch_pending = false;
    tool_hive_anchor = no_tool_hive_anchor;

    // First Queen is inside district0. The others are in its gap/high sky;
    // even Compact district1 starts at y744, so y740 uses global-home metadata.
    // Separation80 keeps all bodies/crescents independent and simultaneously
    // visible inside the camera's320x180-cell rectangle at zoom8.
    constexpr std::array<BeeHome, 3u> visual_homes{{
        {592u, 740u}, {672u, 740u}, {752u, 740u}}};
    auto visual_cells_before = acceptance_atmosphere_world();
    upload_scene_cells(visual_cells_before);
    for (const auto home : visual_homes) {
        immediate_submit([&](const VkCommandBuffer command_buffer) {
            record_paint_at_grid(command_buffer, visual_state, false, true,
                static_cast<std::int32_t>(home.x), static_cast<std::int32_t>(home.y),
                material_id(Material::beehive));
        });
    }
    visual_cells_before = download_scene_cells();
    const auto visual_actor_before = download_actor_state();
    const auto visual_step_before = simulation_step;
    const auto visual_latest_anchor = (visual_homes.back().x & 0xffffu) |
        ((visual_homes.back().y & 0xffffu) << 16u);
    bool visual_fixture_exact = tool_hive_anchor == visual_latest_anchor &&
        count_material(visual_cells_before, Material::bee) == 180u &&
        count_material(visual_cells_before, Material::queen_bee) == 3u;
    for (const auto home : visual_homes) {
        visual_fixture_exact = visual_fixture_exact && canonical_fix29_hive_signature_at(
            visual_cells_before, config.grid_width, config.grid_height, home.x, home.y);
    }

    // The fresh upload already seeded the entire MAP with Atmosphere. Publish
    // only the edited hive row band into that snapshot now: paused draw_frame
    // must not advance the rolling MAP clock or refresh it implicitly.
    immediate_submit([&](const VkCommandBuffer command_buffer) {
        constexpr std::uint32_t first_row = 704u;
        constexpr std::uint32_t row_count = 80u;
        record_map_snapshot_rows(command_buffer, first_row, row_count, current_set);
    });
    const auto visual_map_step_before = map_snapshot_step;
    const auto visual_map_slice_before = map_snapshot_slice;
    const std::filesystem::path visual_report{config.runtime_acceptance_report};
    const auto visual_directory = visual_report.parent_path() /
        (visual_report.stem().string() + "-hive-frames");
    const std::array<std::filesystem::path, 3u> visual_paths{{
        visual_directory / "colonies-camera.bmp",
        visual_directory / "colonies-map.bmp",
        visual_directory / "colonies-camera-no-anchor.bmp"}};
    bool visual_captured = frame_capture_buffer.handle != VK_NULL_HANDLE;
    std::uint32_t visual_frames = 0u;
    if (visual_captured) {
        for (std::size_t frame = 0u; frame < visual_paths.size(); ++frame) {
            visual_state.map_view.store(frame == 1u);
            tool_hive_anchor = frame == 2u ? no_tool_hive_anchor : visual_latest_anchor;
            pending_frame_capture = visual_paths[frame];
            const bool presented = draw_frame(visual_state, 0u, true);
            ++visual_frames;
            std::error_code capture_error;
            const auto capture_bytes = std::filesystem::file_size(visual_paths[frame], capture_error);
            visual_captured = visual_captured && presented && !pending_frame_capture.has_value() &&
                !capture_error && capture_bytes > 54u;
            if (!presented) {
                pending_frame_capture.reset();
                break; // No unbounded retry or additional presentation work.
            }
        }
    }
    const auto visual_cells_after = download_scene_cells();
    const auto visual_actor_after = download_actor_state();
    const bool visual_unchanged = visual_cells_after.size() == visual_cells_before.size() &&
        std::memcmp(visual_cells_after.data(), visual_cells_before.data(),
            visual_cells_before.size() * sizeof(SceneCell)) == 0 &&
        std::memcmp(&visual_actor_after, &visual_actor_before, sizeof(visual_actor_before)) == 0 &&
        simulation_step == visual_step_before && map_snapshot_step == visual_map_step_before &&
        map_snapshot_slice == visual_map_slice_before && ui_snapshot() == visual_user_ui &&
        nuke_flash_frames_remaining == 0u && !nuke_dispatch_pending;
    append("beehive_three_paused_presented_frames_preserve_world_actor_and_ui",
        visual_fixture_exact && visual_captured && visual_frames == 3u && visual_unchanged,
        "three actual gold-hive candidates; normal, MAP, no singleton anchor; frames=" +
            std::to_string(visual_frames) + " exact canonical/actor/clocks/UI=" +
            std::to_string(visual_unchanged ? 1u : 0u) +
            "; captures require human visual review: " + visual_paths[0].string() + " | " +
            visual_paths[1].string() + " | " + visual_paths[2].string());

    // A previous (not authored, not latest-tool) hive must retain its exact
    // MAP picture after a live edit invalidates its Queen index. Comparing
    // MAP pixels alone excludes the intentionally changed live World view.
    visual_state.map_view.store(true);
    tool_hive_anchor = no_tool_hive_anchor;
    const auto frozen_map_before = download_map_snapshot_cells();
    const auto frozen_step = simulation_step;
    const auto frozen_map_step = map_snapshot_step;
    const auto frozen_map_slice = map_snapshot_slice;
    std::size_t frozen_first_hive_pixels = 0u;
    const auto capture_map_pixels = [&](const std::filesystem::path& path) {
        std::vector<std::uint8_t> pixels;
        if (frame_capture_buffer.handle == VK_NULL_HANDLE) return pixels;
        pending_frame_capture = path;
        const bool presented = draw_frame(visual_state, 0u, true);
        if (!presented || pending_frame_capture.has_value()) {
            pending_frame_capture.reset();
            return pixels;
        }
        std::vector<std::uint8_t> frame_bytes(
            static_cast<std::size_t>(frame_capture_buffer.size));
        void* mapped = nullptr;
        check_vk(vkMapMemory(device, frame_capture_buffer.memory, 0u,
            frame_capture_buffer.size, 0u, &mapped),
            "vkMapMemory(paused MAP comparison)");
        std::memcpy(frame_bytes.data(), mapped, frame_bytes.size());
        vkUnmapMemory(device, frame_capture_buffer.memory);
        const auto logical_width = (std::max)(
            visual_state.window_width.load(std::memory_order_relaxed), 1u);
        const auto logical_height = (std::max)(
            visual_state.window_height.load(std::memory_order_relaxed), 1u);
        const auto layout = ui::make_layout(logical_width, logical_height);
        const auto view = map_grid_view(visual_state);
        const auto viewport = ui::make_map_overlay_viewport(layout, view.width, view.height);
        const auto left = static_cast<std::uint32_t>(viewport.rect.position.x);
        const auto top = static_cast<std::uint32_t>(viewport.rect.position.y);
        const auto right = left + static_cast<std::uint32_t>(viewport.rect.size.x);
        const auto bottom = top + static_cast<std::uint32_t>(viewport.rect.size.y);
        // Match logicalWindowPixel's physical pixel-center conversion. The
        // capture may be scaled/high-DPI; UI logical coordinates are not bytes.
        for (std::uint32_t y = 0u; y < swapchain_extent.height; ++y) {
            const auto logical_y = static_cast<std::uint32_t>(
                (static_cast<float>(y) + 0.5f) * static_cast<float>(logical_height) /
                static_cast<float>(swapchain_extent.height));
            if (logical_y < top || logical_y >= bottom) continue;
            for (std::uint32_t x = 0u; x < swapchain_extent.width; ++x) {
                const auto logical_x = static_cast<std::uint32_t>(
                    (static_cast<float>(x) + 0.5f) * static_cast<float>(logical_width) /
                    static_cast<float>(swapchain_extent.width));
                if (logical_x < left || logical_x >= right) continue;
                const auto grid_x = view.origin_x +
                    (logical_x - left) * view.width / (right - left);
                const auto grid_y = view.origin_y +
                    (logical_y - top) * view.height / (bottom - top);
                if (grid_x + 10u >= visual_homes.front().x &&
                    grid_x <= visual_homes.front().x + 11u &&
                    grid_y + 13u >= visual_homes.front().y &&
                    grid_y <= visual_homes.front().y + 11u)
                    ++frozen_first_hive_pixels;
                const auto offset = (static_cast<std::size_t>(y) *
                    swapchain_extent.width + x) * 4u;
                pixels.insert(pixels.end(), frame_bytes.begin() + offset,
                    frame_bytes.begin() + offset + 4u);
            }
        }
        return pixels;
    };
    const auto frozen_pixels_before = capture_map_pixels(
        visual_directory / "colonies-map-before-live-erase.bmp");
    auto live_without_first_queen = visual_cells_before;
    const auto erased_queen_index = static_cast<std::uint32_t>(
        static_cast<std::size_t>(visual_homes.front().y) * config.grid_width +
        visual_homes.front().x);
    live_without_first_queen[erased_queen_index] = visual_cells_before[
        static_cast<std::size_t>(100u) * config.grid_width + 100u];
    upload_bounded_cells(live_without_first_queen, {erased_queen_index},
        "Paused MAP earlier-hive removal fixture");
    const auto live_tiles_without_first = download_tile_states();
    const auto erased_queen_tile =
        (visual_homes.front().y / 8u) * divide_round_up(config.grid_width, 8u) +
        visual_homes.front().x / 8u;
    const auto frozen_pixels_after = capture_map_pixels(
        visual_directory / "colonies-map-after-live-erase.bmp");
    const auto frozen_map_after = download_map_snapshot_cells();
    const auto edited_live_cells = download_scene_cells();
    const auto edited_actor = download_actor_state();
    const bool live_queen_removed =
        count_material(edited_live_cells, Material::queen_bee) == 2u &&
        (live_tiles_without_first[erased_queen_tile].flags & 0x00000100u) == 0u &&
        std::memcmp(edited_live_cells.data(), live_without_first_queen.data(),
            edited_live_cells.size() * sizeof(SceneCell)) == 0;
    const bool frozen_map_exact = frozen_map_after.size() == frozen_map_before.size() &&
        std::memcmp(frozen_map_after.data(), frozen_map_before.data(),
            frozen_map_after.size() * sizeof(SceneCell)) == 0 &&
        frozen_map_after[erased_queen_index].material == material_id(Material::queen_bee) &&
        frozen_first_hive_pixels > 0u && !frozen_pixels_before.empty() &&
        frozen_pixels_before == frozen_pixels_after;
    append("beehive_paused_map_snapshot_survives_earlier_live_hive_removal",
        visual_fixture_exact && visual_captured && live_queen_removed && frozen_map_exact &&
        std::memcmp(&edited_actor, &visual_actor_before, sizeof(edited_actor)) == 0 &&
        simulation_step == frozen_step && map_snapshot_step == frozen_map_step &&
        map_snapshot_slice == frozen_map_slice &&
        tool_hive_anchor == no_tool_hive_anchor && ui_snapshot() == visual_user_ui,
        "first non-authored/non-latest Queen removed from live cells/index=" +
            std::to_string(live_queen_removed ? 1u : 0u) +
            "; paused MAP cells/pixels exact=" + std::to_string(frozen_map_exact ? 1u : 0u) +
            "; compared MAP bytes=" + std::to_string(frozen_pixels_before.size()) +
            "; earlier-hive pixel samples=" + std::to_string(frozen_first_hive_pixels) +
            "; actor and clocks unchanged; two actual presented captures retained");

    pending_frame_capture = visual_saved_capture;
    tool_hive_anchor = visual_saved_anchor;
    simulation_step = visual_saved_step;
    debug_was_visible = visual_saved_debug_visible;
    debug_sample_frame = visual_saved_debug_frame;
    map_snapshot_step = visual_saved_map_step;
    map_snapshot_slice = visual_saved_map_slice;
    map_was_visible = visual_saved_map_visible;
    nuke_flash_frames_remaining = visual_saved_flash;
    nuke_dispatch_pending = visual_saved_nuke;
}
