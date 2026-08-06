#include "sandhybrid/blueprint.hpp"

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace {

[[nodiscard]] bool same_cells(
    const std::span<const sandhybrid::SceneCell> first,
    const std::span<const sandhybrid::SceneCell> second) {
    if (first.size() != second.size()) return false;
    for (std::size_t index = 0u; index < first.size(); ++index) {
        if (first[index].material != second[index].material ||
            first[index].age != second[index].age ||
            first[index].temperature != second[index].temperature ||
            first[index].aux != second[index].aux)
            return false;
    }
    return true;
}

} // namespace

int main() {
    using sandhybrid::BlueprintRotation;
    using sandhybrid::BlueprintTransform;
    using sandhybrid::Material;
    using sandhybrid::SceneCell;
    using sandhybrid::SelectionBounds;

    constexpr std::uint32_t width = 8u;
    constexpr std::uint32_t height = 6u;
    std::vector<SceneCell> source(width * height);
    const auto index = [](const std::uint32_t x, const std::uint32_t y) {
        return static_cast<std::size_t>(y) * width + x;
    };

    source[index(2u, 1u)] = {
        static_cast<std::uint32_t>(Material::sand), 7u, 31, 0x12345678u};
    source[index(3u, 1u)] = {
        static_cast<std::uint32_t>(Material::empty), 9u, -4, 0x11111111u};
    source[index(2u, 2u)] = {
        static_cast<std::uint32_t>(Material::stone), 11u, 40, 0x22222222u};
    source[index(3u, 2u)] = {
        static_cast<std::uint32_t>(Material::water), 13u, 17, 0x33333333u};

    const auto captured = sandhybrid::capture_blueprint(
        source, width, height, SelectionBounds{2u, 1u, 3u, 2u}, "Pump");
    if (!captured.has_value() || captured->width != 2u || captured->height != 2u ||
        captured->cell_count() != 4u || captured->display_name() != "Pump")
        return 1;
    // Captures preserve exact cell metadata, not just material IDs.
    if (captured->at(0u, 0u).age != 7u ||
        captured->at(0u, 0u).temperature != 31 ||
        captured->at(0u, 0u).aux != 0x12345678u)
        return 2;

    std::vector<SceneCell> destination(width * height);
    for (auto& cell : destination)
        cell.material = static_cast<std::uint32_t>(Material::dirt);
    const BlueprintTransform clockwise{BlueprintRotation::degrees_90, false, false};
    // Transparent blueprint cells must not erase destination cells by default.
    if (!sandhybrid::place_blueprint_transactional(
            *captured, destination, width, height, 4u, 1u, clockwise))
        return 3;
    if (destination[index(5u, 1u)].material !=
            static_cast<std::uint32_t>(Material::sand) ||
        destination[index(4u, 1u)].material !=
            static_cast<std::uint32_t>(Material::stone) ||
        destination[index(4u, 2u)].material !=
            static_cast<std::uint32_t>(Material::water) ||
        destination[index(5u, 2u)].material !=
            static_cast<std::uint32_t>(Material::dirt))
        return 4;
    // Rotation must preserve every source cell and its payload.
    if (destination[index(5u, 1u)].age != 7u ||
        destination[index(5u, 1u)].temperature != 31 ||
        destination[index(5u, 1u)].aux != 0x12345678u)
        return 5;

    // An out-of-bounds paste must not partially write.
    const auto before_rejected = destination;
    if (sandhybrid::place_blueprint_transactional(
            *captured, destination, width, height, 7u, 5u, clockwise) ||
        !same_cells(before_rejected, destination))
        return 6;

    // An invalid material must reject the full transaction.
    auto invalid = *captured;
    invalid.cells[0].material = static_cast<std::uint32_t>(Material::count);
    if (sandhybrid::place_blueprint_transactional(
            invalid, destination, width, height, 0u, 0u) ||
        !same_cells(before_rejected, destination))
        return 7;

    // Including Empty cells preserves deliberate Map Chunk erasure metadata.
    if (!sandhybrid::place_blueprint_transactional(
            *captured, destination, width, height, 0u, 0u, {}, true) ||
        destination[index(1u, 0u)].material !=
            static_cast<std::uint32_t>(Material::empty) ||
        destination[index(1u, 0u)].age != 9u)
        return 8;

    std::vector<SceneCell> padded(6u * 5u);
    padded[2u * 6u + 3u].material = static_cast<std::uint32_t>(Material::copper);
    padded[3u * 6u + 4u].material = static_cast<std::uint32_t>(Material::gold);
    const auto trimmed = sandhybrid::capture_trimmed_blueprint(
        padded, 6u, 5u, "Vein");
    if (!trimmed.has_value() || trimmed->width != 2u || trimmed->height != 2u ||
        trimmed->at(0u, 0u).material != static_cast<std::uint32_t>(Material::copper) ||
        trimmed->at(1u, 1u).material != static_cast<std::uint32_t>(Material::gold))
        return 9;

    const auto aligned = sandhybrid::align_selection_to_tiles(
        sandhybrid::selection_from_points(17u, 19u, 8u, 2u), 64u, 64u);
    if (aligned.left != 8u || aligned.top != 0u ||
        aligned.right != 23u || aligned.bottom != 23u)
        return 10;

    const auto clipped = sandhybrid::align_selection_to_tiles(
        SelectionBounds{60u, 60u, 90u, 90u}, 64u, 64u);
    if (clipped.left != 56u || clipped.top != 56u ||
        clipped.right != 63u || clipped.bottom != 63u)
        return 11;
    const auto rejected = sandhybrid::align_selection_to_tiles(
        SelectionBounds{70u, 70u, 90u, 90u}, 64u, 64u);
    if (rejected.valid()) return 12;

    return 0;
}
