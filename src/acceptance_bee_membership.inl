// Included inside run_runtime_acceptance, after acceptance_bee_colonies.inl.
// Exercise actual cross-tile Bee movement, then move the simulation focus away
// before a real birth opportunity. Do not manually manufacture the candidate
// flag: the movement kernel must publish it despite a zero classified count.
{
    const auto membership_saved_step = simulation_step;
    const auto membership_saved_anchor = tool_hive_anchor;
    constexpr BeeHome membership_home{160u, 920u};
    constexpr std::uint32_t membership_slot = 7u;
    constexpr std::uint32_t membership_moved = 0x01000000u;
    constexpr std::uint32_t membership_has_bees = 0x00001000u;
    constexpr std::uint32_t membership_swarm = 0x08000000u;
    constexpr std::uint32_t membership_count_shift = 25u;
    // Section5 is outside the entire four-section-wide home focus(0,2), not
    // merely outside a clipped test dispatch. The one-cell move crosses a tile edge.
    constexpr std::uint32_t far_x = 3208u;
    constexpr std::uint32_t far_y = 900u;
    const auto far_source_index = index_of(far_x, far_y);
    const auto far_target_index = index_of(far_x - 1u, far_y);
    const auto formation = fix29_bee_formation_offset(membership_slot);
    const auto missing_index = index_of(
        static_cast<std::uint32_t>(static_cast<std::int32_t>(membership_home.x) + formation.x),
        static_cast<std::uint32_t>(static_cast<std::int32_t>(membership_home.y) + formation.y));
    const auto tile_columns = divide_round_up(config.grid_width, 8u);
    const auto far_source_tile = (far_y / 8u) * tile_columns + far_x / 8u;
    const auto far_target_tile = (far_y / 8u) * tile_columns + (far_x - 1u) / 8u;

    auto membership_seed = acceptance_atmosphere_world();
    upload_scene_cells(membership_seed);
    immediate_submit([&](const VkCommandBuffer command_buffer) {
        record_paint_at_grid(command_buffer, state, false, true,
            static_cast<std::int32_t>(membership_home.x),
            static_cast<std::int32_t>(membership_home.y), material_id(Material::beehive));
    });
    membership_seed = download_scene_cells();
    const auto initial_owner = membership_seed[missing_index];
    const bool membership_valid_hive =
        canonical_fix29_hive_signature_at(membership_seed, config.grid_width,
            config.grid_height, membership_home.x, membership_home.y) &&
        count_material(membership_seed, Material::bee) == 60u &&
        initial_owner.material == material_id(Material::bee) &&
        bee_slot_from_metadata(initial_owner.aux, config.grid_width, config.grid_height) ==
            membership_slot;
    // Set up an already travelling owner and an unrelated Ash in its vacant
    // formation slot. No owner is killed: the colony still contains exactly60.
    auto travelling_owner = initial_owner;
    travelling_owner.age = fix29_bee_pack_age(192u, fix29_bee_target_none);
    travelling_owner.aux |= membership_moved;
    membership_seed[far_source_index] = travelling_owner;
    membership_seed[missing_index] = SceneCell{material_id(Material::ash), 37u, 43, 0u};
    membership_seed[far_target_index] = SceneCell{material_id(Material::atmosphere), 91u, -17,
        0x40000000u | (3u << 15u) | (material_id(Material::carbon_dioxide) << 8u) | 84u};
    upload_scene_cells(membership_seed);
    const auto initial_tiles = download_tile_states();
    const bool membership_zero_target =
        ((initial_tiles[far_source_tile].occupancy >> membership_count_shift) & 127u) == 1u &&
        ((initial_tiles[far_target_tile].occupancy >> membership_count_shift) & 127u) == 0u &&
        (initial_tiles[far_target_tile].flags & membership_has_bees) == 0u;
    const auto membership_actor_before = download_actor_state();

    simulation_step = 1u; // slot7 owns its normal quarter-rate opportunity.
    immediate_submit([&](const VkCommandBuffer command_buffer) {
        const ActiveCellDispatch movement_scope{3200u, 720u, 640u, 360u};
        const auto scratch = current_set ^ 1u;
        buffer_barrier(command_buffer, cell_buffers[current_set], VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        buffer_barrier(command_buffer, cell_buffers[scratch],
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        copy_cell_rectangle(command_buffer, current_set, scratch, movement_scope);
        buffer_barrier(command_buffer, cell_buffers[scratch], VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        buffer_barrier(command_buffer, cell_buffers[current_set], VK_ACCESS_SHADER_READ_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        buffer_barrier(command_buffer, tile_buffer,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        const SimulationPush movement_push{
            .width = config.grid_width, .height = config.grid_height,
            .step = simulation_step, .seed = random_seed, .material = 0u,
            .active_section_x = 5, .active_section_y = 2, .active_mode = 1u};
        bind_compute(command_buffer, bee_movement_pipeline, current_set);
        vkCmdPushConstants(command_buffer, compute_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(movement_push), &movement_push);
        vkCmdDispatch(command_buffer, divide_round_up(movement_scope.width, simulation_local_size),
            divide_round_up(movement_scope.height, simulation_local_size), 1u);
        compute_barrier_batch(command_buffer, std::array{
            cell_buffers[current_set].handle, tile_buffer.handle, chunk_buffer.handle});
    });
    const auto moved_cells = download_scene_cells();
    auto expected_owner = travelling_owner;
    expected_owner.aux &= ~membership_moved;
    bool membership_exact_move = membership_valid_hive && membership_zero_target;
    std::uint32_t membership_changed = 0u;
    for (std::size_t index = 0u; index < moved_cells.size(); ++index) {
        const auto& expected = index == far_source_index ? membership_seed[far_target_index] :
            (index == far_target_index ? expected_owner : membership_seed[index]);
        membership_exact_move = membership_exact_move &&
            std::memcmp(&moved_cells[index], &expected, sizeof(SceneCell)) == 0;
        membership_changed += std::memcmp(&moved_cells[index], &membership_seed[index],
            sizeof(SceneCell)) != 0 ? 1u : 0u;
    }
    const auto moved_tiles = download_tile_states();
    const bool membership_published =
        ((moved_tiles[far_target_tile].occupancy >> membership_count_shift) & 127u) == 0u &&
        (moved_tiles[far_target_tile].flags & membership_has_bees) != 0u &&
        (moved_tiles[far_source_tile].flags & membership_has_bees) != 0u;
    append("beehive_cross_tile_move_publishes_conservative_membership",
        membership_exact_move && membership_changed == 2u && membership_published,
        "actual movement across x3208->3207; exact owner/Atmosphere swap; changed=" +
            std::to_string(membership_changed) + " stale count0/candidate=" +
            std::to_string(membership_published ? 1u : 0u));

    // Reclassify only home focus after moving the camera away. The far target
    // retains a deliberately stale ZERO count and its real movement candidate.
    simulation_step = 4096u;
    run_acceptance_tile_pass(0, 2, true);
    immediate_submit([&](const VkCommandBuffer command_buffer) {
        const SimulationPush birth_push{
            .width = config.grid_width, .height = config.grid_height,
            .step = simulation_step, .seed = random_seed,
            .active_section_x = 0, .active_section_y = 2, .active_mode = 1u};
        record_bee_birth_pass(command_buffer, birth_push, {0u, 720u, 640u, 360u});
    });
    const auto capped_cells = download_scene_cells();
    const auto membership_actor_after = download_actor_state();
    std::array<std::uint32_t, fix29_bee_formation_count> live_slots{};
    std::uint32_t owned_bees = 0u;
    for (const auto& bee : capped_cells) {
        if (bee.material != material_id(Material::bee) ||
            (bee.aux & membership_swarm) == 0u) continue;
        const auto slot = bee_slot_from_metadata(bee.aux, config.grid_width, config.grid_height);
        if (slot >= live_slots.size() || (bee.aux & bee_home_metadata_mask) !=
            pack_bee_home_metadata(0u, membership_home, slot,
                config.grid_width, config.grid_height)) continue;
        ++live_slots[slot];
        ++owned_bees;
    }
    const bool membership_exact_cap = capped_cells.size() == moved_cells.size() &&
        std::memcmp(capped_cells.data(), moved_cells.data(),
            moved_cells.size() * sizeof(SceneCell)) == 0 && owned_bees == 60u &&
        std::all_of(live_slots.begin(), live_slots.end(),
            [](const std::uint32_t count) { return count == 1u; });
    append("beehive_off_window_forager_prevents_duplicate_replacement",
        membership_exact_move && membership_published && membership_exact_cap &&
            std::memcmp(&membership_actor_before, &membership_actor_after,
                sizeof(membership_actor_before)) == 0,
        "59 resting+1 actual far mover; home-only reclassification; Ash vacancy; real4096 birth; "
        "byte-identical world/actor and one owner per slot=" +
            std::to_string(membership_exact_cap ? 1u : 0u));
    // Actor hits occur after classification and Bee movement in production.
    // Exercise that independent owner mutation, including a destination whose
    // cached count is zero when the camera immediately leaves for the hive.
    {
        const auto laser_saved_actor = download_actor_state();
        const auto laser_target_index = index_of(far_x - 2u, far_y);
        const auto laser_target_tile = (far_y / 8u) * tile_columns + (far_x - 2u) / 8u;
        auto laser_owner = initial_owner;
        laser_owner.age = fix29_bee_pack_age(731u, 12345u);
        laser_owner.temperature = 39;
        laser_owner.aux = (laser_owner.aux & ~membership_moved) | 0x30000000u;
        // FED/POLLEN and the nontrivial timer/target are persistent roles, not
        // fragment provenance. The low byte is valid home metadata above the
        // first shot's damage, reproducing the old nonterminal home corruption.
        const bool laser_valid_owner = membership_valid_hive &&
            (laser_owner.aux & 255u) > 144u;
        auto laser_seed = membership_seed;
        laser_seed[far_source_index] = laser_owner;
        laser_seed[laser_target_index] = SceneCell{
            material_id(Material::atmosphere), 83u, -29,
            0x40000000u | (5u << 15u) | (material_id(Material::carbon_dioxide) << 8u) | 84u};
        ActorStateReadback laser_actor{
            .x = static_cast<std::int32_t>(far_x) - 24,
            .y = static_cast<std::int32_t>(far_y) - player_tool_origin_offset_cells,
            .velocity_y = -2, .enabled = 1u, .gold = 17u, .iron = 23u,
            .ammo = 31u, .shot_timer = 0u, .move_cooldown = 7u, .grounded = 1u,
            .health = 231u, .oxygen = 177u, .hit_x = 19, .hit_y = 29,
            .scene = static_cast<std::uint32_t>(world_scene), .exposure_ticks = 47u,
            .aluminum = 53u, .copper = 59u, .unlocks = 0u, .drill_level = 1u};
        auto laser_expected_actor = laser_actor;
        laser_expected_actor.hit_x = static_cast<std::int32_t>(far_x);
        laser_expected_actor.hit_y = static_cast<std::int32_t>(far_y);
        laser_expected_actor.shot_timer = 4u;
        const auto fire_membership_laser = [&] {
            const ActorPush laser_push{
                .width = config.grid_width, .height = config.grid_height,
                .step = simulation_step, .seed = random_seed,
                .aim_x = static_cast<std::int32_t>(far_x),
                .aim_y = static_cast<std::int32_t>(far_y), .fire = 1u,
                .scene = static_cast<std::uint32_t>(world_scene),
                .simulate = 0u, .active_mode = 0u};
            immediate_submit([&](const VkCommandBuffer command_buffer) {
                bind_compute(command_buffer, actor_pipeline, current_set);
                vkCmdPushConstants(command_buffer, compute_pipeline_layout,
                    VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(laser_push), &laser_push);
                vkCmdDispatch(command_buffer, 1u, 1u, 1u);
                rain_membership_barrier(command_buffer);
                compute_barrier_batch(command_buffer, std::array{
                    cell_buffers[current_set].handle, actor_buffer.handle,
                    tile_buffer.handle, chunk_buffer.handle});
            });
        };

        upload_scene_cells(laser_seed);
        upload_actor_state(laser_actor);
        const auto laser_initial_tiles = download_tile_states();
        const bool laser_initial_zero =
            ((laser_initial_tiles[far_source_tile].occupancy >> membership_count_shift) & 127u) == 1u &&
            ((laser_initial_tiles[laser_target_tile].occupancy >> membership_count_shift) & 127u) == 0u &&
            (laser_initial_tiles[laser_target_tile].flags & membership_has_bees) == 0u;
        fire_membership_laser();
        const auto laser_result = download_scene_cells();
        const auto laser_result_actor = download_actor_state();
        const auto laser_result_tiles = download_tile_states();
        auto laser_expected_owner = laser_owner;
        laser_expected_owner.aux |= membership_moved;
        bool laser_exact_swap = laser_valid_owner && laser_initial_zero;
        std::uint32_t laser_changed = 0u;
        for (std::size_t index = 0u; index < laser_result.size(); ++index) {
            const auto& expected = index == far_source_index ? laser_seed[laser_target_index] :
                (index == laser_target_index ? laser_expected_owner : laser_seed[index]);
            laser_exact_swap = laser_exact_swap &&
                std::memcmp(&laser_result[index], &expected, sizeof(SceneCell)) == 0;
            laser_changed += std::memcmp(&laser_result[index], &laser_seed[index],
                sizeof(SceneCell)) != 0 ? 1u : 0u;
        }
        const bool laser_exact_actor = std::memcmp(&laser_result_actor, &laser_expected_actor,
            sizeof(laser_expected_actor)) == 0;
        const bool laser_candidate_published =
            ((laser_result_tiles[laser_target_tile].occupancy >> membership_count_shift) & 127u) == 0u &&
            (laser_result_tiles[laser_target_tile].flags & membership_has_bees) != 0u &&
            (laser_result_tiles[far_source_tile].flags & membership_has_bees) != 0u;
        append("player_laser_preserves_bee_owner_and_publishes_membership",
            laser_exact_swap && laser_changed == 2u && laser_exact_actor && laser_candidate_published,
            "actual actor ray x3208->3206; exact gas and Bee age/home/roles; changed=" +
                std::to_string(laser_changed) + " inventory/actor=" +
                std::to_string(laser_exact_actor ? 1u : 0u) + " stale count0/candidate=" +
                std::to_string(laser_candidate_published ? 1u : 0u));

        simulation_step = 4096u;
        run_acceptance_tile_pass(0, 2, true);
        immediate_submit([&](const VkCommandBuffer command_buffer) {
            const SimulationPush birth_push{
                .width = config.grid_width, .height = config.grid_height,
                .step = simulation_step, .seed = random_seed,
                .active_section_x = 0, .active_section_y = 2, .active_mode = 1u};
            record_bee_birth_pass(command_buffer, birth_push, {0u, 720u, 640u, 360u});
        });
        const auto laser_capped_cells = download_scene_cells();
        const auto laser_capped_actor = download_actor_state();
        std::array<std::uint32_t, fix29_bee_formation_count> laser_live_slots{};
        for (const auto& bee : laser_capped_cells) {
            if (bee.material != material_id(Material::bee) ||
                (bee.aux & membership_swarm) == 0u) continue;
            const auto slot = bee_slot_from_metadata(bee.aux, config.grid_width, config.grid_height);
            if (slot < laser_live_slots.size() && (bee.aux & bee_home_metadata_mask) ==
                pack_bee_home_metadata(0u, membership_home, slot,
                    config.grid_width, config.grid_height)) ++laser_live_slots[slot];
        }
        const bool laser_cap_retained =
            std::memcmp(laser_capped_cells.data(), laser_result.data(),
                laser_result.size() * sizeof(SceneCell)) == 0 &&
            std::memcmp(&laser_capped_actor, &laser_result_actor, sizeof(laser_result_actor)) == 0 &&
            count_material(laser_capped_cells, Material::bee) == 60u &&
            std::all_of(laser_live_slots.begin(), laser_live_slots.end(),
                [](const std::uint32_t count) { return count == 1u; });
        append("beehive_off_window_laser_owner_prevents_sixty_first_birth",
            laser_exact_swap && laser_candidate_published && laser_cap_retained,
            "59 resting plus exact actor-ejected owner beyond home focus; real4096 birth; "
            "unchanged world/actor and one owner per slot=" +
                std::to_string(laser_cap_retained ? 1u : 0u));

        // Empty is transparent to the ray but is not an admissible gas swap.
        // Deny all eight destinations while retaining an unobstructed hit ray.
        auto blocked_seed = laser_seed;
        constexpr std::array<std::array<std::int32_t, 2>, 8> blocked_offsets{{
            {{-2, 0}}, {{-1, 0}}, {{-2, -1}}, {{-2, 1}},
            {{-1, -1}}, {{-1, 1}}, {{0, -1}}, {{0, 1}}}};
        for (const auto& offset : blocked_offsets) {
            const auto x = static_cast<std::uint32_t>(static_cast<std::int32_t>(far_x) + offset[0]);
            const auto y = static_cast<std::uint32_t>(static_cast<std::int32_t>(far_y) + offset[1]);
            blocked_seed[index_of(x, y)] = SceneCell{material_id(Material::empty), 0u, 20, 0u};
        }
        upload_scene_cells(blocked_seed);
        upload_actor_state(laser_actor);
        fire_membership_laser();
        const auto blocked_result = download_scene_cells();
        const auto blocked_actor = download_actor_state();
        const auto blocked_tiles = download_tile_states();
        const bool blocked_exact = laser_valid_owner &&
            std::memcmp(blocked_result.data(), blocked_seed.data(),
                blocked_seed.size() * sizeof(SceneCell)) == 0 &&
            std::memcmp(&blocked_actor, &laser_expected_actor, sizeof(laser_expected_actor)) == 0 &&
            (blocked_tiles[laser_target_tile].flags & membership_has_bees) == 0u;
        append("blocked_player_laser_retains_exact_bee_home_and_lifecycle",
            blocked_exact,
            "all eight destinations are Vacuum; actual hit changes only actor hit/timer; "
            "no health write, AUX_MOVED, transfer or destination candidate=" +
                std::to_string(blocked_exact ? 1u : 0u));
        upload_actor_state(laser_saved_actor);
    }
    tool_hive_anchor = membership_saved_anchor;
    simulation_step = membership_saved_step;
}
