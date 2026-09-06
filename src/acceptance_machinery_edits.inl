// Narrow production counterexamples for reset-Atmosphere conveyor admission
// and Habitat donor/recipient symmetry. These do not claim a working district
// production cycle or general recipe/thermal conservation acceptance.
{
    constexpr std::uint32_t machinery_side = 192u;
    const auto machinery_prefix = static_cast<std::size_t>(config.grid_width) * machinery_side;
    const auto machinery_saved_step = simulation_step;
    const auto machinery_same = [](const SceneCell& a, const SceneCell& b) {
        return a.material == b.material && a.age == b.age &&
               a.temperature == b.temperature && a.aux == b.aux;
    };
    const auto machinery_stats = [&]() {
        immediate_submit([&](const VkCommandBuffer command_buffer) {
            buffer_barrier(command_buffer, conservation_buffer,
                VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT);
        });
        std::array<std::uint32_t, debug_stat_word_count> result{};
        void* mapped = nullptr;
        check_vk(vkMapMemory(device, conservation_buffer.memory, 0,
                            conservation_buffer.size, 0, &mapped),
                 "vkMapMemory(machinery donor acceptance)");
        std::memcpy(result.data(), mapped, sizeof(result));
        vkUnmapMemory(device, conservation_buffer.memory);
        return result;
    };

    for (std::uint32_t parity = 0u; parity < 2u; ++parity) {
        const SceneCell wall{material_id(Material::stone), 37u, 20,
            fill_aux_structural | fill_aux_supported | 255u};
        std::vector<SceneCell> cells(machinery_prefix, wall);
        auto expected = cells;
        // Two directions, legacy Vacuum, real reset Air with packed component,
        // two stored gases, and non-gas obstructions. Odd parity crosses x47/48.
        const std::array media{Material::empty, Material::atmosphere,
            Material::oxygen, Material::steam, Material::water, Material::stone};
        std::uint32_t expected_swaps = 0u;
        for (std::uint32_t direction = 0u; direction < 2u; ++direction) {
            for (std::uint32_t kind = 0u; kind < media.size(); ++kind) {
                const auto left = 46u + parity;
                const auto y = 20u + (direction * 6u + kind) * 8u;
                const auto cargo_x = direction == 0u ? left : left + 1u;
                const auto medium_x = direction == 0u ? left + 1u : left;
                const SceneCell cargo{material_id(Material::copper), 73u, 87,
                                      0x005544adu};
                SceneCell medium{material_id(media[kind]), 91u, -17, 83u};
                if (media[kind] == Material::atmosphere)
                    medium.aux = 0x40000000u |
                        (material_id(Material::carbon_dioxide) << 8u) | (7u << 15u) | 83u;
                if (media[kind] == Material::stone)
                    medium.aux |= fill_aux_structural | fill_aux_supported;
                if (media[kind] == Material::empty) medium.aux = 0x00123400u;
                const SceneCell belt{material_id(Material::conveyor), 29u, 20,
                    fill_aux_structural | fill_aux_supported | 255u |
                    (direction == 0u ? 0x40000000u : 0u)};
                cells[index_of(cargo_x, y)] = cargo;
                cells[index_of(medium_x, y)] = medium;
                cells[index_of(cargo_x, y + 1u)] = belt;
                expected[index_of(cargo_x, y)] = cargo;
                expected[index_of(medium_x, y)] = medium;
                expected[index_of(cargo_x, y + 1u)] = belt;
                if (kind < 4u) {
                    auto moved_cargo = cargo;
                    moved_cargo.age = 0u;
                    moved_cargo.aux |= fill_aux_moved;
                    auto displaced = medium;
                    if (media[kind] != Material::atmosphere) displaced.age = 0u;
                    displaced.aux |= fill_aux_moved;
                    expected[index_of(medium_x, y)] = moved_cargo;
                    expected[index_of(cargo_x, y)] = displaced;
                    ++expected_swaps;
                }
            }
        }
        // A reversed belt and an absent belt cannot transport copper merely
        // because the adjoining Air is available.
        for (std::uint32_t variant = 0u; variant < 2u; ++variant) {
            const auto x = 78u + parity;
            const auto y = 132u + variant * 8u;
            cells[index_of(x, y)] = {material_id(Material::copper), 31u, 20, 211u};
            cells[index_of(x + 1u, y)] = {material_id(Material::atmosphere), 41u, 20, 54u};
            if (variant == 0u)
                cells[index_of(x, y + 1u)] = {material_id(Material::conveyor), 7u, 20,
                    fill_aux_structural | fill_aux_supported | 255u};
            expected[index_of(x, y)] = cells[index_of(x, y)];
            expected[index_of(x + 1u, y)] = cells[index_of(x + 1u, y)];
            expected[index_of(x, y + 1u)] = cells[index_of(x, y + 1u)];
        }
        upload_acceptance_cell_prefix(cells);
        simulation_step = 19u;
        run_acceptance_horizontal_pass(static_cast<std::int32_t>(parity));
        const auto after = download_scene_cell_prefix(machinery_prefix);
        std::size_t mismatches = 0u;
        std::size_t first = machinery_prefix;
        for (std::size_t index = 0u; index < machinery_prefix; ++index) {
            if (!machinery_same(after[index], expected[index])) {
                if (first == machinery_prefix) first = index;
                ++mismatches;
            }
        }
        const auto stats = machinery_stats();
        append(parity == 0u ? "conveyor_displaced_gas_exact_even_pairs"
                            : "conveyor_displaced_gas_exact_odd_cross_tile_pairs",
            mismatches == 0u && stats[0] == 0u && stats[1] == 0u &&
                stats[2] == 0u && stats[3] == 0u && stats[7] == 0u,
            "probes=14 swaps=" + std::to_string(expected_swaps) +
            " full_payload_mismatches=" + std::to_string(mismatches) +
            " first_index=" + std::to_string(first) +
            " created/destroyed/converted/errors=" + std::to_string(stats[0]) + "/" +
            std::to_string(stats[1]) + "/" + std::to_string(stats[2]) + "/" +
            std::to_string(stats[7]) + " movement_debug_disabled=true");
    }

    {
        constexpr std::uint32_t fixture_step = 19u;
        const SceneCell air{material_id(Material::atmosphere), 41u, 20, 54u};
        std::vector<SceneCell> cells(machinery_prefix, air);
        std::vector<std::pair<std::size_t, SceneCell>> expected_cells;
        std::vector<std::size_t> controller_indices;
        std::vector<std::size_t> donor_indices;
        const auto inventory_bits = [](std::uint32_t x, std::uint32_t y, std::uint32_t z) {
            return (x << 8u) | (y << 12u) | (z << 16u);
        };
        const auto controller = [&](std::uint32_t x, std::uint32_t y,
                                    std::uint32_t before, std::uint32_t after) {
            const auto index = index_of(x, y);
            cells[index] = {material_id(Material::insect_habitat), 5u, 20,
                fill_aux_structural | fill_aux_supported | 255u | before};
            auto expected = cells[index];
            expected.age = 6u;
            expected.aux = (expected.aux & ~fill_aux_random_mask) | after;
            expected_cells.emplace_back(index, expected);
            controller_indices.push_back(index);
        };
        const SceneCell stored{material_id(Material::empty), 0u, 20,
            fill_hash(material_id(Material::empty) ^ random_seed ^ fixture_step) &
                fill_aux_random_mask};
        std::uint32_t accepted = 0u;
        const auto donor = [&](std::uint32_t x, std::uint32_t y, Material material,
                               bool consumed, std::uint32_t age = 11u) {
            const auto index = index_of(x, y);
            cells[index] = {material_id(material), age, 20, 255u};
            auto expected = cells[index];
            ++expected.age;
            if (consumed) { expected = stored; ++accepted; }
            expected_cells.emplace_back(index, expected);
            donor_indices.push_back(index);
        };
        controller(35u, 35u, 0u, inventory_bits(2u, 1u, 1u));
        donor(32u, 34u, Material::food, true);
        donor(33u, 34u, Material::food, true);
        donor(32u, 35u, Material::waste, true);
        donor(33u, 35u, Material::fertilizer, true);
        // Rank is from the immutable row-major donor snapshot, not an atomic
        // race. A full slot leaves every rejected donor's exact payload intact.
        controller(83u, 35u, inventory_bits(14u, 15u, 14u), inventory_bits(15u, 15u, 15u));
        for (std::uint32_t n = 0u; n < 3u; ++n) {
            donor(79u + n, 33u, Material::food, n == 0u);
            donor(79u + n, 34u, Material::waste, false);
            donor(79u + n, 35u, Material::fertilizer, n == 0u);
        }
        controller(131u, 35u, inventory_bits(15u, 15u, 15u), inventory_bits(15u, 15u, 15u));
        donor(128u, 34u, Material::food, false);
        donor(129u, 34u, Material::waste, false);
        donor(128u, 35u, Material::fertilizer, false);
        controller(35u, 83u, 0u, 0u);
        donor(42u, 83u, Material::food, false); // Outside exact six-cell reach.
        donor(35u, 90u, Material::waste, false);
        donor(28u, 83u, Material::fertilizer, false);
        donor(33u, 81u, Material::copper, false); // Wrong machine family.
        controller(99u, 99u, inventory_bits(14u, 0u, 0u), inventory_bits(15u, 0u, 0u));
        controller(107u, 99u, 0u, 0u);
        donor(103u, 98u, Material::food, true);
        donor(103u, 99u, Material::food, false); // Same-distance tie stays left; no double credit.

        // The accepted Waste source also satisfies the later local conversion
        // into Fertilizer. Its exact stored debit must win, not be resurrected.
        controller(147u, 147u, 0u, inventory_bits(0u, 1u, 0u));
        std::uint32_t waste_age = 0u;
        while (waste_age < 65536u &&
               (fill_hash(144u * 73856093u ^ 147u * 19349663u ^
                    fixture_step * 83492791u ^ random_seed ^ waste_age ^ 255u) & 255u) != 0u)
            ++waste_age;
        donor(144u, 147u, Material::waste, true, waste_age);
        cells[index_of(144u, 146u)] = {material_id(Material::ant), 0u, 20, 1u};
        cells[index_of(143u, 147u)] = {material_id(Material::dirt), 0u, 20, 255u};
        cells[index_of(144u, 149u)] = {material_id(Material::water), 0u, 20, 0u};
        const auto represented = [&](const std::vector<SceneCell>& field) {
            std::uint64_t count = 0u;
            for (std::uint32_t y = 0u; y < machinery_side; ++y)
                for (std::uint32_t x = 0u; x < machinery_side; ++x)
                    count += field[index_of(x, y)].material != material_id(Material::empty) ? 1u : 0u;
            for (const auto index : controller_indices)
                for (std::uint32_t slot = 0u; slot < 4u; ++slot)
                    count += (field[index].aux >> (8u + slot * 4u)) & 15u;
            return count;
        };
        const auto before_units = represented(cells);
        upload_acceptance_cell_prefix(cells);
        simulation_step = fixture_step;
        run_acceptance_chemistry_pass(0, 0, false, true);
        const auto after = download_scene_cell_prefix(machinery_prefix);
        const auto stats = machinery_stats();
        std::size_t mismatches = 0u;
        std::size_t first = machinery_prefix;
        for (const auto& [index, expected] : expected_cells) {
            if (!machinery_same(after[index], expected)) {
                if (first == machinery_prefix) first = index;
                ++mismatches;
            }
        }
        const auto after_units = represented(after);
        std::uint32_t disappeared = 0u;
        for (const auto index : donor_indices)
            disappeared += after[index].material == material_id(Material::empty) ? 1u : 0u;
        append("habitat_exact_donor_credit_capacity_and_competing_chemistry",
            waste_age < 65536u && mismatches == 0u && before_units == after_units &&
                disappeared == accepted && stats[0] == 0u && stats[1] == 0u &&
                stats[2] == accepted && stats[3] == 0u && stats[7] == 0u &&
                stats[116] == accepted && stats[117] == 0u,
            "controllers=" + std::to_string(controller_indices.size()) +
            " donors=" + std::to_string(donor_indices.size()) +
            " accepted=" + std::to_string(accepted) +
            " donor_debits=" + std::to_string(disappeared) +
            " input_counter=" + std::to_string(stats[116]) +
            " converted=" + std::to_string(stats[2]) +
            " units=" + std::to_string(before_units) + "/" + std::to_string(after_units) +
            " full_payload_mismatches=" + std::to_string(mismatches) +
            " first_index=" + std::to_string(first) +
            " competing_waste_age=" + std::to_string(waste_age) +
            " step=" + std::to_string(fixture_step));
    }
    for (const bool translated : {false, true}) {
        // A64x64 dispatch tests real-world neighbors outside its copyback
        // rectangle. The translated version also has left/top inactive sections.
        const std::uint32_t ox = translated ? 640u : 0u;
        const std::uint32_t oy = translated ? 360u : 0u;
        if (config.grid_width < ox + 80u || config.grid_height < oy + 80u) continue;
        constexpr std::uint32_t edge_step = 19u;
        const auto prefix_count = static_cast<std::size_t>(config.grid_width) * (oy + 80u);
        const SceneCell air{material_id(Material::atmosphere), 41u, 20, 54u};
        std::vector<SceneCell> cells(prefix_count, air);
        std::vector<std::pair<std::size_t, SceneCell>> wanted;
        std::vector<std::size_t> controllers;
        const auto index_at = [&](int x, int y) {
            return index_of(static_cast<std::uint32_t>(static_cast<int>(ox) + x),
                            static_cast<std::uint32_t>(static_cast<int>(oy) + y));
        };
        const auto machine = [&](int x, int y, bool active, std::uint32_t before,
                                 std::uint32_t after) {
            const auto index = index_at(x, y);
            cells[index] = {material_id(Material::insect_habitat), 5u, 20,
                fill_aux_structural | fill_aux_supported | 255u | (before << 8u)};
            auto expected = cells[index];
            expected.age += active ? 1u : 0u;
            expected.aux = (expected.aux & ~fill_aux_random_mask) | (after << 8u);
            wanted.emplace_back(index, expected);
            controllers.push_back(index);
        };
        const auto food = [&](int x, int y, bool active, bool accepted) {
            const auto index = index_at(x, y);
            cells[index] = {material_id(Material::food), 11u, 20, 255u};
            auto expected = cells[index];
            expected.age += active ? 1u : 0u;
            if (accepted) expected = {material_id(Material::empty), 0u, 20,
                fill_hash(material_id(Material::empty) ^ random_seed ^ edge_step) & fill_aux_random_mask};
            wanted.emplace_back(index, expected);
        };
        machine(59, 19, true, 0u, 0u); food(65, 19, false, false);
        machine(67, 43, false, 0u, 0u); food(62, 43, true, false);
        machine(35, 19, true, 0u, 1u); food(34, 18, true, true);
        machine(59, 59, true, 14u, 15u);
        food(64, 57, false, false); // Earlier row-major, but cannot steal rank.
        food(58, 58, true, true);
        machine(35, 59, true, 0u, 0u); food(35, 65, false, false);
        machine(11, 67, false, 0u, 0u); food(11, 62, true, false);
        if (translated) {
            machine(3, 19, true, 0u, 0u); food(-2, 19, false, false);
            machine(-5, 43, false, 0u, 0u); food(1, 43, true, false);
            machine(11, 3, true, 0u, 0u); food(11, -2, false, false);
            machine(35, -5, false, 0u, 0u); food(35, 1, true, false);
        }
        const auto units = [&](const std::vector<SceneCell>& field) {
            std::uint64_t total = 0u;
            for (const auto& cell : field)
                total += cell.material != material_id(Material::empty) ? 1u : 0u;
            for (const auto index : controllers)
                for (std::uint32_t slot = 0u; slot < 4u; ++slot)
                    total += (field[index].aux >> (8u + slot * 4u)) & 15u;
            return total;
        };
        const auto before_units = units(cells);
        upload_acceptance_cell_prefix(cells);
        simulation_step = edge_step;
        run_acceptance_chemistry_pass(translated ? 1 : 0, translated ? 1 : 0,
                                     translated, true, 64u, 64u);
        const auto after = download_scene_cell_prefix(prefix_count);
        const auto stats = machinery_stats();
        std::size_t mismatches = 0u;
        std::size_t first = prefix_count;
        for (const auto& [index, expected] : wanted) {
            if (!machinery_same(after[index], expected)) {
                if (first == prefix_count) first = index;
                ++mismatches;
            }
        }
        // All out-of-dispatch cells, not only the selected inactive endpoints,
        // must stay byte-exact after chemistry, correction and clipped copyback.
        for (std::size_t index = 0u; index < prefix_count; ++index) {
            const auto x = index % config.grid_width;
            const auto y = index / config.grid_width;
            if (x >= ox && x < ox + 64u && y >= oy && y < oy + 64u) continue;
            if (!machinery_same(after[index], cells[index])) {
                if (first == prefix_count) first = index;
                ++mismatches;
            }
        }
        append(translated ? "machine_translated_inactive_endpoints_and_exact_clip"
                          : "machine_clipped_endpoints_and_rank_capacity",
            mismatches == 0u && units(after) == before_units &&
                stats[0] == 0u && stats[1] == 0u && stats[2] == 2u &&
                stats[3] == 0u && stats[7] == 0u && stats[116] == 2u && stats[117] == 0u,
            "dispatch=64x64 controllers=" + std::to_string(controllers.size()) +
            " full_payload_mismatches=" + std::to_string(mismatches) +
            " first_index=" + std::to_string(first) +
            " input_counter=" + std::to_string(stats[116]) +
            " units=" + std::to_string(before_units) + "/" + std::to_string(units(after)));
    }
    if (config.grid_width >= 704u && config.grid_height >= 384u) {
        // Explicit stale-metadata counterexamples, not a claimed reproduced UI
        // sequence. Y360 starts inside chunk row5, whose origin isY320.
        // Tiny translated passes isolate geometry from the early sleep owners.
        for (std::uint32_t variant = 0u; variant < 4u; ++variant) {
            const auto prefix_count = static_cast<std::size_t>(config.grid_width) * 384u;
            std::vector<SceneCell> cells(prefix_count,
                SceneCell{material_id(Material::atmosphere), 41u, 20, 54u});
            const bool structural_donor = variant != 0u;
            const bool accepted = variant == 1u || variant == 2u;
            const auto controller_index = index_of(643u, 363u);
            const auto donor_index = index_of(648u, 363u);
            cells[controller_index] = {material_id(Material::insect_habitat), 5u, 20,
                fill_aux_structural | fill_aux_supported | 255u};
            cells[donor_index] = {material_id(Material::food), 11u, 20,
                255u | (structural_donor ? fill_aux_structural | fill_aux_supported : 0u)};
            auto expected_controller = cells[controller_index];
            expected_controller.age = 6u;
            if (accepted) expected_controller.aux |= 1u << 8u;
            auto expected_donor = cells[donor_index];
            expected_donor.age = 12u;
            if (accepted) expected_donor = {material_id(Material::empty), 0u, 20,
                fill_hash(material_id(Material::empty) ^ random_seed ^ 19u) & fill_aux_random_mask};
            upload_acceptance_cell_prefix(cells);
            immediate_submit([&](const VkCommandBuffer command_buffer) {
                for (const auto* buffer : std::array{&chunk_buffer, &tile_buffer})
                    buffer_barrier(command_buffer, *buffer,
                        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                        VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT);
                vkCmdFillBuffer(command_buffer, chunk_buffer.handle, 0, chunk_buffer.size, 2u);
                vkCmdFillBuffer(command_buffer, tile_buffer.handle, 0, tile_buffer.size, 4u);
                for (const auto* buffer : std::array{&chunk_buffer, &tile_buffer})
                    buffer_barrier(command_buffer, *buffer,
                        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
                // Filled metadata is deliberately all sleeping. Only the
                // selected controller's wake witness is overridden per case.
                const auto chunk_index = (363u / 64u) * divide_round_up(config.grid_width, 64u) + 643u / 64u;
                const auto tile_index = (363u / 8u) * divide_round_up(config.grid_width, 8u) + 643u / 8u;
                const std::uint32_t chunk_flags = variant == 0u ? 2u : (variant == 2u ? 6u : 1u);
                const std::uint32_t tile_flags = variant == 2u ? 0x40u : (variant == 3u ? 4u : 8u);
                vkCmdUpdateBuffer(command_buffer, chunk_buffer.handle,
                    static_cast<VkDeviceSize>(chunk_index) * 16u, sizeof(chunk_flags), &chunk_flags);
                vkCmdUpdateBuffer(command_buffer, tile_buffer.handle,
                    static_cast<VkDeviceSize>(tile_index) * 16u + 8u, sizeof(tile_flags), &tile_flags);
                for (const auto* buffer : std::array{&chunk_buffer, &tile_buffer})
                    buffer_barrier(command_buffer, *buffer,
                        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            });
            simulation_step = 19u;
            run_acceptance_chemistry_pass(1, 1, true, true, 64u, 16u);
            const auto after = download_scene_cell_prefix(prefix_count);
            const auto stats = machinery_stats();
            const auto expected_count = accepted ? 1u : 0u;
            const bool unit_owner_exact = machinery_same(after[controller_index], expected_controller) &&
                machinery_same(after[donor_index], expected_donor) &&
                ((after[controller_index].aux >> 8u) & 15u) +
                    (after[donor_index].material == material_id(Material::food) ? 1u : 0u) == 1u;
            const std::array names{
                "machine_stale_sleeping_partial_chunk_waits_without_credit",
                "machine_sleeping_structural_stock_uses_active_controller_witness",
                "machine_dirty_chunk_and_collapsing_controller_witness",
                "machine_stale_sleeping_structural_tile_waits_without_credit"};
            append(names[variant], unit_owner_exact && stats[0] == 0u && stats[1] == 0u &&
                stats[2] == expected_count && stats[3] == 0u && stats[7] == 0u &&
                stats[116] == expected_count && stats[117] == 0u,
                "dispatch=64x16 translated_y360 injected_sleep_metadata=true accepted=" +
                    std::to_string(expected_count) + " inventory=" +
                    std::to_string((after[controller_index].aux >> 8u) & 15u) +
                    " donor_material=" + std::to_string(after[donor_index].material) +
                    " input_counter=" + std::to_string(stats[116]) +
                    " converted=" + std::to_string(stats[2]));
        }
    }
    simulation_step = machinery_saved_step;
}
