#include "sandhybrid/scene_image.hpp"

#include "sandhybrid/material.hpp"
#include "sandhybrid/material_color.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <limits>
#include <exception>
#include <string>
#include <string_view>
#include <vector>

namespace sandhybrid {
namespace {

constexpr std::uint32_t aux_charged = 0x40000000u;
constexpr std::uint32_t aux_bee_fed = 0x10000000u;
constexpr std::uint32_t aux_bee_swarm = 0x08000000u;
constexpr std::uint32_t aux_plant_stem = 0x08000000u;
constexpr std::uint32_t aux_structural = 0x04000000u;
constexpr std::uint32_t aux_supported = 0x02000000u;
constexpr std::uint32_t aux_moved = 0x01000000u;
constexpr std::uint32_t aux_state_mask = 0x000000ffu;
constexpr std::uint32_t aux_random_mask = 0x00ffff00u;
constexpr std::uint32_t tile_size = 8u;
constexpr std::uint32_t minimum_cohesive_cells = 32u;
constexpr std::uint32_t full_strength_cells = 52u;
constexpr std::uint32_t bee_target_none = 0xffffu;
constexpr std::uint32_t bee_formation_count = 100u;
constexpr std::uint32_t bee_metadata_mask = 0x00ffffffu;


constexpr std::uint32_t hash32(std::uint32_t value) noexcept {
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value;
}

void set_state(std::uint32_t& aux, const std::uint32_t value) noexcept {
    aux = (aux & ~aux_state_mask) | std::min(value, 255u);
}

std::uint32_t pack_bee_metadata(std::uint32_t aux, const std::uint32_t home_x,
                                const std::uint32_t home_y, const std::uint32_t slot) noexcept {
    const auto packed_home_x = std::min(home_x / 4u, 255u);
    const auto packed_home_y = std::min(home_y / 4u, 127u);
    const auto metadata = packed_home_x | (packed_home_y << 8u) | ((slot & 255u) << 15u);
    return (aux & ~bee_metadata_mask) | metadata;
}

bool structural_candidate(const Material material) noexcept {
    return is_block_material(material) || material == Material::dirt || material == Material::grass ||
           material == Material::sand || material == Material::silt || material == Material::salt ||
           material == Material::ice;
}

std::int32_t default_temperature(const Material material) noexcept {
    if (material == Material::lava || material == Material::magma_vent) return 1300;
    if (material == Material::fire || material == Material::lightning) return 700;
    if (material == Material::ember) return 420;
    if (material == Material::ice) return -20;
    if (material == Material::snow) return -8;
    if (material == Material::steam || material == Material::dirty_steam) return 110;
    if (material == Material::uranium) return 42;
    if (material == Material::smelter) return 180;
    return 20;
}

std::uint32_t default_aux(const Material material, const std::uint32_t entropy) noexcept {
    std::uint32_t aux = hash32(entropy ^ static_cast<std::uint32_t>(material)) & aux_random_mask;
    switch (material) {
    case Material::saltwater:
    case Material::dirty_water:
        set_state(aux, 96u);
        break;
    case Material::salt:
    case Material::honey:
    case Material::silt:
    case Material::fertilizer:
    case Material::food:
    case Material::waste:
    case Material::aluminum_shavings:
    case Material::gold:
    case Material::iron_ore:
    case Material::iron:
    case Material::steel:
    case Material::power_cell:
    case Material::plasma_ammo:
        set_state(aux, 255u);
        break;
    case Material::bee:
        aux |= aux_bee_fed | aux_bee_swarm;
        break;
    case Material::pollen:
        set_state(aux, 16u);
        break;
    case Material::oxygen:
        set_state(aux, 220u);
        break;
    case Material::carbon_dioxide:
        set_state(aux, 180u);
        break;
    case Material::hydrogen:
        set_state(aux, 210u);
        break;
    case Material::ant:
    case Material::beetle:
        set_state(aux, 1u + (hash32(entropy) & 1u));
        break;
    case Material::plant_stem:
        aux |= aux_plant_stem;
        set_state(aux, 1u);
        break;
    case Material::conveyor:
    case Material::factory_core:
    case Material::sluice_box:
        aux |= aux_charged;
        set_state(aux, 255u);
        break;
    default:
        break;
    }
    return aux;
}

std::string scene_file_stem(const Scene scene) {
    switch (scene) {
    case Scene::sandbox: return "sandbox";
    case Scene::blank: return "blank";
    case Scene::volcano: return "volcano";
    case Scene::waterworks: return "waterworks";
    case Scene::ecosystem: return "ecosystem";
    case Scene::engineering_lab: return "engineering_lab";
    case Scene::gold_mine: return "platformer";
    case Scene::demolition: return "demolition";
    case Scene::frontier_base: return "frontier_base";
    case Scene::count: break;
    }
    return "unknown";
}

bool read_token(std::istream& stream, std::string& token) {
    token.clear();
    while (stream) {
        stream >> std::ws;
        if (stream.peek() != '#') break;
        stream.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
    return static_cast<bool>(stream >> token);
}

std::uint32_t material_from_color(const Rgb8 color) noexcept {
    return material_from_editor_color(color);
}

} // namespace

std::filesystem::path scene_image_path(const std::filesystem::path& directory, const Scene scene) {
    return directory / (scene_file_stem(scene) + ".ppm");
}

bool scene_image_exists(const std::filesystem::path& directory, const Scene scene) {
    std::error_code error;
    return std::filesystem::is_regular_file(scene_image_path(directory, scene), error);
}

bool load_scene_ppm(const std::filesystem::path& path,
                    const std::uint32_t width,
                    const std::uint32_t height,
                    const std::span<SceneCell> cells,
                    std::string& error) {
    error.clear();
    if (cells.size() != static_cast<std::size_t>(width) * height) {
        error = "scene cell span has the wrong size";
        return false;
    }

    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        error = "unable to open " + path.string();
        return false;
    }

    std::string token;
    if (!read_token(stream, token) || token != "P6") {
        error = "scene image must be binary PPM P6";
        return false;
    }
    std::uint32_t file_width{};
    std::uint32_t file_height{};
    try {
        if (!read_token(stream, token)) { error = "missing scene width"; return false; }
        file_width = static_cast<std::uint32_t>(std::stoul(token));
        if (!read_token(stream, token)) { error = "missing scene height"; return false; }
        file_height = static_cast<std::uint32_t>(std::stoul(token));
    } catch (const std::exception&) {
        error = "scene image dimensions are invalid";
        return false;
    }
    if (!read_token(stream, token) || token != "255") {
        error = "scene image must use 8-bit RGB channels";
        return false;
    }
    stream.get();
    if (file_width != width || file_height != height) {
        error = "scene image dimensions must be " + std::to_string(width) + "x" + std::to_string(height);
        return false;
    }

    std::vector<Rgb8> pixels(cells.size());
    stream.read(reinterpret_cast<char*>(pixels.data()), static_cast<std::streamsize>(pixels.size() * sizeof(Rgb8)));
    if (!stream) {
        error = "scene image pixel data is truncated";
        return false;
    }

    std::vector<std::uint32_t> materials(cells.size());
    const auto tile_columns = (width + tile_size - 1u) / tile_size;
    const auto tile_rows = (height + tile_size - 1u) / tile_size;
    std::vector<std::uint16_t> counts(
        static_cast<std::size_t>(tile_columns) * tile_rows * material_count, 0u);

    for (std::uint32_t y = 0u; y < height; ++y) {
        for (std::uint32_t x = 0u; x < width; ++x) {
            const auto index = static_cast<std::size_t>(y) * width + x;
            const auto material = material_from_color(pixels[index]);
            materials[index] = material;
            const auto typed = static_cast<Material>(material);
            if (material != 0u && structural_candidate(typed)) {
                const auto tile = static_cast<std::size_t>(y / tile_size) * tile_columns + x / tile_size;
                ++counts[tile * material_count + material];
            }
        }
    }

    for (std::uint32_t y = 0u; y < height; ++y) {
        for (std::uint32_t x = 0u; x < width; ++x) {
            const auto index = static_cast<std::size_t>(y) * width + x;
            const auto material_id = materials[index];
            const auto material = static_cast<Material>(material_id);
            auto aux = default_aux(material, static_cast<std::uint32_t>(index));
            if (material_id != 0u && structural_candidate(material)) {
                const auto tile = static_cast<std::size_t>(y / tile_size) * tile_columns + x / tile_size;
                const auto occupancy = static_cast<std::uint32_t>(counts[tile * material_count + material_id]);
                if (occupancy >= minimum_cohesive_cells) {
                    aux |= aux_structural | aux_supported;
                    const auto health = occupancy >= full_strength_cells
                        ? 255u
                        : std::max(64u, occupancy * 255u / full_strength_cells);
                    set_state(aux, health);
                } else {
                    aux &= ~(aux_structural | aux_supported);
                    aux |= aux_moved;
                    set_state(aux, 48u);
                }
            }
            cells[index] = SceneCell{
                .material = material_id,
                .age = 0u,
                .temperature = default_temperature(material),
                .aux = aux,
            };
        }
    }
    std::vector<std::size_t> queen_indices;
    std::vector<std::size_t> bee_indices;
    for (std::size_t index = 0; index < materials.size(); ++index) {
        if (materials[index] == static_cast<std::uint32_t>(Material::queen_bee)) queen_indices.push_back(index);
        if (materials[index] == static_cast<std::uint32_t>(Material::bee)) bee_indices.push_back(index);
    }

    std::vector<std::uint32_t> colony_sizes(queen_indices.empty() ? 1u : queen_indices.size(), 0u);
    std::uint32_t fallback_home_x = width / 2u;
    std::uint32_t fallback_home_y = height / 2u;
    if (queen_indices.empty() && !bee_indices.empty()) {
        std::uint64_t sum_x = 0u;
        std::uint64_t sum_y = 0u;
        for (const auto bee_index : bee_indices) {
            sum_x += bee_index % width;
            sum_y += bee_index / width;
        }
        fallback_home_x = static_cast<std::uint32_t>(sum_x / bee_indices.size());
        fallback_home_y = static_cast<std::uint32_t>(sum_y / bee_indices.size());
    }

    for (const auto bee_index : bee_indices) {
        const auto bee_x = static_cast<std::uint32_t>(bee_index % width);
        const auto bee_y = static_cast<std::uint32_t>(bee_index / width);
        std::size_t colony = 0u;
        std::uint64_t best_distance = std::numeric_limits<std::uint64_t>::max();
        if (!queen_indices.empty()) {
            for (std::size_t candidate = 0; candidate < queen_indices.size(); ++candidate) {
                const auto queen_x = static_cast<std::uint32_t>(queen_indices[candidate] % width);
                const auto queen_y = static_cast<std::uint32_t>(queen_indices[candidate] / width);
                const auto dx = static_cast<std::int64_t>(bee_x) - queen_x;
                const auto dy = static_cast<std::int64_t>(bee_y) - queen_y;
                const auto distance = static_cast<std::uint64_t>(dx * dx + dy * dy);
                if (distance < best_distance) {
                    best_distance = distance;
                    colony = candidate;
                }
            }
        }
        const auto home_index = queen_indices.empty() ? std::size_t{0} : queen_indices[colony];
        const auto home_x = queen_indices.empty() ? fallback_home_x : static_cast<std::uint32_t>(home_index % width);
        const auto home_y = queen_indices.empty() ? fallback_home_y : static_cast<std::uint32_t>(home_index / width);
        const auto slot = colony_sizes[colony]++ % bee_formation_count;
        auto& bee = cells[bee_index];
        bee.aux = pack_bee_metadata(bee.aux | aux_bee_fed | aux_bee_swarm, home_x, home_y, slot);
        bee.age = (slot * 17u) % 900u | (bee_target_none << 16u);
    }
    return true;
}

bool save_scene_ppm(const std::filesystem::path& path,
                    const std::uint32_t width,
                    const std::uint32_t height,
                    const std::span<const SceneCell> cells,
                    std::string& error) {
    error.clear();
    if (cells.size() != static_cast<std::size_t>(width) * height) {
        error = "scene cell span has the wrong size";
        return false;
    }
    std::error_code directory_error;
    std::filesystem::create_directories(path.parent_path(), directory_error);
    if (directory_error) {
        error = "unable to create scene directory: " + directory_error.message();
        return false;
    }

    std::ofstream stream{path, std::ios::binary | std::ios::trunc};
    if (!stream) {
        error = "unable to write " + path.string();
        return false;
    }
    stream << "P6\n" << width << ' ' << height << "\n255\n";
    for (const auto& cell : cells) {
        const auto color = material_editor_color(cell.material < material_count ? cell.material : 0u);
        stream.write(reinterpret_cast<const char*>(&color), sizeof(color));
    }
    if (!stream) {
        error = "failed while writing scene image";
        return false;
    }
    return true;
}

bool write_scene_material_key(const std::filesystem::path& directory, std::string& error) {
    error.clear();
    std::error_code directory_error;
    std::filesystem::create_directories(directory, directory_error);
    if (directory_error) {
        error = "unable to create scene directory: " + directory_error.message();
        return false;
    }

    std::ofstream text{directory / "material_key.txt", std::ios::trunc};
    if (!text) {
        error = "unable to write material_key.txt";
        return false;
    }
    text << "SandHybrid PPM scene material key\n"
            "Colors are stable representatives of the visible cell palette for ordinary Paint editing. Exact key colors are lossless; nearby colors load as the nearest material.\n"
            "Any structural material with fewer than 32 represented pixels in its aligned 8x8 region crumbles.\n\n";
    constexpr char hex[] = "0123456789ABCDEF";
    for (std::uint32_t material = 0u; material < material_count; ++material) {
        const auto color = material_editor_color(material);
        const std::array channels{color.r, color.g, color.b};
        text << material << "  #";
        for (const auto channel : channels) {
            text << hex[channel >> 4u] << hex[channel & 15u];
        }
        text << "  " << material_names[material] << '\n';
    }

    constexpr std::uint32_t columns = 13u;
    constexpr std::uint32_t swatch = 16u;
    const auto rows = (material_count + columns - 1u) / columns;
    const auto width = columns * swatch;
    const auto height = rows * swatch;
    std::ofstream image{directory / "material_key.ppm", std::ios::binary | std::ios::trunc};
    if (!image) {
        error = "unable to write material_key.ppm";
        return false;
    }
    image << "P6\n" << width << ' ' << height << "\n255\n";
    for (std::uint32_t y = 0u; y < height; ++y) {
        for (std::uint32_t x = 0u; x < width; ++x) {
            const auto material = (y / swatch) * columns + x / swatch;
            const auto color = material_editor_color(material < material_count ? material : 0u);
            image.write(reinterpret_cast<const char*>(&color), sizeof(color));
        }
    }
    if (!text || !image) {
        error = "failed while writing scene material key";
        return false;
    }
    return true;
}

} // namespace sandhybrid
