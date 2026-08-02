#include "sandhybrid/section_grid.hpp"

#include <algorithm>

namespace sandhybrid {

namespace {

[[nodiscard]] constexpr std::uint8_t clamp_local(const std::int32_t value) noexcept {
    const auto clamped = (std::clamp)(value, 0, section_side_cells);
    return static_cast<std::uint8_t>(clamped);
}

[[nodiscard]] constexpr LocalDirtyRect local_intersection(
    const CellRect world_rect,
    const SectionCoordinate coordinate) noexcept {
    const auto origin = section_origin(coordinate);
    return {
        .min_x = clamp_local(world_rect.min_x - origin.x),
        .min_y = clamp_local(world_rect.min_y - origin.y),
        .max_x = clamp_local(world_rect.max_x - origin.x),
        .max_y = clamp_local(world_rect.max_y - origin.y),
    };
}

} // namespace

std::size_t SectionCoordinateHash::operator()(const SectionCoordinate coordinate) const noexcept {
    const auto x = static_cast<std::uint64_t>(static_cast<std::uint32_t>(coordinate.x));
    const auto y = static_cast<std::uint64_t>(static_cast<std::uint32_t>(coordinate.y));
    auto value = (x << 32u) | y;
    value ^= value >> 30u;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27u;
    value *= 0x94d049bb133111ebULL;
    value ^= value >> 31u;
    return static_cast<std::size_t>(value);
}

SparseSectionGrid::SectionRecord& SparseSectionGrid::ensure(const SectionCoordinate coordinate) {
    return sections_.try_emplace(coordinate).first->second;
}

void SparseSectionGrid::queue_pending(
    const SectionCoordinate coordinate,
    const LocalDirtyRect dirty) {
    if (dirty.empty()) return;
    auto& section = ensure(coordinate);
    section.pending.include(dirty);
    section.last_touched_tick = tick_;
    if (!section.pending_listed) {
        section.pending_listed = true;
        pending_sections_.push_back(coordinate);
    }
}

void SparseSectionGrid::mark_dirty_cell(const CellCoordinate cell) {
    mark_dirty({cell.x, cell.y, cell.x + 1, cell.y + 1});
}

void SparseSectionGrid::mark_dirty(const CellRect world_rect) {
    if (world_rect.empty()) return;

    const auto first = section_of({world_rect.min_x, world_rect.min_y});
    const auto last = section_of({world_rect.max_x - 1, world_rect.max_y - 1});
    for (auto y = first.y; y <= last.y; ++y) {
        for (auto x = first.x; x <= last.x; ++x) {
            const SectionCoordinate coordinate{x, y};
            queue_pending(coordinate, local_intersection(world_rect, coordinate));
        }
    }
}

void SparseSectionGrid::begin_tick(const std::uint64_t tick) {
    tick_ = tick;
    for (const auto coordinate : active_sections_) {
        if (auto found = sections_.find(coordinate); found != sections_.end()) {
            found->second.current.clear();
            found->second.active = false;
        }
    }
    active_sections_.clear();
    active_sections_.swap(pending_sections_);

    for (const auto coordinate : active_sections_) {
        auto& section = sections_.at(coordinate);
        section.current = section.pending;
        section.pending.clear();
        section.pending_listed = false;
        section.active = !section.current.empty();
    }
}

SectionPhaseBatch SparseSectionGrid::collect_phase(
    const std::uint8_t phase,
    const std::span<SectionWorkItem> output) const noexcept {
    SectionPhaseBatch result{};
    if (phase >= section_phase_count) return result;

    for (const auto coordinate : active_sections_) {
        const auto found = sections_.find(coordinate);
        if (found == sections_.end() || !found->second.active ||
            section_phase(coordinate) != phase) {
            continue;
        }
        if (result.written < output.size()) {
            output[result.written++] = {coordinate, found->second.current};
        }
        ++result.required;
    }
    return result;
}

void SparseSectionGrid::complete_section(
    const SectionCoordinate coordinate,
    const LocalDirtyRect changed_local,
    const std::uint8_t halo_cells) {
    if (changed_local.empty()) return;
    const auto origin = section_origin(coordinate);
    const auto halo = static_cast<std::int32_t>(halo_cells);
    mark_dirty({
        origin.x + static_cast<std::int32_t>(changed_local.min_x) - halo,
        origin.y + static_cast<std::int32_t>(changed_local.min_y) - halo,
        origin.x + static_cast<std::int32_t>(changed_local.max_x) + halo,
        origin.y + static_cast<std::int32_t>(changed_local.max_y) + halo,
    });
}

std::size_t SparseSectionGrid::resident_section_count() const noexcept {
    return sections_.size();
}

std::size_t SparseSectionGrid::active_section_count() const noexcept {
    return active_sections_.size();
}

bool SparseSectionGrid::is_resident(const SectionCoordinate coordinate) const noexcept {
    return sections_.contains(coordinate);
}

bool SparseSectionGrid::is_active(const SectionCoordinate coordinate) const noexcept {
    const auto found = sections_.find(coordinate);
    return found != sections_.end() && found->second.active;
}

LocalDirtyRect SparseSectionGrid::current_dirty(const SectionCoordinate coordinate) const noexcept {
    const auto found = sections_.find(coordinate);
    return found == sections_.end() ? LocalDirtyRect{} : found->second.current;
}

std::size_t SparseSectionGrid::retire_clean_before(const std::uint64_t oldest_tick_to_keep) {
    std::size_t removed{};
    for (auto iterator = sections_.begin(); iterator != sections_.end();) {
        const auto& section = iterator->second;
        if (!section.active && !section.pending_listed && section.pending.empty() &&
            section.last_touched_tick < oldest_tick_to_keep) {
            iterator = sections_.erase(iterator);
            ++removed;
        } else {
            ++iterator;
        }
    }
    return removed;
}

} // namespace sandhybrid
