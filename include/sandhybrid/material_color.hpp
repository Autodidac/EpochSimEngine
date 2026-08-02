#pragma once

#include "sandhybrid/material.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace sandhybrid {

struct Rgb8 final {
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};

    friend constexpr bool operator==(Rgb8, Rgb8) noexcept = default;
};

// Stable, paint-editor-friendly representatives of the in-simulation material
// colors. Dynamic texture variation, animation, wetness, charge, transparency,
// and checker patterns are intentionally reduced to one recognizable RGB value.
// Material IDs remain append-only and each editor color is unique.
inline constexpr std::array<Rgb8, material_count> material_editor_colors{{
    {6u, 9u, 14u},       // Vacuum
    {224u, 183u, 86u},   // Sand
    {20u, 87u, 235u},    // Water
    {87u, 48u, 21u},     // Soil
    {81u, 86u, 93u},     // Stone
    {85u, 196u, 242u},   // Crystal
    {71u, 46u, 23u},     // Mud
    {115u, 242u, 20u},   // Acid
    {31u, 148u, 41u},    // Grass
    {48u, 51u, 56u},     // Smoke
    {173u, 205u, 224u},  // Steam
    {255u, 82u, 8u},     // Fire
    {250u, 46u, 4u},     // Lava
    {27u, 19u, 14u},     // Oil
    {103u, 54u, 17u},    // Wood
    {204u, 61u, 107u},   // Plastic
    {56u, 191u, 199u},   // AR plastic
    {240u, 138u, 15u},   // Honey
    {245u, 160u, 20u},   // Bee
    {238u, 240u, 245u},  // Salt
    {133u, 204u, 250u},  // Ice
    {124u, 131u, 143u},  // Aluminum
    {86u, 82u, 77u},     // Ash
    {189u, 41u, 10u},    // Ember
    {70u, 145u, 173u},   // Glass
    {37u, 37u, 39u},     // Gunpowder
    {242u, 248u, 255u},  // Snow
    {120u, 80u, 20u},    // Seed
    {242u, 179u, 41u},   // Beeswax
    {245u, 61u, 122u},   // Flower
    {23u, 108u, 189u},   // Saltwater
    {166u, 107u, 20u},   // Beehive
    {112u, 120u, 122u},  // Dirty steam
    {41u, 77u, 80u},     // Dirty water
    {250u, 202u, 20u},   // Pollen
    {199u, 87u, 9u},     // Queen
    {90u, 76u, 67u},     // Iron
    {199u, 79u, 31u},    // Copper
    {122u, 36u, 112u},   // Magnet
    {209u, 193u, 151u},  // Insulator
    {122u, 190u, 255u},  // Lightning
    {92u, 13u, 5u},      // Magma vent
    {69u, 171u, 46u},    // Uranium
    {128u, 230u, 36u},   // Radiation
    {184u, 147u, 37u},   // Aluminum shavings
    {250u, 202u, 26u},   // Gold
    {77u, 194u, 255u},   // Oxygen
    {8u, 10u, 15u},      // Carbon dioxide
    {113u, 67u, 49u},    // Iron ore / legacy slot 48
    {149u, 161u, 176u},  // Steel
    {74u, 83u, 91u},     // Conveyor
    {107u, 48u, 15u},    // Smelter
    {31u, 112u, 133u},   // Assembler
    {87u, 49u, 156u},    // Ant colony
    {115u, 245u, 90u},   // Power cell
    {235u, 46u, 250u},   // Plasma ammo
    {38u, 184u, 250u},   // Ant
    {240u, 41u, 36u},    // Beetle
    {255u, 66u, 31u},    // Plant stem
    {61u, 224u, 209u},   // Factory core
    {92u, 80u, 54u},     // Silt
    {67u, 107u, 36u},    // Fertilizer
    {224u, 143u, 41u},   // Food
    {77u, 51u, 28u},     // Waste
    {255u, 96u, 178u},   // Hydrogen
    {89u, 106u, 87u},    // Sluice box
    {87u, 163u, 184u},   // Atmosphere
}};

[[nodiscard]] constexpr Rgb8 material_editor_color(const std::uint32_t material) noexcept {
    return material < material_count ? material_editor_colors[material]
                                     : material_editor_colors[0u];
}

[[nodiscard]] constexpr std::uint32_t rgb_distance_squared(
    const Rgb8 left, const Rgb8 right) noexcept {
    const auto dr = static_cast<std::int32_t>(left.r) - static_cast<std::int32_t>(right.r);
    const auto dg = static_cast<std::int32_t>(left.g) - static_cast<std::int32_t>(right.g);
    const auto db = static_cast<std::int32_t>(left.b) - static_cast<std::int32_t>(right.b);
    return static_cast<std::uint32_t>(dr * dr + dg * dg + db * db);
}

[[nodiscard]] constexpr std::uint32_t material_from_editor_color(const Rgb8 color) noexcept {
    std::uint32_t best{};
    auto best_distance = std::numeric_limits<std::uint32_t>::max();
    for (std::uint32_t material = 0u; material < material_count; ++material) {
        const auto candidate = material_editor_colors[material];
        if (candidate == color) return material;
        const auto distance = rgb_distance_squared(candidate, color);
        if (distance < best_distance) {
            best_distance = distance;
            best = material;
        }
    }
    return best;
}

[[nodiscard]] consteval bool material_editor_colors_are_unique() noexcept {
    for (std::size_t left = 0u; left < material_editor_colors.size(); ++left) {
        for (std::size_t right = left + 1u; right < material_editor_colors.size(); ++right) {
            if (material_editor_colors[left] == material_editor_colors[right]) return false;
        }
    }
    return true;
}

static_assert(material_editor_colors_are_unique());
static_assert(material_editor_color(static_cast<std::uint32_t>(Material::water)) ==
              Rgb8{20u, 87u, 235u});
static_assert(material_editor_color(static_cast<std::uint32_t>(Material::stone)) ==
              Rgb8{81u, 86u, 93u});
static_assert(material_editor_color(static_cast<std::uint32_t>(Material::lava)) ==
              Rgb8{250u, 46u, 4u});

} // namespace sandhybrid
