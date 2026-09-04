#include "sandhybrid/hive_recovery.hpp"
#include "sandhybrid/actor_medium.hpp"
#include "sandhybrid/material.hpp"
#include "sandhybrid/world_layout.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace {
using namespace sandhybrid;
constexpr SceneCell air{static_cast<std::uint32_t>(Material::atmosphere), 0u, 20, 54u};

constexpr std::uint32_t hash(std::uint32_t value) {
    value ^= value >> 16u; value *= 0x7feb352du;
    value ^= value >> 15u; value *= 0x846ca68bu;
    value ^= value >> 16u; return value;
}

// Independent historical classifier: never use recovery's selector as oracle.
Material historical_material(const int dx, const int dy, const int local_y) {
    if (dx == 0 && dy == 0) return Material::queen_bee;
    if (dx >= 1 && dx <= 10 && dy >= -1 && dy <= 1) return Material::empty;
    const auto radius = dx * dx + dy * dy;
    if (radius >= 24 && radius < 88) return Material::beehive;
    if (radius < 24) {
        const auto entropy = hash(static_cast<std::uint32_t>(
            (local_y + dy) * 640 + 512 + dx) ^ 0xD17A5EEDu);
        if ((entropy & 3u) == 0u) return Material::empty;
        return ((entropy >> 2u) & 3u) == 0u ? Material::pollen : Material::honey;
    }
    return Material::count;
}

std::size_t at(const std::uint32_t width, const int x, const int y) {
    return static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x);
}

SceneCell injected_cell(const Material material, const std::size_t index) {
    const auto id = static_cast<std::uint32_t>(material);
    auto aux = hash(static_cast<std::uint32_t>(index) ^ id * 0x9e3779b9u) & 0x007fff00u;
    if (material == Material::honey || material == Material::pollen) aux |= 0x060000ffu;
    return {id, 0u, 20, aux};
}

bool same(const std::vector<SceneCell>& a, const std::vector<SceneCell>& b) {
    return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(SceneCell)) == 0;
}

std::vector<std::size_t> seed_hive(std::vector<SceneCell>& cells,
    const std::uint32_t width, const int x, const int y, const int local_y,
    const bool legitimate) {
    std::vector<std::size_t> indices;
    for (int dy = -11; dy <= 11; ++dy) {
        for (int dx = -11; dx <= 12; ++dx) {
            const auto material = historical_material(dx, dy, local_y);
            if (material == Material::count) continue;
            const auto index = at(width, x + dx, y + dy);
            cells[index] = injected_cell(material, index);
            if (legitimate && material == Material::beehive) cells[index].aux |= 0x060000ffu;
            indices.push_back(index);
        }
    }
    for (std::size_t slot = 0u; slot < fix29_bee_formation_count; ++slot) {
        const auto offset = fix29_bee_formation_offset(slot);
        const auto index = at(width, x + offset.x, y + offset.y);
        cells[index] = injected_cell(Material::bee, index);
        if (legitimate) {
            cells[index].aux = 0x18000000u | (static_cast<std::uint32_t>(slot) << 13u) | 64u;
            cells[index].age = fix29_bee_pack_age(fix29_bee_initial_timer(slot), fix29_bee_target_none);
        }
        indices.push_back(index);
    }
    return indices;
}

std::vector<SceneCell> baseline(const std::uint32_t width) {
    std::vector<SceneCell> cells(static_cast<std::size_t>(width) * 1440u, air);
    // Exact payload sentinels include actor-adjacent cells and unrelated Bees;
    // recovery receives no actor buffer and has no authority over inventory.
    for (std::size_t index = 13u; index < cells.size(); index += 9973u)
        cells[index] = {static_cast<std::uint32_t>(Material::water),
            static_cast<std::uint32_t>(index), -13, 0x018073aau};
    for (std::uint32_t district = 0u; district < 2u; ++district) {
        const auto x = persistent_world_district_origin_x(width, district) + 512u;
        const auto y = persistent_world_district_origin_y(1440u, district) +
            (district == 0u ? 234u : 232u);
        static_cast<void>(seed_hive(cells, width, static_cast<int>(x), static_cast<int>(y),
            district == 0u ? 234 : 232, true));
    }
    static_cast<void>(seed_hive(cells, width, 3300, 960, 234, true));
    cells[at(width, 1842, 954)] = injected_cell(Material::bee, at(width, 1842, 954));
    for (int y = 1090; y < 1113; ++y)
        for (int x = 2084; x < 2093; ++x)
            cells[at(width, x, y)] = {static_cast<std::uint32_t>(Material::atmosphere),
                9786u, 31, 0x00832135u};
    return cells;
}

bool rejected_unchanged(std::vector<SceneCell>& cells, const std::uint32_t width,
    const std::uint32_t height = 1440u, const Scene scene = world_scene,
    const std::uint32_t schema = 2u) {
    const auto before = cells;
    const auto result = recover_v2527_phantom_hive(cells, width, height, scene, schema);
    return result.status != PhantomHiveRecoveryStatus::repaired &&
        result.changed_cells() == 0u && same(cells, before);
}

bool test_recovery(const std::uint32_t width) {
    auto cells = baseline(width);
    if (!rejected_unchanged(cells, width)) return false;
    const auto injected = seed_hive(cells, width, 1792, 954, 234, false);
    auto expected = cells;
    // Recovery restores ONLY the explicitly proven injected footprint to Air,
    // not an invented earlier world (even when a sentinel preceded injection).
    for (const auto index : injected)
        if (expected[index].material != static_cast<std::uint32_t>(Material::empty))
            expected[index] = air;
    const auto result = recover_v2527_phantom_hive(cells, width, 1440u, world_scene, 2u);
    if (result.status != PhantomHiveRecoveryStatus::repaired || result.queen_x != 1792u ||
        result.queen_y != 954u || result.body_cells != 242u || result.empty_cells != 0u ||
        result.bee_cells != 60u || result.changed_cells() != 302u ||
        result.mismatches != 0u || !same(cells, expected)) return false;
    if (!rejected_unchanged(cells, width)) return false;
    std::cout << "phantom exact repair " << width << "x1440: body=242 Bees=60; all 38 openings/outside bytes retained\n";
    return true;
}

bool test_negative_and_remnants() {
    constexpr auto width = 5120u;
    auto cells = baseline(width);
    static_cast<void>(seed_hive(cells, width, 1792, 954, 234, false));
    if (!rejected_unchanged(cells, width, 1440u, world_scene, 1u) ||
        !rejected_unchanged(cells, width, 1440u, Scene::ecosystem) ||
        !rejected_unchanged(cells, width, 1441u) ||
        !rejected_unchanged(cells, 640u, 360u)) return false;
    const auto shell = at(width, 1792, 945);
    cells[shell].aux ^= 0x00000100u;
    if (!rejected_unchanged(cells, width)) return false;
    cells[shell] = air;
    if (!rejected_unchanged(cells, width)) return false;
    static_cast<void>(seed_hive(cells, width, 1792, 954, 234, false));
    const auto real_queen = at(width, 512, 954);
    const auto saved_queen = cells[real_queen];
    cells[real_queen] = air;
    if (!rejected_unchanged(cells, width)) return false;
    cells[real_queen] = saved_queen;
    cells[at(width, 1793, 954)] = {static_cast<std::uint32_t>(Material::stone), 0u, 20, 0x060000ffu};
    if (!rejected_unchanged(cells, width)) return false;
    static_cast<void>(seed_hive(cells, width, 1792, 954, 234, true));
    if (!rejected_unchanged(cells, width)) return false;
    cells = baseline(width);
    static_cast<void>(seed_hive(cells, width, 1800, 954, 234, false));
    if (!rejected_unchanged(cells, width)) return false;

    cells = baseline(width);
    const auto injected = seed_hive(cells, width, 1792, 954, 234, false);
    const auto first = fix29_bee_formation_offset(0u);
    const auto second = fix29_bee_formation_offset(1u);
    const auto third = fix29_bee_formation_offset(2u);
    const auto packed = at(width, 1792 + first.x, 954 + first.y);
    const auto altered = at(width, 1792 + second.x, 954 + second.y);
    const auto occupied = at(width, 1792 + third.x, 954 + third.y);
    const auto edited_hole = at(width, 1793, 954);
    cells[packed].aux |= 0x18000000u;
    cells[altered].aux ^= 0x100u;
    cells[occupied] = {static_cast<std::uint32_t>(Material::stone), 9u, 57, 0x060000ffu};
    cells[edited_hole].aux ^= 0x100u;
    cells[shell].age = 7654u;
    cells[shell].temperature = 43;
    auto expected = cells;
    for (const auto index : injected)
        if (index != packed && index != altered && index != occupied && index != edited_hole &&
            expected[index].material != static_cast<std::uint32_t>(Material::empty))
            expected[index] = air;
    const auto result = recover_v2527_phantom_hive(cells, width, 1440u, world_scene, 2u);
    if (result.status != PhantomHiveRecoveryStatus::repaired || result.body_cells != 242u ||
        result.empty_cells != 0u || result.bee_cells != 57u || result.changed_cells() != 299u ||
        result.preserved_formation_cells != 3u || !same(cells, expected)) return false;
    std::cout << "negative provenance gates and exact-only aged/remnant repair: PASS\n";

    cells = baseline(width);
    const auto aged_injected = seed_hive(cells, width, 1792, 954, 234, false);
    std::uint32_t relaxed_openings{};
    for (const auto index : aged_injected) {
        if (cells[index].material == static_cast<std::uint32_t>(Material::empty)) {
            cells[index] = {static_cast<std::uint32_t>(Material::atmosphere),
                307u + static_cast<std::uint32_t>(index % 100u), 30, 54u};
            ++relaxed_openings;
        }
    }
    expected = cells;
    for (const auto index : aged_injected)
        if (expected[index].material != static_cast<std::uint32_t>(Material::atmosphere))
            expected[index] = air;
    const auto aged_result = recover_v2527_phantom_hive(cells, width, 1440u, world_scene, 2u);
    if (relaxed_openings != 38u || aged_result.status != PhantomHiveRecoveryStatus::repaired ||
        aged_result.body_cells != 242u || aged_result.bee_cells != 60u ||
        aged_result.empty_cells != 0u || aged_result.changed_cells() != 302u ||
        !same(cells, expected)) return false;
    std::cout << "aged phantom repair: all 38 Atmosphere openings byte-identical; body=242 Bees=60\n";
    return true;
}

bool test_schema1(const std::uint32_t width, const std::uint32_t height) {
    std::vector<SceneCell> cells(static_cast<std::size_t>(width) * height, air);
    const bool persistent = width >= 5120u;
    for (std::uint32_t district = 0u; district < 2u; ++district) {
        const auto scene = district == 0u ? Scene::sandbox : Scene::ecosystem;
        const auto origin_x = persistent ? persistent_world_district_origin_x(width, district) : 0u;
        const auto origin_y = persistent ? persistent_world_district_origin_y(height, district) : 0u;
        const auto local_y = district == 0u ? 234u : 232u;
        std::vector<std::uint32_t> materials(cells.size());
        for (std::size_t index = 0u; index < cells.size(); ++index) materials[index] = cells[index].material;
        normalize_pre_pr19_hives(materials, width, height, origin_x, origin_y, scene);
        for (std::size_t index = 0u; index < cells.size(); ++index)
            if (cells[index].material != materials[index])
                cells[index] = injected_cell(static_cast<Material>(materials[index]), index);
        // Same-material legacy Bees are not passed through a constructor by
        // the loader. Their incompatible role/movement flags must be replaced,
        // not retained alongside the new canonical home/slot/lifecycle owner.
        const auto legacy_offset = fix29_bee_formation_offset(0u);
        auto& legacy_bee = cells[at(width,
            static_cast<int>(origin_x + 512u) + legacy_offset.x,
            static_cast<int>(origin_y + local_y) + legacy_offset.y)];
        legacy_bee.aux |= 0xe7000000u;
        legacy_bee.age = 0x01234567u;
        legacy_bee.temperature = 37;
        auto expected = cells;
        const auto qx = static_cast<int>(origin_x + 512u);
        const auto qy = static_cast<int>(origin_y + local_y);
        for (int dy = -11; dy <= 11; ++dy) {
            for (int dx = -11; dx <= 12; ++dx) {
                const auto material = historical_material(dx, dy, static_cast<int>(local_y));
                if (material == Material::beehive || material == Material::honey || material == Material::pollen)
                    expected[at(width, qx + dx, qy + dy)].aux |= 0x060000ffu;
            }
        }
        for (std::size_t slot = 0u; slot < 60u; ++slot) {
            const auto offset = fix29_bee_formation_offset(slot);
            auto& bee = expected[at(width, qx + offset.x, qy + offset.y)];
            const auto home = persistent ? 64u | ((local_y / 8u) << 7u) | (district << 20u)
                : 128u | ((local_y / 4u) << 8u) | 0x00400000u;
            bee.aux = 0x18000000u | home |
                (static_cast<std::uint32_t>(slot) << (persistent ? 13u : 15u));
            bee.age = fix29_bee_pack_age(fix29_bee_initial_timer(slot), fix29_bee_target_none);
        }
        const auto result = initialize_schema1_hive_owners(cells, width, height, origin_x, origin_y, scene);
        if (!result.initialized || result.bee_cells != 60u ||
            result.structural_cells != (district == 0u ? 241u : 233u) ||
            !same(cells, expected)) return false;
        const auto offset = fix29_bee_formation_offset(59u);
        cells[at(width, qx + offset.x, qy + offset.y)] = air;
        const auto before = cells;
        if (initialize_schema1_hive_owners(cells, width, height, origin_x, origin_y, scene).initialized ||
            !same(cells, before)) return false;
    }
    std::cout << "schema-1 full 60-owner migration " << width << 'x' << height << ": PASS\n";
    return true;
}
} // namespace

int main() {
    for (const auto width : {5120u, 7680u, 10240u})
        if (!test_recovery(width)) return 1;
    if (!test_negative_and_remnants()) return 2;
    if (!test_schema1(5120u, 1440u) || !test_schema1(640u, 360u)) return 3;
    return 0;
}
