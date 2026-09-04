#include "sandhybrid/hive_recovery.hpp"
#include "sandhybrid/actor_medium.hpp"
#include "sandhybrid/material.hpp"
#include "sandhybrid/world_layout.hpp"

#include <limits>

namespace sandhybrid {
namespace {
constexpr std::uint32_t structural_content = 0x060000ffu;
constexpr std::uint32_t swarm_fed = 0x18000000u;
constexpr SceneCell clean_atmosphere{
    static_cast<std::uint32_t>(Material::atmosphere), 0u, 20, 54u};

[[nodiscard]] constexpr std::uint32_t old_constructor_aux(
    const std::uint32_t index, const Material material) noexcept {
    auto aux = pre_pr19_hive_hash(
        index ^ static_cast<std::uint32_t>(material) * 0x9e3779b9u) & 0x007fff00u;
    if (material == Material::honey || material == Material::pollen)
        aux |= structural_content;
    return aux;
}

[[nodiscard]] constexpr Material part_material(const HivePart part) noexcept {
    switch (part) {
    case HivePart::shell: return Material::beehive;
    case HivePart::honey: return Material::honey;
    case HivePart::pollen: return Material::pollen;
    case HivePart::queen: return Material::queen_bee;
    case HivePart::chamber:
    case HivePart::exit: return Material::empty;
    case HivePart::empty: return Material::count;
    }
    return Material::count;
}

[[nodiscard]] constexpr HivePart body_part(
    const std::int32_t dx, const std::int32_t dy,
    const std::int32_t local_queen_y) noexcept {
    return classify_pre_pr19_hive_cell(dx, dy,
        fix29_hive_entropy(512, local_queen_y, dx, dy), 512, local_queen_y);
}

[[nodiscard]] bool valid_span(const std::span<SceneCell> cells,
    const std::uint32_t width, const std::uint32_t height) noexcept {
    return width != 0u && height != 0u &&
        static_cast<std::uint64_t>(width) * height == cells.size() &&
        cells.size() <= (std::numeric_limits<std::uint32_t>::max)();
}

[[nodiscard]] constexpr std::uint32_t index_at(
    const std::uint32_t width, const std::uint32_t queen_x,
    const std::uint32_t queen_y, const std::int32_t dx,
    const std::int32_t dy) noexcept {
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(queen_y) + dy) * width +
           static_cast<std::uint32_t>(static_cast<std::int32_t>(queen_x) + dx);
}
} // namespace

PhantomHiveRecoveryResult recover_v2527_phantom_hive(
    const std::span<SceneCell> cells, const std::uint32_t width,
    const std::uint32_t height, const Scene scene,
    const std::uint32_t format_version) noexcept {
    PhantomHiveRecoveryResult result{};
    if (format_version != 2u || scene != world_scene || height != 1440u ||
        (width != 5120u && width != 7680u && width != 10240u) ||
        !valid_span(cells, width, height)) return result;
    result.queen_x = authored_scene_origin_x(width) + 512u;
    result.queen_y = authored_scene_origin_y(height) + 234u;
    if (cells[index_at(width, result.queen_x, result.queen_y, 0, 0)].material !=
        static_cast<std::uint32_t>(Material::queen_bee)) {
        result.status = PhantomHiveRecoveryStatus::no_candidate;
        return result;
    }
    // A third body is ambiguous unless both legitimate authored Queens remain.
    for (std::uint32_t district = 0u; district < 2u; ++district) {
        const auto x = persistent_world_district_origin_x(width, district) + 512u;
        const auto y = persistent_world_district_origin_y(height, district) +
            (district == 0u ? 234u : 232u);
        if (cells[index_at(width, x, y, 0, 0)].material !=
            static_cast<std::uint32_t>(Material::queen_bee)) ++result.mismatches;
    }
    // Validate every body/chamber/exit coordinate before writes. Age and heat
    // may evolve, but all 242 non-hole aux words must be the old constructor.
    // Empty openings may legitimately relax into Atmosphere after loading;
    // neither kind of opening has unambiguous injected provenance.
    for (std::int32_t dy = -11; dy <= 11; ++dy) {
        for (std::int32_t dx = -11; dx <= 12; ++dx) {
            const auto material = part_material(body_part(dx, dy, 234));
            if (material == Material::count) continue;
            const auto index = index_at(width, result.queen_x, result.queen_y, dx, dy);
            const auto& cell = cells[index];
            if (material == Material::empty) {
                if (cell.material != static_cast<std::uint32_t>(Material::empty) &&
                    cell.material != static_cast<std::uint32_t>(Material::atmosphere))
                    ++result.mismatches;
            } else if (cell.material != static_cast<std::uint32_t>(material) ||
                       cell.aux != old_constructor_aux(index, material)) {
                ++result.mismatches;
            }
        }
    }
    if (result.mismatches != 0u) {
        result.status = PhantomHiveRecoveryStatus::signature_mismatch;
        return result;
    }
    for (std::int32_t dy = -11; dy <= 11; ++dy) {
        for (std::int32_t dx = -11; dx <= 12; ++dx) {
            const auto material = part_material(body_part(dx, dy, 234));
            // Preserve every opening byte, including exact old Empty payloads.
            // Only the non-hole signature proves an injected material owner.
            if (material == Material::count || material == Material::empty) continue;
            const auto index = index_at(width, result.queen_x, result.queen_y, dx, dy);
            auto& cell = cells[index];
            ++result.body_cells;
            cell = clean_atmosphere;
        }
    }
    for (std::size_t slot = 0u; slot < fix29_bee_formation_count; ++slot) {
        const auto offset = fix29_bee_formation_offset(slot);
        const auto index = index_at(width, result.queen_x, result.queen_y,
            offset.x, offset.y);
        auto& cell = cells[index];
        if (cell.material == static_cast<std::uint32_t>(Material::bee) &&
            (cell.aux & swarm_fed) == 0u &&
            cell.aux == old_constructor_aux(index, Material::bee)) {
            cell = clean_atmosphere;
            ++result.bee_cells;
        } else {
            ++result.preserved_formation_cells;
        }
    }
    result.status = PhantomHiveRecoveryStatus::repaired;
    return result;
}

LegacyHiveOwnerResult initialize_schema1_hive_owners(
    const std::span<SceneCell> cells, const std::uint32_t width,
    const std::uint32_t height, const std::uint32_t scene_origin_x,
    const std::uint32_t scene_origin_y, const Scene scene) noexcept {
    if ((scene != Scene::sandbox && scene != Scene::ecosystem) ||
        !valid_span(cells, width, height)) return {};
    const auto local_y = scene == Scene::sandbox ? 234u : 232u;
    if (width < 533u || height < local_y + 15u ||
        scene_origin_x > width - 533u || scene_origin_y > height - local_y - 15u)
        return {};
    const auto queen_x = scene_origin_x + 512u;
    const auto queen_y = scene_origin_y + local_y;
    std::uint32_t home_metadata{};
    const bool persistent = width >= persistent_world_width && height >= persistent_world_height;
    if (persistent) {
        const auto district = persistent_world_district_index(scene);
        if (scene_origin_x != persistent_world_district_origin_x(width, district) ||
            scene_origin_y != persistent_world_district_origin_y(height, district)) return {};
        home_metadata = 64u | ((local_y / 8u) << 7u) | (district << 20u);
    } else {
        if (queen_x / 4u > 255u || queen_y / 4u > 127u) return {};
        home_metadata = (queen_x / 4u) | ((queen_y / 4u) << 8u) | 0x00400000u;
    }
    for (std::int32_t dy = -11; dy <= 11; ++dy) {
        for (std::int32_t dx = -11; dx <= 12; ++dx) {
            const auto material = part_material(body_part(dx, dy,
                static_cast<std::int32_t>(local_y)));
            if (material != Material::count &&
                cells[index_at(width, queen_x, queen_y, dx, dy)].material !=
                    static_cast<std::uint32_t>(material)) return {};
        }
    }
    for (std::size_t slot = 0u; slot < fix29_bee_formation_count; ++slot) {
        const auto offset = fix29_bee_formation_offset(slot);
        if (cells[index_at(width, queen_x, queen_y, offset.x, offset.y)].material !=
            static_cast<std::uint32_t>(Material::bee)) return {};
    }
    LegacyHiveOwnerResult result{true};
    for (std::int32_t dy = -11; dy <= 11; ++dy) {
        for (std::int32_t dx = -11; dx <= 12; ++dx) {
            const auto part = body_part(dx, dy, static_cast<std::int32_t>(local_y));
            if (part == HivePart::shell || part == HivePart::honey || part == HivePart::pollen) {
                cells[index_at(width, queen_x, queen_y, dx, dy)].aux |= structural_content;
                ++result.structural_cells;
            }
        }
    }
    for (std::size_t slot = 0u; slot < fix29_bee_formation_count; ++slot) {
        const auto offset = fix29_bee_formation_offset(slot);
        auto& bee = cells[index_at(width, queen_x, queen_y, offset.x, offset.y)];
        const auto slot_shift = persistent ? 13u : 15u;
        // Normalization reconstructs a fed formation owner, even when the
        // source already happened to be Bee. Retired Queen/migration/pollen
        // and movement flags must not survive and hijack its new lifecycle.
        bee.aux = swarm_fed | home_metadata |
                  (static_cast<std::uint32_t>(slot) << slot_shift);
        bee.age = fix29_bee_pack_age(fix29_bee_initial_timer(slot), fix29_bee_target_none);
        ++result.bee_cells;
    }
    return result;
}
} // namespace sandhybrid
