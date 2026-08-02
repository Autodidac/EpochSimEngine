#include "sandhybrid/section_grid.hpp"

#include <array>
#include <cassert>

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

int main() {
    SparseSectionGrid grid;
    grid.mark_dirty({63, 63, 66, 66});
    assert(grid.resident_section_count() == 4u);

    grid.begin_tick(1u);
    assert(grid.active_section_count() == 4u);
    for (std::uint8_t phase = 0u; phase < section_phase_count; ++phase) {
        std::array<SectionWorkItem, 1> work{};
        const auto batch = grid.collect_phase(phase, work);
        assert(batch.required == 1u);
        assert(batch.complete());
        assert(section_phase(work[0].coordinate) == phase);
    }

    const auto origin_dirty = grid.current_dirty({0, 0});
    assert(origin_dirty.min_x == 63u && origin_dirty.min_y == 63u);
    assert(origin_dirty.max_x == 64u && origin_dirty.max_y == 64u);

    grid.begin_tick(2u);
    assert(grid.active_section_count() == 0u);

    grid.mark_dirty_cell({63, 20});
    grid.begin_tick(3u);
    assert(grid.active_section_count() == 1u);
    grid.complete_section(
        {0, 0}, {.min_x = 63u, .min_y = 20u, .max_x = 64u, .max_y = 21u});
    grid.begin_tick(4u);
    assert(grid.is_active({0, 0}));
    assert(grid.is_active({1, 0}));

    std::array<SectionWorkItem, 0> none{};
    const auto truncated = grid.collect_phase(section_phase({0, 0}), none);
    assert(truncated.written == 0u);
    assert(truncated.required >= 1u);
    assert(!truncated.complete());

    grid.begin_tick(5u);
    const auto resident_before = grid.resident_section_count();
    assert(grid.retire_clean_before(5u) == resident_before);
    assert(grid.resident_section_count() == 0u);
}
