#include "sandhybrid/section_grid.hpp"

#include <array>
#include <cstdint>

using namespace sandhybrid;

static_assert(section_side_cells == 64);
static_assert(stream_page_side_cells == 512);
static_assert(section_of({0, 0}) == SectionCoordinate{0, 0});
static_assert(section_of({63, 63}) == SectionCoordinate{0, 0});
static_assert(section_of({64, 64}) == SectionCoordinate{1, 1});
static_assert(section_of({-1, -1}) == SectionCoordinate{-1, -1});
static_assert(section_of({-64, -64}) == SectionCoordinate{-1, -1});
static_assert(section_of({-65, -65}) == SectionCoordinate{-2, -2});
static_assert(local_cell({-1, -1}) == CellCoordinate{63, 63});
static_assert(section_phase({0, 0}) != section_phase({1, 0}));
static_assert(section_phase({0, 0}) != section_phase({0, 1}));
static_assert(section_phase({0, 0}) != section_phase({1, 1}));

namespace {

[[nodiscard]] int require(const bool condition, const int code) noexcept {
    return condition ? 0 : code;
}

} // namespace

int main() {
    SparseSectionGrid grid;
    grid.mark_dirty({63, 63, 66, 66});
    if (const auto failure = require(grid.resident_section_count() == 4u, 1); failure != 0) return failure;

    grid.begin_tick(1u);
    if (const auto failure = require(grid.active_section_count() == 4u, 2); failure != 0) return failure;
    for (std::uint8_t phase = 0u; phase < section_phase_count; ++phase) {
        std::array<SectionWorkItem, 1> work{};
        const auto batch = grid.collect_phase(phase, work);
        if (const auto failure = require(batch.required == 1u, 3); failure != 0) return failure;
        if (const auto failure = require(batch.complete(), 4); failure != 0) return failure;
        if (const auto failure = require(section_phase(work[0].coordinate) == phase, 5); failure != 0) return failure;
    }

    const auto origin_dirty = grid.current_dirty({0, 0});
    if (const auto failure = require(
            origin_dirty.min_x == 63u && origin_dirty.min_y == 63u,
            6);
        failure != 0) {
        return failure;
    }
    if (const auto failure = require(
            origin_dirty.max_x == 64u && origin_dirty.max_y == 64u,
            7);
        failure != 0) {
        return failure;
    }

    grid.begin_tick(2u);
    if (const auto failure = require(grid.active_section_count() == 0u, 8); failure != 0) return failure;

    grid.mark_dirty_cell({63, 20});
    grid.begin_tick(3u);
    if (const auto failure = require(grid.active_section_count() == 1u, 9); failure != 0) return failure;
    grid.complete_section(
        {0, 0}, {.min_x = 63u, .min_y = 20u, .max_x = 64u, .max_y = 21u});
    grid.begin_tick(4u);
    if (const auto failure = require(grid.is_active({0, 0}), 10); failure != 0) return failure;
    if (const auto failure = require(grid.is_active({1, 0}), 11); failure != 0) return failure;

    std::array<SectionWorkItem, 0> none{};
    const auto truncated = grid.collect_phase(section_phase({0, 0}), none);
    if (const auto failure = require(truncated.written == 0u, 12); failure != 0) return failure;
    if (const auto failure = require(truncated.required >= 1u, 13); failure != 0) return failure;
    if (const auto failure = require(!truncated.complete(), 14); failure != 0) return failure;

    grid.begin_tick(5u);
    const auto resident_before = grid.resident_section_count();
    if (const auto failure = require(
            grid.retire_clean_before(5u) == resident_before,
            15);
        failure != 0) {
        return failure;
    }
    return require(grid.resident_section_count() == 0u, 16);
}
