#pragma once

#include "sandhybrid/section_scheduler.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace sandhybrid {

inline constexpr std::int32_t section_side_cells = 64;
inline constexpr std::int32_t stream_page_side_sections = 8;
inline constexpr std::int32_t stream_page_side_cells =
    section_side_cells * stream_page_side_sections;
inline constexpr std::uint8_t section_phase_count = 4u;

struct CellCoordinate final {
    std::int32_t x{};
    std::int32_t y{};

    friend constexpr bool operator==(CellCoordinate, CellCoordinate) noexcept = default;
};

struct CellRect final {
    std::int32_t min_x{};
    std::int32_t min_y{};
    std::int32_t max_x{};
    std::int32_t max_y{};

    [[nodiscard]] constexpr bool empty() const noexcept {
        return min_x >= max_x || min_y >= max_y;
    }
};

struct LocalDirtyRect final {
    std::uint8_t min_x{static_cast<std::uint8_t>(section_side_cells)};
    std::uint8_t min_y{static_cast<std::uint8_t>(section_side_cells)};
    std::uint8_t max_x{};
    std::uint8_t max_y{};

    [[nodiscard]] constexpr bool empty() const noexcept {
        return min_x >= max_x || min_y >= max_y;
    }

    constexpr void clear() noexcept {
        min_x = static_cast<std::uint8_t>(section_side_cells);
        min_y = static_cast<std::uint8_t>(section_side_cells);
        max_x = 0u;
        max_y = 0u;
    }

    constexpr void include(const LocalDirtyRect other) noexcept {
        if (other.empty()) return;
        if (empty()) {
            *this = other;
            return;
        }
        if (other.min_x < min_x) min_x = other.min_x;
        if (other.min_y < min_y) min_y = other.min_y;
        if (other.max_x > max_x) max_x = other.max_x;
        if (other.max_y > max_y) max_y = other.max_y;
    }
};

struct SectionWorkItem final {
    SectionCoordinate coordinate{};
    LocalDirtyRect dirty{};
};

struct SectionPhaseBatch final {
    std::size_t written{};
    std::size_t required{};

    [[nodiscard]] constexpr bool complete() const noexcept {
        return written == required;
    }
};

struct SectionCoordinateHash final {
    [[nodiscard]] std::size_t operator()(SectionCoordinate coordinate) const noexcept;
};

[[nodiscard]] constexpr std::int32_t floor_divide_section(const std::int32_t value) noexcept {
    const auto quotient = value / section_side_cells;
    const auto remainder = value % section_side_cells;
    return remainder < 0 ? quotient - 1 : quotient;
}

[[nodiscard]] constexpr SectionCoordinate section_of(const CellCoordinate cell) noexcept {
    return {floor_divide_section(cell.x), floor_divide_section(cell.y)};
}

[[nodiscard]] constexpr CellCoordinate section_origin(const SectionCoordinate section) noexcept {
    return {section.x * section_side_cells, section.y * section_side_cells};
}

[[nodiscard]] constexpr CellCoordinate local_cell(const CellCoordinate cell) noexcept {
    const auto section = section_of(cell);
    const auto origin = section_origin(section);
    return {cell.x - origin.x, cell.y - origin.y};
}

[[nodiscard]] constexpr std::uint8_t section_phase(const SectionCoordinate coordinate) noexcept {
    const auto x = static_cast<std::uint32_t>(coordinate.x) & 1u;
    const auto y = static_cast<std::uint32_t>(coordinate.y) & 1u;
    return static_cast<std::uint8_t>(x | (y << 1u));
}

class SparseSectionGrid final {
public:
    SparseSectionGrid() = default;

    void mark_dirty_cell(CellCoordinate cell);
    void mark_dirty(CellRect world_rect);

    // Tick ownership is single-threaded. Workers consume immutable batches and
    // return changed rectangles; they never mutate the sparse table directly.
    void begin_tick(std::uint64_t tick);
    [[nodiscard]] SectionPhaseBatch collect_phase(
        std::uint8_t phase, std::span<SectionWorkItem> output) const noexcept;

    void complete_section(
        SectionCoordinate coordinate,
        LocalDirtyRect changed_local,
        std::uint8_t halo_cells = 1u);

    [[nodiscard]] std::size_t resident_section_count() const noexcept;
    [[nodiscard]] std::size_t active_section_count() const noexcept;
    [[nodiscard]] bool is_resident(SectionCoordinate coordinate) const noexcept;
    [[nodiscard]] bool is_active(SectionCoordinate coordinate) const noexcept;
    [[nodiscard]] LocalDirtyRect current_dirty(SectionCoordinate coordinate) const noexcept;

    // Removes long-clean metadata only. Cell/page storage is owned by the future
    // streaming layer and is intentionally not coupled to scheduler metadata.
    std::size_t retire_clean_before(std::uint64_t oldest_tick_to_keep);

private:
    struct SectionRecord final {
        LocalDirtyRect current{};
        LocalDirtyRect pending{};
        std::uint64_t last_touched_tick{};
        bool pending_listed{};
        bool active{};
    };

    using SectionMap = std::unordered_map<SectionCoordinate, SectionRecord, SectionCoordinateHash>;

    [[nodiscard]] SectionRecord& ensure(SectionCoordinate coordinate);
    void queue_pending(SectionCoordinate coordinate, LocalDirtyRect dirty);

    SectionMap sections_{};
    std::vector<SectionCoordinate> active_sections_{};
    std::vector<SectionCoordinate> pending_sections_{};
    std::uint64_t tick_{};
};

} // namespace sandhybrid
