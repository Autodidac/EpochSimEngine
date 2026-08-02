#include "sandhybrid/material_color.hpp"

#include <cstdint>

using namespace sandhybrid;

static_assert(material_editor_colors.size() == material_count);
static_assert(material_editor_colors_are_unique());
static_assert(material_from_editor_color(material_editor_color(
                  static_cast<std::uint32_t>(Material::water))) ==
              static_cast<std::uint32_t>(Material::water));
static_assert(material_from_editor_color(material_editor_color(
                  static_cast<std::uint32_t>(Material::stone))) ==
              static_cast<std::uint32_t>(Material::stone));
static_assert(material_from_editor_color(material_editor_color(
                  static_cast<std::uint32_t>(Material::lava))) ==
              static_cast<std::uint32_t>(Material::lava));

int main() {
    for (std::uint32_t material = 0u; material < material_count; ++material) {
        if (material_from_editor_color(material_editor_color(material)) != material) return 1;
    }

    // Paint programs may perturb a channel slightly through resampling or color
    // management; nearest-color loading must still recover the intended cell.
    const auto water = material_editor_color(static_cast<std::uint32_t>(Material::water));
    const Rgb8 near_water{
        static_cast<std::uint8_t>(water.r + 1u),
        static_cast<std::uint8_t>(water.g - 1u),
        water.b,
    };
    return material_from_editor_color(near_water) ==
                   static_cast<std::uint32_t>(Material::water)
               ? 0
               : 2;
}
