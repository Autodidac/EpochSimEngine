#pragma once

#include "sandhybrid/material.hpp"
#include "sandhybrid/scene_image.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

namespace sandhybrid {

inline constexpr std::uint32_t blueprint_slot_count = 4u;
inline constexpr std::uint32_t blueprint_max_columns = 64u;
inline constexpr std::uint32_t blueprint_max_rows = 64u;
inline constexpr std::uint32_t blueprint_max_cell_count =
    blueprint_max_columns * blueprint_max_rows;
inline constexpr std::size_t blueprint_name_capacity = 32u;

struct SelectionBounds final {
    std::uint32_t left{};
    std::uint32_t top{};
    std::uint32_t right{};
    std::uint32_t bottom{};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return left <= right && top <= bottom;
    }

    [[nodiscard]] constexpr std::uint32_t width() const noexcept {
        return valid() ? right - left + 1u : 0u;
    }

    [[nodiscard]] constexpr std::uint32_t height() const noexcept {
        return valid() ? bottom - top + 1u : 0u;
    }
};

[[nodiscard]] constexpr SelectionBounds selection_from_points(
    const std::uint32_t first_x,
    const std::uint32_t first_y,
    const std::uint32_t second_x,
    const std::uint32_t second_y) noexcept {
    return {
        (std::min)(first_x, second_x),
        (std::min)(first_y, second_y),
        (std::max)(first_x, second_x),
        (std::max)(first_y, second_y),
    };
}

[[nodiscard]] constexpr SelectionBounds align_selection_to_tiles(
    const SelectionBounds bounds,
    const std::uint32_t world_width,
    const std::uint32_t world_height) noexcept {
    if (!bounds.valid() || world_width == 0u || world_height == 0u ||
        bounds.left >= world_width || bounds.top >= world_height)
        return {1u, 1u, 0u, 0u};
    constexpr std::uint32_t tile_size = 8u;
    const auto left = bounds.left / tile_size * tile_size;
    const auto top = bounds.top / tile_size * tile_size;
    const auto clamped_right = (std::min)(bounds.right, world_width - 1u);
    const auto clamped_bottom = (std::min)(bounds.bottom, world_height - 1u);
    const auto right = (std::min)(
        world_width - 1u,
        clamped_right / tile_size * tile_size + tile_size - 1u);
    const auto bottom = (std::min)(
        world_height - 1u,
        clamped_bottom / tile_size * tile_size + tile_size - 1u);
    return {left, top, right, bottom};
}

enum class BlueprintRotation : std::uint32_t {
    degrees_0 = 0u,
    degrees_90,
    degrees_180,
    degrees_270,
};

enum class BlueprintKind : std::uint32_t {
    static_model = 0u,
    map_chunk,
};

struct BlueprintTransform final {
    BlueprintRotation rotation{BlueprintRotation::degrees_0};
    bool mirror_x{};
    bool mirror_y{};
};

struct Blueprint final {
    std::array<char, blueprint_name_capacity> name{};
    std::uint32_t width{};
    std::uint32_t height{};
    std::array<SceneCell, blueprint_max_cell_count> cells{};
    BlueprintKind kind{BlueprintKind::map_chunk};
    bool occupied{};

    [[nodiscard]] constexpr std::uint32_t cell_count() const noexcept {
        return occupied ? width * height : 0u;
    }

    [[nodiscard]] constexpr std::string_view display_name() const noexcept {
        std::size_t length = 0u;
        while (length < name.size() && name[length] != '\0') ++length;
        return {name.data(), length};
    }

    [[nodiscard]] constexpr const SceneCell& at(
        const std::uint32_t x, const std::uint32_t y) const noexcept {
        return cells[static_cast<std::size_t>(y) * width + x];
    }
};

constexpr void set_blueprint_name(
    Blueprint& blueprint, const std::string_view name) noexcept {
    blueprint.name.fill('\0');
    const auto length = (std::min)(name.size(), blueprint.name.size() - 1u);
    for (std::size_t index = 0u; index < length; ++index)
        blueprint.name[index] = name[index];
}

[[nodiscard]] constexpr bool blueprint_bounds_fit(
    const SelectionBounds bounds,
    const std::uint32_t world_width,
    const std::uint32_t world_height) noexcept {
    return bounds.valid() && bounds.right < world_width && bounds.bottom < world_height &&
           bounds.width() <= blueprint_max_columns &&
           bounds.height() <= blueprint_max_rows;
}

[[nodiscard]] constexpr std::optional<Blueprint> capture_blueprint(
    const std::span<const SceneCell> world,
    const std::uint32_t world_width,
    const std::uint32_t world_height,
    const SelectionBounds bounds,
    const std::string_view name) noexcept {
    const auto expected = static_cast<std::size_t>(world_width) * world_height;
    if (world.size() != expected ||
        !blueprint_bounds_fit(bounds, world_width, world_height))
        return std::nullopt;

    Blueprint result{};
    result.width = bounds.width();
    result.height = bounds.height();
    result.occupied = true;
    set_blueprint_name(result, name);
    for (std::uint32_t y = 0u; y < result.height; ++y) {
        for (std::uint32_t x = 0u; x < result.width; ++x) {
            result.cells[static_cast<std::size_t>(y) * result.width + x] =
                world[static_cast<std::size_t>(bounds.top + y) * world_width +
                      bounds.left + x];
        }
    }
    return result;
}

[[nodiscard]] constexpr std::optional<SelectionBounds> nonempty_blueprint_bounds(
    const std::span<const SceneCell> cells,
    const std::uint32_t width,
    const std::uint32_t height) noexcept {
    if (cells.size() != static_cast<std::size_t>(width) * height ||
        width == 0u || height == 0u)
        return std::nullopt;

    std::uint32_t left = width;
    std::uint32_t top = height;
    std::uint32_t right = 0u;
    std::uint32_t bottom = 0u;
    bool found = false;
    const auto empty = static_cast<std::uint32_t>(Material::empty);
    for (std::uint32_t y = 0u; y < height; ++y) {
        for (std::uint32_t x = 0u; x < width; ++x) {
            if (cells[static_cast<std::size_t>(y) * width + x].material == empty)
                continue;
            found = true;
            left = (std::min)(left, x);
            top = (std::min)(top, y);
            right = (std::max)(right, x);
            bottom = (std::max)(bottom, y);
        }
    }
    if (!found) return std::nullopt;
    return SelectionBounds{left, top, right, bottom};
}

[[nodiscard]] constexpr std::optional<Blueprint> capture_trimmed_blueprint(
    const std::span<const SceneCell> cells,
    const std::uint32_t width,
    const std::uint32_t height,
    const std::string_view name) noexcept {
    const auto bounds = nonempty_blueprint_bounds(cells, width, height);
    if (!bounds.has_value()) return std::nullopt;
    return capture_blueprint(cells, width, height, *bounds, name);
}

[[nodiscard]] constexpr std::pair<std::uint32_t, std::uint32_t>
blueprint_transformed_extent(
    const Blueprint& blueprint,
    const BlueprintTransform transform) noexcept {
    const bool swap_axes = transform.rotation == BlueprintRotation::degrees_90 ||
                           transform.rotation == BlueprintRotation::degrees_270;
    return swap_axes
        ? std::pair{blueprint.height, blueprint.width}
        : std::pair{blueprint.width, blueprint.height};
}

[[nodiscard]] constexpr std::pair<std::uint32_t, std::uint32_t>
blueprint_destination_coordinate(
    const Blueprint& blueprint,
    const BlueprintTransform transform,
    const std::uint32_t source_x,
    const std::uint32_t source_y) noexcept {
    const auto x = transform.mirror_x ? blueprint.width - 1u - source_x : source_x;
    const auto y = transform.mirror_y ? blueprint.height - 1u - source_y : source_y;
    switch (transform.rotation) {
    case BlueprintRotation::degrees_90:
        return {blueprint.height - 1u - y, x};
    case BlueprintRotation::degrees_180:
        return {blueprint.width - 1u - x, blueprint.height - 1u - y};
    case BlueprintRotation::degrees_270:
        return {y, blueprint.width - 1u - x};
    case BlueprintRotation::degrees_0:
    default:
        return {x, y};
    }
}

[[nodiscard]] constexpr bool blueprint_can_place(
    const Blueprint& blueprint,
    const std::uint32_t world_width,
    const std::uint32_t world_height,
    const std::uint32_t origin_x,
    const std::uint32_t origin_y,
    const BlueprintTransform transform = {}) noexcept {
    if (!blueprint.occupied || blueprint.width == 0u || blueprint.height == 0u ||
        blueprint.width > blueprint_max_columns || blueprint.height > blueprint_max_rows ||
        blueprint.cell_count() > blueprint.cells.size())
        return false;
    const auto [width, height] = blueprint_transformed_extent(blueprint, transform);
    return origin_x <= world_width && origin_y <= world_height &&
           width <= world_width - origin_x && height <= world_height - origin_y;
}

[[nodiscard]] constexpr bool blueprint_payload_valid(
    const Blueprint& blueprint) noexcept {
    if (!blueprint.occupied || blueprint.width == 0u || blueprint.height == 0u ||
        blueprint.width > blueprint_max_columns || blueprint.height > blueprint_max_rows ||
        blueprint.cell_count() > blueprint.cells.size())
        return false;
    for (std::uint32_t y = 0u; y < blueprint.height; ++y) {
        for (std::uint32_t x = 0u; x < blueprint.width; ++x) {
            if (blueprint.at(x, y).material >=
                static_cast<std::uint32_t>(Material::count))
                return false;
        }
    }
    return true;
}

[[nodiscard]] constexpr std::optional<std::pair<std::uint32_t, std::uint32_t>>
blueprint_centered_origin(
    const Blueprint& blueprint,
    const std::uint32_t world_width,
    const std::uint32_t world_height,
    const std::uint32_t cursor_x,
    const std::uint32_t cursor_y,
    const BlueprintTransform transform = {}) noexcept {
    const auto [width, height] = blueprint_transformed_extent(blueprint, transform);
    const auto half_width = width / 2u;
    const auto half_height = height / 2u;
    if (cursor_x < half_width || cursor_y < half_height) return std::nullopt;
    const auto origin_x = cursor_x - half_width;
    const auto origin_y = cursor_y - half_height;
    if (!blueprint_can_place(
            blueprint, world_width, world_height, origin_x, origin_y, transform))
        return std::nullopt;
    return std::pair{origin_x, origin_y};
}
[[nodiscard]] constexpr bool place_blueprint_transactional(
    const Blueprint& blueprint,
    const std::span<SceneCell> world,
    const std::uint32_t world_width,
    const std::uint32_t world_height,
    const std::uint32_t origin_x,
    const std::uint32_t origin_y,
    const BlueprintTransform transform = {},
    const bool include_empty = false) noexcept {
    if (world.size() != static_cast<std::size_t>(world_width) * world_height ||
        !blueprint_can_place(
            blueprint, world_width, world_height, origin_x, origin_y, transform))
        return false;

    if (!blueprint_payload_valid(blueprint)) return false;

    const auto empty = static_cast<std::uint32_t>(Material::empty);
    for (std::uint32_t y = 0u; y < blueprint.height; ++y) {
        for (std::uint32_t x = 0u; x < blueprint.width; ++x) {
            const auto& cell = blueprint.at(x, y);
            if (!include_empty && cell.material == empty) continue;
            const auto [destination_x, destination_y] =
                blueprint_destination_coordinate(blueprint, transform, x, y);
            world[static_cast<std::size_t>(origin_y + destination_y) * world_width +
                  origin_x + destination_x] = cell;
        }
    }
    return true;
}

} // namespace sandhybrid
