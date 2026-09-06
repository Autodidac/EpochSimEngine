// Included inside run_runtime_acceptance after the existing hive/respiration
// block. Uses the real queued Editor paint route and production focused ticks;
// this is not a CPU substitute for the new global-home/search shader paths.
{
    const auto saved_step = simulation_step;
    const auto saved_workspace = state.selected_workspace.load(std::memory_order_acquire);
    const auto saved_material = state.selected_material.load(std::memory_order_acquire);
    const auto saved_paused = state.paused.load(std::memory_order_acquire);
    const auto saved_blueprint = state.blueprint_placement_active.load(std::memory_order_acquire);
    const auto saved_request = state.beehive_place_request.exchange(0u, std::memory_order_acq_rel);
    const auto saved_anchor = tool_hive_anchor;
    state.selected_workspace.store(1u, std::memory_order_release);
    state.selected_material.store(material_id(Material::beehive), std::memory_order_release);
    state.blueprint_placement_active.store(false, std::memory_order_release);
    state.paused.store(true, std::memory_order_release);
    simulation_step = 0u;
    tool_hive_anchor = no_tool_hive_anchor;

    constexpr std::uint32_t swarm_bit = 0x08000000u;
    constexpr std::uint32_t fed_bit = 0x10000000u;
    constexpr std::uint32_t pollen_bit = 0x20000000u;
    constexpr std::uint32_t role_bits = 0x62000000u; // Pollen, Queen, migrating.
    constexpr std::uint32_t search_target = 0x3fffdu;
    const auto queued_editor_click = [&](const BeeHome home) {
        const auto action = route_world_primary_action({
            .editor_workspace = state.selected_workspace.load(std::memory_order_acquire) == 1u,
            .pointer_over_world = true,
            .primary_down = true,
            .primary_pressed = true,
            .one_shot_paint = true,
            .paused = state.paused.load(std::memory_order_acquire),
        });
        if (action != WorldPrimaryAction::editor_paint) return false;
        request_beehive_placement(state, static_cast<std::int32_t>(home.x),
                                  static_cast<std::int32_t>(home.y));
        const auto request = consume_beehive_placement(state);
        if (!request || request->x != static_cast<std::int32_t>(home.x) ||
            request->y != static_cast<std::int32_t>(home.y)) return false;
        immediate_submit([&](const VkCommandBuffer command_buffer) {
            record_paint_at_grid(command_buffer, state, false, true,
                request->x, request->y, material_id(Material::beehive));
        });
        return !consume_beehive_placement(state).has_value();
    };

    struct ColonyObservation final {
        bool exact{true};
        std::uint32_t total{};
        std::string detail{};
    };
    // Phase 0: untouched initial placement; 1: six actual searchers after120
    // ticks; 2: all60 returned to their exact cells and NONE lifecycle target.
    const auto observe_colonies = [&](const std::vector<SceneCell>& observed,
                                      const std::vector<BeeHome>& homes,
                                      const std::uint32_t phase) {
        ColonyObservation observation;
        std::vector<std::array<bool, fix29_bee_formation_count>> slots(homes.size());
        std::vector<std::uint32_t> counts(homes.size());
        std::vector<std::uint32_t> moved_foragers(homes.size());
        std::vector<std::uint32_t> fixed_nonforagers(homes.size());
        std::vector<std::uint32_t> fixed_all(homes.size());
        for (std::size_t index = 0u; index < observed.size(); ++index) {
            const auto& bee = observed[index];
            if (bee.material != material_id(Material::bee)) continue;
            ++observation.total;
            const auto slot = bee_slot_from_metadata(bee.aux, config.grid_width,
                                                     config.grid_height);
            if (slot >= fix29_bee_formation_count) {
                observation.exact = false;
                continue;
            }
            std::size_t owner = homes.size();
            for (std::size_t candidate = 0u; candidate < homes.size(); ++candidate) {
                const auto expected = pack_bee_home_metadata(0u, homes[candidate], slot,
                    config.grid_width, config.grid_height);
                if ((bee.aux & bee_home_metadata_mask) == expected &&
                    bee_home_from_metadata(bee.aux, config.grid_width, config.grid_height) ==
                        bee_home_from_metadata(expected, config.grid_width, config.grid_height)) {
                    owner = candidate;
                    break;
                }
            }
            if (owner == homes.size()) {
                observation.exact = false;
                continue;
            }
            ++counts[owner];
            observation.exact = observation.exact && !slots[owner][slot] &&
                (bee.aux & (swarm_bit | fed_bit)) == (swarm_bit | fed_bit) &&
                (bee.aux & role_bits) == 0u;
            slots[owner][slot] = true;
            const auto offset = fix29_bee_formation_offset(slot);
            const auto initial_index = index_of(
                static_cast<std::uint32_t>(static_cast<std::int32_t>(homes[owner].x) + offset.x),
                static_cast<std::uint32_t>(static_cast<std::int32_t>(homes[owner].y) + offset.y));
            const bool at_formation = index == initial_index;
            fixed_all[owner] += at_formation ? 1u : 0u;
            const auto target = fix29_bee_target_from_age(bee.age);
            if (phase == 2u && (!at_formation || target != fix29_bee_target_none)) {
                observation.detail += " [unsettled home=" + std::to_string(homes[owner].x) +
                    "," + std::to_string(homes[owner].y) + " slot=" + std::to_string(slot) +
                    " position=" + std::to_string(index % config.grid_width) + "," +
                    std::to_string(index / config.grid_width) + " target=" +
                    std::to_string(target) + " timer=" +
                    std::to_string(fix29_bee_timer_from_age(bee.age)) + "]";
            }
            if (fix29_bee_forager_slot(slot)) {
                moved_foragers[owner] += !at_formation ? 1u : 0u;
                observation.exact = observation.exact &&
                    target == (phase == 1u ? search_target : fix29_bee_target_none);
            } else {
                fixed_nonforagers[owner] += at_formation ? 1u : 0u;
                observation.exact = observation.exact && target == fix29_bee_target_none;
            }
            if (phase == 0u)
                observation.exact = observation.exact &&
                    fix29_bee_timer_from_age(bee.age) == fix29_bee_initial_timer(slot);
        }
        for (std::size_t owner = 0u; owner < homes.size(); ++owner) {
            observation.exact = observation.exact && counts[owner] == 60u &&
                fixed_nonforagers[owner] == 54u &&
                (phase == 1u ? moved_foragers[owner] == 6u : fixed_all[owner] == 60u) &&
                canonical_fix29_hive_signature_at(observed, config.grid_width,
                    config.grid_height, homes[owner].x, homes[owner].y);
            observation.detail += " [home=" + std::to_string(homes[owner].x) + "," +
                std::to_string(homes[owner].y) + " live=" + std::to_string(counts[owner]) +
                " moved=" + std::to_string(moved_foragers[owner]) + " resting=" +
                std::to_string(fixed_nonforagers[owner]) + " formation=" +
                std::to_string(fixed_all[owner]) + "]";
        }
        observation.exact = observation.exact && observation.total == homes.size() * 60u &&
            count_material(observed, Material::queen_bee) == homes.size() &&
            count_material(observed, Material::beehive) == homes.size() * 193u;
        return observation;
    };

    // Both ordinary district homes are in the same bounded640x360 production
    // focus. No Flower is injected and no Bee age, role or position is patched.
    std::vector<BeeHome> homes{{160u, 920u}, {304u, 920u}};
    auto colony_cells = acceptance_atmosphere_world();
    upload_scene_cells(colony_cells);
    bool clicks = true;
    for (const auto home : homes) clicks = queued_editor_click(home) && clicks;
    colony_cells = download_scene_cells();
    const auto placed = observe_colonies(colony_cells, homes, 0u);
    const auto initial_pollen = count_material(colony_cells, Material::pollen);
    const auto initial_honey = count_material(colony_cells, Material::honey);
    append("beehive_two_independent_editor_colonies",
        clicks && placed.exact && simulation_step == 0u &&
            count_material(colony_cells, Material::flower) == 0u,
        "queued Editor clicks=2 paused=1 separation=144 no Flowers;" + placed.detail);

    const bool overlap_click = queued_editor_click({homes[0].x + 4u, homes[0].y});
    {
        const auto rejected = download_scene_cells();
        const bool unchanged = rejected.size() == colony_cells.size() &&
            std::memcmp(rejected.data(), colony_cells.data(),
                colony_cells.size() * sizeof(SceneCell)) == 0;
        append("beehive_blocked_overlap_preserves_all_canonical_cells",
            overlap_click && unchanged && simulation_step == 0u,
            "queued overlapping click consumed; canonical cells/clock unchanged=" +
                std::to_string(unchanged ? 1u : 0u));
    }

    state.paused.store(false, std::memory_order_release);
    for (std::uint32_t tick = 0u; tick < 120u; ++tick)
        run_acceptance_focused_tick(0, 2, true, true);
    colony_cells = download_scene_cells();
    const auto searching = observe_colonies(colony_cells, homes, 1u);
    append("beehive_no_flower_six_searchers_per_colony",
        searching.exact && simulation_step == 120u &&
            count_material(colony_cells, Material::flower) == 0u &&
            count_material(colony_cells, Material::pollen) == initial_pollen &&
            count_material(colony_cells, Material::honey) == initial_honey,
        "120 real focused ticks; no age/position injection;" + searching.detail);

    for (std::uint32_t tick = 120u; tick < 600u; ++tick)
        run_acceptance_focused_tick(0, 2, true, true);
    colony_cells = download_scene_cells();
    const auto returned = observe_colonies(colony_cells, homes, 2u);
    append("beehive_no_flower_search_returns_without_food_creation",
        returned.exact && simulation_step == 600u &&
            count_material(colony_cells, Material::flower) == 0u &&
            count_material(colony_cells, Material::pollen) == initial_pollen &&
            count_material(colony_cells, Material::honey) == initial_honey,
        "600 real focused ticks; exact60 slots and NONE targets per home;" + returned.detail);

    {
        // Isolate the actual rare birth transaction from hazard chemistry and
        // subsequent flight. A seeded Ash is the one spent owner; existing
        // hazard tests separately prove Bee->Ash. The second intact colony is
        // in this same district, so the obsolete district-wide cap would refuse
        // the replacement at119 Bees instead of admitting60 per exact home.
        auto birth_seed = colony_cells;
        constexpr std::uint32_t missing_slot = 0u;
        const auto missing_offset = fix29_bee_formation_offset(missing_slot);
        const auto missing_index = index_of(
            static_cast<std::uint32_t>(static_cast<std::int32_t>(homes[0].x) + missing_offset.x),
            static_cast<std::uint32_t>(static_cast<std::int32_t>(homes[0].y) + missing_offset.y));
        const bool missing_owner_valid =
            birth_seed[missing_index].material == material_id(Material::bee) &&
            bee_slot_from_metadata(birth_seed[missing_index].aux,
                config.grid_width, config.grid_height) == missing_slot;
        birth_seed[missing_index] = SceneCell{material_id(Material::ash), 37u, 43, 0u};
        std::size_t birth_index = birth_seed.size();
        // Independent row-major election of the nearest-Queen, open-medium,
        // adjacent nest+food frontier. Both Queens are144cells apart and every
        // candidate is within sqrt(6) of A, so no second Queen can win this scan.
        for (std::int32_t dy = -3; dy <= 3; ++dy) {
            for (std::int32_t dx = -3; dx <= 3; ++dx) {
                if (dx * dx + dy * dy == 0 || dx * dx + dy * dy > 6) continue;
                const auto x = static_cast<std::int32_t>(homes[0].x) + dx;
                const auto y = static_cast<std::int32_t>(homes[0].y) + dy;
                const auto index = index_of(static_cast<std::uint32_t>(x),
                                            static_cast<std::uint32_t>(y));
                const auto medium = birth_seed[index].material;
                if (medium != material_id(Material::empty) &&
                    medium != material_id(Material::atmosphere)) continue;
                bool nest = false;
                bool food = false;
                for (std::int32_t oy = -1; oy <= 1; ++oy) {
                    for (std::int32_t ox = -1; ox <= 1; ++ox) {
                        if (ox == 0 && oy == 0) continue;
                        const auto material = birth_seed[index_of(
                            static_cast<std::uint32_t>(x + ox),
                            static_cast<std::uint32_t>(y + oy))].material;
                        nest = nest || material == material_id(Material::queen_bee) ||
                            material == material_id(Material::beehive);
                        food = food || material == material_id(Material::honey) ||
                            material == material_id(Material::pollen);
                    }
                }
                std::uint32_t near_bees = 0u;
                for (std::int32_t oy = -6; oy <= 6; ++oy)
                    for (std::int32_t ox = -6; ox <= 6; ++ox)
                        if (ox * ox + oy * oy <= 36 &&
                            birth_seed[index_of(static_cast<std::uint32_t>(x + ox),
                                static_cast<std::uint32_t>(y + oy))].material == material_id(Material::bee))
                            ++near_bees;
                if (nest && food && near_bees < 4u)
                    birth_index = (std::min)(birth_index, index);
            }
        }
        const bool birth_fixture_valid = returned.exact && missing_owner_valid &&
            birth_index < birth_seed.size() &&
            count_material(birth_seed, Material::bee) == 119u;
        bool exact_pair = false;
        bool exact_cap = false;
        bool actor_unchanged = false;
        std::uint32_t changed_cells = 0u;
        std::array<std::uint32_t, 2u> live_counts{};
        if (birth_fixture_valid) {
            birth_seed[birth_index] = SceneCell{material_id(Material::atmosphere), 91u, -17,
                0x40000000u | (3u << 15u) | (material_id(Material::carbon_dioxide) << 8u) | 84u};
            const auto actor_at_birth = download_actor_state();
            upload_scene_cells(birth_seed);
            simulation_step = 4096u;
            run_acceptance_tile_pass(0, 2, true);
            const SimulationPush birth_push{
                .width = config.grid_width, .height = config.grid_height,
                .step = simulation_step, .seed = random_seed,
                .active_section_x = 0, .active_section_y = 2, .active_mode = 1u};
            constexpr ActiveCellDispatch birth_scope{0u, 720u, 640u, 360u};
            immediate_submit([&](const VkCommandBuffer command_buffer) {
                record_bee_birth_pass(command_buffer, birth_push, birth_scope);
            });
            const auto born = download_scene_cells();
            const SceneCell expected_newborn{material_id(Material::bee),
                fix29_bee_pack_age((homes[0].x & 7u) | ((homes[0].y & 7u) << 3u),
                    fix29_bee_target_newborn), 20,
                pack_bee_home_metadata(swarm_bit | fed_bit, homes[0], missing_slot,
                    config.grid_width, config.grid_height)};
            exact_pair = born.size() == birth_seed.size();
            std::array<std::array<bool, fix29_bee_formation_count>, 2u> seen{};
            for (std::size_t index = 0u; index < born.size() && exact_pair; ++index) {
                const auto& expected = index == birth_index ? expected_newborn :
                    (index == missing_index ? birth_seed[birth_index] : birth_seed[index]);
                changed_cells += std::memcmp(&born[index], &birth_seed[index], sizeof(SceneCell)) != 0 ? 1u : 0u;
                exact_pair = std::memcmp(&born[index], &expected, sizeof(SceneCell)) == 0;
                if (born[index].material != material_id(Material::bee)) continue;
                const auto slot = bee_slot_from_metadata(born[index].aux,
                    config.grid_width, config.grid_height);
                std::size_t owner = homes.size();
                for (std::size_t candidate = 0u; candidate < homes.size(); ++candidate)
                    if ((born[index].aux & bee_home_metadata_mask) ==
                        pack_bee_home_metadata(0u, homes[candidate], slot,
                            config.grid_width, config.grid_height)) owner = candidate;
                if (owner >= seen.size() || slot >= fix29_bee_formation_count || seen[owner][slot]) {
                    exact_pair = false;
                    continue;
                }
                seen[owner][slot] = true;
                ++live_counts[owner];
            }
            exact_pair = exact_pair && changed_cells == 2u &&
                live_counts[0] == 60u && live_counts[1] == 60u &&
                count_material(born, Material::pollen) == initial_pollen &&
                count_material(born, Material::honey) == initial_honey;
            // No fresh Ash owner exists after the exact pair, and each colony
            // now owns60 Bees. A repeated opportunity must not create a121st.
            run_acceptance_tile_pass(0, 2, true);
            immediate_submit([&](const VkCommandBuffer command_buffer) {
                record_bee_birth_pass(command_buffer, birth_push, birth_scope);
            });
            const auto capped = download_scene_cells();
            exact_cap = capped.size() == born.size() &&
                std::memcmp(capped.data(), born.data(), born.size() * sizeof(SceneCell)) == 0 &&
                count_material(capped, Material::bee) == 120u;
            const auto actor_after_birth = download_actor_state();
            actor_unchanged = std::memcmp(&actor_at_birth, &actor_after_birth,
                sizeof(actor_at_birth)) == 0;
        }
        append("beehive_second_colony_does_not_suppress_exact_replacement",
            birth_fixture_valid && exact_pair && exact_cap && actor_unchanged,
            "seeded one Ash; actual birth pass; fixture/pair/cap/actor=" +
                std::to_string(birth_fixture_valid ? 1u : 0u) + "/" +
                std::to_string(exact_pair ? 1u : 0u) + "/" +
                std::to_string(exact_cap ? 1u : 0u) + "/" +
                std::to_string(actor_unchanged ? 1u : 0u) + " changed=" +
                std::to_string(changed_cells) + " live=" + std::to_string(live_counts[0]) +
                "/" + std::to_string(live_counts[1]) + "; food/all other cells byte-identical");
        // This isolated transaction does not replace the returned-colony input
        // used by the independent paused/global placement and persistence tests.
        upload_scene_cells(colony_cells);
        simulation_step = 600u;
    }

    // These formerly failed the district/64-cell guard despite fitting every
    // actual body/formation cell. Last home is also outside focus(0,2).
    std::vector<BeeHome> global_homes{{32u, 32u}, {997u, 213u},
        {config.grid_width - 40u, config.grid_height - 40u}};
    const auto district_one_x = persistent_world_district_origin_x(config.grid_width, 1u);
    if (district_one_x > 640u)
        global_homes.push_back({640u + (district_one_x - 640u) / 2u + 3u, 917u});
    state.paused.store(true, std::memory_order_release);
    bool global_clicks = true;
    for (const auto home : global_homes)
        global_clicks = queued_editor_click(home) && global_clicks;
    colony_cells = download_scene_cells();
    auto all_homes = homes;
    all_homes.insert(all_homes.end(), global_homes.begin(), global_homes.end());
    // Phase2 checks common settled state; fresh owners retain their seeded
    // timers while the two older colonies retain their completed search ages.
    const auto global = observe_colonies(colony_cells, all_homes, 2u);
    bool global_markers = true;
    for (const auto home : global_homes) {
        const auto offset = fix29_bee_formation_offset(0u);
        const auto& bee = colony_cells[index_of(
            static_cast<std::uint32_t>(static_cast<std::int32_t>(home.x) + offset.x),
            static_cast<std::uint32_t>(static_cast<std::int32_t>(home.y) + offset.y))];
        global_markers = global_markers &&
            bee_uses_global_home(bee.aux, config.grid_width, config.grid_height) &&
            (bee.aux & pollen_bit) == 0u;
    }
    append("beehive_gap_high_sky_edges_and_off_focus_are_placeable",
        global_clicks && global.exact && global_markers && simulation_step == 600u,
        "paused; old64-cell margin, high sky, far off-focus and gap=" +
            std::to_string(district_one_x > 640u ? 1u : 0u) + ";" + global.detail);

    const auto actor_before = download_actor_state();
    const WorldSaveOwners owners{.actor_present = true, .actor = actor_before};
    const WorldSaveMetadata metadata{.world_size = config.world_size,
        .width = config.grid_width, .height = config.grid_height, .scene = world_scene};
    auto save_root = std::filesystem::path{config.runtime_acceptance_report};
    save_root += ".bee-colonies-save";
    std::string save_error;
    const bool saved = save_world(save_root, metadata, "colonies", colony_cells, owners, save_error);
    std::vector<SceneCell> loaded(colony_cells.size());
    WorldSaveOwners loaded_owners{};
    WorldSaveMetadata loaded_metadata{};
    const bool restored = saved && load_world(save_root, config.world_size,
        config.grid_width, config.grid_height, world_scene, "colonies", loaded,
        loaded_owners, loaded_metadata, save_error);
    bool exact_round_trip = restored && loaded_owners.actor_present &&
        loaded_metadata.format_version == world_save_format_version &&
        std::memcmp(&actor_before, &loaded_owners.actor, sizeof(actor_before)) == 0 &&
        std::memcmp(colony_cells.data(), loaded.data(), colony_cells.size() * sizeof(SceneCell)) == 0;
    if (restored) {
        upload_scene_cells(loaded, true);
        upload_actor_state(loaded_owners.actor);
        const auto gpu_loaded = download_scene_cells();
        const auto gpu_actor = download_actor_state();
        exact_round_trip = exact_round_trip &&
            std::memcmp(loaded.data(), gpu_loaded.data(), loaded.size() * sizeof(SceneCell)) == 0 &&
            std::memcmp(&actor_before, &gpu_actor, sizeof(actor_before)) == 0 &&
            observe_colonies(gpu_loaded, all_homes, 2u).exact;
    }
    append("beehive_multiple_global_homes_schema2_exact_round_trip",
        exact_round_trip, "schema2 cells/actor/global and legacy homes plus GPU reupload exact=" +
            std::to_string(exact_round_trip ? 1u : 0u) + "; clock not serialized; " + save_error);

    {
        // Exact chemistry-only ventilation probes: the production monolithic
        // proposal and shallow correction run, without movement supplying local
        // Oxygen first. A forced deterministic suffocation opportunity makes
        // both the positive transaction and fail-closed controls observable.
        const auto hash32 = [](std::uint32_t value) {
            value ^= value >> 16u; value *= 0x7feb352du;
            value ^= value >> 15u; value *= 0x846ca68bu;
            return value ^ (value >> 16u);
        };
        const auto force_suffocation_age = [&](SceneCell& queen, const BeeHome home,
                                               const bool old_age,
                                               const std::uint32_t minimum_age = 0u) {
            const auto first = old_age ? 900001u : minimum_age;
            const auto mask = old_age ? 16383u : 1023u;
            for (std::uint32_t age = first; age < first + 262144u; ++age) {
                const auto roll = hash32(home.x * 73856093u ^ home.y * 19349663u ^
                    simulation_step * 83492791u ^ random_seed ^ age ^ queen.aux);
                if ((roll & mask) == 0u) { queen.age = age; return true; }
            }
            return false;
        };
        struct VentCase final { const char* name; std::uint32_t mode; };
        constexpr std::array<VentCase, 11u> cases{{
            {"open", 0u}, {"sealed", 1u}, {"flooded", 2u}, {"depleted", 3u},
            {"heat-hazard", 4u}, {"senescence", 5u}, {"clipped-mouth", 6u},
            {"machine-vent", 7u}, {"structural-donor", 8u},
            {"old-high-stress", 9u}, {"live-thermal-phase", 10u},
        }};
        bool positive = false;
        bool negatives = true;
        bool finite = false;
        bool thermal_phase = false;
        std::string details;
        for (const auto& probe : cases) {
            // The machine witness deliberately aligns its competing controller
            // at (3,3) mod8 and the hive mouth at the controller's real vent.
            const BeeHome home{probe.mode == 6u ? 635u : (probe.mode == 7u ? 21u : 20u),
                probe.mode == 7u ? 922u : 920u};
            simulation_step = 37u;
            state.paused.store(true, std::memory_order_release);
            upload_scene_cells(acceptance_atmosphere_world());
            const bool clicked = queued_editor_click(home);
            auto seed = download_scene_cells();
            const auto queen_index = index_of(home.x, home.y);
            const auto mouth_index = index_of(home.x + 11u, home.y);
            auto& queen = seed[queen_index];
            queen.aux = (queen.aux & ~255u) | (probe.mode == 9u ? 255u : 16u);
            const bool aged = force_suffocation_age(queen, home, probe.mode == 5u,
                probe.mode == 9u ? 36001u : 0u);
            seed[mouth_index] = SceneCell{material_id(Material::atmosphere), 19u, 23,
                0x40000000u | (material_id(Material::carbon_dioxide) << 8u) | (3u << 15u) |
                    (probe.mode == 3u ? 1u : 2u)};
            if (probe.mode == 1u)
                seed[index_of(home.x + 6u, home.y)] =
                    SceneCell{material_id(Material::stone), 0u, 20, fill_aux_structural | fill_aux_supported};
            if (probe.mode == 2u)
                seed[index_of(home.x + 6u, home.y)] = SceneCell{material_id(Material::water), 0u, 20, 0u};
            if (probe.mode == 4u) seed[queen_index].temperature = 240;
            if (probe.mode == 7u) {
                seed[index_of(home.x + 6u, home.y + 1u)] = SceneCell{
                    material_id(Material::assembler), 0u, 20,
                    0x40000000u | fill_aux_structural | 255u |
                        (2u << 8u) | (1u << 12u) | (1u << 16u) | (1u << 20u)};
                seed[index_of(home.x + 11u, home.y + 1u)] =
                    SceneCell{material_id(Material::atmosphere), 31u, 20, 5u};
            }
            if (probe.mode == 8u)
                seed[mouth_index].aux |= fill_aux_structural | fill_aux_supported;
            if (probe.mode == 10u) {
                seed[queen_index].temperature = 119;
                // A hot Empty owner heats the Queen through the unchanged
                // four-cardinal thermal rule without replacing canonical food.
                seed[index_of(home.x + 1u, home.y)].temperature = 1000;
            }
            const auto queen_before = seed[queen_index];
            const auto pollen_before = count_material(seed, Material::pollen);
            const auto honey_before = count_material(seed, Material::honey);
            const auto donor_before = seed[mouth_index];
            upload_scene_cells(seed);
            run_acceptance_tile_pass(0, 2, true);
            run_acceptance_chemistry_pass(0, 2, true);
            const auto actual = download_scene_cells();
            const auto& queen_after = actual[queen_index];
            const auto& donor_after = actual[mouth_index];
            const auto counters = download_conservation_counters();
            const bool food = count_material(actual, Material::pollen) == pollen_before &&
                count_material(actual, Material::honey) == honey_before;
            const bool intact = canonical_fix29_hive_signature_at(actual, config.grid_width,
                config.grid_height, home.x, home.y);
            bool passed = false;
            if (probe.mode == 0u || probe.mode == 10u) {
                const SceneCell expected_queen{material_id(Material::queen_bee),
                    queen_before.age + 1u, probe.mode == 10u ? 121 : 20,
                    (queen_before.aux & ~255u) | 15u};
                const SceneCell expected_donor{material_id(Material::atmosphere), 20u, 23,
                    0x40000000u | (material_id(Material::carbon_dioxide) << 8u) | (4u << 15u) | 1u};
                passed = clicked && aged && food && intact &&
                    std::memcmp(&queen_after, &expected_queen, sizeof(SceneCell)) == 0 &&
                    std::memcmp(&donor_after, &expected_donor, sizeof(SceneCell)) == 0 &&
                    counters[2] == 0u && counters[0] == 0u && counters[1] == 0u &&
                    counters[4] == (probe.mode == 10u ? 1u : 0u);
                if (probe.mode == 10u) thermal_phase = passed;
                else positive = passed;

                // The same finite donor now has only its unspendable final
                // baseline unit. A later forced opportunity must not rescue
                // the Queen or manufacture another stored CO2 unit.
                if (probe.mode == 0u) {
                    auto exhausted = actual;
                    const bool next_age = force_suffocation_age(exhausted[queen_index], home, false);
                    upload_scene_cells(exhausted);
                    run_acceptance_tile_pass(0, 2, true);
                    run_acceptance_chemistry_pass(0, 2, true);
                    const auto depleted = download_scene_cells();
                    finite = positive && next_age &&
                        depleted[queen_index].material == material_id(Material::waste) &&
                        depleted[mouth_index].material == donor_after.material &&
                        depleted[mouth_index].aux == donor_after.aux &&
                        count_material(depleted, Material::queen_bee) == 0u &&
                        count_material(depleted, Material::pollen) == pollen_before &&
                        count_material(depleted, Material::honey) == honey_before;
                }
            } else {
                const auto expected_death = probe.mode == 4u ? Material::ash : Material::waste;
                bool donor_owned = donor_after.material == donor_before.material &&
                    donor_after.aux == donor_before.aux;
                if (probe.mode == 7u) {
                    auto expected_donor = donor_before;
                    expected_donor.aux = (expected_donor.aux & ~255u) | 7u | fill_aux_moved;
                    donor_owned = std::memcmp(&donor_after, &expected_donor, sizeof(SceneCell)) == 0 &&
                        actual[index_of(home.x + 11u, home.y + 1u)].material ==
                            material_id(Material::plasma_ammo);
                } else if (probe.mode == 8u) {
                    // Frozen chemistry may legitimately release structural Air;
                    // ventilation must neither restore those flags nor spend
                    // its pressure/component payload while the Queen dies.
                    constexpr std::uint32_t gas_payload = 0x407fffffu;
                    donor_owned = donor_after.material == donor_before.material &&
                        (donor_after.aux & gas_payload) == (donor_before.aux & gas_payload);
                }
                passed = clicked && aged && food && !intact &&
                    queen_after.material == material_id(expected_death) &&
                    count_material(actual, Material::queen_bee) == 0u &&
                    donor_owned;
                negatives = negatives && passed;
            }
            details += " [" + std::string{probe.name} + " passed=" + std::to_string(passed ? 1u : 0u) +
                " Queen=" + std::to_string(queen_after.material) + " donor_O2=" +
                std::to_string(donor_before.aux & 255u) + "/" +
                std::to_string(donor_after.aux & 255u) + " CO2=" +
                std::to_string((donor_before.aux >> 15u) & 255u) + "/" +
                std::to_string((donor_after.aux >> 15u) & 255u) + " food=" +
                std::to_string(food ? 1u : 0u) + " converted=" + std::to_string(counters[2]) +
                " phase=" + std::to_string(counters[4]) + "]";
        }
        append("beehive_queen_ventilation_is_one_exact_oxygen_carbon_transaction",
            positive, "real chemistry+correction; preserved Queen age/heat/stress and exact donor payload;" + details);
        append("beehive_queen_ventilation_refuses_blocked_depleted_hazard_old_and_clipped_owners",
            negatives, "no Queen immunity or half-dispatch breathing;" + details);
        append("beehive_queen_ventilation_exhausts_its_real_oxygen_owner",
            finite, "finite donor O2=2->1 and stored CO2=3->4; next forced opportunity cannot breathe or rescue");
        append("beehive_queen_ventilation_retains_live_thermal_phase_accounting",
            thermal_phase, "Queen119->121 C remains alive; exactly one recorded phase change and one real respiration;" + details);
    }

    {
        struct EdgeBirthCase final {
            BeeHome home;
            std::uint32_t slot;
            const char* name;
            std::uint32_t start_x{11u};
            std::int32_t start_y{};
        };
        const std::array<EdgeBirthCase, 5u> edge_cases{{
            {{20u, 920u}, 54u, "left"},
            {{304u, 22u}, 0u, "top"},
            {{config.grid_width - 21u, 920u}, 45u, "right"},
            {{304u, config.grid_height - 15u}, 57u, "bottom"},
            // Preserve the four route-entry witnesses and independently cover
            // the actual x9->x10 tunnel mouth, including y1 centering. Slot35
            // also proves its final route does not cycle back into the tunnel.
            {{304u, 22u}, 35u, "top-tunnel", 9u, 1},
        }};
        bool edge_routes = true;
        std::string edge_detail;
        const auto edge_gas_units = [&](const std::vector<SceneCell>& cells) {
            std::uint64_t units = 0u;
            for (const auto& cell : cells) {
                const auto pressure = cell.aux & 255u;
                if (cell.material == material_id(Material::atmosphere)) {
                    units += pressure == 0u ? 220u : pressure;
                    if ((cell.aux & 0x40000000u) != 0u)
                        units += (cell.aux >> 15u) & 255u;
                } else if (cell.material == material_id(Material::oxygen)) {
                    units += pressure == 0u ? 220u : pressure;
                } else if (cell.material == material_id(Material::carbon_dioxide)) {
                    units += (std::max)(pressure, 1u);
                }
            }
            return units;
        };
        for (const auto& edge : edge_cases) {
            // Force only the NEWBORN transit state, not an invented birth or
            // food transaction. The real rare-birth pair is tested above.
            // Reset ordinary departure timers to isolate one replacement
            // against all59 exact stationary neighbors for96 production ticks.
            simulation_step = 0u;
            state.paused.store(true, std::memory_order_release);
            upload_scene_cells(acceptance_atmosphere_world());
            const bool clicked = queued_editor_click(edge.home);
            auto seed = download_scene_cells();
            const auto initial = observe_colonies(seed, {edge.home}, 0u);
            const auto queen_diagnostic = [&](const char* label,
                                               const std::vector<SceneCell>& cells) {
                const auto& queen = cells[index_of(edge.home.x, edge.home.y)];
                std::string detail = " [" + std::string{label} + " queen(mat/age/temp/aux)=" +
                    std::to_string(queen.material) + "/" + std::to_string(queen.age) + "/" +
                    std::to_string(queen.temperature) + "/" + std::to_string(queen.aux) +
                    " queen_count=" + std::to_string(count_material(cells, Material::queen_bee)) +
                    " body=" + std::to_string(canonical_fix29_hive_signature_at(cells,
                        config.grid_width, config.grid_height, edge.home.x, edge.home.y) ? 1u : 0u);
                for (std::int32_t dy = -24; dy <= 24; ++dy)
                    for (std::int32_t dx = -24; dx <= 24; ++dx) {
                        const auto x = static_cast<std::int32_t>(edge.home.x) + dx;
                        const auto y = static_cast<std::int32_t>(edge.home.y) + dy;
                        if (x >= 0 && y >= 0 && x < static_cast<std::int32_t>(config.grid_width) &&
                            y < static_cast<std::int32_t>(config.grid_height) &&
                            cells[index_of(static_cast<std::uint32_t>(x),
                                static_cast<std::uint32_t>(y))].material == material_id(Material::queen_bee))
                            detail += " queen_at=" + std::to_string(x) + "," + std::to_string(y);
                    }
                return detail + "]";
            };
            std::string lifecycle_detail = queen_diagnostic("placed", seed);
            for (auto& cell : seed)
                if (cell.material == material_id(Material::bee))
                    cell.age = fix29_bee_pack_age(0u, fix29_bee_target_none);
            const auto offset = fix29_bee_formation_offset(edge.slot);
            const auto old_index = index_of(
                static_cast<std::uint32_t>(static_cast<std::int32_t>(edge.home.x) + offset.x),
                static_cast<std::uint32_t>(static_cast<std::int32_t>(edge.home.y) + offset.y));
            const auto exit_index = index_of(edge.home.x + edge.start_x,
                static_cast<std::uint32_t>(static_cast<std::int32_t>(edge.home.y) + edge.start_y));
            const bool fixture = clicked && initial.exact &&
                seed[old_index].material == material_id(Material::bee) &&
                (seed[exit_index].material == material_id(Material::atmosphere) ||
                 seed[exit_index].material == material_id(Material::empty));
            bool settled = false;
            bool conserved = false;
            ColonyObservation final;
            if (fixture) {
                auto newborn = seed[old_index];
                const bool global_home = bee_uses_global_home(newborn.aux,
                    config.grid_width, config.grid_height);
                const auto home_timer = global_home
                    ? 256u | (edge.home.x & 15u) | ((edge.home.y & 15u) << 4u)
                    : (edge.home.x & 7u) | ((edge.home.y & 7u) << 3u);
                newborn.age = fix29_bee_pack_age(home_timer, fix29_bee_target_newborn);
                seed[old_index] = seed[exit_index];
                seed[exit_index] = newborn;
                const auto expected_pollen = count_material(seed, Material::pollen);
                const auto expected_honey = count_material(seed, Material::honey);
                const auto expected_atmosphere = count_material(seed, Material::atmosphere);
                const auto expected_gas_units = edge_gas_units(seed);
                upload_scene_cells(seed);
                state.paused.store(false, std::memory_order_release);
                const auto section_x = static_cast<std::int32_t>(edge.home.x / 640u);
                const auto section_y = static_cast<std::int32_t>(edge.home.y / 360u);
                for (std::uint32_t tick = 0u; tick < 96u; ++tick) {
                    run_acceptance_focused_tick(section_x, section_y, true, true);
                    if (tick == 0u)
                        lifecycle_detail += queen_diagnostic("tick1", download_scene_cells());
                }
                const auto actual = download_scene_cells();
                lifecycle_detail += queen_diagnostic("tick96", actual);
                final = observe_colonies(actual, {edge.home}, 2u);
                settled = final.exact && simulation_step == 96u &&
                    actual[old_index].material == material_id(Material::bee) &&
                    bee_slot_from_metadata(actual[old_index].aux,
                        config.grid_width, config.grid_height) == edge.slot;
                const auto actual_pollen = count_material(actual, Material::pollen);
                const auto actual_honey = count_material(actual, Material::honey);
                const auto actual_atmosphere = count_material(actual, Material::atmosphere);
                const auto actual_gas_units = edge_gas_units(actual);
                // Empty chamber/exit cells legitimately receive split Air.
                // Compare represented baseline+stored gas, not the number of
                // cells carrying it; preserve raw cell counts as diagnostics.
                conserved = actual_pollen == expected_pollen &&
                    actual_honey == expected_honey &&
                    actual_gas_units == expected_gas_units &&
                    count_material(actual, Material::flower) == 0u;
                lifecycle_detail += " [before/after pollen=" + std::to_string(expected_pollen) +
                    "/" + std::to_string(actual_pollen) + " honey=" + std::to_string(expected_honey) +
                    "/" + std::to_string(actual_honey) + " atmosphere_cells=" +
                    std::to_string(expected_atmosphere) + "/" + std::to_string(actual_atmosphere) +
                    " represented_gas=" + std::to_string(expected_gas_units) + "/" +
                    std::to_string(actual_gas_units) + "]";
            }
            edge_routes = edge_routes && fixture && settled && conserved;
            edge_detail += " [" + std::string{edge.name} + " slot=" +
                std::to_string(edge.slot) + " start=" + std::to_string(edge.start_x) + "," +
                std::to_string(edge.start_y) + " fixture/settled/conserved=" +
                std::to_string(fixture ? 1u : 0u) + "/" +
                std::to_string(settled ? 1u : 0u) + "/" +
                std::to_string(conserved ? 1u : 0u) + "]" + lifecycle_detail + final.detail;
        }
        append("beehive_true_footprint_edges_return_newborn_without_overwrite",
            edge_routes, "forced transit:4 exit-start edges plus1 tunnel-start witness,96 real ticks each; exact60 slots, fixed59, "
                "body and food preserved; no extra placement margin;" + edge_detail);
    }

    state.selected_workspace.store(saved_workspace, std::memory_order_release);
    state.selected_material.store(saved_material, std::memory_order_release);
    state.paused.store(saved_paused, std::memory_order_release);
    state.blueprint_placement_active.store(saved_blueprint, std::memory_order_release);
    state.beehive_place_request.store(saved_request, std::memory_order_release);
    tool_hive_anchor = saved_anchor;
    simulation_step = saved_step;
}
