            {
                // Real paused Editor dispatch, densely overlapping vertical
                // shifts. Compare every canonical word to a serial oracle;
                // no gas compression is involved in this fixture.
                auto original = acceptance_atmosphere_world();
                seed_rect(original, Material::stone, 156u, 60u, 16u, 72u);
                seed_rect(original, Material::empty, 160u, 64u, 7u, 16u);
                for (std::uint32_t x = 160u; x <= 166u; ++x)
                    for (std::uint32_t y = 80u; y <= 120u; ++y) {
                        SceneCell water = make_fill_cell(material_id(Material::water),
                            static_cast<std::uint32_t>(index_of(x, y)));
                        water.age = y;
                        water.temperature = 20 + static_cast<std::int32_t>(y % 11u);
                        if ((y & 1u) == 0u) water.aux |= 0x10000000u;
                        original[index_of(x, y)] = water;
                    }
                auto expected = original;
                SharedState brush_state{};
                brush_state.paused.store(true);
                brush_state.selected_workspace.store(1u); // Editor
                brush_state.brush_radius.store(3u);
                brush_state.brush_shape.store(0u);
                const auto frozen_step = simulation_step;
                const auto paint_hash = [](std::uint32_t value) {
                    value ^= value >> 16u; value *= 0x7feb352du;
                    value ^= value >> 15u; value *= 0x846ca68bu;
                    return value ^ (value >> 16u);
                };
                for (std::uint32_t phase = 0u; phase < 3u; ++phase)
                    for (std::uint32_t x = 160u; x <= 166u; ++x) {
                        if (x % 3u != phase) continue;
                        for (int y = 107; y >= 101; --y) {
                            const int dx = static_cast<int>(x) - 163;
                            const int dy = y - 104;
                            if (dx * dx + dy * dy > 9) continue;
                            int hole = y - 1;
                            while (expected[index_of(x, static_cast<std::uint32_t>(hole))].material ==
                                   material_id(Material::water)) --hole;
                            for (int row = hole; row < y; ++row) {
                                auto moved = expected[index_of(x, static_cast<std::uint32_t>(row + 1))];
                                moved.age = 0u;
                                moved.aux |= 0x01000000u;
                                expected[index_of(x, static_cast<std::uint32_t>(row))] = moved;
                            }
                            const auto salt = x - 160u + static_cast<std::uint32_t>(y - 101) * 7u;
                            const auto entropy = paint_hash(x * 73856093u ^
                                static_cast<std::uint32_t>(y) * 19349663u ^
                                frozen_step * 83492791u ^ random_seed ^ salt) & 0x007fff00u;
                            expected[index_of(x, static_cast<std::uint32_t>(y))] =
                                SceneCell{material_id(Material::smoke), 0u, 20, entropy};
                        }
                    }
                bool repeat_exact = true;
                bool membership_exact = true;
                for (int repeat = 0; repeat < 3; ++repeat) {
                    upload_scene_cells(original, true);
                    simulation_step = frozen_step;
                    immediate_submit([&](const VkCommandBuffer command_buffer) {
                        record_paint_at_grid(command_buffer, brush_state, false, true,
                            163, 104, material_id(Material::smoke));
                    });
                    const auto after = download_scene_cells();
                    repeat_exact = repeat_exact && simulation_step == frozen_step &&
                        std::memcmp(after.data(), expected.data(), after.size() * sizeof(SceneCell)) == 0;
                    membership_exact = membership_exact && rain_membership_matches(after);
                }
                append("paint_dense_smoke_columns_exact_serial_owners",
                    repeat_exact && membership_exact,
                    "three paused Editor repeats; all canonical words and shifted rain membership exact=" +
                        std::to_string(repeat_exact) + "/" + std::to_string(membership_exact));
            }

            {
                // Adjacent TILE-mode life edits share gas receivers. Use one
                // uniform named gas so this tests scheduling/conservation,
                // not the unresolved packed-Atmosphere or mixed-heat merger.
                constexpr std::uint32_t tile_x = 200u;
                constexpr std::uint32_t tile_y = 144u;
                constexpr std::uint32_t left = tile_x - 1u;
                constexpr std::uint32_t top = tile_y - 1u;
                constexpr std::uint32_t right = tile_x + 9u;
                constexpr std::uint32_t bottom = tile_y + 9u;
                auto original = acceptance_atmosphere_world();
                const SceneCell oxygen{material_id(Material::oxygen), 0u, 20, 1u};
                for (std::uint32_t y = top; y < bottom; ++y)
                    for (std::uint32_t x = left; x < right; ++x)
                        original[index_of(x, y)] = oxygen;

                SharedState brush_state{};
                brush_state.paused.store(true);
                brush_state.selected_workspace.store(1u); // Editor
                brush_state.placement_mode.store(1u); // Actual aligned TILE edit
                brush_state.brush_radius.store(3u); // TILE policy overrides to 8
                brush_state.brush_shape.store(1u);
                const auto frozen_step = simulation_step;
                std::vector<SceneCell> first_result;
                bool counts_exact = true;
                bool payload_exact = true;
                bool outside_exact = true;
                bool repeat_exact = true;
                bool clock_frozen = true;
                std::uint32_t last_beetles = 0u;
                std::uint32_t last_oxygen_cells = 0u;
                std::uint32_t last_oxygen_units = 0u;
                for (int repeat = 0; repeat < 3; ++repeat) {
                    upload_scene_cells(original, true);
                    simulation_step = frozen_step;
                    immediate_submit([&](const VkCommandBuffer command_buffer) {
                        record_paint_at_grid(command_buffer, brush_state, false, true,
                            static_cast<std::int32_t>(tile_x + 3u),
                            static_cast<std::int32_t>(tile_y + 3u),
                            material_id(Material::beetle));
                    });
                    const auto after = download_scene_cells();
                    clock_frozen = clock_frozen && simulation_step == frozen_step &&
                        brush_state.paused.load();
                    last_beetles = 0u;
                    last_oxygen_cells = 0u;
                    last_oxygen_units = 0u;
                    for (std::uint32_t y = top; y < bottom; ++y) {
                        for (std::uint32_t x = left; x < right; ++x) {
                            const auto& cell = after[index_of(x, y)];
                            const bool interior = x >= tile_x && x < tile_x + 8u &&
                                                  y >= tile_y && y < tile_y + 8u;
                            payload_exact = payload_exact && cell.temperature == 20 &&
                                cell.age == 0u;
                            if (interior) {
                                last_beetles += cell.material == material_id(Material::beetle)
                                    ? 1u : 0u;
                            } else {
                                last_oxygen_cells += cell.material == material_id(Material::oxygen)
                                    ? 1u : 0u;
                                const auto volume = cell.aux & 0xffu;
                                last_oxygen_units += volume;
                                payload_exact = payload_exact && volume > 0u &&
                                    (cell.aux & ~0xffu) == 0u;
                            }
                        }
                    }
                    counts_exact = counts_exact && last_beetles == 64u &&
                        last_oxygen_cells == 36u && last_oxygen_units == 100u;

                    // Compare every outside byte in contiguous spans rather
                    // than accepting only matching material totals.
                    const auto same_range = [&](std::size_t first, std::size_t end) {
                        return std::memcmp(after.data() + first, original.data() + first,
                                           (end - first) * sizeof(SceneCell)) == 0;
                    };
                    outside_exact = outside_exact && same_range(0u, index_of(0u, top)) &&
                        same_range(index_of(0u, bottom), after.size());
                    for (std::uint32_t y = top; y < bottom; ++y) {
                        outside_exact = outside_exact &&
                            same_range(index_of(0u, y), index_of(left, y)) &&
                            same_range(index_of(right, y), index_of(0u, y + 1u));
                    }
                    if (repeat == 0) first_result = after;
                    else repeat_exact = repeat_exact &&
                        std::memcmp(after.data(), first_result.data(),
                                    after.size() * sizeof(SceneCell)) == 0;
                }
                append("paint_tile_beetles_conserve_uniform_oxygen_receivers",
                    counts_exact && payload_exact && outside_exact && repeat_exact && clock_frozen,
                    "three paused TILE repeats; beetles=" + std::to_string(last_beetles) +
                        " perimeter_oxygen=" + std::to_string(last_oxygen_cells) +
                        " oxygen_units=" + std::to_string(last_oxygen_units) +
                        " payload_exact=" + std::to_string(payload_exact) +
                        " outside_exact=" + std::to_string(outside_exact) +
                        " repeat_exact=" + std::to_string(repeat_exact) +
                        " clock_frozen=" + std::to_string(clock_frozen));
            }
