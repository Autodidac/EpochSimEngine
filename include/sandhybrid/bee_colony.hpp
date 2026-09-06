#pragma once

#include "sandhybrid/actor_medium.hpp"
#include "sandhybrid/world_layout.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <numeric>

namespace sandhybrid {

inline constexpr std::uint32_t bee_home_metadata_mask = 0x00ffffffu;
// Bit 23 was zero in every persistent district-home constructor. This is a
// Bee-only discriminator, not the unrelated Water Half flag with the same bit.
inline constexpr std::uint32_t bee_global_home_bit = 0x00800000u;
inline constexpr std::uint32_t bee_global_home_max_width = 16'384u;
inline constexpr std::uint32_t bee_global_home_max_height = 2'048u;

struct BeeHome final {
    std::uint32_t x{};
    std::uint32_t y{};
    friend constexpr bool operator==(const BeeHome&, const BeeHome&) = default;
};

struct BeeDistrictAddress final {
    bool valid{};
    std::uint32_t district{};
    BeeHome local{};
};

[[nodiscard]] constexpr bool bee_uses_persistent_home(
    const std::uint32_t width, const std::uint32_t height) noexcept {
    return width >= persistent_world_width && height >= persistent_world_height;
}

// Derived population keys identify decoded home coordinates, not metadata
// bytes: district/global aliases of the same home must share one exact count.
// Legacy authored offsets need not be multiples of four on odd-sized canvases.
[[nodiscard]] constexpr std::uint32_t bee_population_stride(
    const std::uint32_t width, const std::uint32_t height) noexcept {
    if (bee_uses_persistent_home(width, height)) return 8u;
    return std::gcd(4u, std::gcd(authored_scene_origin_x(width),
                                authored_scene_origin_y(height)));
}

// Allocation callers validate this wide count against shader indexing and
// device byte limits before narrowing it; neither ceiling addition may wrap.
[[nodiscard]] constexpr std::uint64_t bee_population_slot_count(
    const std::uint32_t width, const std::uint32_t height) noexcept {
    const auto stride = bee_population_stride(width, height);
    const auto columns = (static_cast<std::uint64_t>(width) + stride - 1u) / stride;
    const auto rows = (static_cast<std::uint64_t>(height) + stride - 1u) / stride;
    return columns * rows;
}

inline constexpr auto bee_population_invalid_key =
    (std::numeric_limits<std::uint32_t>::max)();

// The argument is already decoded. Reject exterior/non-lattice coordinates
// instead of flooring them onto a different valid colony. Negative GLSL homes
// correspond to exterior unsigned coordinates here and are rejected as well.
[[nodiscard]] constexpr std::uint32_t bee_population_key(
    const BeeHome decoded_home, const std::uint32_t width,
    const std::uint32_t height) noexcept {
    const auto stride = bee_population_stride(width, height);
    if (decoded_home.x >= width || decoded_home.y >= height ||
        decoded_home.x % stride != 0u || decoded_home.y % stride != 0u)
        return bee_population_invalid_key;
    const auto columns = (static_cast<std::uint64_t>(width) + stride - 1u) / stride;
    const auto key = (decoded_home.y / stride) * columns + decoded_home.x / stride;
    return key < bee_population_invalid_key
        ? static_cast<std::uint32_t>(key) : bee_population_invalid_key;
}

[[nodiscard]] constexpr BeeDistrictAddress bee_district_address(
    const BeeHome home, const std::uint32_t width, const std::uint32_t height) noexcept {
    for (std::uint32_t district = 0u; district < persistent_world_district_count; ++district) {
        const auto ox = persistent_world_district_origin_x(width, district);
        const auto oy = persistent_world_district_origin_y(height, district);
        if (home.x >= ox && home.y >= oy &&
            home.x - ox < pre_expansion_world_width &&
            home.y - oy < pre_expansion_world_height)
            return {true, district, {home.x - ox, home.y - oy}};
    }
    return {};
}

[[nodiscard]] constexpr bool bee_home_encodable(
    const BeeHome home, const std::uint32_t width, const std::uint32_t height) noexcept {
    if (home.x >= width || home.y >= height) return false;
    if (bee_uses_persistent_home(width, height))
        return bee_district_address(home, width, height).valid ||
            (width <= bee_global_home_max_width && height <= bee_global_home_max_height);
    // The historical CPU PPM constructor stores absolute coordinates but sets
    // the authored-origin flag. Preserve those import bytes below; do not admit
    // a new home that the decoder would shift a second time on an offset canvas.
    if (authored_scene_origin_x(width) != 0u || authored_scene_origin_y(height) != 0u)
        return false;
    return home.x / 4u <= 255u && home.y / 4u <= 127u;
}

[[nodiscard]] constexpr bool bee_uses_global_home(
    const std::uint32_t aux, const std::uint32_t width, const std::uint32_t height) noexcept {
    return bee_uses_persistent_home(width, height) && (aux & bee_global_home_bit) != 0u;
}

// New placements must satisfy bee_home_encodable first. Retain the original
// district bytes exactly; only out-of-district persistent homes use global16.
// The nonpersistent branch preserves the legacy CPU PPM constructor bytes.
[[nodiscard]] constexpr std::uint32_t pack_bee_home_metadata(
    const std::uint32_t aux, const BeeHome home, const std::uint32_t slot,
    const std::uint32_t width, const std::uint32_t height) noexcept {
    std::uint32_t metadata{};
    if (bee_uses_persistent_home(width, height)) {
        const auto address = bee_district_address(home, width, height);
        if (address.valid) {
            metadata = (address.local.x / 8u) |
                ((address.local.y / 8u) << 7u) | ((slot & 127u) << 13u) |
                (address.district << 20u);
        } else {
            metadata = bee_global_home_bit |
                ((std::min)(home.x / 16u, 1023u)) |
                (((std::min)(home.y / 16u, 127u)) << 10u) |
                ((slot & 63u) << 17u);
        }
    } else {
        metadata = (std::min)(home.x / 4u, 255u) |
            ((std::min)(home.y / 4u, 127u) << 8u) |
            (((slot & 127u) | 128u) << 15u);
    }
    return (aux & ~bee_home_metadata_mask) | metadata;
}

[[nodiscard]] constexpr std::uint32_t bee_slot_from_metadata(
    const std::uint32_t aux, const std::uint32_t width, const std::uint32_t height) noexcept {
    if (bee_uses_global_home(aux, width, height)) return (aux >> 17u) & 63u;
    return bee_uses_persistent_home(width, height)
        ? (aux >> 13u) & 127u : (aux >> 15u) & 127u;
}

[[nodiscard]] constexpr std::uint32_t bee_home_alignment(
    const std::uint32_t aux, const std::uint32_t width, const std::uint32_t height) noexcept {
    if (bee_uses_global_home(aux, width, height)) return 16u;
    return bee_uses_persistent_home(width, height) ? 8u : 4u;
}

[[nodiscard]] constexpr BeeHome bee_home_from_metadata(
    const std::uint32_t aux, const std::uint32_t width, const std::uint32_t height) noexcept {
    if (bee_uses_global_home(aux, width, height))
        return {(aux & 1023u) * 16u, ((aux >> 10u) & 127u) * 16u};
    if (bee_uses_persistent_home(width, height)) {
        const auto district = (aux >> 20u) & 7u;
        return {persistent_world_district_origin_x(width, district) + (aux & 127u) * 8u,
            persistent_world_district_origin_y(height, district) + ((aux >> 7u) & 63u) * 8u};
    }
    const bool authored = ((aux >> 15u) & 128u) != 0u;
    return {(aux & 255u) * 4u + (authored ? authored_scene_origin_x(width) : 0u),
        ((aux >> 8u) & 127u) * 4u + (authored ? authored_scene_origin_y(height) : 0u)};
}

struct BeehiveFootprint final {
    std::int32_t min_x{};
    std::int32_t min_y{};
    std::int32_t max_x{};
    std::int32_t max_y{};
};

[[nodiscard]] constexpr BeehiveFootprint beehive_placement_footprint() noexcept {
    BeehiveFootprint bounds{};
    const auto include = [&bounds](const std::int32_t x, const std::int32_t y) {
        bounds.min_x = (std::min)(bounds.min_x, x);
        bounds.min_y = (std::min)(bounds.min_y, y);
        bounds.max_x = (std::max)(bounds.max_x, x);
        bounds.max_y = (std::max)(bounds.max_y, y);
    };
    // Include every canonical body cell, including Empty chamber/exit owners.
    // This bounded scan follows the photographed classifier, not the cleanup
    // radius. Formation offsets are the same accepted 60-cell source table.
    for (std::int32_t y = -11; y <= 11; ++y)
        for (std::int32_t x = -11; x <= 12; ++x)
            if (classify_pre_pr19_hive_cell(x, y) != HivePart::empty) include(x, y);
    for (std::size_t slot = 0u; slot < fix29_bee_formation_count; ++slot) {
        const auto offset = fix29_bee_formation_offset(slot);
        include(offset.x, offset.y);
    }
    return bounds;
}

[[nodiscard]] constexpr bool beehive_placement_fits(
    const std::int32_t x, const std::int32_t y,
    const std::uint32_t width, const std::uint32_t height) noexcept {
    constexpr auto bounds = beehive_placement_footprint();
    return static_cast<std::int64_t>(x) + bounds.min_x >= 0 &&
        static_cast<std::int64_t>(y) + bounds.min_y >= 0 &&
        static_cast<std::int64_t>(x) + bounds.max_x < width &&
        static_cast<std::int64_t>(y) + bounds.max_y < height;
}

} // namespace sandhybrid
