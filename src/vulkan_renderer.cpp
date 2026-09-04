#include "sandhybrid/vulkan_renderer.hpp"

#include "sandhybrid/actor_medium.hpp"
#include "sandhybrid/hive_recovery.hpp"
#include "sandhybrid/input_routing.hpp"
#include "sandhybrid/material.hpp"
#include "sandhybrid/scene.hpp"
#include "sandhybrid/scene_spawn.hpp"
#include "sandhybrid/section_scheduler.hpp"
#include "sandhybrid/simulation_policy.hpp"
#include "sandhybrid/scene_image.hpp"
#include "sandhybrid/ui_layout.hpp"
#include "sandhybrid/world_layout.hpp"
#include "sandhybrid/world_save.hpp"
#include "sandhybrid/ui_text_data.hpp"

#include <vulkan/vulkan.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <iterator>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace sandhybrid {

namespace {

constexpr std::uint32_t simulation_local_size = 16;
constexpr std::uint32_t sunlight_local_size = 64;
constexpr std::uint32_t debug_stats_local_size = 256;
constexpr std::uint32_t debug_stat_word_count = 128;
constexpr std::uint32_t nuke_high_sky_bottom_y =
    persistent_world_weather_cloud_tile_y;

static_assert(nuke_high_sky_bottom_y == 536u);
static_assert(nuke_high_sky_bottom_y % authored_scene_foundation_cells == 0u);


[[noreturn]] void throw_vk(const char* operation, const VkResult result) {
    throw std::runtime_error(std::string{operation} + " failed with VkResult " + std::to_string(result));
}

void check_vk(const VkResult result, const char* operation) {
    if (result != VK_SUCCESS) {
        throw_vk(operation, result);
    }
}

std::uint32_t divide_round_up(const std::uint32_t value, const std::uint32_t divisor) {
    return (value + divisor - 1u) / divisor;
}


constexpr std::uint32_t fill_aux_structural = 0x04000000u;
constexpr std::uint32_t fill_aux_supported = 0x02000000u;
constexpr std::uint32_t fill_aux_moved = 0x01000000u;
constexpr std::uint32_t fill_aux_state_mask = 0x000000ffu;
constexpr std::uint32_t fill_aux_random_mask = 0x007fff00u;
constexpr std::uint32_t bee_authored_home_slot_bit = 0x00400000u;

std::uint32_t fill_hash(std::uint32_t value) noexcept {
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value;
}

SceneCell make_fill_cell(const std::uint32_t material_id, const std::uint32_t index) {
    const auto material = static_cast<Material>(material_id < material_count ? material_id : 0u);
    SceneCell cell{
        .material = static_cast<std::uint32_t>(material),
        .age = 0u,
        .temperature = 20,
        .aux = material == Material::atmosphere
            ? 0u : (fill_hash(index ^ material_id * 0x9e3779b9u) & fill_aux_random_mask),
    };
    if (material == Material::magma_vent || material == Material::lava) cell.temperature = 1300;
    else if (material == Material::fire || material == Material::lightning) cell.temperature = 700;
    else if (material == Material::ember) cell.temperature = 420;
    else if (material == Material::ice) cell.temperature = -20;
    else if (material == Material::snow) cell.temperature = -8;
    else if (material == Material::steam || material == Material::dirty_steam) cell.temperature = 110;

    if (material == Material::saltwater || material == Material::dirty_water) cell.aux |= 96u;
    else if (material == Material::salt || material == Material::honey ||
             material == Material::silt || material == Material::fertilizer ||
             material == Material::food || material == Material::waste) cell.aux |= 255u;
    else if (material == Material::oxygen) cell.aux |= 220u;
    else if (material == Material::atmosphere) cell.aux |= 54u;
    else if (material == Material::carbon_dioxide) cell.aux |= 180u;
    else if (material == Material::hydrogen) cell.aux |= 210u;

    if (is_block_material(material)) {
        cell.aux |= fill_aux_structural | fill_aux_supported;
        cell.aux = (cell.aux & ~fill_aux_state_mask) | 255u;
    }
    return cell;
}

[[nodiscard]] bool canonical_fix29_hive_signature_at(
    const std::span<const SceneCell> cells,
    const std::uint32_t width,
    const std::uint32_t height,
    const std::uint32_t queen_x,
    const std::uint32_t queen_y) {
    if (queen_x >= width || queen_y >= height ||
        cells.size() != static_cast<std::size_t>(width) * height)
        return false;

    std::int32_t entropy_queen_x = static_cast<std::int32_t>(queen_x);
    std::int32_t entropy_queen_y = static_cast<std::int32_t>(queen_y);
    std::optional<std::uint32_t> expected_home;
    for (std::uint32_t district = 0u;
         district < persistent_world_district_count; ++district) {
        const auto origin_x = persistent_world_district_origin_x(width, district);
        const auto origin_y = persistent_world_district_origin_y(height, district);
        if (queen_x >= origin_x && queen_x < origin_x + pre_expansion_world_width &&
            queen_y >= origin_y && queen_y < origin_y + pre_expansion_world_height) {
            entropy_queen_x = static_cast<std::int32_t>(queen_x - origin_x);
            entropy_queen_y = static_cast<std::int32_t>(queen_y - origin_y);
            expected_home = ((queen_x - origin_x) / 8u) |
                (((queen_y - origin_y) / 8u) << 7u) | (district << 20u);
            break;
        }
    }

    std::uint32_t shell = 0u;
    std::uint32_t honey = 0u;
    std::uint32_t pollen = 0u;
    std::uint32_t chamber = 0u;
    for (std::int32_t dy = -10; dy <= 10; ++dy) {
        for (std::int32_t dx = -10; dx <= 10; ++dx) {
            const auto x = static_cast<std::int32_t>(queen_x) + dx;
            const auto y = static_cast<std::int32_t>(queen_y) + dy;
            if (x < 0 || y < 0 || x >= static_cast<std::int32_t>(width) ||
                y >= static_cast<std::int32_t>(height))
                return false;
            const auto part = classify_pre_pr19_hive_cell(
                dx, dy,
                fix29_hive_entropy(entropy_queen_x, entropy_queen_y, dx, dy),
                entropy_queen_x, entropy_queen_y);
            if (part == HivePart::empty) continue;
            const auto& cell = cells[
                static_cast<std::size_t>(y) * width + static_cast<std::uint32_t>(x)];
            const bool fixed = (cell.aux &
                (fill_aux_structural | fill_aux_supported)) ==
                (fill_aux_structural | fill_aux_supported);
            switch (part) {
            case HivePart::shell:
                if (cell.material != static_cast<std::uint32_t>(Material::beehive) ||
                    !fixed) return false;
                ++shell;
                break;
            case HivePart::queen:
                if (cell.material != static_cast<std::uint32_t>(Material::queen_bee))
                    return false;
                break;
            case HivePart::honey:
                if (cell.material != static_cast<std::uint32_t>(Material::honey) ||
                    !fixed) return false;
                ++honey;
                break;
            case HivePart::pollen:
                if (cell.material != static_cast<std::uint32_t>(Material::pollen) ||
                    !fixed) return false;
                ++pollen;
                break;
            case HivePart::chamber:
            case HivePart::exit:
                // The authored opening begins Empty, but a running closed
                // system legitimately relaxes Atmosphere into it. Both retain
                // the same intact hive-body signature for load presentation.
                if (cell.material == static_cast<std::uint32_t>(Material::bee) &&
                    expected_home.has_value() && (cell.aux & 0x08000000u) != 0u &&
                    (cell.aux & 0x00701fffu) == *expected_home &&
                    ((cell.aux >> 13u) & 127u) < fix29_bee_formation_count) {
                    // A live home-owned returning Bee may occupy an opening;
                    // its presence must not switch the intact body to raw art.
                } else if (cell.material != static_cast<std::uint32_t>(Material::empty) &&
                    cell.material !=
                        static_cast<std::uint32_t>(Material::atmosphere))
                    return false;
                if (part == HivePart::chamber) ++chamber;
                break;
            case HivePart::empty:
                break;
            }
        }
    }
    return shell == 193u && honey + pollen + chamber == 56u;
}

SceneCell make_resident_substrate_cell(
    const Material material,
    const std::uint32_t index) {
    auto cell = make_fill_cell(static_cast<std::uint32_t>(material), index);
    if (resident_substrate_is_structural(material)) {
        cell.aux |= fill_aux_structural | fill_aux_supported;
        cell.aux = (cell.aux & ~fill_aux_state_mask) | 255u;
    }
    return cell;
}

std::filesystem::path executable_directory() {
#ifdef _WIN32
    std::wstring path(32768, L'\0');
    const auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size()) {
        throw std::runtime_error("GetModuleFileNameW failed.");
    }
    path.resize(length);
    return std::filesystem::path{path}.parent_path();
#else
    std::array<char, 4096> path{};
    const auto length = ::readlink("/proc/self/exe", path.data(), path.size() - 1u);
    if (length <= 0) {
        throw std::runtime_error("Unable to resolve /proc/self/exe.");
    }
    path[static_cast<std::size_t>(length)] = '\0';
    return std::filesystem::path{path.data()}.parent_path();
#endif
}

void startup_log(const std::string_view message) {
    std::fprintf(stderr, "[SandHybrid] %.*s\n", static_cast<int>(message.size()), message.data());
    std::fflush(stderr);
#ifdef _WIN32
    const std::string line = std::string{"[SandHybrid] "} + std::string{message} + "\n";
    OutputDebugStringA(line.c_str());
#endif
}

std::vector<std::uint32_t> read_spirv(const std::filesystem::path& path) {
    std::ifstream stream{path, std::ios::binary | std::ios::ate};
    if (!stream) {
        throw std::runtime_error("Unable to open shader: " + path.string());
    }

    const auto end_position = stream.tellg();
    if (end_position <= 0) {
        throw std::runtime_error("Invalid SPIR-V file size: " + path.string());
    }
    const auto size = static_cast<std::size_t>(end_position);
    if ((size % 4u) != 0u) {
        throw std::runtime_error("Invalid SPIR-V file size: " + path.string());
    }

    std::vector<std::uint32_t> data(size / sizeof(std::uint32_t));
    stream.seekg(0);
    stream.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
    if (!stream) {
        throw std::runtime_error("Failed to read shader: " + path.string());
    }
    return data;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void*) {
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT && callback_data != nullptr) {
#ifdef _WIN32
        OutputDebugStringA(callback_data->pMessage);
        OutputDebugStringA("\n");
#else
        std::fprintf(stderr, "Vulkan: %s\n", callback_data->pMessage);
#endif
    }
    return VK_FALSE;
}

struct Buffer final {
    VkBuffer handle{};
    VkDeviceMemory memory{};
    VkDeviceSize size{};
};

struct QueueFamilies final {
    std::optional<std::uint32_t> graphics_compute;
    std::optional<std::uint32_t> present;

    [[nodiscard]] bool complete() const noexcept {
        return graphics_compute.has_value() && present.has_value();
    }
};

struct SwapchainSupport final {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

struct FrameContext final {
    VkCommandBuffer command_buffer{};
    VkSemaphore image_available{};
    VkSemaphore render_finished{};
    VkFence fence{};
};

struct SimulationPush final {
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint32_t step{};
    std::uint32_t seed{};
    std::int32_t brush_x{};
    std::int32_t brush_y{};
    std::uint32_t radius{};
    std::uint32_t material{};
    std::int32_t active_section_x{};
    std::int32_t active_section_y{};
    std::uint32_t active_mode{};
    std::uint32_t reserved{};
};
static_assert(sizeof(SimulationPush) == 48);

struct MovementPush final {
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint32_t step{};
    std::uint32_t seed{};
    std::int32_t phase{};
    std::int32_t parity{};
    std::uint32_t reserved0{};
    std::uint32_t reserved1{};
    std::int32_t active_section_x{};
    std::int32_t active_section_y{};
    std::uint32_t active_mode{};
    std::uint32_t worker_count{};
};
static_assert(sizeof(MovementPush) == 48);

struct ActorPush final {
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint32_t step{};
    std::uint32_t seed{};
    std::int32_t move_x{};
    std::int32_t move_y{};
    std::int32_t aim_x{};
    std::int32_t aim_y{};
    std::uint32_t fire{};
    std::uint32_t reset{};
    std::uint32_t scene{};
    std::uint32_t deposit{};
    std::uint32_t simulate{};
    std::int32_t active_section_x{};
    std::int32_t active_section_y{};
    std::uint32_t active_mode{};
    std::uint32_t inventory_slot{};
};
static_assert(sizeof(ActorPush) == 68);

struct RenderPush final {
    std::uint32_t grid_width{};
    std::uint32_t grid_height{};
    std::uint32_t window_width{};
    std::uint32_t window_height{};
    std::uint32_t selected_material{};
    std::uint32_t material_count{};
    std::int32_t cursor_x{};
    std::int32_t cursor_y{};
    std::uint32_t brush_radius{};
    std::uint32_t status_height{};
    std::uint32_t palette_height{};
    std::uint32_t group_tabs_height{};
    std::uint32_t material_slots{};
    std::uint32_t frames_per_second{};
    std::uint32_t paused{};
    std::uint32_t presentation_limit{};
    std::uint32_t selected_group{};
    std::uint32_t hovered_group{};
    std::uint32_t hovered_material{};
    // Persistent World uses the legacy graphics-only selected-scene slot for
    // the latest explicit Beehive-tool Queen anchor.
    std::uint32_t selected_scene{};
    std::uint32_t group_count{};
    std::uint32_t scene_count{};
    std::uint32_t mining_mode{};
    std::uint32_t inspect_mode{};
    std::uint32_t debug_mode{};
    std::uint32_t tile_columns{};
    std::uint32_t tile_rows{};
    std::uint32_t viewport_left{};
    std::uint32_t viewport_top{};
    std::uint32_t viewport_width{};
    std::uint32_t viewport_height{};
    std::uint32_t view_origin_x{};
    std::uint32_t view_origin_y{};
    std::uint32_t view_width{};
    std::uint32_t view_height{};
    std::uint32_t brush_shape{};
    std::uint32_t placement_mode{};
    std::uint32_t active_area_count{};
    std::int32_t active_area_x{};
    std::int32_t active_area_y{};
    std::uint32_t active_scope_mode{};
    std::uint32_t camera_controls{};
    std::uint32_t map_mode{};
    std::uint32_t camera_origin_x{};
    std::uint32_t camera_origin_y{};
    std::uint32_t camera_view_width{};
    std::uint32_t camera_view_height{};
    std::uint32_t map_viewport_left{};
    std::uint32_t map_viewport_top{};
    std::uint32_t map_viewport_width{};
    std::uint32_t map_viewport_height{};
    std::uint32_t map_origin_x{};
    std::uint32_t map_origin_y{};
    std::uint32_t map_view_width{};
    std::uint32_t map_view_height{};
    std::uint32_t selected_inventory_slot{};
    std::uint32_t selected_workspace{};
    std::uint32_t render_frame{};
    std::uint32_t world_time{};
    std::uint32_t day_cycle_steps{};
    std::uint32_t designer_flags{};
    std::uint32_t blueprint_flags{};
    std::uint32_t framebuffer_width{};
    std::uint32_t framebuffer_height{};
};
static_assert(sizeof(RenderPush) == 256);

bool contains_extension(const std::vector<VkExtensionProperties>& extensions, const char* name) {
    return std::ranges::any_of(extensions, [name](const VkExtensionProperties& extension) {
        return std::strcmp(extension.extensionName, name) == 0;
    });
}

[[maybe_unused]] bool contains_layer(const std::vector<VkLayerProperties>& layers, const char* name) {
    return std::ranges::any_of(layers, [name](const VkLayerProperties& layer) {
        return std::strcmp(layer.layerName, name) == 0;
    });
}

} // namespace

struct VulkanRenderer::Impl final {
    const NativeWindow& window;
    SimulationConfig config;
    std::string save_slot;

    VkInstance instance{};
    VkDebugUtilsMessengerEXT debug_messenger{};
    VkSurfaceKHR surface{};
    VkPhysicalDevice physical_device{};
    bool cpu_physical_device{};
    VkDevice device{};
    std::uint32_t graphics_family{};
    std::uint32_t present_family{};
    VkQueue graphics_queue{};
    VkQueue present_queue{};

    VkSwapchainKHR swapchain{};
    VkFormat swapchain_format{};
    VkExtent2D swapchain_extent{};
    std::vector<VkImage> swapchain_images;
    std::vector<VkFence> image_fences;
    std::vector<VkImageView> swapchain_views;
    std::vector<VkFramebuffer> framebuffers;
    VkRenderPass render_pass{};

    VkDescriptorSetLayout descriptor_set_layout{};
    VkDescriptorPool descriptor_pool{};
    std::array<VkDescriptorSet, 2> descriptor_sets{};
    VkPipelineLayout compute_pipeline_layout{};
    VkPipelineLayout graphics_pipeline_layout{};
    VkPipeline reset_pipeline{};
    VkPipeline paint_pipeline{};
    VkPipeline sunlight_pipeline{};
    VkPipeline tile_pipeline{};
    VkPipeline chunk_pipeline{};
    VkPipeline copy_cells_pipeline{};
    VkPipeline chemistry_pipeline{};
    VkPipeline conservation_corrections_pipeline{};
    VkPipeline bee_movement_pipeline{};
    VkPipeline rainfall_pipeline{};
    VkPipeline macro_movement_pipeline{};
    VkPipeline movement_pipeline{};
    VkPipeline actor_pipeline{};
    VkPipeline debug_stats_pipeline{};
    VkPipeline graphics_pipeline{};

    VkCommandPool command_pool{};
    std::vector<FrameContext> frames;
    std::uint32_t frame_index{};

    std::array<Buffer, 2> cell_buffers{};
    Buffer map_snapshot_buffer{};
    Buffer sunlight_buffer{};
    Buffer actor_buffer{};
    Buffer tile_buffer{};
    Buffer chunk_buffer{};
    Buffer conservation_buffer{};
    Buffer rainfall_buffer{};
    Buffer ui_text_buffer{};
    Buffer designer_buffer{};
    Buffer scene_staging_buffer{};
    Buffer frame_capture_buffer{};
    std::uint32_t current_set{};
    std::uint32_t simulation_step{};
    std::uint32_t random_seed{0xD17A5EEDu};
    bool needs_reset{true};
    bool gpu_stalled{false};
    bool first_submission_logged{false};
    bool first_present_logged{false};
    bool debug_was_visible{};
    std::uint32_t debug_sample_frame{};
    std::uint32_t map_snapshot_step{};
    std::uint32_t map_snapshot_slice{};
    bool map_was_visible{};
    std::uint32_t nuke_flash_frames_remaining{};
    bool nuke_dispatch_pending{};
    static constexpr std::uint32_t no_tool_hive_anchor = 0xffffffffu;
    std::uint32_t tool_hive_anchor{no_tool_hive_anchor};
    std::optional<std::filesystem::path> pending_frame_capture{};
#if SANDHYBRID_ENABLE_VALIDATION
    std::chrono::steady_clock::time_point next_conservation_log{};
#endif

    explicit Impl(const NativeWindow& native_window,
        const SimulationConfig simulation_config,
        std::string requested_save_slot)
        : window(native_window), config(simulation_config),
save_slot(normalize_world_slot(requested_save_slot)) {
        if (config.grid_width == 0 || config.grid_height == 0) {
            throw std::invalid_argument("Simulation dimensions must be non-zero.");
        }
        if (config.frames_in_flight == 0 || config.frames_in_flight > 4) {
            throw std::invalid_argument("frames_in_flight must be between one and four.");
        }
        if (config.max_frames_per_second == 0 || config.max_frames_per_second > 1000) {
            throw std::invalid_argument("max_frames_per_second must be between one and 1000.");
        }

        try {
            startup_log("Creating Vulkan instance...");
            create_instance();
            startup_log("Creating window surface...");
            surface = window.create_surface(instance);
            startup_log("Selecting physical device...");
            select_physical_device();
            startup_log("Creating logical device...");
            create_device();
            startup_log("Creating command pool...");
            create_command_pool();
            startup_log("Creating descriptor layouts...");
            create_descriptor_layouts();
            startup_log("Allocating simulation buffers...");
            create_buffers();
            startup_log("Creating descriptor sets...");
            create_descriptors();
            startup_log("Creating compute pipelines...");
            create_compute_pipelines();
            startup_log("Creating frame synchronization...");
            create_frames();
            startup_log("Creating swapchain and graphics pipeline...");
            create_swapchain_resources(1280, 720);
            startup_log("Vulkan startup complete.");
        } catch (...) {
            cleanup();
            throw;
        }
    }

    ~Impl() {
        cleanup();
    }

    void cleanup() noexcept {
        if (device != VK_NULL_HANDLE && gpu_stalled) {
            startup_log("GPU did not complete within the bounded wait; abandoning Vulkan objects for OS cleanup.");
            return;
        }
        if (device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device);
        }

        destroy_swapchain_resources();

        if (device != VK_NULL_HANDLE) {
            for (auto& frame : frames) {
                if (frame.image_available != VK_NULL_HANDLE) vkDestroySemaphore(device, frame.image_available, nullptr);
                if (frame.render_finished != VK_NULL_HANDLE) vkDestroySemaphore(device, frame.render_finished, nullptr);
                if (frame.fence != VK_NULL_HANDLE) vkDestroyFence(device, frame.fence, nullptr);
                frame = {};
            }

            if (reset_pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, reset_pipeline, nullptr);
            if (paint_pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, paint_pipeline, nullptr);
            if (sunlight_pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, sunlight_pipeline, nullptr);
            if (tile_pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, tile_pipeline, nullptr);
            if (chunk_pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, chunk_pipeline, nullptr);
            if (copy_cells_pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, copy_cells_pipeline, nullptr);
            if (chemistry_pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, chemistry_pipeline, nullptr);
            if (conservation_corrections_pipeline != VK_NULL_HANDLE)
                vkDestroyPipeline(device, conservation_corrections_pipeline, nullptr);
            if (bee_movement_pipeline != VK_NULL_HANDLE)
                vkDestroyPipeline(device, bee_movement_pipeline, nullptr);
            if (rainfall_pipeline != VK_NULL_HANDLE)
                vkDestroyPipeline(device, rainfall_pipeline, nullptr);
            if (macro_movement_pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, macro_movement_pipeline, nullptr);
            if (movement_pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, movement_pipeline, nullptr);
            if (actor_pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, actor_pipeline, nullptr);
            if (debug_stats_pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, debug_stats_pipeline, nullptr);
            if (graphics_pipeline_layout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, graphics_pipeline_layout, nullptr);
            if (compute_pipeline_layout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, compute_pipeline_layout, nullptr);
            if (descriptor_pool != VK_NULL_HANDLE) vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
            if (descriptor_set_layout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);

            destroy_buffer(scene_staging_buffer);
            destroy_buffer(designer_buffer);
            destroy_buffer(ui_text_buffer);
            destroy_buffer(conservation_buffer);
            destroy_buffer(rainfall_buffer);
            destroy_buffer(chunk_buffer);
            destroy_buffer(tile_buffer);
            destroy_buffer(actor_buffer);
            destroy_buffer(map_snapshot_buffer);
            destroy_buffer(sunlight_buffer);
            for (auto& buffer : cell_buffers) destroy_buffer(buffer);

            if (command_pool != VK_NULL_HANDLE) vkDestroyCommandPool(device, command_pool, nullptr);
            vkDestroyDevice(device, nullptr);
            device = VK_NULL_HANDLE;
        }

        if (surface != VK_NULL_HANDLE && instance != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(instance, surface, nullptr);
            surface = VK_NULL_HANDLE;
        }

        if (debug_messenger != VK_NULL_HANDLE && instance != VK_NULL_HANDLE) {
            const auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
            if (destroy != nullptr) destroy(instance, debug_messenger, nullptr);
            debug_messenger = VK_NULL_HANDLE;
        }
        if (instance != VK_NULL_HANDLE) {
            vkDestroyInstance(instance, nullptr);
            instance = VK_NULL_HANDLE;
        }
    }

    void create_instance() {
        std::uint32_t extension_count{};
        check_vk(vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr),
                 "vkEnumerateInstanceExtensionProperties(count)");
        std::vector<VkExtensionProperties> available_extensions(extension_count);
        check_vk(vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, available_extensions.data()),
                 "vkEnumerateInstanceExtensionProperties(data)");

        std::uint32_t layer_count{};
        check_vk(vkEnumerateInstanceLayerProperties(&layer_count, nullptr),
                 "vkEnumerateInstanceLayerProperties(count)");
        std::vector<VkLayerProperties> available_layers(layer_count);
        check_vk(vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data()),
                 "vkEnumerateInstanceLayerProperties(data)");

        auto extensions = window.required_instance_extensions();
        for (const auto* extension : extensions) {
            if (!contains_extension(available_extensions, extension)) {
                throw std::runtime_error(std::string{"Required Vulkan instance extension is unavailable: "} + extension);
            }
        }

        std::vector<const char*> layers;
        bool enable_debug = false;
#if SANDHYBRID_ENABLE_VALIDATION
        if (contains_layer(available_layers, "VK_LAYER_KHRONOS_validation")) {
            layers.push_back("VK_LAYER_KHRONOS_validation");
            if (contains_extension(available_extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
                extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
                enable_debug = true;
            }
        }
#endif

        const VkApplicationInfo application_info{
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = "SandHybrid",
            .applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
            .pEngineName = "SandHybrid",
            .engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
            .apiVersion = VK_API_VERSION_1_2,
        };

        VkDebugUtilsMessengerCreateInfoEXT debug_info{
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = debug_callback,
        };

        const VkInstanceCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = enable_debug ? &debug_info : nullptr,
            .pApplicationInfo = &application_info,
            .enabledLayerCount = static_cast<std::uint32_t>(layers.size()),
            .ppEnabledLayerNames = layers.data(),
            .enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()),
            .ppEnabledExtensionNames = extensions.data(),
        };
        check_vk(vkCreateInstance(&create_info, nullptr, &instance), "vkCreateInstance");

        if (enable_debug) {
            const auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
            if (create != nullptr) {
                check_vk(create(instance, &debug_info, nullptr, &debug_messenger),
                         "vkCreateDebugUtilsMessengerEXT");
            }
        }
    }

    QueueFamilies find_queue_families(const VkPhysicalDevice candidate) const {
        std::uint32_t count{};
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &count, nullptr);
        std::vector<VkQueueFamilyProperties> properties(count);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &count, properties.data());

        QueueFamilies result;
        for (std::uint32_t index = 0; index < count; ++index) {
            const auto flags = properties[index].queueFlags;
            if ((flags & VK_QUEUE_GRAPHICS_BIT) != 0 && (flags & VK_QUEUE_COMPUTE_BIT) != 0) {
                result.graphics_compute = index;
            }

            VkBool32 present_supported = VK_FALSE;
            check_vk(vkGetPhysicalDeviceSurfaceSupportKHR(candidate, index, surface, &present_supported),
                     "vkGetPhysicalDeviceSurfaceSupportKHR");
            if (present_supported == VK_TRUE) {
                result.present = index;
            }
            if (result.complete()) break;
        }
        return result;
    }

    SwapchainSupport query_swapchain_support(const VkPhysicalDevice candidate) const {
        SwapchainSupport support;
        check_vk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(candidate, surface, &support.capabilities),
                 "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

        std::uint32_t format_count{};
        check_vk(vkGetPhysicalDeviceSurfaceFormatsKHR(candidate, surface, &format_count, nullptr),
                 "vkGetPhysicalDeviceSurfaceFormatsKHR(count)");
        support.formats.resize(format_count);
        if (format_count > 0) {
            check_vk(vkGetPhysicalDeviceSurfaceFormatsKHR(candidate, surface, &format_count, support.formats.data()),
                     "vkGetPhysicalDeviceSurfaceFormatsKHR(data)");
        }

        std::uint32_t mode_count{};
        check_vk(vkGetPhysicalDeviceSurfacePresentModesKHR(candidate, surface, &mode_count, nullptr),
                 "vkGetPhysicalDeviceSurfacePresentModesKHR(count)");
        support.present_modes.resize(mode_count);
        if (mode_count > 0) {
            check_vk(vkGetPhysicalDeviceSurfacePresentModesKHR(candidate, surface, &mode_count,
                                                               support.present_modes.data()),
                     "vkGetPhysicalDeviceSurfacePresentModesKHR(data)");
        }
        return support;
    }

    bool device_supports_swapchain(const VkPhysicalDevice candidate) const {
        std::uint32_t count{};
        check_vk(vkEnumerateDeviceExtensionProperties(candidate, nullptr, &count, nullptr),
                 "vkEnumerateDeviceExtensionProperties(count)");
        std::vector<VkExtensionProperties> extensions(count);
        check_vk(vkEnumerateDeviceExtensionProperties(candidate, nullptr, &count, extensions.data()),
                 "vkEnumerateDeviceExtensionProperties(data)");
        return contains_extension(extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    void select_physical_device() {
        std::uint32_t count{};
        check_vk(vkEnumeratePhysicalDevices(instance, &count, nullptr), "vkEnumeratePhysicalDevices(count)");
        if (count == 0) {
            throw std::runtime_error("No Vulkan-capable GPU was found.");
        }

        std::vector<VkPhysicalDevice> devices(count);
        check_vk(vkEnumeratePhysicalDevices(instance, &count, devices.data()), "vkEnumeratePhysicalDevices(data)");

        std::uint64_t best_score{};
        for (const auto candidate : devices) {
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(candidate, &properties);
            if (properties.apiVersion < VK_API_VERSION_1_2) continue;

            const auto families = find_queue_families(candidate);
            if (!families.complete() || !device_supports_swapchain(candidate)) continue;
            const auto support = query_swapchain_support(candidate);
            if (support.formats.empty() || support.present_modes.empty()) continue;

            std::uint64_t score = properties.limits.maxComputeSharedMemorySize;
            if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 1'000'000u;
            if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 500'000u;
            if (score > best_score) {
                best_score = score;
                physical_device = candidate;
                graphics_family = *families.graphics_compute;
                present_family = *families.present;
            }
        }

        if (physical_device == VK_NULL_HANDLE) {
            throw std::runtime_error("No Vulkan 1.2 device supports compute, graphics, and presentation.");
        }

        VkPhysicalDeviceProperties selected_properties{};
        vkGetPhysicalDeviceProperties(physical_device, &selected_properties);
        cpu_physical_device =
            selected_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU;
        startup_log(std::string{"Selected GPU: "} + selected_properties.deviceName);
    }

    void create_device() {
        const float priority = 1.0f;
        const std::set<std::uint32_t> unique_families{graphics_family, present_family};
        std::vector<VkDeviceQueueCreateInfo> queue_infos;
        queue_infos.reserve(unique_families.size());
        for (const auto family : unique_families) {
            queue_infos.push_back(VkDeviceQueueCreateInfo{
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = family,
                .queueCount = 1,
                .pQueuePriorities = &priority,
            });
        }

        const std::array extensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        const VkPhysicalDeviceFeatures features{};
        const VkDeviceCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .queueCreateInfoCount = static_cast<std::uint32_t>(queue_infos.size()),
            .pQueueCreateInfos = queue_infos.data(),
            .enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()),
            .ppEnabledExtensionNames = extensions.data(),
            .pEnabledFeatures = &features,
        };
        check_vk(vkCreateDevice(physical_device, &create_info, nullptr, &device), "vkCreateDevice");
        vkGetDeviceQueue(device, graphics_family, 0, &graphics_queue);
        vkGetDeviceQueue(device, present_family, 0, &present_queue);
    }

    void create_command_pool() {
        const VkCommandPoolCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = graphics_family,
        };
        check_vk(vkCreateCommandPool(device, &create_info, nullptr, &command_pool), "vkCreateCommandPool");
    }

    std::uint32_t find_memory_type(const std::uint32_t type_bits, const VkMemoryPropertyFlags properties) const {
        VkPhysicalDeviceMemoryProperties memory_properties{};
        vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);
        for (std::uint32_t index = 0; index < memory_properties.memoryTypeCount; ++index) {
            if ((type_bits & (1u << index)) != 0 &&
                (memory_properties.memoryTypes[index].propertyFlags & properties) == properties) {
                return index;
            }
        }
        throw std::runtime_error("No compatible Vulkan memory type was found.");
    }

    Buffer create_buffer(const VkDeviceSize size, const VkBufferUsageFlags usage,
                         const VkMemoryPropertyFlags properties) {
        Buffer buffer{.size = size};
        const VkBufferCreateInfo buffer_info{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = size,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        check_vk(vkCreateBuffer(device, &buffer_info, nullptr, &buffer.handle), "vkCreateBuffer");

        try {
            VkMemoryRequirements requirements{};
            vkGetBufferMemoryRequirements(device, buffer.handle, &requirements);
            const VkMemoryAllocateInfo allocation_info{
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = requirements.size,
                .memoryTypeIndex = find_memory_type(requirements.memoryTypeBits, properties),
            };
            check_vk(vkAllocateMemory(device, &allocation_info, nullptr, &buffer.memory), "vkAllocateMemory");
            check_vk(vkBindBufferMemory(device, buffer.handle, buffer.memory, 0), "vkBindBufferMemory");
        } catch (...) {
            if (buffer.memory != VK_NULL_HANDLE) {
                vkFreeMemory(device, buffer.memory, nullptr);
            }
            vkDestroyBuffer(device, buffer.handle, nullptr);
            throw;
        }
        return buffer;
    }

    void destroy_buffer(Buffer& buffer) const {
        if (device == VK_NULL_HANDLE) return;
        if (buffer.handle != VK_NULL_HANDLE) vkDestroyBuffer(device, buffer.handle, nullptr);
        if (buffer.memory != VK_NULL_HANDLE) vkFreeMemory(device, buffer.memory, nullptr);
        buffer = {};
    }

    void create_buffers() {
        constexpr VkDeviceSize cell_size = sizeof(std::uint32_t) * 4u;
        const auto cell_count = static_cast<VkDeviceSize>(config.grid_width) * config.grid_height;
        const auto cells_size = cell_count * cell_size;
        const auto light_size = cell_count * sizeof(std::uint32_t);        const auto tile_columns = divide_round_up(config.grid_width, 8u);
        const auto tile_rows = divide_round_up(config.grid_height, 8u);
        const auto tile_count = static_cast<VkDeviceSize>(tile_columns) * tile_rows;
        const auto tile_size = tile_count * sizeof(std::uint32_t) * 4u;
        const auto chunk_count = static_cast<VkDeviceSize>(divide_round_up(tile_columns, 8u)) *
                                  divide_round_up(tile_rows, 8u);
        const auto chunk_size = chunk_count * sizeof(std::uint32_t) * 4u;
const auto storage_usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        for (auto& buffer : cell_buffers) {
            buffer = create_buffer(cells_size, storage_usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        }
        scene_staging_buffer = create_buffer(cells_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        map_snapshot_buffer = create_buffer(cells_size, storage_usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        sunlight_buffer = create_buffer(light_size, storage_usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        actor_buffer = create_buffer(sizeof(std::uint32_t) * 20u, storage_usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        tile_buffer = create_buffer(tile_size, storage_usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        chunk_buffer = create_buffer(chunk_size, storage_usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        conservation_buffer = create_buffer(sizeof(std::uint32_t) * debug_stat_word_count, storage_usage,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        rainfall_buffer = create_buffer(
            static_cast<VkDeviceSize>(config.grid_width) * sizeof(std::uint32_t),
            storage_usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        const auto ui_text_size = static_cast<VkDeviceSize>(ui::text_storage.size() * sizeof(std::uint32_t));
        ui_text_buffer = create_buffer(ui_text_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        designer_buffer = create_buffer(
            static_cast<VkDeviceSize>(designer_grid_cell_count * sizeof(std::uint32_t)),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        void* mapped = nullptr;
        check_vk(vkMapMemory(device, ui_text_buffer.memory, 0, ui_text_buffer.size, 0, &mapped),
                 "vkMapMemory(ui text)");
        std::memcpy(mapped, ui::text_storage.data(), ui::text_storage.size() * sizeof(std::uint32_t));
        vkUnmapMemory(device, ui_text_buffer.memory);
    }

    void create_descriptor_layouts() {
        const std::array bindings{
            VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            },
            VkDescriptorSetLayoutBinding{
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            },
            VkDescriptorSetLayoutBinding{
                .binding = 2,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            },
            VkDescriptorSetLayoutBinding{
                .binding = 3,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            },
            VkDescriptorSetLayoutBinding{
                .binding = 4,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            },
            VkDescriptorSetLayoutBinding{
                .binding = 5,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            },
            VkDescriptorSetLayoutBinding{
                .binding = 6,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
            VkDescriptorSetLayoutBinding{
                .binding = 7,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            },
            VkDescriptorSetLayoutBinding{
                .binding = 8,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
            VkDescriptorSetLayoutBinding{
                .binding = 9,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
            VkDescriptorSetLayoutBinding{
                .binding = 10,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            },
        };
        const VkDescriptorSetLayoutCreateInfo layout_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = static_cast<std::uint32_t>(bindings.size()),
            .pBindings = bindings.data(),
        };
        check_vk(vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &descriptor_set_layout),
                 "vkCreateDescriptorSetLayout");

        const VkPushConstantRange compute_push{
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset = 0,
            .size = sizeof(ActorPush),
        };
        const VkPipelineLayoutCreateInfo compute_layout_info{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &descriptor_set_layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &compute_push,
        };
        check_vk(vkCreatePipelineLayout(device, &compute_layout_info, nullptr, &compute_pipeline_layout),
                 "vkCreatePipelineLayout(compute)");

        const VkPushConstantRange graphics_push{
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof(RenderPush),
        };
        const VkPipelineLayoutCreateInfo graphics_layout_info{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &descriptor_set_layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &graphics_push,
        };
        check_vk(vkCreatePipelineLayout(device, &graphics_layout_info, nullptr, &graphics_pipeline_layout),
                 "vkCreatePipelineLayout(graphics)");
    }

    void create_descriptors() {
        const VkDescriptorPoolSize pool_size{
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 22,
        };
        const VkDescriptorPoolCreateInfo pool_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 2,
            .poolSizeCount = 1,
            .pPoolSizes = &pool_size,
        };
        check_vk(vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptor_pool),
                 "vkCreateDescriptorPool");

        const std::array layouts{descriptor_set_layout, descriptor_set_layout};
        const VkDescriptorSetAllocateInfo allocate_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = descriptor_pool,
            .descriptorSetCount = static_cast<std::uint32_t>(layouts.size()),
            .pSetLayouts = layouts.data(),
        };
        check_vk(vkAllocateDescriptorSets(device, &allocate_info, descriptor_sets.data()),
                 "vkAllocateDescriptorSets");

        for (std::uint32_t index = 0; index < 2; ++index) {
            const VkDescriptorBufferInfo current_info{cell_buffers[index].handle, 0, cell_buffers[index].size};
            const VkDescriptorBufferInfo next_info{cell_buffers[index ^ 1u].handle, 0, cell_buffers[index ^ 1u].size};
            const VkDescriptorBufferInfo light_info{sunlight_buffer.handle, 0, sunlight_buffer.size};
            const VkDescriptorBufferInfo actor_info{actor_buffer.handle, 0, actor_buffer.size};
            const VkDescriptorBufferInfo tile_info{tile_buffer.handle, 0, tile_buffer.size};
            const VkDescriptorBufferInfo conservation_info{conservation_buffer.handle, 0, conservation_buffer.size};
            const VkDescriptorBufferInfo chunk_info{chunk_buffer.handle, 0, chunk_buffer.size};
            const VkDescriptorBufferInfo ui_text_info{ui_text_buffer.handle, 0, ui_text_buffer.size};
            const VkDescriptorBufferInfo map_info{map_snapshot_buffer.handle, 0, map_snapshot_buffer.size};
            const VkDescriptorBufferInfo designer_info{designer_buffer.handle, 0, designer_buffer.size};
            const VkDescriptorBufferInfo rainfall_info{
                rainfall_buffer.handle, 0, rainfall_buffer.size};
            const std::array writes{
                VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = descriptor_sets[index],
                    .dstBinding = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .pBufferInfo = &current_info,
                },
                VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = descriptor_sets[index],
                    .dstBinding = 1,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .pBufferInfo = &next_info,
                },
                VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = descriptor_sets[index],
                    .dstBinding = 2,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .pBufferInfo = &light_info,
                },
                VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = descriptor_sets[index],
                    .dstBinding = 3,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .pBufferInfo = &actor_info,
                },
                VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = descriptor_sets[index],
                    .dstBinding = 4,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .pBufferInfo = &tile_info,
                },
                VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = descriptor_sets[index],
                    .dstBinding = 5,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .pBufferInfo = &conservation_info,
                },
                VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = descriptor_sets[index],
                    .dstBinding = 6,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .pBufferInfo = &ui_text_info,
                },
                VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = descriptor_sets[index],
                    .dstBinding = 7,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .pBufferInfo = &chunk_info,
                },
                VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = descriptor_sets[index],
                    .dstBinding = 8,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .pBufferInfo = &map_info,
                },
                VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = descriptor_sets[index],
                    .dstBinding = 9,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .pBufferInfo = &designer_info,
                },
                VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = descriptor_sets[index],
                    .dstBinding = 10,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .pBufferInfo = &rainfall_info,
                },
            };
            vkUpdateDescriptorSets(device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }
    }

    VkShaderModule create_shader_module(const std::string_view filename) const {
        const auto code = read_spirv(executable_directory() / "shaders" / filename);
        const VkShaderModuleCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = code.size() * sizeof(std::uint32_t),
            .pCode = code.data(),
        };
        VkShaderModule module{};
        check_vk(vkCreateShaderModule(device, &create_info, nullptr, &module), "vkCreateShaderModule");
        return module;
    }

    VkPipeline create_compute_pipeline(const std::string_view shader_name,
                                       const VkPipelineCreateFlags flags = 0) const {
        const std::string label{shader_name};
        startup_log(std::string{"  compute: "} + label + " [loading SPIR-V]");
        const auto module = create_shader_module(shader_name);
        startup_log(std::string{"  compute: "} + label + " [creating pipeline]");
        const VkPipelineShaderStageCreateInfo stage_info{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = module,
            .pName = "main",
        };
        const VkComputePipelineCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .flags = flags,
            .stage = stage_info,
            .layout = compute_pipeline_layout,
        };
        VkPipeline pipeline{};
        const auto result = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &create_info, nullptr, &pipeline);
        vkDestroyShaderModule(device, module, nullptr);
        check_vk(result, "vkCreateComputePipelines");
        startup_log(std::string{"  compute: "} + label + " [ready]");
        return pipeline;
    }

    void create_compute_pipelines() {
        reset_pipeline = create_compute_pipeline("reset.comp.spv");
        paint_pipeline = create_compute_pipeline("paint.comp.spv");
        sunlight_pipeline = create_compute_pipeline("sunlight.comp.spv");
        tile_pipeline = create_compute_pipeline("tiles.comp.spv");
        chunk_pipeline = create_compute_pipeline("chunks.comp.spv");
        copy_cells_pipeline = create_compute_pipeline("copy_cells.comp.spv");
        chemistry_pipeline = create_compute_pipeline("chemistry.comp.spv");
        conservation_corrections_pipeline =
            create_compute_pipeline("conservation_corrections.comp.spv");
        bee_movement_pipeline = create_compute_pipeline("bee_move.comp.spv");
        rainfall_pipeline = create_compute_pipeline("rainfall.comp.spv");
        macro_movement_pipeline = create_compute_pipeline("macro_move.comp.spv");
        movement_pipeline = create_compute_pipeline("move.comp.spv");
        actor_pipeline = create_compute_pipeline("actor.comp.spv");
        debug_stats_pipeline = create_compute_pipeline("debug_stats.comp.spv");
    }

    void create_frames() {
        frames.resize(config.frames_in_flight);
        std::vector<VkCommandBuffer> command_buffers(frames.size());
        const VkCommandBufferAllocateInfo allocate_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = static_cast<std::uint32_t>(command_buffers.size()),
        };
        check_vk(vkAllocateCommandBuffers(device, &allocate_info, command_buffers.data()),
                 "vkAllocateCommandBuffers");

        const VkSemaphoreCreateInfo semaphore_info{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        const VkFenceCreateInfo fence_info{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };
        for (std::size_t index = 0; index < frames.size(); ++index) {
            frames[index].command_buffer = command_buffers[index];
            check_vk(vkCreateSemaphore(device, &semaphore_info, nullptr, &frames[index].image_available),
                     "vkCreateSemaphore(image_available)");
            check_vk(vkCreateSemaphore(device, &semaphore_info, nullptr, &frames[index].render_finished),
                     "vkCreateSemaphore(render_finished)");
            check_vk(vkCreateFence(device, &fence_info, nullptr, &frames[index].fence), "vkCreateFence");
        }
    }

    VkSurfaceFormatKHR choose_surface_format(const std::vector<VkSurfaceFormatKHR>& formats) const {
        if (formats.size() == 1 && formats.front().format == VK_FORMAT_UNDEFINED) {
            return VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
        }
        const auto preferred = std::ranges::find_if(formats, [](const VkSurfaceFormatKHR& format) {
            return format.format == VK_FORMAT_B8G8R8A8_SRGB &&
                   format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        });
        return preferred != formats.end() ? *preferred : formats.front();
    }

    VkCompositeAlphaFlagBitsKHR choose_composite_alpha(
        const VkCompositeAlphaFlagsKHR supported) const {
        constexpr std::array candidates{
            VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
        };
        for (const auto candidate : candidates) {
            if ((supported & candidate) != 0) return candidate;
        }
        throw std::runtime_error("The surface reports no supported composite-alpha mode.");
    }

    VkPresentModeKHR choose_present_mode(const std::vector<VkPresentModeKHR>& modes) const {
        // Mailbox decouples queueing from the compositor's FIFO wait without
        // tearing.  The fixed 60 Hz simulation and CPU presentation limiter
        // still own cadence; FIFO is the portable fallback.
        const auto mailbox = std::ranges::find(modes, VK_PRESENT_MODE_MAILBOX_KHR);
        if (mailbox != modes.end()) return VK_PRESENT_MODE_MAILBOX_KHR;
        const auto fifo = std::ranges::find(modes, VK_PRESENT_MODE_FIFO_KHR);
        return fifo != modes.end() ? VK_PRESENT_MODE_FIFO_KHR : modes.front();
    }

    VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR& capabilities,
                             const std::uint32_t requested_width,
                             const std::uint32_t requested_height) const {
        if (capabilities.currentExtent.width != (std::numeric_limits<std::uint32_t>::max)()) {
            return capabilities.currentExtent;
        }
        return VkExtent2D{
            std::clamp(requested_width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            std::clamp(requested_height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height),
        };
    }

    void create_swapchain_resources(const std::uint32_t requested_width,
                                    const std::uint32_t requested_height) {
        const auto support = query_swapchain_support(physical_device);
        const auto surface_format = choose_surface_format(support.formats);
        const auto present_mode = choose_present_mode(support.present_modes);
        const auto extent = choose_extent(support.capabilities, requested_width, requested_height);

        std::uint32_t image_count = support.capabilities.minImageCount + 1u;
        if (support.capabilities.maxImageCount > 0 && image_count > support.capabilities.maxImageCount) {
            image_count = support.capabilities.maxImageCount;
        }

        const std::array queue_indices{graphics_family, present_family};
        const bool capture_frames = !config.interactive_acceptance_report.empty();
        if (capture_frames &&
            (support.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) == 0u) {
            throw std::runtime_error(
                "The Vulkan surface cannot copy presented frames for interactive acceptance.");
        }
        VkSwapchainCreateInfoKHR create_info{
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = surface,
            .minImageCount = image_count,
            .imageFormat = surface_format.format,
            .imageColorSpace = surface_format.colorSpace,
            .imageExtent = extent,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                (capture_frames
                    ? static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
                    : VkImageUsageFlags{}),
            .preTransform = support.capabilities.currentTransform,
            .compositeAlpha = choose_composite_alpha(support.capabilities.supportedCompositeAlpha),
            .presentMode = present_mode,
            .clipped = VK_TRUE,
        };
        if (graphics_family != present_family) {
            create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            create_info.queueFamilyIndexCount = static_cast<std::uint32_t>(queue_indices.size());
            create_info.pQueueFamilyIndices = queue_indices.data();
        } else {
            create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        check_vk(vkCreateSwapchainKHR(device, &create_info, nullptr, &swapchain), "vkCreateSwapchainKHR");
        swapchain_format = surface_format.format;
        swapchain_extent = extent;

        check_vk(vkGetSwapchainImagesKHR(device, swapchain, &image_count, nullptr),
                 "vkGetSwapchainImagesKHR(count)");
        swapchain_images.resize(image_count);
        check_vk(vkGetSwapchainImagesKHR(device, swapchain, &image_count, swapchain_images.data()),
                 "vkGetSwapchainImagesKHR(data)");
        image_fences.assign(swapchain_images.size(), VK_NULL_HANDLE);
        if (capture_frames) {
            const auto capture_size = static_cast<VkDeviceSize>(extent.width) *
                static_cast<VkDeviceSize>(extent.height) * 4u;
            frame_capture_buffer = create_buffer(
                capture_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        }

        swapchain_views.resize(swapchain_images.size());
        for (std::size_t index = 0; index < swapchain_images.size(); ++index) {
            const VkImageViewCreateInfo view_info{
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = swapchain_images[index],
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = swapchain_format,
                .components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                               VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
                .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            };
            check_vk(vkCreateImageView(device, &view_info, nullptr, &swapchain_views[index]),
                     "vkCreateImageView");
        }

        create_render_pass();
        create_graphics_pipeline();

        framebuffers.resize(swapchain_views.size());
        for (std::size_t index = 0; index < swapchain_views.size(); ++index) {
            const VkImageView attachment = swapchain_views[index];
            const VkFramebufferCreateInfo framebuffer_info{
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass = render_pass,
                .attachmentCount = 1,
                .pAttachments = &attachment,
                .width = swapchain_extent.width,
                .height = swapchain_extent.height,
                .layers = 1,
            };
            check_vk(vkCreateFramebuffer(device, &framebuffer_info, nullptr, &framebuffers[index]),
                     "vkCreateFramebuffer");
        }
    }

    void create_render_pass() {
        const VkAttachmentDescription color_attachment{
            .format = swapchain_format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        };
        const VkAttachmentReference color_reference{
            .attachment = 0,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };
        const VkSubpassDescription subpass{
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &color_reference,
        };
        const VkSubpassDependency dependency{
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        };
        const VkRenderPassCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &color_attachment,
            .subpassCount = 1,
            .pSubpasses = &subpass,
            .dependencyCount = 1,
            .pDependencies = &dependency,
        };
        check_vk(vkCreateRenderPass(device, &create_info, nullptr, &render_pass), "vkCreateRenderPass");
    }

    void create_graphics_pipeline() {
        startup_log("  graphics: fullscreen.vert.spv + fullscreen.frag.spv");
        const auto vertex_module = create_shader_module("fullscreen.vert.spv");
        VkShaderModule fragment_module{};
        try {
            fragment_module = create_shader_module("fullscreen.frag.spv");
        } catch (...) {
            vkDestroyShaderModule(device, vertex_module, nullptr);
            throw;
        }
        const std::array stages{
            VkPipelineShaderStageCreateInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = vertex_module,
                .pName = "main",
            },
            VkPipelineShaderStageCreateInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = fragment_module,
                .pName = "main",
            },
        };
        const VkPipelineVertexInputStateCreateInfo vertex_input{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        };
        const VkPipelineInputAssemblyStateCreateInfo input_assembly{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        };
        const VkPipelineViewportStateCreateInfo viewport_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1,
        };
        const VkPipelineRasterizationStateCreateInfo rasterization{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_NONE,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .lineWidth = 1.0f,
        };
        const VkPipelineMultisampleStateCreateInfo multisampling{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        };
        const VkPipelineColorBlendAttachmentState blend_attachment{
            .blendEnable = VK_FALSE,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        };
        const VkPipelineColorBlendStateCreateInfo blend_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &blend_attachment,
        };
        const std::array dynamic_states{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        const VkPipelineDynamicStateCreateInfo dynamic_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = static_cast<std::uint32_t>(dynamic_states.size()),
            .pDynamicStates = dynamic_states.data(),
        };
        const VkGraphicsPipelineCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = static_cast<std::uint32_t>(stages.size()),
            .pStages = stages.data(),
            .pVertexInputState = &vertex_input,
            .pInputAssemblyState = &input_assembly,
            .pViewportState = &viewport_state,
            .pRasterizationState = &rasterization,
            .pMultisampleState = &multisampling,
            .pColorBlendState = &blend_state,
            .pDynamicState = &dynamic_state,
            .layout = graphics_pipeline_layout,
            .renderPass = render_pass,
            .subpass = 0,
        };
        const auto result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &create_info, nullptr,
                                                       &graphics_pipeline);
        vkDestroyShaderModule(device, fragment_module, nullptr);
        vkDestroyShaderModule(device, vertex_module, nullptr);
        check_vk(result, "vkCreateGraphicsPipelines");
    }

    void destroy_swapchain_resources() {
        if (device == VK_NULL_HANDLE) return;
        destroy_buffer(frame_capture_buffer);
        pending_frame_capture.reset();
        for (const auto framebuffer : framebuffers) {
            if (framebuffer != VK_NULL_HANDLE) vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
        framebuffers.clear();
        if (graphics_pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, graphics_pipeline, nullptr);
        graphics_pipeline = VK_NULL_HANDLE;
        if (render_pass != VK_NULL_HANDLE) vkDestroyRenderPass(device, render_pass, nullptr);
        render_pass = VK_NULL_HANDLE;
        for (const auto view : swapchain_views) {
            if (view != VK_NULL_HANDLE) vkDestroyImageView(device, view, nullptr);
        }
        swapchain_views.clear();
        swapchain_images.clear();
        image_fences.clear();
        if (swapchain != VK_NULL_HANDLE) vkDestroySwapchainKHR(device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }

    void recreate_swapchain(const std::uint32_t width, const std::uint32_t height) {
        if (width == 0 || height == 0) return;
        check_vk(vkDeviceWaitIdle(device), "vkDeviceWaitIdle(recreate swapchain)");
        destroy_swapchain_resources();
        create_swapchain_resources(width, height);
    }

    void buffer_barrier(const VkCommandBuffer command_buffer, const Buffer& buffer,
                        const VkAccessFlags source_access, const VkAccessFlags destination_access,
                        const VkPipelineStageFlags source_stage,
                        const VkPipelineStageFlags destination_stage) const {
        const VkBufferMemoryBarrier barrier{
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = source_access,
            .dstAccessMask = destination_access,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = buffer.handle,
            .offset = 0,
            .size = VK_WHOLE_SIZE,
        };
        vkCmdPipelineBarrier(command_buffer, source_stage, destination_stage, 0,
                             0, nullptr, 1, &barrier, 0, nullptr);
    }

    void copy_cell_rectangle(const VkCommandBuffer command_buffer,
                             const std::uint32_t source_set,
                             const std::uint32_t destination_set,
                             const ActiveCellDispatch rectangle) const {
        if (rectangle.width == 0u || rectangle.height == 0u ||
            destination_set != (source_set ^ 1u)) return;
        const SimulationPush copy_push{
            .width = config.grid_width,
            .height = config.grid_height,
            .brush_x = static_cast<std::int32_t>(rectangle.origin_x),
            .brush_y = static_cast<std::int32_t>(rectangle.origin_y),
            .radius = rectangle.width,
            .material = rectangle.height,
            .reserved = 0u,
        };
        bind_compute(command_buffer, copy_cells_pipeline, source_set);
        vkCmdPushConstants(command_buffer, compute_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(copy_push), &copy_push);
        vkCmdDispatch(command_buffer, divide_round_up(rectangle.width, simulation_local_size),
                      divide_round_up(rectangle.height, simulation_local_size), 1);
    }

    void record_bee_birth_pass(const VkCommandBuffer command_buffer,
                               const SimulationPush& simulation_push,
                               const ActiveCellDispatch dispatch) {
        if ((simulation_push.step & 4095u) != 0u ||
            dispatch.width == 0u || dispatch.height == 0u) return;

        const auto next_set = current_set ^ 1u;
        buffer_barrier(command_buffer, cell_buffers[current_set],
                       VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        buffer_barrier(command_buffer, cell_buffers[next_set],
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        copy_cell_rectangle(command_buffer, current_set, next_set, dispatch);
        buffer_barrier(command_buffer, cell_buffers[next_set],
                       VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        auto bee_birth_push = simulation_push;
        bee_birth_push.material = 1u;
        bind_compute(command_buffer, bee_movement_pipeline, current_set);
        vkCmdPushConstants(command_buffer, compute_pipeline_layout,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof(bee_birth_push), &bee_birth_push);
        vkCmdDispatch(command_buffer,
                      divide_round_up(dispatch.width, simulation_local_size),
                      divide_round_up(dispatch.height, simulation_local_size), 1);
        buffer_barrier(command_buffer, cell_buffers[next_set],
                       VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        buffer_barrier(command_buffer, cell_buffers[current_set],
                       VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        copy_cell_rectangle(command_buffer, next_set, current_set, dispatch);
        buffer_barrier(command_buffer, cell_buffers[current_set],
                       VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        buffer_barrier(command_buffer, chunk_buffer,
                       VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    }


    void bind_compute(const VkCommandBuffer command_buffer, const VkPipeline pipeline,
                      const std::uint32_t set_index) const {
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                compute_pipeline_layout, 0, 1, &descriptor_sets[set_index], 0, nullptr);
    }

    template <typename Recorder>
    void immediate_submit(Recorder&& recorder) {
        const VkCommandBufferAllocateInfo allocate_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        VkCommandBuffer command_buffer{};
        check_vk(vkAllocateCommandBuffers(device, &allocate_info, &command_buffer),
                 "vkAllocateCommandBuffers(scene I/O)");
        try {
            const VkCommandBufferBeginInfo begin_info{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            };
            check_vk(vkBeginCommandBuffer(command_buffer, &begin_info),
                     "vkBeginCommandBuffer(scene I/O)");
            recorder(command_buffer);
            check_vk(vkEndCommandBuffer(command_buffer), "vkEndCommandBuffer(scene I/O)");
            const VkSubmitInfo submit_info{
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .commandBufferCount = 1,
                .pCommandBuffers = &command_buffer,
            };
            check_vk(vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE),
                     "vkQueueSubmit(scene I/O)");
            check_vk(vkQueueWaitIdle(graphics_queue), "vkQueueWaitIdle(scene I/O)");
        } catch (...) {
            vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
            throw;
        }
        vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
    }

    [[nodiscard]] std::filesystem::path scene_directory() const {
        return executable_directory() / "scenes";
    }

    [[nodiscard]] std::vector<SceneCell> download_scene_cell_prefix(
        const std::size_t cell_count) {
        const auto resident_cell_count =
            static_cast<std::size_t>(config.grid_width) * config.grid_height;
        if (cell_count > resident_cell_count)
            throw std::runtime_error("Scene prefix readback exceeds resident cell count.");
        std::vector<SceneCell> cells(cell_count);
        const auto readback_bytes = cells.size() * sizeof(SceneCell);
        immediate_submit([&](const VkCommandBuffer command_buffer) {
            buffer_barrier(command_buffer, cell_buffers[current_set],
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_TRANSFER_READ_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT);
            buffer_barrier(command_buffer, scene_staging_buffer,
                           VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT |
                               VK_ACCESS_TRANSFER_READ_BIT,
                           VK_ACCESS_TRANSFER_WRITE_BIT,
                           VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT);
            const VkBufferCopy copy{.size = readback_bytes};
            vkCmdCopyBuffer(command_buffer, cell_buffers[current_set].handle,
                            scene_staging_buffer.handle, 1, &copy);
            buffer_barrier(command_buffer, scene_staging_buffer,
                           VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT);
        });
        void* mapped = nullptr;
        check_vk(vkMapMemory(device, scene_staging_buffer.memory, 0,
                             readback_bytes, 0, &mapped),
                 "vkMapMemory(fill readback)");
        std::memcpy(cells.data(), mapped, readback_bytes);
        vkUnmapMemory(device, scene_staging_buffer.memory);
        return cells;
    }

    [[nodiscard]] std::vector<SceneCell> download_scene_cells() {
        return download_scene_cell_prefix(
            static_cast<std::size_t>(config.grid_width) * config.grid_height);
    }

    [[nodiscard]] std::vector<SceneCell> download_map_snapshot_cells() {
        std::vector<SceneCell> cells(
            static_cast<std::size_t>(config.grid_width) * config.grid_height);
        immediate_submit([&](const VkCommandBuffer command_buffer) {
            buffer_barrier(command_buffer, map_snapshot_buffer,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
                           VK_ACCESS_TRANSFER_READ_BIT,
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                               VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT);
            buffer_barrier(command_buffer, scene_staging_buffer,
                           VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT |
                               VK_ACCESS_TRANSFER_READ_BIT,
                           VK_ACCESS_TRANSFER_WRITE_BIT,
                           VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT);
            const VkBufferCopy copy{.size = scene_staging_buffer.size};
            vkCmdCopyBuffer(command_buffer, map_snapshot_buffer.handle,
                            scene_staging_buffer.handle, 1, &copy);
            buffer_barrier(command_buffer, scene_staging_buffer,
                           VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT);
            buffer_barrier(command_buffer, map_snapshot_buffer,
                           VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });
        void* mapped = nullptr;
        check_vk(vkMapMemory(device, scene_staging_buffer.memory, 0,
                             scene_staging_buffer.size, 0, &mapped),
                 "vkMapMemory(map snapshot readback)");
        std::memcpy(cells.data(), mapped, cells.size() * sizeof(SceneCell));
        vkUnmapMemory(device, scene_staging_buffer.memory);
        return cells;
    }

    using ActorStateReadback = WorldSaveActorState;
    static_assert(sizeof(ActorStateReadback) == sizeof(std::uint32_t) * 20u);

    [[nodiscard]] ActorStateReadback download_actor_state() {
        ActorStateReadback actor{};
        immediate_submit([&](const VkCommandBuffer command_buffer) {
            buffer_barrier(command_buffer, actor_buffer,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_TRANSFER_READ_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT);
            const VkBufferCopy copy{.size = actor_buffer.size};
            vkCmdCopyBuffer(command_buffer, actor_buffer.handle,
                            scene_staging_buffer.handle, 1, &copy);
            buffer_barrier(command_buffer, scene_staging_buffer,
                           VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT);
        });
        void* mapped = nullptr;
        check_vk(vkMapMemory(device, scene_staging_buffer.memory, 0,
                             actor_buffer.size, 0, &mapped),
                 "vkMapMemory(actor readback)");
        std::memcpy(&actor, mapped, sizeof(actor));
        vkUnmapMemory(device, scene_staging_buffer.memory);
        return actor;
    }

    void upload_actor_state(const ActorStateReadback& actor) {
        void* mapped = nullptr;
        check_vk(vkMapMemory(device, scene_staging_buffer.memory, 0,
                             actor_buffer.size, 0, &mapped),
                 "vkMapMemory(actor upload)");
        std::memcpy(mapped, &actor, sizeof(actor));
        vkUnmapMemory(device, scene_staging_buffer.memory);

        immediate_submit([&](const VkCommandBuffer command_buffer) {
            buffer_barrier(command_buffer, scene_staging_buffer,
                           VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT |
                               VK_ACCESS_TRANSFER_WRITE_BIT,
                           VK_ACCESS_TRANSFER_READ_BIT,
                           VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT);
            buffer_barrier(command_buffer, actor_buffer,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_TRANSFER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT);
            const VkBufferCopy copy{.size = actor_buffer.size};
            vkCmdCopyBuffer(command_buffer, scene_staging_buffer.handle,
                            actor_buffer.handle, 1, &copy);
            buffer_barrier(command_buffer, actor_buffer,
                           VK_ACCESS_TRANSFER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });
    }
    struct TileStateReadback final {
        std::uint32_t material{};
        std::uint32_t occupancy{};
        std::uint32_t flags{};
        std::uint32_t counters{};
    };
    static_assert(sizeof(TileStateReadback) == 16u);

    [[nodiscard]] std::vector<TileStateReadback> download_tile_states() {
        std::vector<TileStateReadback> states(
            tile_buffer.size / sizeof(TileStateReadback));
        immediate_submit([&](const VkCommandBuffer command_buffer) {
            buffer_barrier(command_buffer, tile_buffer,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_TRANSFER_READ_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT);
            const VkBufferCopy copy{.size = tile_buffer.size};
            vkCmdCopyBuffer(command_buffer, tile_buffer.handle,
                            scene_staging_buffer.handle, 1, &copy);
            buffer_barrier(command_buffer, scene_staging_buffer,
                           VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT);
        });
        void* mapped = nullptr;
        check_vk(vkMapMemory(device, scene_staging_buffer.memory, 0,
                             tile_buffer.size, 0, &mapped),
                 "vkMapMemory(tile readback)");
        std::memcpy(states.data(), mapped, tile_buffer.size);
        vkUnmapMemory(device, scene_staging_buffer.memory);
        return states;
    }
    std::vector<std::uint32_t> flood_replace_connected(
        std::vector<SceneCell>& cells,
        const std::uint32_t start,
        const std::uint32_t target,
        const std::uint32_t replacement) {
        std::vector<std::uint32_t> queue;
        if (static_cast<std::size_t>(start) >= cells.size() || target == replacement ||
            cells[start].material != target)
            return queue;

        queue.reserve(4096u);
        cells[start] = make_fill_cell(replacement, start);
        queue.push_back(start);
        for (std::size_t head = 0u; head < queue.size(); ++head) {
            const auto index = queue[head];
            const auto x = index % config.grid_width;
            const auto y = index / config.grid_width;
            const auto enqueue = [&](const std::uint32_t candidate) {
                if (cells[candidate].material == target) {
                    cells[candidate] = make_fill_cell(replacement, candidate);
                    queue.push_back(candidate);
                }
            };
            if (x > 0u) enqueue(index - 1u);
            if (x + 1u < config.grid_width) enqueue(index + 1u);
            if (y > 0u) enqueue(index - config.grid_width);
            if (y + 1u < config.grid_height) enqueue(index + config.grid_width);
        }
        return queue;
    }

    void upload_bounded_cells(const std::span<const SceneCell> cells,
                              std::vector<std::uint32_t> indices,
                              const std::string_view operation) {
        if (indices.empty()) return;
        std::ranges::sort(indices);
        indices.erase(std::unique(indices.begin(), indices.end()), indices.end());

        std::vector<SceneCell> payload;
        std::vector<VkBufferCopy> regions;
        payload.reserve(indices.size());
        regions.reserve(indices.size());
        for (std::size_t index = 0u; index < indices.size(); ++index) {
            payload.push_back(cells[indices[index]]);
            const auto source_offset =
                static_cast<VkDeviceSize>(index * sizeof(SceneCell));
            const auto destination_offset =
                static_cast<VkDeviceSize>(indices[index]) * sizeof(SceneCell);
            if (!regions.empty() && index > 0u &&
                indices[index] == indices[index - 1u] + 1u) {
                regions.back().size += sizeof(SceneCell);
            } else {
                regions.push_back({
                    .srcOffset = source_offset,
                    .dstOffset = destination_offset,
                    .size = sizeof(SceneCell),
                });
            }
        }

        const auto payload_bytes =
            static_cast<VkDeviceSize>(payload.size() * sizeof(SceneCell));
        void* mapped = nullptr;
        check_vk(vkMapMemory(device, scene_staging_buffer.memory, 0,
                             payload_bytes, 0, &mapped),
                 "vkMapMemory(bounded world edit)");
        std::memcpy(mapped, payload.data(), static_cast<std::size_t>(payload_bytes));
        vkUnmapMemory(device, scene_staging_buffer.memory);

        immediate_submit([&](const VkCommandBuffer command_buffer) {
            for (const auto& destination : cell_buffers) {
                buffer_barrier(command_buffer, destination,
                               VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
                                   VK_ACCESS_TRANSFER_READ_BIT,
                               VK_ACCESS_TRANSFER_WRITE_BIT,
                               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                   VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_PIPELINE_STAGE_TRANSFER_BIT);
                vkCmdCopyBuffer(command_buffer, scene_staging_buffer.handle,
                                destination.handle,
                                static_cast<std::uint32_t>(regions.size()),
                                regions.data());
                buffer_barrier(command_buffer, destination,
                               VK_ACCESS_TRANSFER_WRITE_BIT,
                               VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                               VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            }
            vkCmdFillBuffer(command_buffer, tile_buffer.handle, 0, tile_buffer.size, 0u);
            vkCmdFillBuffer(command_buffer, chunk_buffer.handle, 0, chunk_buffer.size, 0u);
            buffer_barrier(command_buffer, chunk_buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            buffer_barrier(command_buffer, tile_buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });
        startup_log(std::string{operation} + " used a bounded upload of " +
                    std::to_string(indices.size()) + " cells in " +
                    std::to_string(regions.size()) + " contiguous ranges.");
    }

    void fill_connected_region(SharedState& state) {
        auto cells = download_scene_cells();
        const auto cursor_x = state.last_world_cursor_x.load(std::memory_order_relaxed);
        const auto cursor_y = state.last_world_cursor_y.load(std::memory_order_relaxed);
        if (cursor_x < 0 || cursor_y < 0 ||
            cursor_x >= static_cast<std::int32_t>(config.grid_width) ||
            cursor_y >= static_cast<std::int32_t>(config.grid_height)) return;
        const auto start = static_cast<std::uint32_t>(cursor_y) * config.grid_width +
                           static_cast<std::uint32_t>(cursor_x);
        const auto target = cells[start].material;
        const auto replacement = static_cast<std::uint32_t>(Material::atmosphere);
        auto changed = flood_replace_connected(cells, start, target, replacement);
        if (changed.empty()) {
            startup_log("Air fill found no different connected cells.");
            return;
        }
        const auto changed_count = changed.size();
        upload_bounded_cells(cells, std::move(changed), "Air fill");
        startup_log("Filled connected region with Air: " + std::to_string(changed_count) + " cells.");
    }

    void place_selected_blueprint(SharedState& state) {
        const auto slot =
            state.selected_blueprint_slot.load(std::memory_order_relaxed) %
            blueprint_slot_count;
        Blueprint blueprint{};
        {
            const std::scoped_lock lock{state.blueprint_mutex};
            blueprint = state.blueprints[slot];
        }
        if (!blueprint.occupied) {
            startup_log("Blueprint placement skipped: selected slot is empty.");
            return;
        }

        const BlueprintTransform transform{
            .rotation = static_cast<BlueprintRotation>(
                state.blueprint_rotation.load(std::memory_order_relaxed) & 3u),
            .mirror_x = state.blueprint_mirror_x.load(std::memory_order_relaxed),
            .mirror_y = state.blueprint_mirror_y.load(std::memory_order_relaxed),
        };
        const auto cursor_x = state.last_world_cursor_x.load(std::memory_order_relaxed);
        const auto cursor_y = state.last_world_cursor_y.load(std::memory_order_relaxed);
        if (cursor_x < 0 || cursor_y < 0) {
            startup_log("Blueprint placement rejected outside the world.");
            return;
        }
        const auto origin = blueprint_centered_origin(
            blueprint, config.grid_width, config.grid_height,
            static_cast<std::uint32_t>(cursor_x),
            static_cast<std::uint32_t>(cursor_y), transform);
        if (!origin.has_value()) {
            startup_log("Blueprint placement rejected at the world boundary.");
            return;
        }

        // Validate the complete payload before mapping staging memory or changing
        // either resident buffer. This is the same all-or-nothing contract used
        // by the platform-neutral placement path.
        if (!blueprint_payload_valid(blueprint)) {
            startup_log("Blueprint placement rejected: invalid cell payload.");
            return;
        }

        struct BlueprintWrite final {
            std::uint32_t destination_index{};
            SceneCell cell{};
        };
        std::vector<BlueprintWrite> writes;
        writes.reserve(blueprint.cell_count());
        const auto empty = static_cast<std::uint32_t>(Material::empty);
        const bool include_empty = blueprint.kind == BlueprintKind::map_chunk;
        for (std::uint32_t y = 0u; y < blueprint.height; ++y) {
            for (std::uint32_t x = 0u; x < blueprint.width; ++x) {
                auto cell = blueprint.at(x, y);
                if (!include_empty && cell.material == empty) continue;
                const auto [destination_x, destination_y] =
                    blueprint_destination_coordinate(blueprint, transform, x, y);
                const auto world_index =
                    (origin->second + destination_y) * config.grid_width +
                    origin->first + destination_x;
                if (blueprint.kind == BlueprintKind::static_model)
                    cell = make_fill_cell(cell.material, world_index);
                writes.push_back({world_index, cell});
            }
        }
        if (writes.empty()) {
            startup_log("Blueprint placement skipped: payload has no writable cells.");
            return;
        }

        std::ranges::sort(writes, {}, &BlueprintWrite::destination_index);
        std::vector<SceneCell> payload;
        std::vector<VkBufferCopy> regions;
        payload.reserve(writes.size());
        regions.reserve(writes.size());
        for (std::size_t index = 0u; index < writes.size(); ++index) {
            payload.push_back(writes[index].cell);
            const auto source_offset =
                static_cast<VkDeviceSize>(index * sizeof(SceneCell));
            const auto destination_offset = static_cast<VkDeviceSize>(
                writes[index].destination_index) * sizeof(SceneCell);
            if (!regions.empty() && index > 0u &&
                writes[index].destination_index ==
                    writes[index - 1u].destination_index + 1u) {
                regions.back().size += sizeof(SceneCell);
            } else {
                regions.push_back({
                    .srcOffset = source_offset,
                    .dstOffset = destination_offset,
                    .size = sizeof(SceneCell),
                });
            }
        }

        const auto payload_bytes =
            static_cast<VkDeviceSize>(payload.size() * sizeof(SceneCell));
        void* mapped = nullptr;
        check_vk(vkMapMemory(device, scene_staging_buffer.memory, 0,
                             payload_bytes, 0, &mapped),
                 "vkMapMemory(blueprint upload)");
        std::memcpy(mapped, payload.data(), static_cast<std::size_t>(payload_bytes));
        vkUnmapMemory(device, scene_staging_buffer.memory);

        immediate_submit([&](const VkCommandBuffer command_buffer) {
            for (const auto& destination : cell_buffers) {
                buffer_barrier(command_buffer, destination,
                               VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
                                   VK_ACCESS_TRANSFER_READ_BIT,
                               VK_ACCESS_TRANSFER_WRITE_BIT,
                               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                   VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_PIPELINE_STAGE_TRANSFER_BIT);
                vkCmdCopyBuffer(command_buffer, scene_staging_buffer.handle,
                                destination.handle,
                                static_cast<std::uint32_t>(regions.size()),
                                regions.data());
                buffer_barrier(command_buffer, destination,
                               VK_ACCESS_TRANSFER_WRITE_BIT,
                               VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                               VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            }

            // Conservative metadata invalidation is GPU-only and cheap compared
            // with the former 236 MiB world readback/re-upload.
            vkCmdFillBuffer(command_buffer, tile_buffer.handle, 0, tile_buffer.size, 0u);
            vkCmdFillBuffer(command_buffer, chunk_buffer.handle, 0, chunk_buffer.size, 0u);
            buffer_barrier(command_buffer, chunk_buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            buffer_barrier(command_buffer, tile_buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

            vkCmdFillBuffer(command_buffer, sunlight_buffer.handle, 0,
                            sunlight_buffer.size, 0u);
            buffer_barrier(command_buffer, sunlight_buffer,
                           VK_ACCESS_TRANSFER_WRITE_BIT,
                           VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            if (config.runtime_acceptance_report.empty() &&
                config.long_cycle_acceptance_report.empty()) {
                const SimulationPush sunlight_push{
                    .width = config.grid_width,
                    .height = config.grid_height,
                    .step = simulation_step,
                    .seed = random_seed,
                    .active_mode = 0u,
                };
                bind_compute(command_buffer, sunlight_pipeline, 0u);
                vkCmdPushConstants(command_buffer, compute_pipeline_layout,
                                   VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                   sizeof(sunlight_push), &sunlight_push);
                vkCmdDispatch(command_buffer,
                              divide_round_up(config.grid_width,
                                              sunlight_local_size),
                              1, 1);
                buffer_barrier(command_buffer, sunlight_buffer,
                               VK_ACCESS_SHADER_WRITE_BIT,
                               VK_ACCESS_SHADER_READ_BIT,
                               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            }
        });
        startup_log("Placed blueprint slot " + std::to_string(slot + 1u) +
                    " with a bounded transactional upload of " +
                    std::to_string(writes.size()) + " cells.");
    }

    void upload_scene_cells(const std::span<const SceneCell> cells,
                            const bool rebuild_rain_tracker = false) {
        if (cells.size_bytes() != scene_staging_buffer.size)
            throw std::runtime_error("Scene image produced an unexpected cell count.");
        void* mapped = nullptr;
        check_vk(vkMapMemory(device, scene_staging_buffer.memory, 0,
                             scene_staging_buffer.size, 0, &mapped),
                 "vkMapMemory(scene upload)");
        std::memcpy(mapped, cells.data(), cells.size_bytes());
        vkUnmapMemory(device, scene_staging_buffer.memory);

        immediate_submit([&](const VkCommandBuffer command_buffer) {
            buffer_barrier(command_buffer, scene_staging_buffer,
                           VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT |
                               VK_ACCESS_TRANSFER_WRITE_BIT,
                           VK_ACCESS_TRANSFER_READ_BIT,
                           VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT);
            const VkBufferCopy copy{.size = scene_staging_buffer.size};
            for (const auto& destination : cell_buffers) {
                vkCmdCopyBuffer(command_buffer, scene_staging_buffer.handle,
                                destination.handle, 1, &copy);
                buffer_barrier(command_buffer, destination, VK_ACCESS_TRANSFER_WRITE_BIT,
                               VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                               VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            }
            vkCmdFillBuffer(command_buffer, rainfall_buffer.handle, 0,
                            rainfall_buffer.size, 0u);
            buffer_barrier(command_buffer, rainfall_buffer,
                           VK_ACCESS_TRANSFER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            if (rebuild_rain_tracker) {
                const SimulationPush rain_push{
                    .width = config.grid_width,
                    .height = config.grid_height,
                    .step = simulation_step,
                    .seed = random_seed,
                    .active_mode = 2u,
                };
                bind_compute(command_buffer, rainfall_pipeline, current_set);
                vkCmdPushConstants(command_buffer, compute_pipeline_layout,
                                   VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                   sizeof(rain_push), &rain_push);
                vkCmdDispatch(command_buffer,
                              divide_round_up(config.grid_width, 64u),
                              config.grid_height, 1);
                buffer_barrier(command_buffer, rainfall_buffer,
                               VK_ACCESS_SHADER_WRITE_BIT,
                               VK_ACCESS_SHADER_READ_BIT |
                                   VK_ACCESS_SHADER_WRITE_BIT,
                               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            }
            vkCmdFillBuffer(command_buffer, tile_buffer.handle, 0, tile_buffer.size, 0u);
            vkCmdFillBuffer(command_buffer, chunk_buffer.handle, 0, chunk_buffer.size, 0u);
            buffer_barrier(command_buffer, chunk_buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            buffer_barrier(command_buffer, tile_buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            vkCmdFillBuffer(command_buffer, conservation_buffer.handle, 0,
                            conservation_buffer.size, 0u);
            buffer_barrier(command_buffer, conservation_buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

            vkCmdFillBuffer(command_buffer, sunlight_buffer.handle, 0,
                            sunlight_buffer.size, 0u);
            buffer_barrier(command_buffer, sunlight_buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
                           VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            const SimulationPush sunlight_push{
                .width = config.grid_width,
                .height = config.grid_height,
                .step = 0u,
                .seed = random_seed,
                .active_mode = 0u,
            };
            if (config.runtime_acceptance_report.empty()) {
                bind_compute(command_buffer, sunlight_pipeline, 0u);
                vkCmdPushConstants(command_buffer, compute_pipeline_layout,
                                   VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                   sizeof(sunlight_push), &sunlight_push);
                vkCmdDispatch(command_buffer,
                              divide_round_up(config.grid_width, sunlight_local_size), 1, 1);
                buffer_barrier(command_buffer, sunlight_buffer, VK_ACCESS_SHADER_WRITE_BIT,
                               VK_ACCESS_SHADER_READ_BIT,
                               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            }

            buffer_barrier(command_buffer, cell_buffers[0],
                           VK_ACCESS_TRANSFER_WRITE_BIT |
                               VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_TRANSFER_READ_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT |
                               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT);
            buffer_barrier(command_buffer, map_snapshot_buffer,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
                           VK_ACCESS_TRANSFER_WRITE_BIT,
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                               VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT);
            const VkBufferCopy snapshot_copy{.size = map_snapshot_buffer.size};
            vkCmdCopyBuffer(command_buffer, cell_buffers[0].handle,
                            map_snapshot_buffer.handle, 1, &snapshot_copy);
            buffer_barrier(command_buffer, map_snapshot_buffer,
                           VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            buffer_barrier(command_buffer, cell_buffers[0],
                           VK_ACCESS_TRANSFER_READ_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });
        current_set = 0u;
        simulation_step = 0u;
        debug_sample_frame = 0u;
        map_snapshot_step = 0u;
        map_snapshot_slice = 0u;
        map_was_visible = false;
        needs_reset = false;
    }

    void upload_acceptance_cell_prefix(const std::span<const SceneCell> cells) {
        if (cells.empty() || cells.size_bytes() > scene_staging_buffer.size)
            throw std::runtime_error("Acceptance fixture produced an invalid cell prefix.");
        void* mapped = nullptr;
        check_vk(vkMapMemory(device, scene_staging_buffer.memory, 0,
                             cells.size_bytes(), 0, &mapped),
                 "vkMapMemory(acceptance prefix upload)");
        std::memcpy(mapped, cells.data(), cells.size_bytes());
        vkUnmapMemory(device, scene_staging_buffer.memory);

        immediate_submit([&](const VkCommandBuffer command_buffer) {
            buffer_barrier(command_buffer, scene_staging_buffer,
                           VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT |
                               VK_ACCESS_TRANSFER_WRITE_BIT,
                           VK_ACCESS_TRANSFER_READ_BIT,
                           VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT);
            const VkBufferCopy copy{.size = cells.size_bytes()};
            for (const auto& destination : cell_buffers) {
                vkCmdCopyBuffer(command_buffer, scene_staging_buffer.handle,
                                destination.handle, 1, &copy);
                buffer_barrier(command_buffer, destination,
                               VK_ACCESS_TRANSFER_WRITE_BIT,
                               VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                               VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            }
            vkCmdFillBuffer(command_buffer, rainfall_buffer.handle, 0,
                            rainfall_buffer.size, 0u);
            vkCmdFillBuffer(command_buffer, tile_buffer.handle, 0,
                            tile_buffer.size, 0u);
            vkCmdFillBuffer(command_buffer, chunk_buffer.handle, 0,
                            chunk_buffer.size, 0u);
            vkCmdFillBuffer(command_buffer, conservation_buffer.handle, 0,
                            conservation_buffer.size, 0u);
            for (const auto* buffer :
                 std::array{&rainfall_buffer, &tile_buffer, &chunk_buffer,
                            &conservation_buffer}) {
                buffer_barrier(command_buffer, *buffer,
                               VK_ACCESS_TRANSFER_WRITE_BIT,
                               VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                               VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            }
        });
        current_set = 0u;
    }

    [[nodiscard]] std::uint32_t authored_map_origin_x() const noexcept {
        return authored_scene_origin_x(config.grid_width);
    }

    [[nodiscard]] std::uint32_t authored_map_origin_y() const noexcept {
        return authored_scene_origin_y(config.grid_height);
    }

    [[nodiscard]] bool import_authored_scene_ppm(const std::uint32_t scene_index) {
        const auto scene = static_cast<Scene>(scene_index % scene_count);
        const auto path = scene_image_path(scene_directory(), scene);
        const auto map_width = (std::min)(config.grid_width, pre_expansion_world_width);
        const auto map_height = (std::min)(config.grid_height, pre_expansion_world_height);
        std::vector<SceneCell> map_cells(static_cast<std::size_t>(map_width) * map_height);
        std::string error;
        if (!load_scene_ppm(path, scene, map_width, map_height, map_cells, error)) {
            startup_log("Scene image load skipped: " + error);
            return false;
        }

        std::vector<SceneCell> world_cells(
            static_cast<std::size_t>(config.grid_width) * config.grid_height);
        for (std::size_t index = 0u; index < world_cells.size(); ++index)
            world_cells[index] = make_fill_cell(
                static_cast<std::uint32_t>(Material::atmosphere),
                static_cast<std::uint32_t>(index));

        const auto origin_x = authored_map_origin_x();
        const auto origin_y = authored_map_origin_y();
        for (std::uint32_t y = 0u; y < map_height; ++y) {
            for (std::uint32_t x = 0u; x < map_width; ++x) {
                auto cell = map_cells[static_cast<std::size_t>(y) * map_width + x];
                if (cell.material == static_cast<std::uint32_t>(Material::bee))
                    cell.aux |= bee_authored_home_slot_bit;
                const auto world_index = static_cast<std::size_t>(origin_y + y) *
                                         config.grid_width + origin_x + x;
                world_cells[world_index] = cell;
            }
        }

        for (std::uint32_t y = 0u; y < config.grid_height; ++y) {
            for (std::uint32_t x = 0u; x < config.grid_width; ++x) {
                const auto material = resident_substrate_material(
                    config.grid_width, config.grid_height, scene, x, y);
                if (material == Material::empty) continue;
                const auto index = static_cast<std::size_t>(y) * config.grid_width + x;
                const bool inside_authored = x >= origin_x && x < origin_x + map_width &&
                                             y >= origin_y && y < origin_y + map_height;
                // Imported non-Blank artwork owns intentional empty rooms just
                // like generated reset scenes. Blank alone inherits substrate.
                const bool authored_foundation = inside_authored &&
                    y >= origin_y + map_height - authored_scene_foundation_cells;
                if (inside_authored && scene != Scene::blank && !authored_foundation) continue;
                world_cells[index] = make_resident_substrate_cell(
                    material, static_cast<std::uint32_t>(index));
            }
        }
        upload_scene_cells(world_cells);
        std::string key_error;
        if (!write_scene_material_key(scene_directory(), key_error))
            startup_log("Scene material-key warning: " + key_error);
        startup_log("Loaded aligned 640x360 authored scene image; legacy boundary sky normalized to Air and resident geology rebuilt: " + path.string());
        return true;
    }

    void export_authored_scene_ppm(const std::uint32_t scene_index) {
        const auto world_cells = download_scene_cells();
        const auto map_width = (std::min)(config.grid_width, pre_expansion_world_width);
        const auto map_height = (std::min)(config.grid_height, pre_expansion_world_height);
        const auto origin_x = authored_map_origin_x();
        const auto origin_y = authored_map_origin_y();
        std::vector<SceneCell> map_cells(static_cast<std::size_t>(map_width) * map_height);
        for (std::uint32_t y = 0u; y < map_height; ++y) {
            const auto world_begin = world_cells.begin() + static_cast<std::ptrdiff_t>(
                static_cast<std::size_t>(origin_y + y) * config.grid_width + origin_x);
            const auto map_begin = map_cells.begin() + static_cast<std::ptrdiff_t>(
                static_cast<std::size_t>(y) * map_width);
            std::copy_n(world_begin, map_width, map_begin);
        }

        const auto scene = static_cast<Scene>(scene_index % scene_count);
        const auto path = scene_image_path(scene_directory(), scene);
        std::string error;
        if (!save_scene_ppm(path, map_width, map_height, map_cells, error))
            throw std::runtime_error("Unable to save scene image: " + error);
        if (!write_scene_material_key(scene_directory(), error))
            throw std::runtime_error("Unable to save scene material key: " + error);
        startup_log("Saved crystal-row 640x360 authored scene image: " + path.string());
    }

    [[nodiscard]] bool load_world_slot(const std::uint32_t scene_index) {
        const auto scene = static_cast<Scene>(scene_index % scene_count);
        std::vector<SceneCell> cells(
  static_cast<std::size_t>(config.grid_width) * config.grid_height);
        WorldSaveOwners owners{};
        WorldSaveMetadata metadata{};
        std::string error;
        if (!load_world(executable_directory(), config.world_size,
              config.grid_width, config.grid_height, scene,
              save_slot, cells, owners, metadata, error)) {
  startup_log("World load skipped: " + error);
  return false;
        }
        // Schema-2 persistent World saves already contain exact canonical cells
        // and current Bee ownership. Re-running the pre-PR19 single-scene image
        // migration here injected a phantom third hive at authored_map_origin,
        // then created 60 metadata-free bees that unraveled after the first tick.
        if (requires_pre_pr19_hive_migration(metadata.format_version) &&
            (scene == Scene::ecosystem || scene == Scene::sandbox)) {
            std::vector<std::uint32_t> materials(static_cast<std::size_t>(cells.size()));
            for (std::size_t index = 0u; index < cells.size(); ++index)
                materials[index] = cells[index].material;
            // Persistent schema-1 saves own both authored districts. The old
            // single-scene origin is never a legitimate persistent Bee home.
            for (const auto hive_scene : {Scene::sandbox, Scene::ecosystem}) {
                const auto district = persistent_world_district_index(hive_scene);
                normalize_pre_pr19_hives(materials, config.grid_width, config.grid_height,
                    persistent_world_district_origin_x(config.grid_width, district),
                    persistent_world_district_origin_y(config.grid_height, district), hive_scene);
            }
            for (std::size_t index = 0u; index < cells.size(); ++index) {
                const auto normalized = materials[index];
                if (cells[index].material != normalized)
                    cells[index] = make_fill_cell(normalized, static_cast<std::uint32_t>(index));
            }
            for (const auto hive_scene : {Scene::sandbox, Scene::ecosystem}) {
                const auto district = persistent_world_district_index(hive_scene);
                const auto migrated = initialize_schema1_hive_owners(cells,
                    config.grid_width, config.grid_height,
                    persistent_world_district_origin_x(config.grid_width, district),
                    persistent_world_district_origin_y(config.grid_height, district), hive_scene);
                if (!migrated.initialized) {
                    startup_log("World load rejected: schema-1 hive owners failed validation.");
                    return false;
                }
            }
        }
        const auto phantom = recover_v2527_phantom_hive(cells,
            config.grid_width, config.grid_height, scene, metadata.format_version);
        if (phantom.status == PhantomHiveRecoveryStatus::repaired ||
            phantom.status == PhantomHiveRecoveryStatus::signature_mismatch) {
            startup_log("World load recovery: " + std::string{
                phantom_hive_recovery_status_name(phantom.status)} +
                " changed=" + std::to_string(phantom.changed_cells()) +
                " bees=" + std::to_string(phantom.bee_cells) +
                " mismatches=" + std::to_string(phantom.mismatches));
        }
        tool_hive_anchor = no_tool_hive_anchor;
        const auto sandbox_district = persistent_world_district_index(Scene::sandbox);
        const auto ecosystem_district = persistent_world_district_index(Scene::ecosystem);
        const auto sandbox_queen_x =
            persistent_world_district_origin_x(config.grid_width, sandbox_district) + 512u;
        const auto sandbox_queen_y =
            persistent_world_district_origin_y(config.grid_height, sandbox_district) + 234u;
        const auto ecosystem_queen_x =
            persistent_world_district_origin_x(config.grid_width, ecosystem_district) + 512u;
        const auto ecosystem_queen_y =
            persistent_world_district_origin_y(config.grid_height, ecosystem_district) + 232u;
        for (std::size_t index = 0u; index < cells.size(); ++index) {
            if (cells[index].material !=
                static_cast<std::uint32_t>(Material::queen_bee)) continue;
            const auto x = static_cast<std::uint32_t>(index % config.grid_width);
            const auto y = static_cast<std::uint32_t>(index / config.grid_width);
            const bool authored_queen =
                (x == sandbox_queen_x && y == sandbox_queen_y) ||
                (x == ecosystem_queen_x && y == ecosystem_queen_y);
            if (!authored_queen && canonical_fix29_hive_signature_at(
                    cells, config.grid_width, config.grid_height, x, y)) {
                tool_hive_anchor = (x & 0xffffu) | ((y & 0xffffu) << 16u);
                break;
            }
        }
        upload_scene_cells(cells, true);
        if (owners.actor_present) upload_actor_state(owners.actor);
        if (!error.empty()) startup_log("World load recovery: " + error);
        startup_log("Loaded exact world save: " +
          world_save_path(executable_directory(), config.world_size,
                          scene, save_slot).string());
        return true;
    }

    void save_world_slot(const std::uint32_t scene_index) {
        const auto scene = static_cast<Scene>(scene_index % scene_count);
        const auto cells = download_scene_cells();
        const WorldSaveOwners owners{
            .actor_present = true,
            .actor = download_actor_state(),
        };
        const WorldSaveMetadata metadata{
  .world_size = config.world_size,
  .width = config.grid_width,
  .height = config.grid_height,
  .scene = scene,
        };
        std::string error;
        if (!save_world(executable_directory(), metadata, save_slot, cells, owners, error))
  throw std::runtime_error("Unable to save world: " + error);
        startup_log("Saved exact world state: " +
          world_save_path(executable_directory(), config.world_size,
                          scene, save_slot).string());
    }

    void record_reset(const VkCommandBuffer command_buffer, const std::uint32_t scene_index) {
        tool_hive_anchor = no_tool_hive_anchor;
        SimulationPush push{
            .width = config.grid_width,
            .height = config.grid_height,
            .step = 0u,
            .seed = random_seed,
            .material = scene_index % scene_count,
        };
        bind_compute(command_buffer, reset_pipeline, 0);
        vkCmdPushConstants(command_buffer, compute_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(push), &push);
        vkCmdDispatch(command_buffer, divide_round_up(config.grid_width, simulation_local_size),
                      divide_round_up(config.grid_height, simulation_local_size), 1);

        bind_compute(command_buffer, reset_pipeline, 1);
        vkCmdPushConstants(command_buffer, compute_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(push), &push);
        vkCmdDispatch(command_buffer, divide_round_up(config.grid_width, simulation_local_size),
                      divide_round_up(config.grid_height, simulation_local_size), 1);

        vkCmdFillBuffer(command_buffer, conservation_buffer.handle, 0, conservation_buffer.size, 0u);
        buffer_barrier(command_buffer, conservation_buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        vkCmdFillBuffer(command_buffer, tile_buffer.handle, 0, tile_buffer.size, 0u);
        vkCmdFillBuffer(command_buffer, chunk_buffer.handle, 0, chunk_buffer.size, 0u);
        vkCmdFillBuffer(command_buffer, sunlight_buffer.handle, 0, sunlight_buffer.size, 0u);
        vkCmdFillBuffer(command_buffer, rainfall_buffer.handle, 0,
                        rainfall_buffer.size, 0u);
        buffer_barrier(command_buffer, chunk_buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        buffer_barrier(command_buffer, tile_buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        buffer_barrier(command_buffer, sunlight_buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
                       VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        buffer_barrier(command_buffer, rainfall_buffer,
                       VK_ACCESS_TRANSFER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        for (const auto& buffer : cell_buffers) {
            buffer_barrier(command_buffer, buffer, VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        }

        const SimulationPush sunlight_push{
            .width = config.grid_width,
            .height = config.grid_height,
            .step = 0u,
            .seed = random_seed,
            .active_mode = 0u,
        };
        bind_compute(command_buffer, sunlight_pipeline, 0u);
        vkCmdPushConstants(command_buffer, compute_pipeline_layout,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof(sunlight_push), &sunlight_push);
        vkCmdDispatch(command_buffer,
                      divide_round_up(config.grid_width, sunlight_local_size), 1, 1);
        buffer_barrier(command_buffer, sunlight_buffer, VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        buffer_barrier(command_buffer, cell_buffers[0], VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_TRANSFER_READ_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT);
        buffer_barrier(command_buffer, map_snapshot_buffer,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
                       VK_ACCESS_TRANSFER_WRITE_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT);
        const VkBufferCopy snapshot_copy{.size = map_snapshot_buffer.size};
        vkCmdCopyBuffer(command_buffer, cell_buffers[0].handle,
                        map_snapshot_buffer.handle, 1, &snapshot_copy);
        buffer_barrier(command_buffer, map_snapshot_buffer,
                       VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        buffer_barrier(command_buffer, cell_buffers[0], VK_ACCESS_TRANSFER_READ_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        current_set = 0;
        simulation_step = 0;
        debug_sample_frame = 0u;
        map_snapshot_step = 0u;
        map_snapshot_slice = 0u;
        map_was_visible = false;
        needs_reset = false;
    }

    struct GridView final {
        std::uint32_t origin_x{};
        std::uint32_t origin_y{};
        std::uint32_t width{};
        std::uint32_t height{};
    };

    [[nodiscard]] GridView grid_view_from(
        const std::uint32_t requested_zoom,
        const int requested_center_x,
        const int requested_center_y,
        const bool map_view) const {
        const auto zoom = map_view
            ? std::clamp(requested_zoom, map_zoom_min, map_zoom_max)
            : std::clamp(requested_zoom, camera_zoom_min, camera_zoom_max);
        const auto visible_width = (std::min)(config.grid_width, map_view
            ? map_view_width(config.grid_width, zoom) : camera_view_width(zoom));
        const auto visible_height = (std::min)(config.grid_height, map_view
            ? map_view_height(config.grid_height, zoom) : camera_view_height(zoom));
        const auto center_x = std::clamp(
            requested_center_x, 0, static_cast<int>(config.grid_width - 1u));
        const auto center_y = std::clamp(
            requested_center_y, 0, static_cast<int>(config.grid_height - 1u));
        const auto origin_x = static_cast<std::uint32_t>(std::clamp(
            center_x - static_cast<int>(visible_width / 2u), 0,
            static_cast<int>(config.grid_width - visible_width)));
        const auto origin_y = static_cast<std::uint32_t>(std::clamp(
            center_y - static_cast<int>(visible_height / 2u), 0,
            static_cast<int>(config.grid_height - visible_height)));
        return {origin_x, origin_y, visible_width, visible_height};
    }

    [[nodiscard]] GridView camera_grid_view(const SharedState& state) const {
        return grid_view_from(
            state.camera_zoom.load(std::memory_order_relaxed),
            state.camera_center_x.load(std::memory_order_relaxed),
            state.camera_center_y.load(std::memory_order_relaxed), false);
    }

    [[nodiscard]] GridView render_grid_view(const SharedState& state) const {
        return camera_grid_view(state);
    }

    [[nodiscard]] GridView map_grid_view(const SharedState& state) const {
        return grid_view_from(
            state.map_zoom.load(std::memory_order_relaxed),
            state.map_center_x.load(std::memory_order_relaxed),
            state.map_center_y.load(std::memory_order_relaxed), true);
    }

    std::pair<std::int32_t, std::int32_t> grid_cursor(const SharedState& state) const {
        // GLFW pointer coordinates are logical window units; the swapchain may
        // be larger on high-DPI displays, so edits stay in input space.
        const auto width = (std::max)(state.window_width.load(std::memory_order_relaxed), 1u);
        const auto height = (std::max)(state.window_height.load(std::memory_order_relaxed), 1u);
        const auto layout = ui::make_layout(width, height);
        const auto view = render_grid_view(state);
        const auto viewport = ui::make_simulation_viewport(layout, view.width, view.height);
        return ui::pointer_to_grid(
            viewport, view.origin_x, view.origin_y, view.width, view.height,
            state.mouse_x.load(std::memory_order_relaxed),
            state.mouse_y.load(std::memory_order_relaxed));
    }

    void record_paint_at_grid(const VkCommandBuffer command_buffer,
                              const SharedState& state,
                              const bool erase,
                              const bool paint,
                              const std::int32_t grid_x,
                              const std::int32_t grid_y,
                              const std::optional<std::uint32_t> material_override =
                                  std::nullopt) {
        if (!erase && !paint) return;

        const auto requested_radius = state.brush_radius.load(std::memory_order_relaxed);
        const auto material = erase
            ? static_cast<std::uint32_t>(Material::oxygen)
            : material_override.value_or(
                  state.selected_material.load(std::memory_order_relaxed));
        const bool beehive = material == static_cast<std::uint32_t>(Material::beehive);
        if (beehive) {
            bool valid_home = false;
            for (std::uint32_t district = 0u; district < persistent_world_district_count; ++district) {
                const auto ox = static_cast<std::int32_t>(
                    persistent_world_district_origin_x(config.grid_width, district));
                const auto oy = static_cast<std::int32_t>(
                    persistent_world_district_origin_y(config.grid_height, district));
                valid_home = valid_home || (grid_x >= ox && grid_x < ox + 640 &&
                    grid_y >= oy && grid_y < oy + 360);
            }
            if (!valid_home || grid_x < 64 || grid_y < 64 ||
                grid_x + 64 >= static_cast<std::int32_t>(config.grid_width) ||
                grid_y + 64 >= static_cast<std::int32_t>(config.grid_height)) {
                startup_log("Beehive placement rejected: complete footprint and persistent district home required.");
                return;
            }
            const auto cleanup = [&](std::int32_t x, std::int32_t y, std::uint32_t mode) {
                const SimulationPush cleanup_push{
                    .width = config.grid_width, .height = config.grid_height,
                    .step = simulation_step, .seed = random_seed,
                    .brush_x = x, .brush_y = y, .radius = 64u,
                    .active_mode = mode,
                };
                bind_compute(command_buffer, paint_pipeline, current_set);
                vkCmdPushConstants(command_buffer, compute_pipeline_layout,
                    VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(cleanup_push), &cleanup_push);
                vkCmdDispatch(command_buffer, 1u, 1u, 1u);
                buffer_barrier(command_buffer, cell_buffers[current_set], VK_ACCESS_SHADER_WRITE_BIT,
                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            };
            if (tool_hive_anchor != no_tool_hive_anchor) {
                const auto previous_x = static_cast<std::int32_t>(tool_hive_anchor & 0xffffu);
                const auto previous_y = static_cast<std::int32_t>(tool_hive_anchor >> 16u);
                bool authored = false;
                for (std::uint32_t district = 0u; district < 2u; ++district)
                    authored = authored || (previous_x == static_cast<std::int32_t>(
                        persistent_world_district_origin_x(config.grid_width, district) + 512u) &&
                        previous_y == static_cast<std::int32_t>(
                        persistent_world_district_origin_y(config.grid_height, district) +
                        (district == 0u ? 234u : 232u)));
                if (!authored) cleanup(previous_x, previous_y, 4u);
            }
            cleanup(grid_x, grid_y, 3u);
        }
        const bool tile_mode = policy::effective_world_tile_mode(
            beehive, state.placement_mode.load(std::memory_order_relaxed) != 0u);
        const auto radius = policy::effective_world_brush_radius(
            beehive, tile_mode, requested_radius);
        const auto shape = policy::effective_world_brush_shape(
            beehive, tile_mode,
            state.brush_shape.load(std::memory_order_relaxed));
        const auto packed_material = material | (shape << 16u) | (tile_mode ? (1u << 18u) : 0u);
        if (!erase && beehive) {
            tool_hive_anchor =
                (static_cast<std::uint32_t>(grid_x) & 0xffffu) |
                ((static_cast<std::uint32_t>(grid_y) & 0xffffu) << 16u);
        }
        SimulationPush push{
            .width = config.grid_width,
            .height = config.grid_height,
            .step = simulation_step,
            .seed = random_seed,
            .brush_x = grid_x,
            .brush_y = grid_y,
            .radius = radius,
            .material = packed_material,
        };

        bind_compute(command_buffer, paint_pipeline, current_set);
        vkCmdPushConstants(command_buffer, compute_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(push), &push);
        const auto diameter = radius * 2u + 1u;
        vkCmdDispatch(command_buffer, divide_round_up(diameter, simulation_local_size),
                      divide_round_up(diameter, simulation_local_size), 1);
        buffer_barrier(command_buffer, cell_buffers[current_set], VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        buffer_barrier(command_buffer, chunk_buffer, VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    }

    void record_paint(const VkCommandBuffer command_buffer, const SharedState& state) {
        const bool erase = state.secondary_down.load(std::memory_order_relaxed);
        const bool paint = state.primary_down.load(std::memory_order_relaxed);
        if (!erase && !paint) return;
        const auto [grid_x, grid_y] = grid_cursor(state);
        record_paint_at_grid(command_buffer, state, erase, paint, grid_x, grid_y);
    }

    void record_nuke_from_space(const VkCommandBuffer command_buffer) {
        // The old action downloaded and flood-filled the complete resident
        // world on the CPU from the upper-left Atmosphere owner. The continuous
        // Cloud deck was the physical boundary the user accepted: only the
        // connected high sky above it burned. Preserve that topology without a
        // readback by dispatching only the aligned rows above the deck center.
        const auto high_sky_bottom =
            (std::min)(config.grid_height, nuke_high_sky_bottom_y);
        const SimulationPush push{
            .width = config.grid_width,
            .height = config.grid_height,
            .step = simulation_step,
            .seed = random_seed,
            .brush_x = 0,
            .brush_y = 0,
            .radius = config.grid_width,
            .material = static_cast<std::uint32_t>(Material::fire),
            .active_mode = 2u,
            .reserved = high_sky_bottom,
        };
        bind_compute(command_buffer, paint_pipeline, current_set);
        vkCmdPushConstants(command_buffer, compute_pipeline_layout,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);
        vkCmdDispatch(command_buffer,
                      divide_round_up(config.grid_width, simulation_local_size),
                      divide_round_up(high_sky_bottom, simulation_local_size), 1);
        buffer_barrier(command_buffer, cell_buffers[current_set],
                       VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        buffer_barrier(command_buffer, chunk_buffer, VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        // Nuke remains one material-edit dispatch. Reclassify exactly the
        // high-sky tile/chunk rows immediately so PAUSED and the next presented
        // frame cannot combine new Fire with stale Atmosphere ownership.
        const SimulationPush hierarchy_push{
            .width = config.grid_width,
            .height = config.grid_height,
            .step = simulation_step,
            .seed = random_seed,
            .reserved = policy::macro_packet_step_due(simulation_step) ? 2u : 0u,
        };
        const auto high_sky_tile_columns =
            divide_round_up(config.grid_width, authored_scene_foundation_cells);
        const auto high_sky_tile_rows =
            divide_round_up(high_sky_bottom, authored_scene_foundation_cells);
        bind_compute(command_buffer, tile_pipeline, current_set);
        vkCmdPushConstants(command_buffer, compute_pipeline_layout,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof(hierarchy_push), &hierarchy_push);
        vkCmdDispatch(command_buffer,
                      divide_round_up(high_sky_tile_columns, 8u),
                      divide_round_up(high_sky_tile_rows, 8u), 1);
        buffer_barrier(command_buffer, tile_buffer, VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        const auto high_sky_chunk_columns =
            divide_round_up(config.grid_width, 64u);
        const auto high_sky_chunk_rows =
            divide_round_up(high_sky_bottom, 64u);
        bind_compute(command_buffer, chunk_pipeline, current_set);
        vkCmdPushConstants(command_buffer, compute_pipeline_layout,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof(hierarchy_push), &hierarchy_push);
        vkCmdDispatch(command_buffer,
                      divide_round_up(high_sky_chunk_columns, 8u),
                      divide_round_up(high_sky_chunk_rows, 8u), 1);
        buffer_barrier(command_buffer, chunk_buffer, VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    }


    void reset_debug_stats(const VkCommandBuffer command_buffer) const {
        constexpr VkDeviceSize first_debug_word = sizeof(std::uint32_t) * 8u;
        constexpr VkDeviceSize debug_bytes = sizeof(std::uint32_t) * (debug_stat_word_count - 8u);
        vkCmdFillBuffer(command_buffer, conservation_buffer.handle,
                        first_debug_word, debug_bytes, 0u);
        buffer_barrier(command_buffer, conservation_buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    }

    void record_debug_stats(const VkCommandBuffer command_buffer, const SharedState& state,
                            const std::uint32_t movement_pair_tests) const {
        const SimulationPush push{
            .width = config.grid_width,
            .height = config.grid_height,
            .step = simulation_step,
            .seed = random_seed,
            .radius = movement_pair_tests,
            .material = state.selected_material.load(std::memory_order_relaxed),
            .active_section_x = state.active_window_origin_x.load(std::memory_order_relaxed),
            .active_section_y = state.active_window_origin_y.load(std::memory_order_relaxed),
            .active_mode = 1u,
            .reserved = (debug_sample_frame / 120u) & 15u,
        };
        bind_compute(command_buffer, debug_stats_pipeline, current_set);
        vkCmdPushConstants(command_buffer, compute_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(push), &push);
        constexpr auto sampled_cell_count =
            static_cast<std::uint32_t>(active_region_width_cells * active_region_height_cells);
        vkCmdDispatch(command_buffer, divide_round_up(sampled_cell_count, debug_stats_local_size), 1, 1);
        buffer_barrier(command_buffer, conservation_buffer, VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    }

    void record_simulation_step(const VkCommandBuffer command_buffer,
                                const SharedState& state,
                                const bool /*collect_debug_stats*/) {
        const auto active_section_x = state.active_window_origin_x.load(std::memory_order_relaxed);
        const auto active_section_y = state.active_window_origin_y.load(std::memory_order_relaxed);
        const auto active_dispatch = active_cell_dispatch(
            config.grid_width, config.grid_height,
            {active_section_x, active_section_y});
        const bool macro_step_due = policy::macro_packet_step_due(simulation_step);
        SimulationPush simulation_push{
            .width = config.grid_width,
            .height = config.grid_height,
            .step = simulation_step,
            .seed = random_seed,
            .active_section_x = active_section_x,
            .active_section_y = active_section_y,
            .active_mode = 1u,
            .reserved = macro_step_due ? 2u : 0u,
        };

        if ((simulation_step & 3u) == 0u) {
            buffer_barrier(command_buffer, sunlight_buffer, VK_ACCESS_SHADER_READ_BIT,
                           VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            bind_compute(command_buffer, sunlight_pipeline, current_set);
            vkCmdPushConstants(command_buffer, compute_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                               0, sizeof(simulation_push), &simulation_push);
            vkCmdDispatch(command_buffer, divide_round_up(active_dispatch.width, sunlight_local_size), 1, 1);
            buffer_barrier(command_buffer, sunlight_buffer, VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        }

        bind_compute(command_buffer, tile_pipeline, current_set);
        vkCmdPushConstants(command_buffer, compute_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(simulation_push), &simulation_push);
        vkCmdDispatch(command_buffer, divide_round_up(divide_round_up(active_dispatch.width, 8u), 8u),
                      divide_round_up(divide_round_up(active_dispatch.height, 8u), 8u), 1);
        buffer_barrier(command_buffer, tile_buffer, VK_ACCESS_SHADER_WRITE_BIT,
             VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        bind_compute(command_buffer, chunk_pipeline, current_set);
        vkCmdPushConstants(command_buffer, compute_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                 0, sizeof(simulation_push), &simulation_push);
        const auto chunk_columns = divide_round_up(config.grid_width, 64u);
        const auto chunk_rows = divide_round_up(config.grid_height, 64u);
        vkCmdDispatch(command_buffer, divide_round_up(chunk_columns, 8u),
            divide_round_up(chunk_rows, 8u), 1);
        buffer_barrier(command_buffer, chunk_buffer, VK_ACCESS_SHADER_WRITE_BIT,
             VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        const auto next_set = current_set ^ 1u;
        buffer_barrier(command_buffer, cell_buffers[next_set],
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        bind_compute(command_buffer, chemistry_pipeline, current_set);
        vkCmdPushConstants(command_buffer, compute_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(simulation_push), &simulation_push);
        vkCmdDispatch(command_buffer,
                      divide_round_up(active_dispatch.width, simulation_local_size),
                      divide_round_up(active_dispatch.height, simulation_local_size), 1);
        buffer_barrier(command_buffer, cell_buffers[next_set], VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        bind_compute(command_buffer, conservation_corrections_pipeline, current_set);
        vkCmdPushConstants(command_buffer, compute_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(simulation_push), &simulation_push);
        vkCmdDispatch(command_buffer,
                      divide_round_up(active_dispatch.width, simulation_local_size),
                      divide_round_up(active_dispatch.height, simulation_local_size), 1);
        buffer_barrier(command_buffer, cell_buffers[next_set], VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        buffer_barrier(command_buffer, rainfall_buffer,
                       VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        buffer_barrier(command_buffer, cell_buffers[current_set],
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        copy_cell_rectangle(command_buffer, next_set, current_set, active_dispatch);
        buffer_barrier(command_buffer, cell_buffers[next_set], VK_ACCESS_SHADER_READ_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        buffer_barrier(command_buffer, cell_buffers[current_set], VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        // Only one uint per world column is inspected here. Already-emitted
        // rain therefore continues outside the 4x4 active window without a
        // complete-world cell scan or any global Water wake-up.
        const SimulationPush rainfall_push{
            .width = config.grid_width,
            .height = config.grid_height,
            .step = simulation_step,
            .seed = random_seed,
            .active_mode = 0u,
        };
        bind_compute(command_buffer, rainfall_pipeline, current_set);
        vkCmdPushConstants(command_buffer, compute_pipeline_layout,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof(rainfall_push), &rainfall_push);
        vkCmdDispatch(command_buffer,
                      divide_round_up(config.grid_width, 64u), 1, 1);
        buffer_barrier(command_buffer, rainfall_buffer,
                       VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        buffer_barrier(command_buffer, cell_buffers[current_set],
                       VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        buffer_barrier(command_buffer, chunk_buffer,
                       VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        // Full uniform 8x8 regions use the same fall/diagonal/spread decisions
        // as cells, but transfer all 64 canonical cells in parallel. Mixed,
        // partial, structural, reacting, or half-water regions fall through to
        // the ordinary fine-grained movement passes below.
        if (macro_step_due) {
            bind_compute(command_buffer, macro_movement_pipeline, current_set);
            const std::array<std::int32_t, 6> macro_phases = (simulation_step & 1u) == 0u
      ? std::array<std::int32_t, 6>{0, 5, 1, 2, 3, 4}
      : std::array<std::int32_t, 6>{0, 5, 2, 1, 4, 3};
            const auto tile_columns = divide_round_up(active_dispatch.width, 8u);
            const auto tile_rows = divide_round_up(active_dispatch.height, 8u);
            for (std::size_t phase_index = 0; phase_index < macro_phases.size(); ++phase_index) {
      const auto phase = macro_phases[phase_index];
      const MovementPush macro_push{
          .width = config.grid_width,
          .height = config.grid_height,
          .step = simulation_step,
          .seed = random_seed,
          .phase = phase,
          .parity = static_cast<std::int32_t>(
              (simulation_step + static_cast<std::uint32_t>(phase_index)) & 1u),
          .reserved0 = 0u,
          .active_section_x = active_section_x,
          .active_section_y = active_section_y,
          .active_mode = 1u,
          .worker_count = state.section_worker_count.load(std::memory_order_relaxed),
      };
      vkCmdPushConstants(command_buffer, compute_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                         0, sizeof(macro_push), &macro_push);
      if (phase <= 2 || phase == 5) {
          vkCmdDispatch(command_buffer, tile_columns, divide_round_up(tile_rows, 2u), 1);
      } else {
          vkCmdDispatch(command_buffer, divide_round_up(tile_columns, 2u), tile_rows, 1);
      }
      buffer_barrier(command_buffer, cell_buffers[current_set], VK_ACCESS_SHADER_WRITE_BIT,
                     VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      buffer_barrier(command_buffer, tile_buffer, VK_ACCESS_SHADER_WRITE_BIT,
                     VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      buffer_barrier(command_buffer, chunk_buffer, VK_ACCESS_SHADER_WRITE_BIT,
                     VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            }
        }

        // Freeze the post-chemistry and macro-movement state for all neighborhood decisions in
        // the movement passes. Pair endpoints still use the writable current
        // buffer, while pressure, support, and bee attraction read this exact
        // immutable snapshot, eliminating cross-invocation read/write races.
        const auto snapshot_set = current_set ^ 1u;
        constexpr std::uint32_t movement_snapshot_halo = 16u;
        const auto snapshot_dispatch = expanded_cell_dispatch(
            active_dispatch, config.grid_width, config.grid_height, movement_snapshot_halo);
        buffer_barrier(command_buffer, cell_buffers[current_set], VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        buffer_barrier(command_buffer, cell_buffers[snapshot_set],
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        copy_cell_rectangle(command_buffer, current_set, snapshot_set, snapshot_dispatch);
        buffer_barrier(command_buffer, cell_buffers[current_set], VK_ACCESS_SHADER_READ_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        buffer_barrier(command_buffer, cell_buffers[snapshot_set], VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        bind_compute(command_buffer, movement_pipeline, current_set);
        // One complete fine pair schedule per fixed tick. AUX_MOVED now owns
        // single-tick liquid transactions, so replaying horizontal pairs only
        // re-spends stale frontier state and creates visible surface jitter.
        const std::array<std::int32_t, 7> phases = (simulation_step & 1u) == 0u
  ? std::array<std::int32_t, 7>{0, 1, 2, 3, 4, 5, 5}
  : std::array<std::int32_t, 7>{0, 2, 1, 4, 3, 5, 5};
        for (std::size_t phase_index = 0; phase_index < phases.size(); ++phase_index) {
            const auto phase = phases[phase_index];
            const MovementPush movement_push{
                .width = config.grid_width,
                .height = config.grid_height,
                .step = simulation_step,
                .seed = random_seed,
                .phase = phase,
                .parity = static_cast<std::int32_t>(
                    phase == 5
                        ? ((simulation_step + static_cast<std::uint32_t>(phase_index)) & 1u)
                        : ((simulation_step + static_cast<std::uint32_t>(phase)) & 1u)),
                .reserved0 = 0u,
                .reserved1 = ((simulation_step & 3u) == 0u) ? 2u : 0u,
                .active_section_x = active_section_x,
                .active_section_y = active_section_y,
                .active_mode = 1u,
                .worker_count = state.section_worker_count.load(std::memory_order_relaxed),
            };
            vkCmdPushConstants(command_buffer, compute_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                               0, sizeof(movement_push), &movement_push);
            if (phase >= 5) {
                vkCmdDispatch(command_buffer,
                              divide_round_up(divide_round_up(active_dispatch.width, 2u), simulation_local_size),
                              divide_round_up(active_dispatch.height, simulation_local_size), 1);
            } else {
                vkCmdDispatch(command_buffer, divide_round_up(active_dispatch.width, simulation_local_size),
                              divide_round_up(divide_round_up(active_dispatch.height, 2u), simulation_local_size), 1);
            }
            buffer_barrier(command_buffer, cell_buffers[current_set], VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            buffer_barrier(command_buffer, chunk_buffer, VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        }

        // A rare bounded double-buffer phase exchanges a missing Bee's Ash with
        // one local birth medium before any Bee can move. Ordinary ticks pay no
        // copy or dispatch cost.
        record_bee_birth_pass(command_buffer, simulation_push, active_dispatch);
        bind_compute(command_buffer, bee_movement_pipeline, current_set);
        // Authored bees are held out of the frozen generic material kernel by
        // the post-chemistry AUX_MOVED marker, then advanced transactionally
        // here with the current 60-slot colony and large-World target format.
        vkCmdPushConstants(command_buffer, compute_pipeline_layout,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof(simulation_push), &simulation_push);
        vkCmdDispatch(command_buffer,
                      divide_round_up(active_dispatch.width, simulation_local_size),
                      divide_round_up(active_dispatch.height, simulation_local_size), 1);
        buffer_barrier(command_buffer, cell_buffers[current_set],
                       VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        buffer_barrier(command_buffer, chunk_buffer, VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        ++simulation_step;
    }

    void record_actor(const VkCommandBuffer command_buffer, SharedState& state,
                      const bool reset_actor, const bool simulate_actor,
                      const bool consume_one_shots) {
        const auto [aim_x, aim_y] = grid_cursor(state);
        const bool fire_pressed = consume_one_shots &&
            state.fire_tool_pressed.exchange(false, std::memory_order_acq_rel);
        const bool deposit_pressed = consume_one_shots &&
            state.deposit_resource_pressed.exchange(false, std::memory_order_acq_rel);
        const bool fire = state.fire_tool.load(std::memory_order_relaxed) || fire_pressed;
        const bool deposit = state.deposit_resource.load(std::memory_order_relaxed) || deposit_pressed;
        const ActorPush push{
            .width = config.grid_width,
            .height = config.grid_height,
            .step = simulation_step,
            .seed = random_seed,
            .move_x = state.move_x.load(std::memory_order_relaxed),
            .move_y = state.jump.load(std::memory_order_relaxed)
                ? -1
                : state.move_y.load(std::memory_order_relaxed),
            .aim_x = aim_x,
            .aim_y = aim_y,
            .fire = fire ? 1u : 0u,
            .reset = reset_actor ? 1u : 0u,
            .scene = state.selected_scene.load(std::memory_order_relaxed) % scene_count,
            .deposit = deposit ? 1u : 0u,
            .simulate = simulate_actor ? 1u : 0u,
            .active_section_x = state.active_window_origin_x.load(std::memory_order_relaxed),
            .active_section_y = state.active_window_origin_y.load(std::memory_order_relaxed),
            .active_mode = 1u,
            .inventory_slot = state.selected_inventory_slot.load(std::memory_order_relaxed) %
                              player_inventory_slot_count,
        };
        bind_compute(command_buffer, actor_pipeline, current_set);
        vkCmdPushConstants(command_buffer, compute_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(push), &push);
        vkCmdDispatch(command_buffer, 1, 1, 1);
        buffer_barrier(command_buffer, cell_buffers[current_set], VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        buffer_barrier(command_buffer, chunk_buffer, VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        buffer_barrier(command_buffer, actor_buffer, VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    }

    void record_map_snapshot(const VkCommandBuffer command_buffer) {
        buffer_barrier(command_buffer, cell_buffers[current_set],
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_TRANSFER_READ_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT);
        buffer_barrier(command_buffer, map_snapshot_buffer,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
                       VK_ACCESS_TRANSFER_WRITE_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT);
        // Refresh one contiguous row band per cadence instead of copying the
        // 225 MiB Large resident field in one frame. Sixty-four bands cap the
        // per-frame Large transfer near 3.6 MiB while a four-tick cadence still
        // rolls a complete snapshot in roughly 4.3 seconds. Reset/load already
        // seed a complete valid snapshot, so rolling bands only update changes.
        constexpr std::uint32_t slice_count = 64u;
        const auto rows_per_slice = divide_round_up(config.grid_height, slice_count);
        const auto first_row = map_snapshot_slice * rows_per_slice;
        const auto row_count = (std::min)(rows_per_slice,
            first_row < config.grid_height ? config.grid_height - first_row : 0u);
        const VkDeviceSize byte_offset = static_cast<VkDeviceSize>(first_row) *
            config.grid_width * sizeof(SceneCell);
        const VkBufferCopy copy{
            .srcOffset = byte_offset,
            .dstOffset = byte_offset,
            .size = static_cast<VkDeviceSize>(row_count) * config.grid_width *
                    sizeof(SceneCell),
        };
        vkCmdCopyBuffer(command_buffer, cell_buffers[current_set].handle,
                        map_snapshot_buffer.handle, 1, &copy);
        buffer_barrier(command_buffer, map_snapshot_buffer,
                       VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        buffer_barrier(command_buffer, cell_buffers[current_set],
                       VK_ACCESS_TRANSFER_READ_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        map_snapshot_step = simulation_step;
        map_snapshot_slice = (map_snapshot_slice + 1u) % slice_count;
    }

    void record_designer_snapshot(const VkCommandBuffer command_buffer, SharedState& state) {
        if (!state.designer_dirty.exchange(false, std::memory_order_acq_rel)) return;
        std::array<std::uint32_t, designer_grid_cell_count> snapshot{};
        for (std::size_t index = 0; index < snapshot.size(); ++index)
            snapshot[index] = state.designer_cells[index].load(std::memory_order_relaxed) % material_count;
        vkCmdUpdateBuffer(command_buffer, designer_buffer.handle, 0,
                          static_cast<VkDeviceSize>(snapshot.size() * sizeof(std::uint32_t)),
                          snapshot.data());
        buffer_barrier(command_buffer, designer_buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    }

    void record_render(const VkCommandBuffer command_buffer, const std::uint32_t image_index,
                       const SharedState& state) {
        buffer_barrier(command_buffer, cell_buffers[current_set], VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        buffer_barrier(command_buffer, actor_buffer, VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        buffer_barrier(command_buffer, tile_buffer, VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        buffer_barrier(command_buffer, chunk_buffer, VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        buffer_barrier(command_buffer, conservation_buffer,
                       VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        VkClearValue clear_value{};
        clear_value.color.float32[0] = 0.02f;
        clear_value.color.float32[1] = 0.03f;
        clear_value.color.float32[2] = 0.05f;
        clear_value.color.float32[3] = 1.0f;
        const VkRenderPassBeginInfo begin_info{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = render_pass,
            .framebuffer = framebuffers[image_index],
            .renderArea = {.offset = {0, 0}, .extent = swapchain_extent},
            .clearValueCount = 1,
            .pClearValues = &clear_value,
        };
        vkCmdBeginRenderPass(command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                graphics_pipeline_layout, 0, 1, &descriptor_sets[current_set], 0, nullptr);

        const VkViewport viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(swapchain_extent.width),
            .height = static_cast<float>(swapchain_extent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        const VkRect2D scissor{.offset = {0, 0}, .extent = swapchain_extent};
        vkCmdSetViewport(command_buffer, 0, 1, &viewport);
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);

        const auto [cursor_x, cursor_y] = grid_cursor(state);
        const auto logical_width =
            (std::max)(state.window_width.load(std::memory_order_relaxed), 1u);
        const auto logical_height =
            (std::max)(state.window_height.load(std::memory_order_relaxed), 1u);
        const auto layout = ui::make_layout(logical_width, logical_height);
        const auto view = render_grid_view(state);
        const auto simulation_viewport = ui::make_simulation_viewport(
            layout, view.width, view.height);
        const auto camera_view = camera_grid_view(state);
        const auto map_view = map_grid_view(state);
        const auto map_overlay_viewport = ui::make_map_overlay_viewport(
            layout, map_view.width, map_view.height);
        const auto& input_simulation_viewport = simulation_viewport;
        const auto& input_map_overlay_viewport = map_overlay_viewport;
        const epochengine::gui_lib::Vec2 pointer{
            static_cast<float>(state.mouse_x.load(std::memory_order_relaxed)),
            static_cast<float>(state.mouse_y.load(std::memory_order_relaxed))
        };
        const bool pointer_over_map = state.map_view.load(std::memory_order_relaxed) &&
            epochengine::gui_lib::contains(input_map_overlay_viewport.rect, pointer);
        const bool pointer_over_world =
            epochengine::gui_lib::contains(input_simulation_viewport.rect, pointer) && !pointer_over_map;
        const bool inspect_visible =
            state.selected_workspace.load(std::memory_order_relaxed) % ui::workspace_tab_count != 3u &&
            state.inspect_material.load(std::memory_order_relaxed) && !pointer_over_map &&
            epochengine::gui_lib::contains(input_simulation_viewport.rect, pointer);
        const auto selected_workspace = state.selected_workspace.load(std::memory_order_relaxed) %
                                        ui::workspace_tab_count;
        const bool designer_workspace = selected_workspace == 3u;
        const auto active_selected_material = designer_workspace
            ? state.designer_selected_material.load(std::memory_order_relaxed)
            : state.selected_material.load(std::memory_order_relaxed);
        const auto active_selected_group = designer_workspace
            ? state.designer_selected_group.load(std::memory_order_relaxed)
            : state.selected_group.load(std::memory_order_relaxed);
        const auto active_hovered_group = designer_workspace
            ? state.designer_hovered_group.load(std::memory_order_relaxed)
            : state.hovered_group.load(std::memory_order_relaxed);
        const auto active_hovered_material = designer_workspace
            ? state.designer_hovered_material.load(std::memory_order_relaxed)
            : state.hovered_material.load(std::memory_order_relaxed);
        constexpr std::uint32_t nuke_warning_stage_count = 6u;
        const std::uint32_t nuke_presentations_per_stage =
            cpu_physical_device && !config.interactive_acceptance_report.empty() ? 1u : 8u;
        const auto nuke_warning_stage = nuke_flash_frames_remaining == 0u
            ? 0u
            : (std::min)(
                nuke_warning_stage_count,
                (nuke_flash_frames_remaining + nuke_presentations_per_stage - 1u) /
                    nuke_presentations_per_stage);
        const auto designer_flags =
            (state.designer_placement_mode.load(std::memory_order_relaxed) & 1u) |
            ((state.designer_brush_shape.load(std::memory_order_relaxed) & 3u) << 1u) |
            ((state.designer_mode.load(std::memory_order_relaxed) & 1u) << 3u) |
            ((state.designer_pane.load(std::memory_order_relaxed) & 1u) << 4u) |
            ((state.inventory_pane.load(std::memory_order_relaxed) & 1u) << 5u) |
            ((state.designer_zoom.load(std::memory_order_relaxed) & 0xffu) << 8u) |
            ((state.designer_brush_radius.load(std::memory_order_relaxed) & 0xffu) << 16u) |
            ((nuke_warning_stage & 0x0fu) << 28u);
        const auto selected_blueprint =
            state.selected_blueprint_slot.load(std::memory_order_relaxed) %
            blueprint_slot_count;
        std::uint32_t blueprint_flags = selected_blueprint << 4u;
        {
            const std::scoped_lock lock{state.blueprint_mutex};
            for (std::uint32_t slot = 0u; slot < blueprint_slot_count; ++slot) {
                if (state.blueprints[slot].occupied)
                    blueprint_flags |= 1u << slot;
            }
            const auto& blueprint = state.blueprints[selected_blueprint];
            if (blueprint.occupied) {
                const BlueprintTransform transform{
                    .rotation = static_cast<BlueprintRotation>(
                        state.blueprint_rotation.load(std::memory_order_relaxed) & 3u),
                    .mirror_x =
                        state.blueprint_mirror_x.load(std::memory_order_relaxed),
                    .mirror_y =
                        state.blueprint_mirror_y.load(std::memory_order_relaxed),
                };
                const auto [width, height] =
                    blueprint_transformed_extent(blueprint, transform);
                blueprint_flags |= (width & 0x7fu) << 8u;
                blueprint_flags |= (height & 0x7fu) << 16u;
                blueprint_flags |=
                    (static_cast<std::uint32_t>(blueprint.kind) & 1u) << 24u;
            }
        }
        if (state.blueprint_placement_active.load(std::memory_order_relaxed))
            blueprint_flags |= 1u << 6u;
        const bool world_beehive_selected =
            active_selected_material == static_cast<std::uint32_t>(Material::beehive);
        const bool effective_world_tile_mode = policy::effective_world_tile_mode(
            world_beehive_selected,
            state.placement_mode.load(std::memory_order_relaxed) != 0u);
        const RenderPush push{
            .grid_width = config.grid_width,
            .grid_height = config.grid_height,
            .window_width = logical_width,
            .window_height = logical_height,
            .selected_material = active_selected_material,
            .material_count = material_count,
            .cursor_x = pointer_over_world ? cursor_x : -1'000'000,
            .cursor_y = pointer_over_world ? cursor_y : -1'000'000,
            .brush_radius = [&state, designer_workspace, active_selected_material,
                             effective_world_tile_mode]() {
                if (designer_workspace)
                    return state.designer_brush_radius.load(std::memory_order_relaxed);
                return policy::effective_world_brush_radius(
                    active_selected_material == static_cast<std::uint32_t>(Material::beehive),
                    effective_world_tile_mode,
                    state.brush_radius.load(std::memory_order_relaxed));
            }(),
            .status_height = static_cast<std::uint32_t>(layout.status.size.y),
            // Existing push slot carries compact sidebar width.
            .palette_height = static_cast<std::uint32_t>(layout.status.size.x),
            .group_tabs_height = static_cast<std::uint32_t>(layout.group_tabs.size.y),
            .material_slots = material_slots_per_group,
            .frames_per_second = state.frames_per_second.load(std::memory_order_relaxed),
            .paused = state.paused.load(std::memory_order_relaxed) ? 1u : 0u,
            .presentation_limit = state.presentation_limit.load(std::memory_order_relaxed) & 3u,
            .selected_group = active_selected_group % material_group_count,
            .hovered_group = active_hovered_group,
            .hovered_material = active_hovered_material,
            .selected_scene = tool_hive_anchor,
            .group_count = material_group_count,
            .scene_count = scene_count,
            .mining_mode = state.mining_mode.load(std::memory_order_relaxed) ? 1u : 0u,
            .inspect_mode = inspect_visible ? 1u : 0u,
            .debug_mode = state.debug_visualization.load(std::memory_order_relaxed)
                ? 1u + (state.debug_page.load(std::memory_order_relaxed) & 1u)
                : 0u,
            .tile_columns = divide_round_up(config.grid_width, 8u),
            .tile_rows = divide_round_up(config.grid_height, 8u),
            .viewport_left = static_cast<std::uint32_t>(simulation_viewport.rect.position.x),
            .viewport_top = static_cast<std::uint32_t>(simulation_viewport.rect.position.y),
            .viewport_width = static_cast<std::uint32_t>(simulation_viewport.rect.size.x),
            .viewport_height = static_cast<std::uint32_t>(simulation_viewport.rect.size.y),
            .view_origin_x = view.origin_x,
            .view_origin_y = view.origin_y,
            .view_width = view.width,
            .view_height = view.height,
            .brush_shape = designer_workspace
                ? state.designer_brush_shape.load(std::memory_order_relaxed) % 4u
                : policy::effective_world_brush_shape(
                    world_beehive_selected,
                    effective_world_tile_mode,
                    state.brush_shape.load(std::memory_order_relaxed)),
            .placement_mode = designer_workspace
                ? (state.designer_placement_mode.load(std::memory_order_relaxed) & 1u)
                : (effective_world_tile_mode ? 1u : 0u),
            .active_area_count = state.active_section_count.load(std::memory_order_relaxed),
            .active_area_x = state.active_window_origin_x.load(std::memory_order_relaxed),
            .active_area_y = state.active_window_origin_y.load(std::memory_order_relaxed),
            .active_scope_mode = state.active_scope_mode.load(std::memory_order_relaxed),
            .camera_controls = state.camera_controls.load(std::memory_order_relaxed) ? 1u : 0u,
            .map_mode = state.map_view.load(std::memory_order_relaxed) ? 1u : 0u,
            .camera_origin_x = camera_view.origin_x,
            .camera_origin_y = camera_view.origin_y,
            .camera_view_width = camera_view.width,
            .camera_view_height = camera_view.height,
            .map_viewport_left = static_cast<std::uint32_t>(map_overlay_viewport.rect.position.x),
            .map_viewport_top = static_cast<std::uint32_t>(map_overlay_viewport.rect.position.y),
            .map_viewport_width = static_cast<std::uint32_t>(map_overlay_viewport.rect.size.x),
            .map_viewport_height = static_cast<std::uint32_t>(map_overlay_viewport.rect.size.y),
            .map_origin_x = map_view.origin_x,
            .map_origin_y = map_view.origin_y,
            .map_view_width = map_view.width,
            .map_view_height = map_view.height,
            .selected_inventory_slot = state.selected_inventory_slot.load(std::memory_order_relaxed) %
                                       player_inventory_slot_count,
            .selected_workspace = selected_workspace,
            // Presentation animation is simulation-time based so pause freezes
            // every material effect and reset returns every effect to frame zero.
            .render_frame = simulation_step,
            .world_time = simulation_step,
            .day_cycle_steps = sandhybrid::policy::day_cycle_steps,
            .designer_flags = designer_flags,
            .blueprint_flags = blueprint_flags,
            .framebuffer_width = swapchain_extent.width,
            .framebuffer_height = swapchain_extent.height,
        };
        vkCmdPushConstants(command_buffer, graphics_pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(push), &push);
        vkCmdDraw(command_buffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(command_buffer);
    }

    void record_frame_capture(const VkCommandBuffer command_buffer,
                              const std::uint32_t image_index) const {
        if (frame_capture_buffer.handle == VK_NULL_HANDLE) return;
        const VkImageSubresourceRange color_range{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0u,
            .levelCount = 1u,
            .baseArrayLayer = 0u,
            .layerCount = 1u,
        };
        const VkImageMemoryBarrier to_transfer{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = swapchain_images[image_index],
            .subresourceRange = color_range,
        };
        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
                             0u, nullptr, 0u, nullptr, 1u, &to_transfer);
        const VkBufferImageCopy copy{
            .bufferOffset = 0u,
            .bufferRowLength = 0u,
            .bufferImageHeight = 0u,
            .imageSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0u,
                .baseArrayLayer = 0u,
                .layerCount = 1u,
            },
            .imageOffset = {0, 0, 0},
            .imageExtent = {swapchain_extent.width, swapchain_extent.height, 1u},
        };
        vkCmdCopyImageToBuffer(command_buffer, swapchain_images[image_index],
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               frame_capture_buffer.handle, 1u, &copy);
        buffer_barrier(command_buffer, frame_capture_buffer,
                       VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT);
        const VkImageMemoryBarrier to_present{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .dstAccessMask = 0u,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = swapchain_images[image_index],
            .subresourceRange = color_range,
        };
        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0u,
                             0u, nullptr, 0u, nullptr, 1u, &to_present);
    }

    void write_frame_capture(const std::filesystem::path& path) {
        const bool bgra = swapchain_format == VK_FORMAT_B8G8R8A8_SRGB ||
                          swapchain_format == VK_FORMAT_B8G8R8A8_UNORM;
        const bool rgba = swapchain_format == VK_FORMAT_R8G8B8A8_SRGB ||
                          swapchain_format == VK_FORMAT_R8G8B8A8_UNORM;
        if (!bgra && !rgba)
            throw std::runtime_error("Interactive capture requires an 8-bit BGRA or RGBA swapchain.");
        if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());

        void* mapped = nullptr;
        check_vk(vkMapMemory(device, frame_capture_buffer.memory, 0,
                             frame_capture_buffer.size, 0u, &mapped),
                 "vkMapMemory(frame capture)");
        try {
            const auto width = swapchain_extent.width;
            const auto height = swapchain_extent.height;
            const auto row_stride = (width * 3u + 3u) & ~3u;
            const auto pixel_bytes = row_stride * height;
            std::vector<std::uint8_t> bmp(54u + pixel_bytes, 0u);
            const auto put_u16 = [&bmp](const std::size_t offset, const std::uint16_t value) {
                bmp[offset] = static_cast<std::uint8_t>(value);
                bmp[offset + 1u] = static_cast<std::uint8_t>(value >> 8u);
            };
            const auto put_u32 = [&bmp](const std::size_t offset, const std::uint32_t value) {
                bmp[offset] = static_cast<std::uint8_t>(value);
                bmp[offset + 1u] = static_cast<std::uint8_t>(value >> 8u);
                bmp[offset + 2u] = static_cast<std::uint8_t>(value >> 16u);
                bmp[offset + 3u] = static_cast<std::uint8_t>(value >> 24u);
            };
            bmp[0] = 'B';
            bmp[1] = 'M';
            put_u32(2u, static_cast<std::uint32_t>(bmp.size()));
            put_u32(10u, 54u);
            put_u32(14u, 40u);
            put_u32(18u, width);
            put_u32(22u, height);
            put_u16(26u, 1u);
            put_u16(28u, 24u);
            put_u32(34u, pixel_bytes);

            const auto* source = static_cast<const std::uint8_t*>(mapped);
            for (std::uint32_t y = 0u; y < height; ++y) {
                auto* destination = bmp.data() + 54u +
                    static_cast<std::size_t>(height - 1u - y) * row_stride;
                const auto* row = source + static_cast<std::size_t>(y) * width * 4u;
                for (std::uint32_t x = 0u; x < width; ++x) {
                    const auto* pixel = row + static_cast<std::size_t>(x) * 4u;
                    destination[x * 3u] = bgra ? pixel[0] : pixel[2];
                    destination[x * 3u + 1u] = pixel[1];
                    destination[x * 3u + 2u] = bgra ? pixel[2] : pixel[0];
                }
            }
            std::ofstream output{path, std::ios::binary | std::ios::trunc};
            if (!output) throw std::runtime_error("Unable to create frame capture: " + path.string());
            output.write(reinterpret_cast<const char*>(bmp.data()),
                         static_cast<std::streamsize>(bmp.size()));
            if (!output) throw std::runtime_error("Unable to write frame capture: " + path.string());
        } catch (...) {
            vkUnmapMemory(device, frame_capture_buffer.memory);
            throw;
        }
        vkUnmapMemory(device, frame_capture_buffer.memory);
    }

    bool draw_frame(SharedState& state, const std::uint32_t scheduled_simulation_ticks,
                    const bool present_frame,
                    const bool force_debug_sample = false) {
        auto& frame = frames[frame_index];
        const std::uint64_t gpu_timeout_ns = cpu_physical_device &&
            !config.interactive_acceptance_report.empty()
            ? 60'000'000'000ull : 5'000'000'000ull;
        const auto fence_result = vkWaitForFences(device, 1, &frame.fence, VK_TRUE, gpu_timeout_ns);
        if (fence_result == VK_TIMEOUT) {
            gpu_stalled = true;
            throw std::runtime_error(
                "GPU fence timed out after 5 seconds. The first simulation submission stalled; "
                "update the GPU driver and inspect the last SandHybrid startup line.");
        }
        check_vk(fence_result, "vkWaitForFences");

        const auto selected_scene = state.selected_scene.load(std::memory_order_relaxed) % scene_count;
        const bool paused = state.paused.load(std::memory_order_relaxed);
        if (state.save_scene_image.exchange(false, std::memory_order_acq_rel)) {
            if (!needs_reset) save_world_slot(selected_scene);
            else startup_log("World save skipped until the initial scene exists.");
        }
        const bool explicit_load = state.load_scene_image.exchange(false, std::memory_order_acq_rel);
        if (state.fill_region.exchange(false, std::memory_order_acq_rel)) {
            if (!needs_reset) fill_connected_region(state);
            else startup_log("Fill skipped until the initial scene exists.");
        }
        if (state.ignite_air.exchange(false, std::memory_order_acq_rel)) {
            if (!needs_reset) {
                constexpr std::uint32_t nuke_warning_stage_count = 6u;
                const std::uint32_t nuke_presentations_per_stage =
                    cpu_physical_device && !config.interactive_acceptance_report.empty() ? 1u : 8u;
                nuke_flash_frames_remaining =
                    nuke_warning_stage_count * nuke_presentations_per_stage;
                nuke_dispatch_pending = true;
                startup_log("Nuke from Space warning flash staged before GPU detonation.");
            } else {
                startup_log("Nuke from Space skipped until the initial World exists.");
            }
        }
        const bool reset_requested = needs_reset || state.reset.exchange(false, std::memory_order_acq_rel);
        bool image_loaded = false;
        if (explicit_load) {
  image_loaded = load_world_slot(selected_scene);
  if (!image_loaded) {
      const auto selected = static_cast<Scene>(selected_scene);
      if (scene_image_exists(scene_directory(), selected)) {
          startup_log("No valid world save; importing the legacy authored PPM instead.");
          image_loaded = import_authored_scene_ppm(selected_scene);
      } else {
          startup_log("No valid world save or authored PPM exists for the selected scene.");
      }
  }
        }
        if (state.blueprint_place_requested.exchange(
                false, std::memory_order_acq_rel)) {
            if (!reset_requested && !image_loaded && !needs_reset)
                place_selected_blueprint(state);
            else
                startup_log("Blueprint placement skipped across a load/reset boundary.");
        }

        std::uint32_t image_index{};
        VkResult acquire_result = VK_SUCCESS;
        if (present_frame) {
            acquire_result = vkAcquireNextImageKHR(device, swapchain, gpu_timeout_ns,
                                                   frame.image_available, VK_NULL_HANDLE,
                                                   &image_index);
            if (acquire_result == VK_TIMEOUT || acquire_result == VK_NOT_READY) {
                throw std::runtime_error("Timed out acquiring a swapchain image after 5 seconds.");
            }
            if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) return false;
            if (acquire_result != VK_SUCCESS && acquire_result != VK_SUBOPTIMAL_KHR) {
                throw_vk("vkAcquireNextImageKHR", acquire_result);
            }

            if (image_fences[image_index] != VK_NULL_HANDLE) {
                const auto image_fence_result = vkWaitForFences(
                    device, 1, &image_fences[image_index], VK_TRUE, gpu_timeout_ns);
                if (image_fence_result == VK_TIMEOUT) {
                    gpu_stalled = true;
                    throw std::runtime_error("Swapchain image fence timed out after 5 seconds.");
                }
                check_vk(image_fence_result, "vkWaitForFences(swapchain image)");
            }
            image_fences[image_index] = frame.fence;
        }

        check_vk(vkResetFences(device, 1, &frame.fence), "vkResetFences");
        check_vk(vkResetCommandBuffer(frame.command_buffer, 0), "vkResetCommandBuffer");
        const VkCommandBufferBeginInfo begin_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        check_vk(vkBeginCommandBuffer(frame.command_buffer, &begin_info), "vkBeginCommandBuffer");

        for (const auto& buffer : cell_buffers) {
            buffer_barrier(frame.command_buffer, buffer,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        }

        bool reset_actor = image_loaded;
        bool reset_this_frame = image_loaded;
        if (reset_requested && !image_loaded) {
            record_reset(frame.command_buffer, selected_scene);
            reset_actor = true;
            reset_this_frame = true;
        }
        if (policy::editor_mutation_allowed(paused, reset_this_frame)) {
            if (const auto request = consume_beehive_placement(state)) {
                record_paint_at_grid(
                    frame.command_buffer, state, false, true,
                    request->x, request->y,
                    static_cast<std::uint32_t>(Material::beehive));
            }
            record_paint(frame.command_buffer, state);
        }
        if (!reset_this_frame && nuke_dispatch_pending &&
            nuke_flash_frames_remaining == 0u) {
            record_nuke_from_space(frame.command_buffer);
            nuke_dispatch_pending = false;
            startup_log(
                "Nuke from Space committed one GPU high-sky Atmosphere-to-Fire edit above Cloud.");
        }

        const bool debug_visible = state.debug_visualization.load(std::memory_order_relaxed);
        const bool debug_region_visible = debug_visible &&
            (state.debug_page.load(std::memory_order_relaxed) & 1u) == 0u;
        const bool step_once = state.single_step.exchange(false, std::memory_order_acq_rel);
        const auto simulation_ticks = reset_this_frame ? 0u :
            (paused ? (step_once ? 1u : 0u)
                    : (std::max)(scheduled_simulation_ticks, step_once ? 1u : 0u));
        const bool run_simulation = simulation_ticks != 0u;
        bool collect_debug_stats = false;
        if (debug_region_visible && run_simulation) {
            collect_debug_stats = !debug_was_visible || (debug_sample_frame % 120u) == 0u;
            ++debug_sample_frame;
        } else if (!debug_region_visible) {
            debug_sample_frame = 0u;
        }
        if (force_debug_sample && debug_region_visible)
            collect_debug_stats = true;
        debug_was_visible = debug_region_visible;
        if (collect_debug_stats) reset_debug_stats(frame.command_buffer);
        if (run_simulation) {
            for (std::uint32_t tick = 0u; tick < simulation_ticks; ++tick) {
                const bool final_tick = tick + 1u == simulation_ticks;
                record_simulation_step(
                    frame.command_buffer, state, collect_debug_stats && final_tick);
                record_actor(frame.command_buffer, state, false, true, tick == 0u);
            }
        }
        if (reset_this_frame) {
            // Reset is a hard epoch boundary: no held/queued edit or actor action
            // may leak into the freshly rebuilt scene on the next frame.
            state.primary_down.store(false, std::memory_order_release);
            state.beehive_place_request.store(0u, std::memory_order_release);
            state.fill_region.store(false, std::memory_order_release);
            state.fill_armed.store(false, std::memory_order_release);
            state.ignite_air.store(false, std::memory_order_release);
            state.fire_tool_pressed.store(false, std::memory_order_release);
            state.deposit_resource_pressed.store(false, std::memory_order_release);
            state.blueprint_place_requested.store(false, std::memory_order_release);
            nuke_flash_frames_remaining = 0u;
            nuke_dispatch_pending = false;
        }
        if (paused) {
            // Do not queue one-shot actor/tool input for the first unpaused frame.
            state.fire_tool_pressed.exchange(false, std::memory_order_acq_rel);
            state.deposit_resource_pressed.exchange(false, std::memory_order_acq_rel);
        }
        const bool actor_action = !paused &&
            (state.fire_tool.load(std::memory_order_relaxed) ||
             state.deposit_resource.load(std::memory_order_relaxed) ||
             state.fire_tool_pressed.load(std::memory_order_acquire) ||
             state.deposit_resource_pressed.load(std::memory_order_acquire));
        const bool actor_motion = !paused &&
            (state.move_x.load(std::memory_order_relaxed) != 0 ||
             state.move_y.load(std::memory_order_relaxed) != 0 ||
             state.jump.load(std::memory_order_relaxed));
        const bool actor_simulation = run_simulation || actor_motion;
        if (!run_simulation && (reset_actor || actor_action || actor_motion))
            record_actor(frame.command_buffer, state, reset_actor, actor_simulation, true);

        if (collect_debug_stats) {
            const auto active_area_count = std::max(
                state.active_section_count.load(std::memory_order_relaxed), 1u);
            const auto resident_cells = static_cast<std::uint64_t>(config.grid_width) *
                                        static_cast<std::uint64_t>(config.grid_height);
            const auto active_cells = std::min(
                resident_cells,
                static_cast<std::uint64_t>(active_area_count) *
                    static_cast<std::uint64_t>(active_region_width_cells) *
                    static_cast<std::uint64_t>(active_region_height_cells));
            const auto tested_pairs = std::min(
                active_cells * 13u / 2u,
                static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()));
            record_debug_stats(frame.command_buffer, state,
                               static_cast<std::uint32_t>(tested_pairs));
        }
        const bool map_visible = state.map_view.load(std::memory_order_relaxed);
        constexpr std::uint32_t map_refresh_steps = 4u;
        if (map_visible && !paused && !reset_this_frame &&
            (!map_was_visible || simulation_step - map_snapshot_step >= map_refresh_steps))
            record_map_snapshot(frame.command_buffer);
        map_was_visible = map_visible;
        if (present_frame) {
            record_designer_snapshot(frame.command_buffer, state);
            record_render(frame.command_buffer, image_index, state);
            if (pending_frame_capture.has_value())
                record_frame_capture(frame.command_buffer, image_index);
        }
        check_vk(vkEndCommandBuffer(frame.command_buffer), "vkEndCommandBuffer");

        constexpr VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        const VkSubmitInfo submit_info{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = present_frame ? 1u : 0u,
            .pWaitSemaphores = present_frame ? &frame.image_available : nullptr,
            .pWaitDstStageMask = &wait_stage,
            .commandBufferCount = 1,
            .pCommandBuffers = &frame.command_buffer,
            .signalSemaphoreCount = present_frame ? 1u : 0u,
            .pSignalSemaphores = present_frame ? &frame.render_finished : nullptr,
        };
        check_vk(vkQueueSubmit(graphics_queue, 1, &submit_info, frame.fence), "vkQueueSubmit");
        if (!first_submission_logged) {
            startup_log("First GPU submission queued.");
            first_submission_logged = true;
        }

        VkResult present_result = VK_SUCCESS;
        if (present_frame) {
            if (pending_frame_capture.has_value()) {
                const auto capture_result = vkWaitForFences(
                    device, 1u, &frame.fence, VK_TRUE, gpu_timeout_ns);
                if (capture_result == VK_TIMEOUT) {
                    gpu_stalled = true;
                    throw std::runtime_error("Frame capture fence timed out.");
                }
                check_vk(capture_result, "vkWaitForFences(frame capture)");
                write_frame_capture(*pending_frame_capture);
                pending_frame_capture.reset();
            }
            const VkPresentInfoKHR present_info{
                .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &frame.render_finished,
                .swapchainCount = 1,
                .pSwapchains = &swapchain,
                .pImageIndices = &image_index,
            };
            present_result = vkQueuePresentKHR(present_queue, &present_info);
            if (present_result != VK_SUCCESS && present_result != VK_SUBOPTIMAL_KHR &&
                present_result != VK_ERROR_OUT_OF_DATE_KHR) {
                throw_vk("vkQueuePresentKHR", present_result);
            }
            if (!first_present_logged) {
                startup_log("First frame presented.");
                first_present_logged = true;
            }
            if (nuke_flash_frames_remaining != 0u)
                --nuke_flash_frames_remaining;
        }

        frame_index = (frame_index + 1u) % static_cast<std::uint32_t>(frames.size());
        return !present_frame || (acquire_result != VK_SUBOPTIMAL_KHR &&
               present_result != VK_SUBOPTIMAL_KHR &&
               present_result != VK_ERROR_OUT_OF_DATE_KHR);
    }


    [[nodiscard]] std::array<std::uint32_t, 8>
    download_conservation_counters() const {
        check_vk(vkDeviceWaitIdle(device), "vkDeviceWaitIdle(conservation log)");
        void* mapped = nullptr;
        check_vk(vkMapMemory(device, conservation_buffer.memory, 0, conservation_buffer.size, 0, &mapped),
                 "vkMapMemory(conservation)");
        std::array<std::uint32_t, 8> counters{};
        std::memcpy(counters.data(), mapped, sizeof(counters));
        vkUnmapMemory(device, conservation_buffer.memory);
        return counters;
    }

#if SANDHYBRID_ENABLE_VALIDATION
    void log_conservation_if_due(const SharedState& state) {
        if (!state.debug_visualization.load(std::memory_order_relaxed)) return;
        const auto now = std::chrono::steady_clock::now();
        if (next_conservation_log != std::chrono::steady_clock::time_point{} && now < next_conservation_log) return;
        next_conservation_log = now + std::chrono::seconds{5};

        const auto counters = download_conservation_counters();
        std::fprintf(stderr,
            "[SandHybrid conservation] created=%u destroyed=%u converted=%u boundary=%u "
            "phase=%u rebuilt=%u broken=%u errors=%u\n",
            counters[0], counters[1], counters[2], counters[3],
            counters[4], counters[5], counters[6], counters[7]);
    }
#endif

    struct RuntimeAcceptanceCheck final {
        std::string name;
        bool passed{};
        std::string details;
    };

    [[nodiscard]] static std::string json_escape(const std::string_view value) {
        std::string escaped;
        escaped.reserve(value.size());
        for (const char character : value) {
            switch (character) {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped += character; break;
            }
        }
        return escaped;
    }

    [[nodiscard]] std::vector<SceneCell> acceptance_atmosphere_world() const {
        const auto cell_count = static_cast<std::size_t>(config.grid_width) * config.grid_height;
        std::vector<SceneCell> cells(cell_count);
        for (std::size_t index = 0u; index < cells.size(); ++index) {
            cells[index] = make_fill_cell(
                static_cast<std::uint32_t>(Material::atmosphere),
                static_cast<std::uint32_t>(index));
        }
        return cells;
    }

    void run_acceptance_tile_pass(const std::int32_t active_section_x = 0,
                                  const std::int32_t active_section_y = 0,
                                  const bool translated_active_window = false) {
        immediate_submit([&](const VkCommandBuffer command_buffer) {
            const auto origin_x = translated_active_window
                ? static_cast<std::uint32_t>((std::max)(active_section_x, 0) *
                                             active_region_width_cells) : 0u;
            const auto origin_y = translated_active_window
                ? static_cast<std::uint32_t>((std::max)(active_section_y, 0) *
                                             active_region_height_cells) : 0u;
            const auto acceptance_width = translated_active_window
                ? (std::min)(config.grid_width - origin_x,
                             static_cast<std::uint32_t>(active_region_width_cells))
                : (std::min)(config.grid_width, 192u);
            const auto acceptance_height = translated_active_window
                ? (std::min)(config.grid_height - origin_y,
                             static_cast<std::uint32_t>(active_region_height_cells))
                : (std::min)(config.grid_height, 192u);
            const SimulationPush push{
                .width = config.grid_width,
                .height = translated_active_window ? config.grid_height : acceptance_height,
                .step = simulation_step,
                .seed = random_seed,
                .active_section_x = active_section_x,
                .active_section_y = active_section_y,
                .active_mode = translated_active_window ? 1u : 0u,
                .reserved = 2u, // Focused acceptance classifies a macro movement-due tick.
            };
            bind_compute(command_buffer, tile_pipeline, current_set);
            vkCmdPushConstants(command_buffer, compute_pipeline_layout,
                               VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);
            vkCmdDispatch(command_buffer,
                          divide_round_up(divide_round_up(acceptance_width, 8u), 8u),
                          divide_round_up(divide_round_up(acceptance_height, 8u), 8u), 1);
            buffer_barrier(command_buffer, tile_buffer, VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        });
    }

    void run_acceptance_macro_pass(const std::int32_t phase,
                                   const std::int32_t parity,
                                   const std::int32_t active_section_x = 0,
                                   const std::int32_t active_section_y = 0,
                                   const bool translated_active_window = false) {
        immediate_submit([&](const VkCommandBuffer command_buffer) {
            const auto origin_x = translated_active_window
                ? static_cast<std::uint32_t>((std::max)(active_section_x, 0) *
                                             active_region_width_cells) : 0u;
            const auto origin_y = translated_active_window
                ? static_cast<std::uint32_t>((std::max)(active_section_y, 0) *
                                             active_region_height_cells) : 0u;
            const auto acceptance_width = translated_active_window
                ? (std::min)(config.grid_width - origin_x,
                             static_cast<std::uint32_t>(active_region_width_cells))
                : (std::min)(config.grid_width, 192u);
            const auto acceptance_height = translated_active_window
                ? (std::min)(config.grid_height - origin_y,
                             static_cast<std::uint32_t>(active_region_height_cells))
                : (std::min)(config.grid_height, 192u);
            const MovementPush push{
                .width = config.grid_width,
                .height = translated_active_window ? config.grid_height : acceptance_height,
                .step = simulation_step,
                .seed = random_seed,
                .phase = phase,
                .parity = parity,
                .active_section_x = active_section_x,
                .active_section_y = active_section_y,
                .active_mode = translated_active_window ? 1u : 0u,
            };
            bind_compute(command_buffer, macro_movement_pipeline, current_set);
            vkCmdPushConstants(command_buffer, compute_pipeline_layout,
                               VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);
            const auto tile_columns = divide_round_up(acceptance_width, 8u);
            const auto tile_rows = divide_round_up(acceptance_height, 8u);
            if (phase <= 2 || phase == 5) {
                vkCmdDispatch(command_buffer, tile_columns,
                              divide_round_up(tile_rows, 2u), 1);
            } else {
                vkCmdDispatch(command_buffer, divide_round_up(tile_columns, 2u),
                              tile_rows, 1);
            }
            buffer_barrier(command_buffer, cell_buffers[current_set],
                           VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            buffer_barrier(command_buffer, tile_buffer, VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            buffer_barrier(command_buffer, chunk_buffer, VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        });
    }

    void run_acceptance_chemistry_pass(const std::int32_t active_section_x = 0,
                                       const std::int32_t active_section_y = 0,
                                       const bool translated_active_window = false,
                                       const bool collect_debug = false) {
        immediate_submit([&](const VkCommandBuffer command_buffer) {
            const auto origin_x = translated_active_window
                ? static_cast<std::uint32_t>((std::max)(active_section_x, 0) *
                                             active_region_width_cells) : 0u;
            const auto origin_y = translated_active_window
                ? static_cast<std::uint32_t>((std::max)(active_section_y, 0) *
                                             active_region_height_cells) : 0u;
            const auto acceptance_width = translated_active_window
                ? (std::min)(config.grid_width - origin_x,
                             static_cast<std::uint32_t>(active_region_width_cells))
                : (std::min)(config.grid_width, 192u);
            const auto acceptance_height = translated_active_window
                ? (std::min)(config.grid_height - origin_y,
                             static_cast<std::uint32_t>(active_region_height_cells))
                : (std::min)(config.grid_height, 192u);
            const ActiveCellDispatch acceptance_dispatch{
                origin_x, origin_y, acceptance_width, acceptance_height};
            const SimulationPush push{
                .width = config.grid_width,
                .height = translated_active_window ? config.grid_height : acceptance_height,
                .step = simulation_step,
                .seed = random_seed,
                .active_section_x = active_section_x,
                .active_section_y = active_section_y,
                .active_mode = translated_active_window ? 1u : 0u,
                .reserved = collect_debug ? 1u : 0u,
            };
            const auto next_set = current_set ^ 1u;
            buffer_barrier(command_buffer, cell_buffers[next_set],
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            bind_compute(command_buffer, chemistry_pipeline, current_set);
            vkCmdPushConstants(command_buffer, compute_pipeline_layout,
                               VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);
            vkCmdDispatch(command_buffer,
                          divide_round_up(acceptance_width, simulation_local_size),
                          divide_round_up(acceptance_height, simulation_local_size), 1);
            buffer_barrier(command_buffer, cell_buffers[next_set],
                           VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            bind_compute(command_buffer, conservation_corrections_pipeline, current_set);
            vkCmdPushConstants(command_buffer, compute_pipeline_layout,
                               VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);
            vkCmdDispatch(command_buffer,
                          divide_round_up(acceptance_width, simulation_local_size),
                          divide_round_up(acceptance_height, simulation_local_size), 1);
            buffer_barrier(command_buffer, cell_buffers[next_set],
                           VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            buffer_barrier(command_buffer, rainfall_buffer,
                           VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            buffer_barrier(command_buffer, cell_buffers[current_set],
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            copy_cell_rectangle(command_buffer, next_set, current_set,
                                acceptance_dispatch);
            buffer_barrier(command_buffer, cell_buffers[next_set],
                           VK_ACCESS_SHADER_READ_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            buffer_barrier(command_buffer, cell_buffers[current_set],
                           VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            record_bee_birth_pass(command_buffer, push, acceptance_dispatch);
        });
    }

    void run_acceptance_sunlight_pass() {
        immediate_submit([&](const VkCommandBuffer command_buffer) {
            const auto acceptance_height = (std::min)(config.grid_height, 192u);
            vkCmdFillBuffer(command_buffer, sunlight_buffer.handle, 0,
                            sunlight_buffer.size, 0u);
            buffer_barrier(command_buffer, sunlight_buffer,
                           VK_ACCESS_TRANSFER_WRITE_BIT,
                           VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            const SimulationPush push{
                .width = config.grid_width,
                .height = acceptance_height,
                .step = simulation_step,
                .seed = random_seed,
                .active_mode = 0u,
            };
            bind_compute(command_buffer, sunlight_pipeline, current_set);
            vkCmdPushConstants(command_buffer, compute_pipeline_layout,
                               VK_SHADER_STAGE_COMPUTE_BIT, 0,
                               sizeof(push), &push);
            vkCmdDispatch(command_buffer,
                          divide_round_up(config.grid_width, sunlight_local_size),
                          1u, 1u);
            buffer_barrier(command_buffer, sunlight_buffer,
                           VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        });
    }

    void run_acceptance_fine_pass(const std::int32_t phase, const std::int32_t parity) {
        immediate_submit([&](const VkCommandBuffer command_buffer) {
            const auto acceptance_width = (std::min)(config.grid_width, 192u);
            const auto acceptance_height = (std::min)(config.grid_height, 192u);
            const auto snapshot_set = current_set ^ 1u;
            buffer_barrier(command_buffer, cell_buffers[current_set],
                           VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            buffer_barrier(command_buffer, cell_buffers[snapshot_set],
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            copy_cell_rectangle(command_buffer, current_set, snapshot_set,
                                {0u, 0u, acceptance_width, acceptance_height});
            buffer_barrier(command_buffer, cell_buffers[current_set],
                           VK_ACCESS_SHADER_READ_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            buffer_barrier(command_buffer, cell_buffers[snapshot_set],
                           VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            const MovementPush push{
                .width = config.grid_width,
                .height = acceptance_height,
                .step = simulation_step,
                .seed = random_seed,
                .phase = phase,
                .parity = parity,
                .active_mode = 0u,
            };
            bind_compute(command_buffer, movement_pipeline, current_set);
            vkCmdPushConstants(command_buffer, compute_pipeline_layout,
                               VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);
            if (phase >= 5) {
                vkCmdDispatch(command_buffer,
                              divide_round_up(divide_round_up(acceptance_width, 2u),
                                              simulation_local_size),
                              divide_round_up(acceptance_height, simulation_local_size), 1);
            } else {
                vkCmdDispatch(command_buffer,
                              divide_round_up(acceptance_width, simulation_local_size),
                              divide_round_up(divide_round_up(acceptance_height, 2u),
                                              simulation_local_size), 1);
            }
            buffer_barrier(command_buffer, cell_buffers[current_set],
                           VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            buffer_barrier(command_buffer, chunk_buffer, VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        });
    }

    void run_acceptance_horizontal_pass(const std::int32_t parity) {
        immediate_submit([&](const VkCommandBuffer command_buffer) {
            const auto acceptance_width = (std::min)(config.grid_width, 192u);
            const auto acceptance_height = (std::min)(config.grid_height, 192u);
            const auto snapshot_set = current_set ^ 1u;
            buffer_barrier(command_buffer, cell_buffers[current_set],
                           VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            buffer_barrier(command_buffer, cell_buffers[snapshot_set],
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            copy_cell_rectangle(command_buffer, current_set, snapshot_set,
                                {0u, 0u, acceptance_width, acceptance_height});
            buffer_barrier(command_buffer, cell_buffers[current_set],
                           VK_ACCESS_SHADER_READ_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            buffer_barrier(command_buffer, cell_buffers[snapshot_set],
                           VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            const MovementPush push{
                .width = config.grid_width,
                .height = acceptance_height,
                .step = simulation_step,
                .seed = random_seed,
                .phase = 5,
                .parity = parity,
                .active_mode = 0u,
            };
            bind_compute(command_buffer, movement_pipeline, current_set);
            vkCmdPushConstants(command_buffer, compute_pipeline_layout,
                               VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);
            vkCmdDispatch(command_buffer,
                          divide_round_up(divide_round_up(acceptance_width, 2u),
                                          simulation_local_size),
                          divide_round_up(acceptance_height, simulation_local_size), 1);
            buffer_barrier(command_buffer, cell_buffers[current_set],
                           VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            buffer_barrier(command_buffer, chunk_buffer,
                           VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        });
    }

    void run_acceptance_rainfall_pass() {
        immediate_submit([&](const VkCommandBuffer command_buffer) {
            const SimulationPush push{
                .width = config.grid_width,
                .height = config.grid_height,
                .step = simulation_step,
                .seed = random_seed,
                .active_mode = 0u,
            };
            bind_compute(command_buffer, rainfall_pipeline, current_set);
            vkCmdPushConstants(command_buffer, compute_pipeline_layout,
                               VK_SHADER_STAGE_COMPUTE_BIT, 0,
                               sizeof(push), &push);
            vkCmdDispatch(command_buffer,
                          divide_round_up(config.grid_width, 64u), 1, 1);
            buffer_barrier(command_buffer, rainfall_buffer,
                           VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            buffer_barrier(command_buffer, cell_buffers[current_set],
                           VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            buffer_barrier(command_buffer, chunk_buffer,
                           VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        });
        ++simulation_step;
    }

    void run_acceptance_focused_tick(const std::int32_t active_section_x = 0,
                                     const std::int32_t active_section_y = 0,
                                     const bool translated_active_window = false,
                                     const bool district_bounded = false) {
        const auto origin_x = translated_active_window
            ? static_cast<std::uint32_t>((std::max)(active_section_x, 0) *
                                         active_region_width_cells) : 0u;
        const auto origin_y = translated_active_window
            ? static_cast<std::uint32_t>((std::max)(active_section_y, 0) *
                                         active_region_height_cells) : 0u;
        const auto translated_width = district_bounded
            ? pre_expansion_world_width
            : static_cast<std::uint32_t>(active_region_width_cells);
        const auto translated_height = district_bounded
            ? pre_expansion_world_height
            : static_cast<std::uint32_t>(active_region_height_cells);
        const auto acceptance_width = translated_active_window
            ? (std::min)(config.grid_width - origin_x,
                         translated_width)
            : (std::min)(config.grid_width, 192u);
        const auto acceptance_height = translated_active_window
            ? (std::min)(config.grid_height - origin_y,
                         translated_height)
            : (std::min)(config.grid_height, 192u);
        const ActiveCellDispatch acceptance_dispatch{
            origin_x, origin_y, acceptance_width, acceptance_height};
        immediate_submit([&](const VkCommandBuffer command_buffer) {
            const SimulationPush simulation_push{
                .width = config.grid_width,
                .height = translated_active_window ? config.grid_height : acceptance_height,
                .step = simulation_step,
                .seed = random_seed,
                .active_section_x = active_section_x,
                .active_section_y = active_section_y,
                .active_mode = translated_active_window ? 1u : 0u,
            };

            bind_compute(command_buffer, tile_pipeline, current_set);
            vkCmdPushConstants(command_buffer, compute_pipeline_layout,
                               VK_SHADER_STAGE_COMPUTE_BIT, 0,
                               sizeof(simulation_push), &simulation_push);
            vkCmdDispatch(
                command_buffer,
                divide_round_up(divide_round_up(acceptance_width, 8u), 8u),
                divide_round_up(divide_round_up(acceptance_height, 8u), 8u), 1);
            buffer_barrier(command_buffer, tile_buffer, VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

            const auto next_set = current_set ^ 1u;
            buffer_barrier(command_buffer, cell_buffers[next_set],
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            bind_compute(command_buffer, chemistry_pipeline, current_set);
            vkCmdPushConstants(command_buffer, compute_pipeline_layout,
                               VK_SHADER_STAGE_COMPUTE_BIT, 0,
                               sizeof(simulation_push), &simulation_push);
            vkCmdDispatch(command_buffer,
                          divide_round_up(acceptance_width, simulation_local_size),
                          divide_round_up(acceptance_height, simulation_local_size), 1);
            buffer_barrier(command_buffer, cell_buffers[next_set],
                           VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            bind_compute(command_buffer, conservation_corrections_pipeline, current_set);
            vkCmdPushConstants(command_buffer, compute_pipeline_layout,
                               VK_SHADER_STAGE_COMPUTE_BIT, 0,
                               sizeof(simulation_push), &simulation_push);
            vkCmdDispatch(command_buffer,
                          divide_round_up(acceptance_width, simulation_local_size),
                          divide_round_up(acceptance_height, simulation_local_size), 1);
            buffer_barrier(command_buffer, cell_buffers[next_set],
                           VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            buffer_barrier(command_buffer, rainfall_buffer,
                           VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            buffer_barrier(command_buffer, cell_buffers[current_set],
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            copy_cell_rectangle(command_buffer, next_set, current_set,
                                acceptance_dispatch);
            buffer_barrier(command_buffer, cell_buffers[next_set],
                           VK_ACCESS_SHADER_READ_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            buffer_barrier(command_buffer, cell_buffers[current_set],
                           VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

            bind_compute(command_buffer, rainfall_pipeline, current_set);
            vkCmdPushConstants(command_buffer, compute_pipeline_layout,
                               VK_SHADER_STAGE_COMPUTE_BIT, 0,
                               sizeof(simulation_push), &simulation_push);
            vkCmdDispatch(command_buffer,
                          divide_round_up(config.grid_width, 64u), 1, 1);
            buffer_barrier(command_buffer, rainfall_buffer,
                           VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            buffer_barrier(command_buffer, cell_buffers[current_set],
                           VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            buffer_barrier(command_buffer, chunk_buffer,
                           VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

            const auto snapshot_set = current_set ^ 1u;
            const auto snapshot_dispatch = translated_active_window
                ? expanded_cell_dispatch(acceptance_dispatch, config.grid_width,
                                         config.grid_height, 16u)
                : acceptance_dispatch;
            buffer_barrier(command_buffer, cell_buffers[current_set],
                           VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            buffer_barrier(command_buffer, cell_buffers[snapshot_set],
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            copy_cell_rectangle(command_buffer, current_set, snapshot_set,
                                snapshot_dispatch);
            buffer_barrier(command_buffer, cell_buffers[current_set],
                           VK_ACCESS_SHADER_READ_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            buffer_barrier(command_buffer, cell_buffers[snapshot_set],
                           VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

            bind_compute(command_buffer, movement_pipeline, current_set);
            const std::array<std::int32_t, 7> phases =
                (simulation_step & 1u) == 0u
                ? std::array<std::int32_t, 7>{0, 1, 2, 3, 4, 5, 5}
                : std::array<std::int32_t, 7>{0, 2, 1, 4, 3, 5, 5};
            for (std::size_t phase_index = 0u;
                 phase_index < phases.size(); ++phase_index) {
                const auto phase = phases[phase_index];
                const MovementPush movement_push{
                    .width = config.grid_width,
                    .height = translated_active_window ? config.grid_height : acceptance_height,
                    .step = simulation_step,
                    .seed = random_seed,
                    .phase = phase,
                    .parity = static_cast<std::int32_t>(
                        phase == 5
                            ? ((simulation_step +
                                static_cast<std::uint32_t>(phase_index)) & 1u)
                            : ((simulation_step +
                                static_cast<std::uint32_t>(phase)) & 1u)),
                    .reserved1 = ((simulation_step & 3u) == 0u) ? 2u : 0u,
                    .active_section_x = active_section_x,
                    .active_section_y = active_section_y,
                    .active_mode = translated_active_window ? 1u : 0u,
                };
                vkCmdPushConstants(command_buffer, compute_pipeline_layout,
                                   VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                   sizeof(movement_push), &movement_push);
                if (phase >= 5) {
                    vkCmdDispatch(
                        command_buffer,
                        divide_round_up(divide_round_up(acceptance_width, 2u),
                                        simulation_local_size),
                        divide_round_up(acceptance_height, simulation_local_size),
                        1);
                } else {
                    vkCmdDispatch(
                        command_buffer,
                        divide_round_up(acceptance_width, simulation_local_size),
                        divide_round_up(divide_round_up(acceptance_height, 2u),
                                        simulation_local_size),
                        1);
                }
                buffer_barrier(command_buffer, cell_buffers[current_set],
                               VK_ACCESS_SHADER_WRITE_BIT,
                               VK_ACCESS_SHADER_READ_BIT |
                                   VK_ACCESS_SHADER_WRITE_BIT,
                               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            }

            record_bee_birth_pass(command_buffer, simulation_push,
                                  acceptance_dispatch);
            bind_compute(command_buffer, bee_movement_pipeline, current_set);
            vkCmdPushConstants(command_buffer, compute_pipeline_layout,
                               VK_SHADER_STAGE_COMPUTE_BIT, 0,
                               sizeof(simulation_push), &simulation_push);
            vkCmdDispatch(command_buffer,
                          divide_round_up(acceptance_width,
                                          simulation_local_size),
                          divide_round_up(acceptance_height,
                                          simulation_local_size), 1);
            buffer_barrier(command_buffer, cell_buffers[current_set],
                           VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT |
                               VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            buffer_barrier(command_buffer, chunk_buffer,
                           VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT |
                               VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        });
        ++simulation_step;
    }

    void run_acceptance_paint_pass(const std::int32_t center_x,
                                   const std::int32_t center_y,
                                   const Material material,
                                   const std::uint32_t radius) {
        immediate_submit([&](const VkCommandBuffer command_buffer) {
            const SimulationPush push{
                .width = config.grid_width,
                .height = config.grid_height,
                .step = simulation_step,
                .seed = random_seed,
                .brush_x = center_x,
                .brush_y = center_y,
                .radius = radius,
                .material = static_cast<std::uint32_t>(material),
            };
            bind_compute(command_buffer, paint_pipeline, current_set);
            vkCmdPushConstants(command_buffer, compute_pipeline_layout,
                               VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);
            const auto diameter = radius * 2u + 1u;
            vkCmdDispatch(command_buffer,
                          divide_round_up(diameter, simulation_local_size),
                          divide_round_up(diameter, simulation_local_size), 1);
            buffer_barrier(command_buffer, cell_buffers[current_set],
                           VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            buffer_barrier(command_buffer, chunk_buffer,
                           VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        });
    }

    [[nodiscard]] int run_runtime_acceptance(SharedState& state) {
        startup_log("Running deterministic packaged Vulkan state acceptance...");
        std::vector<RuntimeAcceptanceCheck> checks;
        const auto material_id = [](const Material material) {
            return static_cast<std::uint32_t>(material);
        };
        const auto index_of = [&](const std::uint32_t x, const std::uint32_t y) {
            return static_cast<std::size_t>(y) * config.grid_width + x;
        };
        const auto count_material = [&](const std::vector<SceneCell>& cells,
                                        const Material material) {
            return static_cast<std::uint32_t>(std::count_if(
                cells.begin(), cells.end(), [&](const SceneCell& cell) {
                    return cell.material == material_id(material);
                }));
        };
        const auto count_rect = [&](const std::vector<SceneCell>& cells,
                                    const Material material,
                                    const std::uint32_t left,
                                    const std::uint32_t top,
                                    const std::uint32_t width,
                                    const std::uint32_t height) {
            std::uint32_t count = 0u;
            for (std::uint32_t y = top; y < top + height; ++y) {
                for (std::uint32_t x = left; x < left + width; ++x) {
                    if (cells[index_of(x, y)].material == material_id(material)) ++count;
                }
            }
            return count;
        };
        const auto seed_rect = [&](std::vector<SceneCell>& cells,
                                   const Material material,
                                   const std::uint32_t left,
                                   const std::uint32_t top,
                                   const std::uint32_t width,
                                   const std::uint32_t height) {
            for (std::uint32_t y = top; y < top + height; ++y) {
                for (std::uint32_t x = left; x < left + width; ++x) {
                    const auto index = index_of(x, y);
                    cells[index] = make_fill_cell(material_id(material),
                                                  static_cast<std::uint32_t>(index));
                }
            }
        };
        const auto append = [&](std::string name, const bool passed, std::string details) {
            startup_log(std::string{passed ? "PASS " : "FAIL "} + name + ": " + details);
            checks.push_back({std::move(name), passed, std::move(details)});
        };
        struct ScheduledRainCandidate final {
            std::uint32_t x{};
            std::uint32_t step{};
        };
        const auto find_scheduled_rain_candidate =
            [&](const std::uint32_t minimum_x,
                const std::uint32_t maximum_x)
                -> std::optional<ScheduledRainCandidate> {
                constexpr std::uint32_t weather_cycle_ticks = 7200u;
                constexpr std::uint32_t rain_start_tick = 4800u;
                constexpr std::uint32_t rain_duration_ticks = 600u;
                constexpr std::uint32_t emission_cadence = 360u;
                constexpr std::uint32_t sector_width = 256u;
                for (std::uint32_t step = rain_start_tick;
                     step < rain_start_tick + rain_duration_ticks; ++step) {
                    const auto rain_tick = step - rain_start_tick;
                    for (std::uint32_t x = minimum_x; x <= maximum_x; ++x) {
                        const auto sector = x / sector_width;
                        const auto sector_phase =
                            (sector * 37u) % emission_cadence;
                        if ((rain_tick % emission_cadence) != sector_phase) continue;
                        const auto event_index = rain_tick / emission_cadence;
                        const auto lane = fill_hash(
                            sector ^ event_index * 0x9e3779b9u ^
                            (step / weather_cycle_ticks) * 0x85ebca6bu) %
                            sector_width;
                        if ((x % sector_width) == lane)
                            return ScheduledRainCandidate{x, step};
                    }
                }
                return std::nullopt;
            };

        if (config.grid_width < 256u || config.grid_height < 256u) {
            append("world_dimensions", false, "acceptance requires at least 256x256 cells");
        } else {
            {
                const auto previous_selected_material =
                    state.selected_material.load(std::memory_order_acquire);
                state.selected_material.store(material_id(Material::sand),
                                              std::memory_order_release);
                std::uint32_t running_action = 0u;
                std::uint32_t paused_action = 0u;
                const auto paint_once = [&](const bool paused) {
                    auto cells = acceptance_atmosphere_world();
                    upload_scene_cells(cells);
                    const auto action = route_world_primary_action({
                        .editor_workspace = true,
                        .pointer_over_world = true,
                        .primary_down = true,
                        .primary_pressed = true,
                        .player_present = true,
                        .mining = true,
                        .paused = paused,
                    });
                    (paused ? paused_action : running_action) =
                        static_cast<std::uint32_t>(action);
                    state.primary_down.store(
                        action == WorldPrimaryAction::editor_paint,
                        std::memory_order_release);
                    immediate_submit([&](const VkCommandBuffer command_buffer) {
                        record_paint_at_grid(command_buffer, state, false,
                                             state.primary_down.load(std::memory_order_acquire),
                                             100, 100);
                    });
                    state.primary_down.store(false, std::memory_order_release);
                    return count_material(download_scene_cells(), Material::sand);
                };
                const auto running_cells = paint_once(false);
                const auto paused_cells = paint_once(true);
                state.selected_material.store(previous_selected_material,
                                              std::memory_order_release);
                append("running_and_paused_editor_mutation",
                       running_cells > 0u && paused_cells == running_cells,
                       "running_sand=" + std::to_string(running_cells) +
                           " paused_sand=" + std::to_string(paused_cells) +
                           " running_action=" + std::to_string(running_action) +
                           " paused_action=" + std::to_string(paused_action) +
                           " test_material=" +
                               std::to_string(material_id(Material::sand)));
            }

            {
                auto cells = acceptance_atmosphere_world();
                seed_rect(cells, Material::water, 64u, 64u, 8u, 8u);
                upload_scene_cells(cells);
                run_acceptance_tile_pass();
                run_acceptance_macro_pass(0, 0);
                const auto result = download_scene_cells();
                const auto total = count_material(result, Material::water);
                const auto source = count_rect(result, Material::water, 64u, 64u, 8u, 8u);
                const auto target = count_rect(result, Material::water, 64u, 72u, 8u, 8u);
                const auto tile_states = download_tile_states();
                const auto tile_columns = divide_round_up(config.grid_width, 8u);
                const auto source_tile = tile_states[8u * tile_columns + 8u];
                const auto target_tile = tile_states[9u * tile_columns + 8u];
                std::uint32_t outside_min_x = config.grid_width;
                std::uint32_t outside_max_x = 0u;
                std::uint32_t outside_min_y = config.grid_height;
                std::uint32_t outside_max_y = 0u;
                for (std::uint32_t y = 0u; y < config.grid_height; ++y) {
                    for (std::uint32_t x = 0u; x < config.grid_width; ++x) {
                        if (result[index_of(x, y)].material != material_id(Material::water) ||
                            (x >= 64u && x < 72u && y >= 64u && y < 72u)) continue;
                        outside_min_x = std::min(outside_min_x, x);
                        outside_max_x = std::max(outside_max_x, x);
                        outside_min_y = std::min(outside_min_y, y);
                        outside_max_y = std::max(outside_max_y, y);
                    }
                }
                append("macro_liquid_exact_packet",
                       total == 64u && source == 0u && target == 64u,
                       "water=" + std::to_string(total) +
                           " source=" + std::to_string(source) +
                           " target=" + std::to_string(target) +
                           " source_flags=" + std::to_string(source_tile.flags) +
                           " target_flags=" + std::to_string(target_tile.flags) +
                           " outside_bounds=" + std::to_string(outside_min_x) + "," +
                           std::to_string(outside_min_y) + ".." +
                           std::to_string(outside_max_x) + "," +
                           std::to_string(outside_max_y));

                // A production chemistry pass must expire AUX_MOVED, after
                // which the same packet metadata travels into a second exact
                // 8x8 fall instead of remaining permanently locked.
                run_acceptance_chemistry_pass();
                run_acceptance_tile_pass();
                run_acceptance_macro_pass(0, 1);
                const auto second_result = download_scene_cells();
                const auto second_total = count_material(second_result, Material::water);
                const auto first_target_after_second =
                    count_rect(second_result, Material::water, 64u, 72u, 8u, 8u);
                const auto second_target =
                    count_rect(second_result, Material::water, 64u, 80u, 8u, 8u);
                append("macro_liquid_consecutive_packets",
                       second_total == 64u && first_target_after_second == 0u &&
                           second_target == 64u,
                       "water=" + std::to_string(second_total) +
                           " previous_target=" +
                           std::to_string(first_target_after_second) +
                           " second_target=" + std::to_string(second_target));
            }

            {
                auto cells = acceptance_atmosphere_world();
                seed_rect(cells, Material::hydrogen, 64u, 80u, 8u, 8u);
                upload_scene_cells(cells);
                run_acceptance_tile_pass();
                run_acceptance_macro_pass(5, 1);
                const auto result = download_scene_cells();
                const auto total = count_material(result, Material::hydrogen);
                const auto source = count_rect(result, Material::hydrogen, 64u, 80u, 8u, 8u);
                const auto target = count_rect(result, Material::hydrogen, 64u, 72u, 8u, 8u);
                append("macro_gas_exact_packet",
                       total == 64u && source == 0u && target == 64u,
                       "hydrogen=" + std::to_string(total) +
                           " source=" + std::to_string(source) +
                           " target=" + std::to_string(target));
            }

            {
                // One complete Hydrogen packet rises through eight complete
                // Water tiles. Water performs each exact downward swap, so the
                // lighter target packet advances upward without fine-cell loss.
                constexpr std::int32_t packet_active_section_x = 4;
                constexpr std::int32_t packet_active_section_y = 0;
                constexpr std::uint32_t packet_x =
                    packet_active_section_x * active_region_width_cells + 64u;
                constexpr std::uint32_t packet_tile_column = packet_x / 8u;
                auto cells = acceptance_atmosphere_world();
                seed_rect(cells, Material::water, packet_x, 64u, 8u, 64u);
                seed_rect(cells, Material::hydrogen, packet_x, 128u, 8u, 8u);
                upload_scene_cells(cells);
                run_acceptance_tile_pass(packet_active_section_x,
                                         packet_active_section_y, true);

                constexpr std::uint32_t tile_macro_movable = 0x00010000u;
                constexpr std::uint32_t tile_fine_active = 0x00020000u;
                constexpr std::uint32_t tile_medium_breakup = 0x04000000u;
                const auto tile_columns = divide_round_up(config.grid_width, 8u);
                std::uint32_t gas_tile_row = 16u;
                bool retained_first_seven = true;
                bool counted_each_step = true;
                std::uint32_t seventh_counters = 0u;
                for (std::uint32_t step = 1u; step <= 8u; ++step) {
                    const auto water_source_row = gas_tile_row - 1u;
                    run_acceptance_macro_pass(
                        0, static_cast<std::int32_t>(water_source_row & 1u),
                        packet_active_section_x, packet_active_section_y, true);
                    --gas_tile_row;
                    const auto moved_states = download_tile_states();
                    const auto moved_state =
                        moved_states[gas_tile_row * tile_columns +
                                     packet_tile_column];
                    counted_each_step = counted_each_step &&
                        (moved_state.counters & 0xffu) == step;
                    if (step < 8u) {
                        retained_first_seven = retained_first_seven &&
                            (moved_state.flags & tile_macro_movable) != 0u &&
                            (moved_state.flags & tile_fine_active) == 0u;
                        seventh_counters = moved_state.counters;
                        run_acceptance_chemistry_pass(packet_active_section_x,
                                                      packet_active_section_y, true);
                        run_acceptance_tile_pass(packet_active_section_x,
                                                 packet_active_section_y, true);
                    }
                }
                run_acceptance_tile_pass(packet_active_section_x,
                                         packet_active_section_y, true);
                const auto final_states = download_tile_states();
                const auto final_state =
                    final_states[8u * tile_columns + packet_tile_column];
                const auto result = download_scene_cells();
                const auto hydrogen = count_material(result, Material::hydrogen);
                const auto water = count_material(result, Material::water);
                const auto final_hydrogen =
                    count_rect(result, Material::hydrogen, packet_x, 64u, 8u, 8u);
                append("macro_bubble_eight_step_breakup",
                       retained_first_seven && counted_each_step &&
                           (final_state.flags & tile_fine_active) != 0u &&
                           (final_state.flags & tile_medium_breakup) != 0u &&
                           (final_state.flags & tile_macro_movable) == 0u &&
                           (final_state.counters & 0xffu) == 8u &&
                           hydrogen == 64u && final_hydrogen == 64u && water == 512u,
                       "hydrogen=" + std::to_string(hydrogen) +
                           " final_hydrogen=" + std::to_string(final_hydrogen) +
                           " water=" + std::to_string(water) +
                           " seventh_progress=" +
                           std::to_string(seventh_counters & 0xffu) +
                           " final_progress=" +
                           std::to_string(final_state.counters & 0xffu) +
                           " final_flags=" + std::to_string(final_state.flags) +
                           " active_section=" +
                           std::to_string(packet_active_section_x) + "," +
                           std::to_string(packet_active_section_y));
            }
            {
                auto cells = acceptance_atmosphere_world();
                seed_rect(cells, Material::water, 64u, 64u, 8u, 8u);
                for (std::uint32_t y = 64u; y < 72u; ++y)
                    for (std::uint32_t x = 64u; x < 72u; ++x)
                        cells[index_of(x, y)].age = 8u;
                const auto obstacle = index_of(64u, 72u);
                cells[obstacle] = make_fill_cell(material_id(Material::stone),
                                                 static_cast<std::uint32_t>(obstacle));
                upload_scene_cells(cells);
                constexpr std::uint32_t tile_macro_movable = 0x00010000u;
                constexpr std::uint32_t tile_fine_active = 0x00020000u;
                const auto tile_columns = divide_round_up(config.grid_width, 8u);
                const auto source_tile_index = 8u * tile_columns + 8u;
                bool retained_for_first_seven = true;
                std::uint32_t seventh_flags = 0u;
                std::uint32_t eighth_flags = 0u;
                std::uint32_t eighth_blocked_attempts = 0u;
                run_acceptance_tile_pass();
                for (std::uint32_t failure = 1u; failure <= 8u; ++failure) {
                    run_acceptance_macro_pass(0, 0);
                    run_acceptance_tile_pass();
                    const auto states = download_tile_states();
                    const auto tile_state = states[source_tile_index];
                    if (failure < 8u) {
                        retained_for_first_seven = retained_for_first_seven &&
                            (tile_state.flags & tile_macro_movable) != 0u &&
                            (tile_state.flags & tile_fine_active) == 0u &&
                            ((tile_state.counters >> 8u) & 0xffu) == failure;
                        seventh_flags = tile_state.flags;
                    } else {
                        eighth_flags = tile_state.flags;
                        eighth_blocked_attempts =
                            (tile_state.counters >> 8u) & 0xffu;
                    }
                }
                run_acceptance_fine_pass(0, 0);
                run_acceptance_fine_pass(1, 1);
                run_acceptance_fine_pass(2, 0);
                const auto result = download_scene_cells();
                const auto total = count_material(result, Material::water);
                const auto source = count_rect(result, Material::water, 64u, 64u, 8u, 8u);
                const auto outside = total - source;
                append("macro_blocked_fine_fallback",
                       retained_for_first_seven &&
                           (eighth_flags & tile_fine_active) != 0u &&
                           eighth_blocked_attempts == 8u &&
                           total == 64u && source > 0u && source < 64u && outside > 0u,
                       "water=" + std::to_string(total) +
                           " source=" + std::to_string(source) +
                           " moved_out=" + std::to_string(outside) +
                           " seventh_flags=" + std::to_string(seventh_flags) +
                           " eighth_flags=" + std::to_string(eighth_flags) +
                           " blocked_attempts=" +
                           std::to_string(eighth_blocked_attempts));
            }

            {
                auto cells = acceptance_atmosphere_world();
                for (std::uint32_t x = 95u; x <= 104u; ++x) {
                    const auto north = index_of(x, 95u);
                    const auto south = index_of(x, 104u);
                    cells[north] = make_fill_cell(material_id(Material::stone),
                                                  static_cast<std::uint32_t>(north));
                    cells[south] = make_fill_cell(material_id(Material::stone),
                                                  static_cast<std::uint32_t>(south));
                }
                for (std::uint32_t y = 96u; y < 104u; ++y) {
                    const auto west = index_of(95u, y);
                    const auto east = index_of(104u, y);
                    cells[west] = make_fill_cell(material_id(Material::stone),
                                                 static_cast<std::uint32_t>(west));
                    cells[east] = make_fill_cell(material_id(Material::stone),
                                                 static_cast<std::uint32_t>(east));
                }
                upload_scene_cells(cells);
                for (std::uint32_t attempt = 0u; attempt < 12u; ++attempt)
                    run_acceptance_tile_pass();
                const auto states = download_tile_states();
                const auto tile_columns = divide_round_up(config.grid_width, 8u);
                const auto flags = states[12u * tile_columns + 12u].flags;
                constexpr std::uint32_t tile_macro_gas = 0x00800000u;
                constexpr std::uint32_t tile_fine_active = 0x00020000u;
                constexpr std::uint32_t tile_medium_enclosed = 0x02000000u;
                constexpr std::uint32_t tile_medium_breakup = 0x04000000u;
                append("enclosed_air_remains_tiled",
                       (flags & tile_macro_gas) != 0u &&
                           (flags & tile_medium_enclosed) != 0u &&
                           (flags & (tile_fine_active | tile_medium_breakup)) == 0u,
                       "flags=" + std::to_string(flags));
            }
            constexpr std::uint32_t water_half_bit = 0x00800000u;
            const auto water_half_units = [&](const std::vector<SceneCell>& cells) {
                std::uint32_t units = 0u;
                std::uint32_t halves = 0u;
                for (const auto& cell : cells) {
                    if (cell.material != material_id(Material::water)) continue;
                    if ((cell.aux & water_half_bit) != 0u) {
                        ++units;
                        ++halves;
                    } else {
                        units += 2u;
                    }
                }
                return std::pair{units, halves};
            };


            const auto check_pre_pr19_hive = [&](const std::string_view name,
                                                 const Scene scene,
                                                 const std::uint32_t delayed_ticks = 0u) {
                const bool composed_world =
                    config.grid_width >= persistent_world_width &&
                    config.grid_height >= persistent_world_height;
                immediate_submit([&](const VkCommandBuffer command_buffer) {
                    record_reset(command_buffer, composed_world
                        ? static_cast<std::uint32_t>(world_scene)
                        : static_cast<std::uint32_t>(scene));
                });
                const std::uint32_t queen_x = 512u;
                const std::uint32_t queen_y = scene == Scene::sandbox ? 234u : 232u;
                const auto district = persistent_world_district_index(scene);
                const std::uint32_t hive_origin_x = composed_world
                    ? persistent_world_district_origin_x(config.grid_width, district)
                    : authored_map_origin_x();
                const std::uint32_t hive_origin_y = composed_world
                    ? persistent_world_district_origin_y(config.grid_height, district)
                    : authored_map_origin_y();
                if (delayed_ticks > 0u && composed_world) {
                    const auto global_queen_x = hive_origin_x + queen_x;
                    const auto global_queen_y = hive_origin_y + queen_y;
                const auto active_section_x = static_cast<std::int32_t>(
                        global_queen_x / active_region_width_cells);
                    const auto active_section_y = static_cast<std::int32_t>(
                        global_queen_y / active_region_height_cells);
                    for (std::uint32_t tick = 0u; tick < delayed_ticks; ++tick) {
                        run_acceptance_focused_tick(active_section_x,
                                                    active_section_y, true);
                    }
                }
                const auto cells = download_scene_cells();
                std::uint32_t mismatches = 0u;
                std::uint32_t shell = 0u;
                std::uint32_t legacy_perch_wood = 0u;
                std::uint32_t honey = 0u;
                std::uint32_t pollen = 0u;
                std::uint32_t empty_chamber = 0u;
                std::uint32_t bee_count = 0u;
                std::uint32_t bee_metadata_mismatches = 0u;
                std::uint32_t bee_position_mismatches = 0u;
                std::uint32_t swarm_bees = 0u;
                std::uint32_t fed_bees = 0u;
                std::uint32_t queen_carrier_bees = 0u;
                std::int32_t queen_carrier_dx = 0;
                std::int32_t queen_carrier_dy = 0;
                std::uint32_t queen_carrier_aux = 0u;
                std::uint32_t district_home_bees = 0u;
                std::uint32_t x_home_bees = 0u;
                std::uint32_t y_home_bees = 0u;
                std::uint32_t first_bee_aux = 0u;
                std::array<bool, fix29_bee_formation_count> bee_slots{};
                std::uint32_t unique_bee_slots = 0u;
                std::uint32_t compact_bees = 0u;
                std::uint32_t top_lobe_bees = 0u;
                std::uint32_t left_lobe_bees = 0u;
                std::uint32_t right_lobe_bees = 0u;
                for (std::int32_t dy = -18; dy <= 11; ++dy) {
                    for (std::int32_t dx = -40; dx <= 31; ++dx) {
                        const auto part = classify_pre_pr19_hive_cell(
                            dx, dy, fix29_hive_entropy(static_cast<std::int32_t>(queen_x),
                                                       static_cast<std::int32_t>(queen_y), dx, dy),
                            static_cast<std::int32_t>(queen_x),
                            static_cast<std::int32_t>(queen_y));
                        const auto x = hive_origin_x + static_cast<std::uint32_t>(
                            static_cast<std::int32_t>(queen_x) + dx);
                        const auto y = hive_origin_y + static_cast<std::uint32_t>(
                            static_cast<std::int32_t>(queen_y) + dy);
                        const auto& actual_cell = cells[index_of(x, y)];
                        const auto actual = static_cast<Material>(actual_cell.material);
                        bool matches = true;
                        if (dx >= -40 && dx <= 31 && dy >= -18 && dy <= -11 &&
                            actual == Material::wood)
                            ++legacy_perch_wood;
                        switch (part) {
                        case HivePart::shell:
                            matches = actual == Material::beehive;
                            ++shell;
                            break;
                        case HivePart::queen:
                            matches = actual == Material::queen_bee;
                            break;
                        case HivePart::honey:
                            matches = actual == Material::honey &&
                                (actual_cell.aux &
                                 (fill_aux_structural | fill_aux_supported)) ==
                                    (fill_aux_structural | fill_aux_supported);
                            ++honey;
                            break;
                        case HivePart::pollen:
                            matches = actual == Material::pollen &&
                                (actual_cell.aux &
                                 (fill_aux_structural | fill_aux_supported)) ==
                                    (fill_aux_structural | fill_aux_supported);
                            ++pollen;
                            break;
                        case HivePart::chamber:
                            matches = actual == Material::atmosphere || actual == Material::bee;
                            ++empty_chamber;
                            break;
                        case HivePart::exit:
                        case HivePart::empty:
                            matches = actual == Material::atmosphere || actual == Material::bee;
                            break;
                        }
                        if (!matches) ++mismatches;
                    }
                }
                const bool scan_complete_district = delayed_ticks > 0u && composed_world;
                const auto bee_min_dx = scan_complete_district
                    ? -static_cast<std::int32_t>(queen_x) : -36;
                const auto bee_max_dx = scan_complete_district
                    ? static_cast<std::int32_t>(pre_expansion_world_width - 1u - queen_x) : 36;
                const auto bee_min_dy = scan_complete_district
                    ? -static_cast<std::int32_t>(queen_y) : -36;
                const auto bee_max_dy = scan_complete_district
                    ? static_cast<std::int32_t>(pre_expansion_world_height - 1u - queen_y) : 24;
                for (std::int32_t dy = bee_min_dy; dy <= bee_max_dy; ++dy) {
                    for (std::int32_t dx = bee_min_dx; dx <= bee_max_dx; ++dx) {
                        const auto x = hive_origin_x + static_cast<std::uint32_t>(
                            static_cast<std::int32_t>(queen_x) + dx);
                        const auto y = hive_origin_y + static_cast<std::uint32_t>(
                            static_cast<std::int32_t>(queen_y) + dy);
                        const auto& bee = cells[index_of(x, y)];
                        if (bee.material != material_id(Material::bee)) continue;
                        ++bee_count;
                        constexpr std::uint32_t bee_swarm_bit = 0x08000000u;
                        constexpr std::uint32_t bee_fed_bit = 0x10000000u;
                        constexpr std::uint32_t bee_queen_bit = 0x40000000u;
                        const auto slot = (bee.aux >> 13u) & 127u;
                        const auto encoded_district = (bee.aux >> 20u) & 7u;
                        const auto decoded_home_x =
                            persistent_world_district_origin_x(
                                config.grid_width, encoded_district) +
                            (bee.aux & 127u) * 8u;
                        const auto decoded_home_y =
                            persistent_world_district_origin_y(
                                config.grid_height, encoded_district) +
                            ((bee.aux >> 7u) & 63u) * 8u;
                        const auto expected_home_x = hive_origin_x + queen_x;
                        const auto expected_home_y = hive_origin_y + (queen_y / 8u) * 8u;
                        if (bee_count == 1u) first_bee_aux = bee.aux;
                        swarm_bees += (bee.aux & bee_swarm_bit) != 0u ? 1u : 0u;
                        fed_bees += (bee.aux & bee_fed_bit) != 0u ? 1u : 0u;
                        if ((bee.aux & bee_queen_bit) != 0u) {
                            ++queen_carrier_bees;
                            queen_carrier_dx = dx;
                            queen_carrier_dy = dy;
                            queen_carrier_aux = bee.aux;
                        }
                        district_home_bees += encoded_district == district ? 1u : 0u;
                        x_home_bees += decoded_home_x == expected_home_x ? 1u : 0u;
                        y_home_bees += decoded_home_y == expected_home_y ? 1u : 0u;
                        const bool metadata_matches = composed_world &&
                            (bee.aux & bee_swarm_bit) != 0u &&
                            (delayed_ticks > 0u || (bee.aux & bee_fed_bit) != 0u) &&
                            encoded_district == district &&
                            decoded_home_x == expected_home_x &&
                            decoded_home_y == expected_home_y && slot < bee_slots.size();
                        if (!metadata_matches) {
                            ++bee_metadata_mismatches;
                        } else {
                            if (!bee_slots[slot]) {
                                bee_slots[slot] = true;
                                ++unique_bee_slots;
                            }
                            const auto expected_offset =
                                fix29_bee_formation_offset(slot);
                            if ((dx != expected_offset.x || dy != expected_offset.y) &&
                                !(delayed_ticks > 0u && fix29_bee_forager_slot(slot)))
                                ++bee_position_mismatches;
                            const auto compact_x = dx;
                            const auto compact_y = static_cast<std::int32_t>(queen_y) + dy -
                                static_cast<std::int32_t>((queen_y / 8u) * 8u);
                            if (compact_x >= -24 && compact_x <= 24 &&
                                compact_y >= -32 && compact_y <= 18)
                                ++compact_bees;
                            if (dy < 0)
                                ++top_lobe_bees;
                            if (dx < 0 && dy > 0)
                                ++left_lobe_bees;
                            if (dx > 0 && dy > 0)
                                ++right_lobe_bees;
                        }
                    }
                }
                const auto expected_honey = scene == Scene::sandbox ? 35u : 31u;
                const auto expected_pollen = scene == Scene::sandbox ? 13u : 9u;
                const auto expected_empty = scene == Scene::sandbox ? 8u : 16u;
                const auto hive_tile_states = download_tile_states();
                const auto hive_tile_columns = divide_round_up(config.grid_width, 8u);
                const auto global_queen_x = hive_origin_x + queen_x;
                const auto global_queen_y = hive_origin_y + queen_y;
                const auto hive_tile_index =
                    (global_queen_y / 8u) * hive_tile_columns + global_queen_x / 8u;
                const auto hive_tile_occupancy =
                    hive_tile_index < hive_tile_states.size()
                    ? hive_tile_states[hive_tile_index].occupancy : 0u;
                const auto summarized_queen_x =
                    (hive_tile_occupancy >> 7u) & 7u;
                const auto summarized_queen_y =
                    (hive_tile_occupancy >> 10u) & 7u;
                append(std::string{name},
                       mismatches == 0u && shell == 193u && legacy_perch_wood == 0u &&
                           honey == expected_honey && pollen == expected_pollen &&
                           empty_chamber == expected_empty &&
                           bee_count == fix29_bee_formation_count &&
                           bee_metadata_mismatches == 0u &&
                           unique_bee_slots == fix29_bee_formation_count &&
                           bee_position_mismatches == 0u &&
                           compact_bees >= (delayed_ticks > 0u ? 54u : 60u) &&
                           top_lobe_bees >= (delayed_ticks > 0u ? 18u : 20u) &&
                           left_lobe_bees >= (delayed_ticks > 0u ? 18u : 20u) &&
                           right_lobe_bees >= (delayed_ticks > 0u ? 18u : 20u),
                       "mismatches=" + std::to_string(mismatches) +
                           " legacy_perch_wood=" +
                           std::to_string(legacy_perch_wood) +
                           " shell=" + std::to_string(shell) +
                           " honey=" + std::to_string(honey) +
                           " pollen=" + std::to_string(pollen) +
                           " chamber_empty=" + std::to_string(empty_chamber) +
                           " bees=" + std::to_string(bee_count) +
                           " bee_metadata_mismatches=" +
                           std::to_string(bee_metadata_mismatches) +
                           " bee_position_mismatches=" +
                           std::to_string(bee_position_mismatches) +
                           " unique_slots=" + std::to_string(unique_bee_slots) +
                           " compact=" + std::to_string(compact_bees) +
                           " lobes=" + std::to_string(top_lobe_bees) + "/" +
                               std::to_string(left_lobe_bees) + "/" +
                               std::to_string(right_lobe_bees) +
                           " delayed_ticks=" + std::to_string(delayed_ticks) +
                           " world_bees=" + std::to_string(
                               count_material(cells, Material::bee)) +
                           " swarm=" + std::to_string(swarm_bees) +
                           " fed=" + std::to_string(fed_bees) +
                           " queen_carriers=" + std::to_string(queen_carrier_bees) +
                           " queen_carrier=" + std::to_string(queen_carrier_dx) + "," +
                               std::to_string(queen_carrier_dy) + "/" +
                               std::to_string(queen_carrier_aux) +
                           " district_home=" + std::to_string(district_home_bees) +
                           " x_home=" + std::to_string(x_home_bees) +
                           " y_home=" + std::to_string(y_home_bees) +
                           " first_aux=" + std::to_string(first_bee_aux) +
                           " tile_queen=" + std::to_string(summarized_queen_x) + "," +
                               std::to_string(summarized_queen_y) +
                           " tile_occupancy=" + std::to_string(hive_tile_occupancy) +
                           " tile_material=" + std::to_string(
                               hive_tile_index < hive_tile_states.size()
                                   ? hive_tile_states[hive_tile_index].material : 0u) +
                           " tile_counters=" + std::to_string(
                               hive_tile_index < hive_tile_states.size()
                                   ? hive_tile_states[hive_tile_index].counters : 0u) +
                           " tile_flags=" + std::to_string(
                               hive_tile_index < hive_tile_states.size()
                                   ? hive_tile_states[hive_tile_index].flags : 0u) +
                           " expected_tile_queen=" +
                               std::to_string(global_queen_x & 7u) + "," +
                               std::to_string(global_queen_y & 7u));
            };

            {
                const auto sandbox_district =
                    persistent_world_district_index(Scene::sandbox);
                constexpr std::int32_t local_queen_x = 310;
                constexpr std::int32_t local_queen_y = 100;
                const auto queen_x = static_cast<std::int32_t>(
                    persistent_world_district_origin_x(
                        config.grid_width, sandbox_district) + local_queen_x);
                const auto queen_y = static_cast<std::int32_t>(
                    persistent_world_district_origin_y(
                        config.grid_height, sandbox_district) + local_queen_y);
                auto cells = acceptance_atmosphere_world();
                // Seed the obsolete larger circular body, complete retired
                // perch, and metadata-free swarm that clean-air tests missed.
                const auto legacy_x = queen_x + 24;
                const auto legacy_y = queen_y;
                for (std::int32_t dy = 0; dy < 8; ++dy)
                    for (std::int32_t dx = 0; dx < 72; ++dx) {
                        const auto index = index_of(
                            static_cast<std::uint32_t>((legacy_x / 8) * 8 - 40 + dx),
                            static_cast<std::uint32_t>((legacy_y / 8) * 8 - 16 + dy));
                        cells[index] = make_fill_cell(material_id(Material::wood),
                            static_cast<std::uint32_t>(index));
                    }
                for (std::size_t slot = 0u; slot < fix29_bee_formation_count; ++slot) {
                    const auto offset = fix29_bee_formation_offset(slot);
                    const auto index = index_of(static_cast<std::uint32_t>(legacy_x + offset.x),
                        static_cast<std::uint32_t>(legacy_y + offset.y));
                    cells[index] = make_fill_cell(material_id(Material::bee),
                        static_cast<std::uint32_t>(index));
                }
                for (std::int32_t dy = -10; dy <= 10; ++dy)
                    for (std::int32_t dx = -10; dx <= 10; ++dx) {
                        const auto r2 = dx * dx + dy * dy;
                        if (r2 >= 100) continue;
                        const auto material = r2 == 0 ? Material::queen_bee :
                            (r2 >= 24 ? Material::beehive : Material::honey);
                        const auto index = index_of(static_cast<std::uint32_t>(legacy_x + dx),
                            static_cast<std::uint32_t>(legacy_y + dy));
                        cells[index] = make_fill_cell(material_id(material),
                            static_cast<std::uint32_t>(index));
                    }
                const auto unrelated_index = index_of(
                    static_cast<std::uint32_t>(queen_x - 50),
                    static_cast<std::uint32_t>(queen_y + 50));
                cells[unrelated_index] = make_fill_cell(material_id(Material::wood),
                    static_cast<std::uint32_t>(unrelated_index));
                const auto unrelated_cell = cells[unrelated_index];
                const auto loose_honey_index = unrelated_index + 2u;
                const auto loose_pollen_index = unrelated_index + 3u;
                cells[loose_honey_index] = make_fill_cell(material_id(Material::honey),
                    static_cast<std::uint32_t>(loose_honey_index));
                cells[loose_pollen_index] = make_fill_cell(material_id(Material::pollen),
                    static_cast<std::uint32_t>(loose_pollen_index));
                cells[loose_honey_index].aux &= ~(fill_aux_structural | fill_aux_supported);
                cells[loose_pollen_index].aux &= ~(fill_aux_structural | fill_aux_supported);
                upload_scene_cells(cells);
                const auto previous_selected_material =
                    state.selected_material.load(std::memory_order_acquire);
                const auto previous_placement_mode =
                    state.placement_mode.load(std::memory_order_acquire);
                const auto previous_brush_shape =
                    state.brush_shape.load(std::memory_order_acquire);
                state.selected_material.store(
                    material_id(Material::beehive), std::memory_order_release);
                // Exercise the normal edge-triggered Editor route at a
                // non-authored location. The UI polls eight times before the
                // renderer consumes the queued click; selection then changes,
                // proving that the original Beehive payload is retained.
                state.placement_mode.store(1u, std::memory_order_release);
                state.brush_shape.store(3u, std::memory_order_release);
                std::uint32_t hive_paint_requests = 0u;
                for (std::uint32_t frame = 0u; frame < 8u; ++frame) {
                    const auto action = route_world_primary_action({
                        .editor_workspace = true,
                        .pointer_over_world = true,
                        .primary_down = true,
                        .primary_pressed = frame == 0u,
                        .one_shot_paint = true,
                        .paused = frame >= 4u,
                    });
                    if (action == WorldPrimaryAction::editor_paint) {
                        ++hive_paint_requests;
                        request_beehive_placement(
                            state, queen_x + static_cast<std::int32_t>(frame),
                            queen_y);
                    }
                    state.primary_down.store(false, std::memory_order_release);
                }
                state.selected_material.store(
                    material_id(Material::sand), std::memory_order_release);
                const auto queued_hive = consume_beehive_placement(state);
                std::uint32_t hive_consumes = 0u;
                if (queued_hive) {
                    ++hive_consumes;
                    immediate_submit([&](const VkCommandBuffer command_buffer) {
                        record_paint_at_grid(
                            command_buffer, state, false, true,
                            queued_hive->x, queued_hive->y,
                            material_id(Material::beehive));
                    });
                }
                const bool second_consume_empty =
                    !consume_beehive_placement(state).has_value();
                state.selected_material.store(
                    previous_selected_material, std::memory_order_release);
                state.placement_mode.store(
                    previous_placement_mode, std::memory_order_release);
                state.brush_shape.store(
                    previous_brush_shape, std::memory_order_release);
                const auto result = download_scene_cells();
                const bool canonical_tool_signature =
                    canonical_fix29_hive_signature_at(
                        result, config.grid_width, config.grid_height,
                        static_cast<std::uint32_t>(queen_x),
                        static_cast<std::uint32_t>(queen_y));
                append("beehive_legacy_overlap_cleanup",
                    canonical_tool_signature && count_material(result, Material::queen_bee) == 1u &&
                    count_material(result, Material::bee) == 60u &&
                    count_material(result, Material::beehive) == 193u &&
                    count_material(result, Material::wood) == 1u &&
                    std::memcmp(&result[loose_honey_index], &cells[loose_honey_index], sizeof(SceneCell)) == 0 &&
                    std::memcmp(&result[loose_pollen_index], &cells[loose_pollen_index], sizeof(SceneCell)) == 0 &&
                    std::memcmp(&result[unrelated_index], &unrelated_cell, sizeof(SceneCell)) == 0,
                    "queens=" + std::to_string(count_material(result, Material::queen_bee)) +
                    " bees=" + std::to_string(count_material(result, Material::bee)) +
                    " shell=" + std::to_string(count_material(result, Material::beehive)) +
                    " unrelated_wood=" + std::to_string(count_material(result, Material::wood)));
                auto isolated_queen = acceptance_atmosphere_world();
                isolated_queen[index_of(100u, 100u)] = make_fill_cell(
                    material_id(Material::queen_bee),
                    static_cast<std::uint32_t>(index_of(100u, 100u)));
                const bool isolated_queen_rejected =
                    !canonical_fix29_hive_signature_at(
                        isolated_queen, config.grid_width, config.grid_height,
                        100u, 100u);
                // The sidebar Beehive item owns this same production paint
                // pipeline. Refresh all bounded MAP bands, then require its
                // canonical payload to be byte-identical to the placed world.
                immediate_submit([&](const VkCommandBuffer command_buffer) {
                    for (std::uint32_t slice = 0u; slice < 64u; ++slice)
                        record_map_snapshot(command_buffer);
                });
                const auto map_result = download_map_snapshot_cells();
                const bool map_payload_exact = map_result.size() == result.size() &&
                    std::memcmp(map_result.data(), result.data(),
                                result.size() * sizeof(SceneCell)) == 0;
                bool placed_bees_exact =
                    count_material(result, Material::bee) ==
                    fix29_bee_formation_count;
                std::uint32_t structural_honey = 0u;
                std::uint32_t structural_pollen = 0u;
                for (const auto& candidate : result) {
                    const bool fixed = (candidate.aux &
                        (fill_aux_structural | fill_aux_supported)) ==
                        (fill_aux_structural | fill_aux_supported);
                    if (fixed && candidate.material == material_id(Material::honey))
                        ++structural_honey;
                    if (fixed && candidate.material == material_id(Material::pollen))
                        ++structural_pollen;
                }
                for (std::size_t slot = 0u;
                     slot < fix29_bee_formation_count; ++slot) {
                    const auto offset = fix29_bee_formation_offset(slot);
                    const auto& bee = result[index_of(
                        static_cast<std::uint32_t>(queen_x + offset.x),
                        static_cast<std::uint32_t>(queen_y + offset.y))];
                    const auto encoded_slot = (bee.aux >> 13u) & 127u;
                    placed_bees_exact =
                        placed_bees_exact &&
                        bee.material == material_id(Material::bee) &&
                        encoded_slot == slot &&
                        (bee.aux & 0x18000000u) == 0x18000000u;
                }
                std::uint32_t mismatches = 0u;
                std::uint32_t legacy_perch_wood = 0u;
                std::uint32_t shell = 0u;
                std::uint32_t honey = 0u;
                std::uint32_t pollen = 0u;
                std::uint32_t chamber_empty = 0u;
                for (std::int32_t dy = -18; dy <= 11; ++dy) {
                    for (std::int32_t dx = -40; dx <= 31; ++dx) {
                        const auto part = classify_pre_pr19_hive_cell(
                            dx, dy, fix29_hive_entropy(
                                local_queen_x, local_queen_y, dx, dy),
                            local_queen_x, local_queen_y);
                        const auto x = static_cast<std::uint32_t>(queen_x + dx);
                        const auto y = static_cast<std::uint32_t>(queen_y + dy);
                        const auto& actual_cell = result[index_of(x, y)];
                        const auto actual = static_cast<Material>(actual_cell.material);
                        bool matches = true;
                        if (dx >= -40 && dx <= 31 && dy >= -18 && dy <= -11 &&
                            actual == Material::wood)
                            ++legacy_perch_wood;
                        switch (part) {
                        case HivePart::shell:
                            matches = actual == Material::beehive;
                            ++shell;
                            break;
                        case HivePart::queen:
                            matches = actual == Material::queen_bee;
                            break;
                        case HivePart::honey:
                            matches = actual == Material::honey &&
                                (actual_cell.aux &
                                 (fill_aux_structural | fill_aux_supported)) ==
                                    (fill_aux_structural | fill_aux_supported);
                            ++honey;
                            break;
                        case HivePart::pollen:
                            matches = actual == Material::pollen &&
                                (actual_cell.aux &
                                 (fill_aux_structural | fill_aux_supported)) ==
                                    (fill_aux_structural | fill_aux_supported);
                            ++pollen;
                            break;
                        case HivePart::chamber:
                            matches = actual == Material::empty;
                            ++chamber_empty;
                            break;
                        case HivePart::exit:
                            matches = actual == Material::empty;
                            break;
                        case HivePart::empty:
                            matches = actual == Material::atmosphere ||
                                      actual == Material::bee;
                            break;
                        }
                        if (!matches) ++mismatches;
                    }
                }
                append("placed_fix29_hive_exact",
                       mismatches == 0u && legacy_perch_wood == 0u && shell == 193u &&
                           honey == 35u && pollen == 13u &&
                           chamber_empty == 8u && placed_bees_exact &&
                           count_material(result, Material::beehive) == 193u &&
                           count_material(result, Material::queen_bee) == 1u &&
                           structural_honey == 35u && structural_pollen == 13u,
                       "mismatches=" + std::to_string(mismatches) +
                           " legacy_perch_wood=" +
                           std::to_string(legacy_perch_wood) +
                           " shell=" + std::to_string(shell) +
                           " honey=" + std::to_string(honey) +
                           " pollen=" + std::to_string(pollen) +
                            " chamber_empty=" + std::to_string(chamber_empty) +
                            " global_body=" + std::to_string(
                                count_material(result, Material::beehive)) + "/" +
                                std::to_string(structural_honey) + "/" +
                                std::to_string(structural_pollen) +
                            " bees_exact=" +
                            std::to_string(placed_bees_exact ? 1u : 0u));
                append("beehive_button_tool_map_payload_exact",
                       placed_bees_exact && map_payload_exact &&
                           legacy_perch_wood == 0u && mismatches == 0u,
                       "button_material=" +
                           std::to_string(material_id(Material::beehive)) +
                           " map_byte_exact=" +
                           std::to_string(map_payload_exact ? 1u : 0u) +
                           " bees=" + std::to_string(
                               count_material(map_result, Material::bee)) +
                            " legacy_perch_wood=" +
                            std::to_string(legacy_perch_wood));
                append("beehive_normal_press_is_one_shot",
                        hive_paint_requests == 1u &&
                            hive_consumes == 1u && second_consume_empty &&
                            canonical_tool_signature && isolated_queen_rejected &&
                            count_material(result, Material::queen_bee) == 1u &&
                            count_material(result, Material::bee) ==
                                fix29_bee_formation_count &&
                            tool_hive_anchor ==
                                ((static_cast<std::uint32_t>(queen_x) & 0xffffu) |
                                 ((static_cast<std::uint32_t>(queen_y) & 0xffffu)
                                  << 16u)),
                        "held_frames=8 paint_requests=" +
                            std::to_string(hive_paint_requests) +
                            " consumes=" + std::to_string(hive_consumes) +
                            " second_empty=" +
                            std::to_string(second_consume_empty ? 1u : 0u) +
                            " canonical_signature=" +
                            std::to_string(canonical_tool_signature ? 1u : 0u) +
                            " isolated_rejected=" +
                            std::to_string(isolated_queen_rejected ? 1u : 0u) +
                            " queens=" + std::to_string(
                                count_material(result, Material::queen_bee)) +
                            " bees=" + std::to_string(
                                count_material(result, Material::bee)));

                for (std::uint32_t tick = 0u; tick < 120u; ++tick)
                    run_acceptance_focused_tick(
                        queen_x / active_region_width_cells,
                        queen_y / active_region_height_cells, true, true);
                const auto delayed = download_scene_cells();
                const bool delayed_canonical_signature =
                    canonical_fix29_hive_signature_at(
                        delayed, config.grid_width, config.grid_height,
                        static_cast<std::uint32_t>(queen_x),
                        static_cast<std::uint32_t>(queen_y));
                bool delayed_bees_exact =
                    count_material(delayed, Material::bee) ==
                    fix29_bee_formation_count;
                bool delayed_bee_timers_exact = true;
                for (std::size_t slot = 0u;
                     slot < fix29_bee_formation_count; ++slot) {
                    const auto offset = fix29_bee_formation_offset(slot);
                    const auto& bee = delayed[index_of(
                        static_cast<std::uint32_t>(queen_x + offset.x),
                        static_cast<std::uint32_t>(queen_y + offset.y))];
                    delayed_bees_exact =
                        delayed_bees_exact &&
                        bee.material == material_id(Material::bee) &&
                        ((bee.aux >> 13u) & 127u) == slot;
                    delayed_bee_timers_exact = delayed_bee_timers_exact &&
                        (bee.age & 0x3fffu) == fix29_bee_initial_timer(slot) + 120u &&
                        (bee.age >> 14u) == 0x3ffffu;
                }
                std::uint32_t delayed_mismatches = 0u;
                std::uint32_t delayed_legacy_perch_wood = 0u;
                std::uint32_t delayed_shell = 0u;
                std::uint32_t delayed_honey = 0u;
                std::uint32_t delayed_pollen = 0u;
                std::string delayed_mismatch_detail;
                for (std::int32_t dy = -18; dy <= 11; ++dy) {
                    for (std::int32_t dx = -40; dx <= 31; ++dx) {
                        const auto part = classify_pre_pr19_hive_cell(
                            dx, dy, fix29_hive_entropy(
                                local_queen_x, local_queen_y, dx, dy),
                            local_queen_x, local_queen_y);
                        const auto x = static_cast<std::uint32_t>(queen_x + dx);
                        const auto y = static_cast<std::uint32_t>(queen_y + dy);
                        const auto& actual_cell = delayed[index_of(x, y)];
                        const auto actual =
                            static_cast<Material>(actual_cell.material);
                        const bool fixed = (actual_cell.aux &
                            (fill_aux_structural | fill_aux_supported)) ==
                            (fill_aux_structural | fill_aux_supported);
                        bool checked = true;
                        bool matches = true;
                        if (dx >= -40 && dx <= 31 && dy >= -18 && dy <= -11 &&
                            actual == Material::wood)
                            ++delayed_legacy_perch_wood;
                        switch (part) {
                        case HivePart::shell:
                            matches = actual == Material::beehive;
                            ++delayed_shell;
                            break;
                        case HivePart::queen:
                            matches = actual == Material::queen_bee;
                            break;
                        case HivePart::honey:
                            matches = actual == Material::honey && fixed;
                            ++delayed_honey;
                            break;
                        case HivePart::pollen:
                            matches = actual == Material::pollen && fixed;
                            ++delayed_pollen;
                            break;
                        case HivePart::chamber:
                        case HivePart::exit:
                            // The canonical opening is authored Empty; normal
                            // closed-system gas relaxation may fill it with
                            // Atmosphere without changing the hive body.
                            matches = actual == Material::empty ||
                                      actual == Material::atmosphere;
                            break;
                        default:
                            checked = false;
                            break;
                        }
                        if (checked && !matches) {
                            ++delayed_mismatches;
                            if (delayed_mismatches <= 12u) {
                                delayed_mismatch_detail +=
                                    " [" + std::to_string(dx) + "," +
                                    std::to_string(dy) + " expected=" +
                                    std::to_string(static_cast<std::uint32_t>(part)) +
                                    " actual=" + std::to_string(actual_cell.material) +
                                    " aux=" + std::to_string(actual_cell.aux) + "]";
                            }
                        }
                    }
                }
                append("placed_fix29_hive_delayed_body_exact",
                       delayed_mismatches == 0u &&
                           delayed_legacy_perch_wood == 0u &&
                           delayed_shell == 193u &&
                           delayed_honey == 35u &&
                           delayed_pollen == 13u && delayed_bees_exact &&
                           delayed_bee_timers_exact &&
                           delayed_canonical_signature &&
                           count_material(delayed, Material::beehive) == 193u &&
                           count_material(delayed, Material::queen_bee) == 1u,
                       "ticks=120 mismatches=" +
                           std::to_string(delayed_mismatches) +
                           " legacy_perch_wood=" +
                           std::to_string(delayed_legacy_perch_wood) +
                           " shell=" + std::to_string(delayed_shell) +
                           " honey=" + std::to_string(delayed_honey) +
                           " pollen=" + std::to_string(delayed_pollen) +
                            " bees_exact=" +
                            std::to_string(delayed_bees_exact ? 1u : 0u) +
                            " timers_exact=" +
                            std::to_string(delayed_bee_timers_exact ? 1u : 0u) +
                            " load_signature=" +
                            std::to_string(
                                delayed_canonical_signature ? 1u : 0u) +
                            delayed_mismatch_detail);

                auto autonomous_cells = result;
                const auto bloom_x = static_cast<std::uint32_t>((queen_x + 48) / 8 * 8);
                const auto bloom_y = static_cast<std::uint32_t>((queen_y - 40) / 8 * 8);
                for (std::uint32_t y = bloom_y; y < bloom_y + 8u; ++y)
                    for (std::uint32_t x = bloom_x; x < bloom_x + 8u; ++x)
                        autonomous_cells[index_of(x, y)] = make_fill_cell(
                            material_id(Material::flower), static_cast<std::uint32_t>(index_of(x, y)));
                upload_scene_cells(autonomous_cells);
                for (std::uint32_t tick = 0u; tick < 120u; ++tick)
                    run_acceptance_focused_tick(queen_x / active_region_width_cells,
                        queen_y / active_region_height_cells, true, true);
                const auto autonomous = download_scene_cells();
                std::array<bool, fix29_bee_formation_count> live_slots{};
                std::uint32_t moved_foragers = 0u;
                std::uint32_t stationary_nonforagers = 0u;
                std::string autonomous_detail;
                bool autonomous_homes = true;
                for (std::size_t index = 0u; index < autonomous.size(); ++index) {
                    const auto& bee = autonomous[index];
                    if (bee.material != material_id(Material::bee)) continue;
                    const auto slot = (bee.aux >> 13u) & 127u;
                    if (slot >= fix29_bee_formation_count || live_slots[slot]) {
                        autonomous_homes = false;
                        continue;
                    }
                    live_slots[slot] = true;
                    const auto offset = fix29_bee_formation_offset(slot);
                    const auto initial_index = index_of(static_cast<std::uint32_t>(queen_x + offset.x),
                        static_cast<std::uint32_t>(queen_y + offset.y));
                    autonomous_homes = autonomous_homes &&
                        (bee.aux & 0x1fffu) == (result[initial_index].aux & 0x1fffu) &&
                        ((bee.aux >> 20u) & 7u) == sandbox_district &&
                        (bee.aux & 0x08000000u) != 0u;
                    if (fix29_bee_forager_slot(slot)) {
                        moved_foragers += index != initial_index ? 1u : 0u;
                        autonomous_detail += " slot" + std::to_string(slot) + "@" +
                            std::to_string(index % config.grid_width) + "," +
                            std::to_string(index / config.grid_width) + " target=" +
                            std::to_string(fix29_bee_target_from_age(bee.age));
                    }
                    else stationary_nonforagers += index == initial_index ? 1u : 0u;
                }
                append("beehive_autonomous_six_foragers",
                    moved_foragers == 6u && stationary_nonforagers == 54u && autonomous_homes &&
                    count_material(autonomous, Material::bee) == 60u &&
                    canonical_fix29_hive_signature_at(autonomous, config.grid_width, config.grid_height,
                        static_cast<std::uint32_t>(queen_x), static_cast<std::uint32_t>(queen_y)),
                    "ticks=120 unmodified_initial_timers=1 moved_foragers=" + std::to_string(moved_foragers) +
                    " stationary_nonforagers=" + std::to_string(stationary_nonforagers) +
                    " exact_homes=" + std::to_string(autonomous_homes ? 1u : 0u) + autonomous_detail);

                const auto retained_anchor = tool_hive_anchor;
                immediate_submit([&](const VkCommandBuffer command_buffer) {
                    record_paint_at_grid(command_buffer, state, false, true, 1, queen_y,
                        material_id(Material::beehive));
                    if (config.grid_width > persistent_world_width)
                        record_paint_at_grid(command_buffer, state, false, true, 700, queen_y,
                            material_id(Material::beehive));
                });
                const auto invalid_result = download_scene_cells();
                append("beehive_invalid_placement_is_atomic",
                    retained_anchor == tool_hive_anchor &&
                    std::memcmp(invalid_result.data(), autonomous.data(), autonomous.size() * sizeof(SceneCell)) == 0,
                    "clipped_and_gap_rejected=1 anchor_retained=" +
                        std::to_string(retained_anchor == tool_hive_anchor ? 1u : 0u));

                const auto next_queen_x = queen_x + 144;
                request_beehive_placement(state, next_queen_x, queen_y);
                const auto repeated_request = consume_beehive_placement(state);
                if (repeated_request) immediate_submit([&](const VkCommandBuffer command_buffer) {
                    record_paint_at_grid(command_buffer, state, false, true,
                        repeated_request->x, repeated_request->y, material_id(Material::beehive));
                });
                const auto repeated = download_scene_cells();
                append("beehive_repeat_replaces_prior_tool_colony",
                    repeated_request.has_value() && count_material(repeated, Material::queen_bee) == 1u &&
                    count_material(repeated, Material::bee) == 60u &&
                    count_material(repeated, Material::beehive) == 193u &&
                    canonical_fix29_hive_signature_at(repeated, config.grid_width, config.grid_height,
                        static_cast<std::uint32_t>(next_queen_x), static_cast<std::uint32_t>(queen_y)) &&
                    repeated[index_of(static_cast<std::uint32_t>(queen_x), static_cast<std::uint32_t>(queen_y))].material ==
                        material_id(Material::atmosphere) &&
                    std::memcmp(&repeated[unrelated_index], &autonomous[unrelated_index], sizeof(SceneCell)) == 0,
                    "queens=" + std::to_string(count_material(repeated, Material::queen_bee)) +
                    " bees=" + std::to_string(count_material(repeated, Material::bee)) +
                    " shell=" + std::to_string(count_material(repeated, Material::beehive)));

                // Force the frozen legacy Empty -> Beehive proposal once. The
                // shallow rejection must restore the Empty source and reverse
                // the CREATED counter (not underflow CONVERTED).
                const auto saved_growth_step = simulation_step;
                constexpr std::uint32_t growth_x = 100u;
                constexpr std::uint32_t growth_y = 100u;
                auto retired_growth_cells = acceptance_atmosphere_world();
                auto growth_source = make_fill_cell(
                    material_id(Material::empty),
                    static_cast<std::uint32_t>(index_of(growth_x, growth_y)));
                growth_source.age = 0u;
                growth_source.aux = 0u;
                retired_growth_cells[index_of(growth_x, growth_y)] = growth_source;
                retired_growth_cells[index_of(growth_x + 1u, growth_y)] =
                    make_fill_cell(
                        material_id(Material::beehive),
                        static_cast<std::uint32_t>(
                            index_of(growth_x + 1u, growth_y)));
                retired_growth_cells[index_of(growth_x + 3u, growth_y)] =
                    make_fill_cell(
                        material_id(Material::queen_bee),
                        static_cast<std::uint32_t>(
                            index_of(growth_x + 3u, growth_y)));
                bool growth_step_found = false;
                for (std::uint32_t candidate = 0u;
                     candidate < 1'048'576u; ++candidate) {
                    const auto random_value = fill_hash(
                        growth_x * 73856093u ^ growth_y * 19349663u ^
                        candidate * 83492791u ^ random_seed);
                    if ((random_value & 4095u) == 0u) {
                        simulation_step = candidate;
                        growth_step_found = true;
                        break;
                    }
                }
                upload_scene_cells(retired_growth_cells);
                if (growth_step_found) run_acceptance_chemistry_pass();
                const auto rejected_growth = download_scene_cells();
                const auto growth_counters = download_conservation_counters();
                const auto& restored_growth_source =
                    rejected_growth[index_of(growth_x, growth_y)];
                const bool growth_source_restored =
                    restored_growth_source.material == material_id(Material::empty) &&
                    restored_growth_source.age == growth_source.age + 1u &&
                    restored_growth_source.temperature == growth_source.temperature &&
                    restored_growth_source.aux == growth_source.aux;
                const bool growth_counters_clear = std::all_of(
                    growth_counters.begin(), growth_counters.end(),
                    [](const std::uint32_t value) { return value == 0u; });
                const auto retained_hives =
                    count_material(rejected_growth, Material::beehive);
                const auto retained_queens =
                    count_material(rejected_growth, Material::queen_bee);
                const auto retired_bees =
                    count_material(rejected_growth, Material::bee);
                append("retired_hive_growth_rejected_without_false_conservation",
                       growth_step_found && growth_source_restored &&
                           retained_hives == 1u && retained_queens == 1u &&
                           retired_bees == 0u && growth_counters_clear,
                       "step_found=" +
                           std::to_string(growth_step_found ? 1u : 0u) +
                           " source_restored=" +
                           std::to_string(growth_source_restored ? 1u : 0u) +
                           " hives=" + std::to_string(retained_hives) +
                           " queens=" + std::to_string(retained_queens) +
                           " bees=" + std::to_string(retired_bees) +
                           " counters_clear=" +
                           std::to_string(growth_counters_clear ? 1u : 0u) +
                           " created=" + std::to_string(growth_counters[0]) +
                           " converted=" + std::to_string(growth_counters[2]));
                simulation_step = saved_growth_step;

                // Reject only the retired Empty -> Bee proposal. A stressed
                // Queen becoming the current lifecycle's migrating queen
                // carrier is a legitimate, one-for-one material conversion.
                constexpr std::uint32_t migration_queen_x = 64u;
                constexpr std::uint32_t migration_queen_y = 96u;
                constexpr std::uint32_t migration_flower_x = 144u;
                constexpr std::uint32_t migration_flower_y = 96u;
                auto migration_cells = acceptance_atmosphere_world();
                auto migration_queen = make_fill_cell(
                    material_id(Material::queen_bee),
                    static_cast<std::uint32_t>(
                        index_of(migration_queen_x, migration_queen_y)));
                migration_queen.age = 36'001u;
                migration_queen.aux =
                    (migration_queen.aux & ~fill_aux_state_mask) | 240u;
                migration_cells[index_of(migration_queen_x, migration_queen_y)] =
                    migration_queen;
                migration_cells[index_of(migration_flower_x, migration_flower_y)] =
                    make_fill_cell(
                        material_id(Material::flower),
                        static_cast<std::uint32_t>(
                            index_of(migration_flower_x, migration_flower_y)));
                upload_scene_cells(migration_cells);
                run_acceptance_tile_pass();
                run_acceptance_chemistry_pass();
                const auto migrated_queen_cells = download_scene_cells();
                const auto migration_counters = download_conservation_counters();
                const auto& queen_carrier =
                    migrated_queen_cells[index_of(migration_queen_x,
                                                  migration_queen_y)];
                constexpr std::uint32_t bee_queen_carrier_bit = 0x40000000u;
                constexpr std::uint32_t bee_swarm_owner_bit = 0x08000000u;
                const auto migration_tile_columns =
                    divide_round_up(config.grid_width, 8u);
                const auto expected_migration_target =
                    (migration_flower_y / 8u) * migration_tile_columns +
                    migration_flower_x / 8u;
                const auto actual_migration_target =
                    (queen_carrier.age >> 14u) & 0x3ffffu;
                constexpr std::uint32_t bee_target_none = 0x3ffffu;
                append("queen_to_bee_migration_remains_live",
                       queen_carrier.material == material_id(Material::bee) &&
                           (queen_carrier.aux & bee_queen_carrier_bit) != 0u &&
                           (queen_carrier.aux & bee_swarm_owner_bit) != 0u &&
                           actual_migration_target != bee_target_none &&
                           migration_counters[0] == 0u &&
                           migration_counters[1] == 0u &&
                           migration_counters[2] == 1u &&
                           migration_counters[3] == 0u &&
                           migration_counters[7] == 0u,
                       "material=" + std::to_string(queen_carrier.material) +
                           " queen_carrier=" + std::to_string(
                               (queen_carrier.aux & bee_queen_carrier_bit) != 0u
                                   ? 1u : 0u) +
                           " swarm=" + std::to_string(
                               (queen_carrier.aux & bee_swarm_owner_bit) != 0u
                                   ? 1u : 0u) +
                           " target=" + std::to_string(actual_migration_target) +
                           "/" + std::to_string(expected_migration_target) +
                           " converted=" +
                           std::to_string(migration_counters[2]));

                // Exercise the real persistent-World lifecycle using the same
                // button-placed colony. The chosen forager first acquires an
                // addressable Large-world flower tile, then picks up pollen,
                // returns through the no-perch body, deposits one exact Pollen
                // cell, and completes the two-tick Honey feed handshake.
                constexpr std::uint32_t bee_pollen_bit = 0x20000000u;
                constexpr std::uint32_t bee_fed_bit = 0x10000000u;
                const auto tile_columns = divide_round_up(config.grid_width, 8u);
                const auto active_section_x = static_cast<std::int32_t>(
                    static_cast<std::uint32_t>(queen_x) /
                    active_region_width_cells);
                const auto active_section_y = static_cast<std::int32_t>(
                    static_cast<std::uint32_t>(queen_y) /
                    active_region_height_cells);
                std::size_t forager_slot = 0u;
                while (forager_slot < fix29_bee_formation_count &&
                       !fix29_bee_forager_slot(forager_slot))
                    ++forager_slot;
                const auto forager_offset =
                    fix29_bee_formation_offset(forager_slot);
                const auto forager_x = static_cast<std::uint32_t>(
                    queen_x + forager_offset.x);
                const auto forager_y = static_cast<std::uint32_t>(
                    queen_y + forager_offset.y);
                const auto flower_x = forager_x + 24u;
                const auto flower_y = forager_y;
                const auto flower_tile = (flower_y / 8u) * tile_columns +
                                         flower_x / 8u;

                auto lifecycle_cells = result;
                lifecycle_cells[index_of(flower_x, flower_y)] = make_fill_cell(
                    material_id(Material::flower),
                    static_cast<std::uint32_t>(index_of(flower_x, flower_y)));
                auto& departing_bee = lifecycle_cells[index_of(forager_x, forager_y)];
                const auto departure_threshold = 1200u +
                    static_cast<std::uint32_t>((forager_slot * 29u) % 600u);
                departing_bee.age = fix29_bee_pack_age(
                    departure_threshold - 1u, fix29_bee_target_none);
                upload_scene_cells(lifecycle_cells);
                run_acceptance_tile_pass(active_section_x, active_section_y, true);
                run_acceptance_chemistry_pass(active_section_x, active_section_y, true);
                const auto departed = download_scene_cells();
                const auto& departed_bee = departed[index_of(forager_x, forager_y)];
                const bool flower_target_acquired =
                    departed_bee.material == material_id(Material::bee) &&
                    fix29_bee_target_from_age(departed_bee.age) == flower_tile &&
                    (departed_bee.aux & bee_pollen_bit) == 0u;

                auto pickup_cells = departed;
                pickup_cells[index_of(forager_x, forager_y)] = make_fill_cell(
                    material_id(Material::atmosphere),
                    static_cast<std::uint32_t>(index_of(forager_x, forager_y)));
                const auto pickup_x = flower_x - 1u;
                const auto pickup_y = flower_y;
                pickup_cells[index_of(pickup_x, pickup_y)] = departed_bee;
                upload_scene_cells(pickup_cells);
                run_acceptance_tile_pass(active_section_x, active_section_y, true);
                run_acceptance_chemistry_pass(active_section_x, active_section_y, true);
                const auto picked_up = download_scene_cells();
                const auto& pollen_bee = picked_up[index_of(pickup_x, pickup_y)];
                const bool pollen_picked_up =
                    pollen_bee.material == material_id(Material::bee) &&
                    (pollen_bee.aux & bee_pollen_bit) != 0u &&
                    (pollen_bee.aux & bee_fed_bit) == 0u;

                auto deposit_cells = picked_up;
                deposit_cells[index_of(pickup_x, pickup_y)] = make_fill_cell(
                    material_id(Material::atmosphere),
                    static_cast<std::uint32_t>(index_of(pickup_x, pickup_y)));
                const auto deposit_x = static_cast<std::uint32_t>(queen_x + 10);
                const auto deposit_y = static_cast<std::uint32_t>(queen_y + 1);
                deposit_cells[index_of(deposit_x, deposit_y)] = pollen_bee;
                const auto pollen_before_deposit =
                    count_material(deposit_cells, Material::pollen);
                upload_scene_cells(deposit_cells);
                run_acceptance_tile_pass(active_section_x, active_section_y, true);
                run_acceptance_chemistry_pass(active_section_x, active_section_y, true);
                const auto deposited = download_scene_cells();
                const auto& returned_bee = deposited[index_of(deposit_x, deposit_y)];
                const bool pollen_deposited =
                    returned_bee.material == material_id(Material::bee) &&
                    (returned_bee.aux & (bee_pollen_bit | bee_fed_bit)) == 0u &&
                    count_material(deposited, Material::pollen) ==
                        pollen_before_deposit + 1u;

                constexpr std::array<std::pair<std::int32_t, std::int32_t>, 8>
                    lifecycle_neighbors{{
                        {-1, -1}, {0, -1}, {1, -1}, {-1, 0},
                        {1, 0}, {-1, 1}, {0, 1}, {1, 1}}};
                std::uint32_t honey_x = 0u;
                std::uint32_t honey_y = 0u;
                std::uint32_t feeding_x = 0u;
                std::uint32_t feeding_y = 0u;
                bool feeding_site_found = false;
                for (std::int32_t dy = -4; dy <= 4 && !feeding_site_found; ++dy) {
                    for (std::int32_t dx = -4; dx <= 4 && !feeding_site_found; ++dx) {
                        const auto candidate_x = static_cast<std::uint32_t>(queen_x + dx);
                        const auto candidate_y = static_cast<std::uint32_t>(queen_y + dy);
                        if (deposited[index_of(candidate_x, candidate_y)].material !=
                            material_id(Material::honey))
                            continue;
                        for (const auto& [neighbor_x, neighbor_y] : lifecycle_neighbors) {
                            const auto open_x = static_cast<std::uint32_t>(
                                static_cast<std::int32_t>(candidate_x) + neighbor_x);
                            const auto open_y = static_cast<std::uint32_t>(
                                static_cast<std::int32_t>(candidate_y) + neighbor_y);
                            if (deposited[index_of(open_x, open_y)].material ==
                                material_id(Material::empty)) {
                                honey_x = candidate_x;
                                honey_y = candidate_y;
                                feeding_x = open_x;
                                feeding_y = open_y;
                                feeding_site_found = true;
                                break;
                            }
                        }
                    }
                }

                auto feeding_cells = deposited;
                feeding_cells[index_of(deposit_x, deposit_y)] = make_fill_cell(
                    material_id(Material::empty),
                    static_cast<std::uint32_t>(index_of(deposit_x, deposit_y)));
                if (feeding_site_found) {
                    auto feeding_bee = returned_bee;
                    feeding_bee.age = fix29_bee_pack_age(
                        0u, (honey_y / 8u) * tile_columns + honey_x / 8u);
                    feeding_cells[index_of(feeding_x, feeding_y)] = feeding_bee;
                }
                upload_scene_cells(feeding_cells);
                run_acceptance_tile_pass(active_section_x, active_section_y, true);
                run_acceptance_chemistry_pass(active_section_x, active_section_y, true);
                run_acceptance_chemistry_pass(active_section_x, active_section_y, true);
                const auto fed_cells = download_scene_cells();
                const auto& fed_bee = fed_cells[index_of(feeding_x, feeding_y)];
                const bool honey_fed = feeding_site_found &&
                    fed_bee.material == material_id(Material::bee) &&
                    (fed_bee.aux & bee_fed_bit) != 0u &&
                    (fed_bee.aux & bee_pollen_bit) == 0u;
                const auto final_bee_count = count_material(fed_cells, Material::bee);

                append("bee_lifecycle_gpu_transitions",
                       flower_target_acquired && pollen_picked_up &&
                           pollen_deposited && honey_fed &&
                           final_bee_count == fix29_bee_formation_count,
                       "forager_slot=" + std::to_string(forager_slot) +
                           " flower_tile=" + std::to_string(flower_tile) +
                           " target_capacity=" +
                               std::to_string(fix29_bee_target_none) +
                           " target_acquired=" +
                               std::to_string(flower_target_acquired ? 1u : 0u) +
                           " pollen_pickup=" +
                               std::to_string(pollen_picked_up ? 1u : 0u) +
                           " pollen_deposit=" +
                               std::to_string(pollen_deposited ? 1u : 0u) +
                           " feeding_site=" +
                               std::to_string(feeding_site_found ? 1u : 0u) +
                            " honey_fed=" +
                                std::to_string(honey_fed ? 1u : 0u) +
                            " final_bees=" + std::to_string(final_bee_count));

                // Close the lifecycle boundary that the frozen general chemistry
                // kernel cannot own: one hazard death must recover to exactly 60,
                // fill the actual missing current-format slot, and never admit a
                // 61st legacy newborn. Repeat from three distinct formation
                // owners so this is a result test rather than one lucky seed.
                const auto saved_lifecycle_step = simulation_step;
                constexpr std::array<std::pair<std::int32_t, std::int32_t>, 8>
                    bee_neighbors{{
                        {-1, -1}, {0, -1}, {1, -1}, {-1, 0},
                        {1, 0}, {-1, 1}, {0, 1}, {1, 1}}};
                const auto find_birth_candidate =
                    [&](const std::vector<SceneCell>& candidate_cells) {
                        for (std::int32_t dy = -2; dy <= 2; ++dy) {
                            for (std::int32_t dx = -2; dx <= 2; ++dx) {
                                const auto distance = dx * dx + dy * dy;
                                if (distance == 0 || distance > 6) continue;
                                const auto x = static_cast<std::uint32_t>(
                                    static_cast<std::int32_t>(queen_x) + dx);
                                const auto y = static_cast<std::uint32_t>(
                                    static_cast<std::int32_t>(queen_y) + dy);
                                const auto birth_medium =
                                    candidate_cells[index_of(x, y)].material;
                                if (birth_medium != material_id(Material::empty) &&
                                    birth_medium !=
                                        material_id(Material::atmosphere))
                                    continue;
                                bool food_neighbor = false;
                                bool nest_frontier = false;
                                for (const auto& [nx, ny] : bee_neighbors) {
                                    const auto material = candidate_cells[index_of(
                                        static_cast<std::uint32_t>(
                                            static_cast<std::int32_t>(x) + nx),
                                        static_cast<std::uint32_t>(
                                            static_cast<std::int32_t>(y) + ny))].material;
                                    food_neighbor = food_neighbor ||
                                        material == material_id(Material::honey) ||
                                        material == material_id(Material::pollen);
                                    nest_frontier = nest_frontier ||
                                        material == material_id(Material::queen_bee) ||
                                        material == material_id(Material::beehive);
                                }
                                std::uint32_t nearby_bees = 0u;
                                std::uint32_t nearby_hive = 0u;
                                for (std::int32_t oy = -6; oy <= 6; ++oy) {
                                    for (std::int32_t ox = -6; ox <= 6; ++ox) {
                                        if (ox * ox + oy * oy > 36) continue;
                                        const auto nearby_material = candidate_cells[index_of(
                                            static_cast<std::uint32_t>(
                                                static_cast<std::int32_t>(x) + ox),
                                            static_cast<std::uint32_t>(
                                                static_cast<std::int32_t>(y) + oy))].material;
                                        nearby_bees += nearby_material ==
                                            material_id(Material::bee) ? 1u : 0u;
                                        nearby_hive += ox * ox + oy * oy <= 25 &&
                                            nearby_material == material_id(Material::beehive)
                                            ? 1u : 0u;
                                    }
                                }
                                if (nest_frontier && food_neighbor && nearby_bees < 4u &&
                                    nearby_hive < 36u)
                                    return std::pair{x, y};
                            }
                        }
                        return std::pair{config.grid_width, config.grid_height};
                    };
                const auto find_birth_step =
                    [&]() {
                        std::uint32_t step = 0u;
                        bool found = false;
                        for (std::uint32_t candidate = 0u;
                             candidate < 1'048'576u && !found; ++candidate) {
                            if ((candidate & 4095u) == 0u) {
                                step = candidate;
                                found = true;
                            }
                        }
                        return std::pair{step, found};
                    };

                bool replacement_cycles_passed = true;
                std::array<bool, fix29_bee_formation_count> removed_slots{};
                std::string replacement_detail;
                auto chained_cycle_cells = result;
                const auto district_origin_x =
                    persistent_world_district_origin_x(
                        config.grid_width, sandbox_district);
                const auto district_origin_y =
                    persistent_world_district_origin_y(
                        config.grid_height, sandbox_district);
                const auto expected_home_x =
                    static_cast<std::uint32_t>(queen_x -
                        static_cast<std::int32_t>(district_origin_x)) / 8u;
                const auto expected_home_y =
                    static_cast<std::uint32_t>(queen_y -
                        static_cast<std::int32_t>(district_origin_y)) / 8u;
                const auto lifecycle_save_root = executable_directory() /
                    "runtime-acceptance-hive-cycle";
                std::error_code lifecycle_cleanup_error;
                std::filesystem::remove_all(
                    lifecycle_save_root, lifecycle_cleanup_error);
                bool lifecycle_round_trips_passed =
                    !lifecycle_cleanup_error;
                std::string lifecycle_round_trip_detail;
                const WorldSaveActorState lifecycle_saved_actor{
                    .x = 2088,
                    .y = 1111,
                    .velocity_y = 0,
                    .enabled = 1u,
                    .gold = 17u,
                    .iron = 23u,
                    .ammo = 91u,
                    .shot_timer = 0u,
                    .move_cooldown = 0u,
                    .grounded = 1u,
                    .health = 201u,
                    .oxygen = 187u,
                    .hit_x = -1,
                    .hit_y = -1,
                    .scene = static_cast<std::uint32_t>(world_scene),
                    .exposure_ticks = 3u,
                    .aluminum = 5u,
                    .copper = 6u,
                    .unlocks = 15u,
                    .drill_level = 2u,
                };
                const WorldSaveOwners lifecycle_saved_owners{
                    .actor_present = true,
                    .actor = lifecycle_saved_actor,
                };
                upload_actor_state(lifecycle_saved_actor);
                const WorldSaveMetadata lifecycle_save_metadata{
                    .world_size = config.world_size,
                    .width = config.grid_width,
                    .height = config.grid_height,
                    .scene = world_scene,
                };
                for (std::uint32_t cycle = 0u; cycle < 3u; ++cycle) {
                    auto hazard_cells = chained_cycle_cells;
                    std::size_t removed_slot = fix29_bee_formation_count;
                    std::uint32_t fire_x = config.grid_width;
                    std::uint32_t fire_y = config.grid_height;
                    for (std::size_t slot = 0u;
                         slot < fix29_bee_formation_count &&
                         removed_slot == fix29_bee_formation_count; ++slot) {
                        if (removed_slots[slot]) continue;
                        const auto offset = fix29_bee_formation_offset(slot);
                        const auto bee_x = static_cast<std::uint32_t>(
                            static_cast<std::int32_t>(queen_x) + offset.x);
                        const auto bee_y = static_cast<std::uint32_t>(
                            static_cast<std::int32_t>(queen_y) + offset.y);
                        for (const auto& [dx, dy] : bee_neighbors) {
                            const auto candidate_x = static_cast<std::uint32_t>(
                                static_cast<std::int32_t>(bee_x) + dx);
                            const auto candidate_y = static_cast<std::uint32_t>(
                                static_cast<std::int32_t>(bee_y) + dy);
                            if (hazard_cells[index_of(candidate_x, candidate_y)].material !=
                                material_id(Material::atmosphere))
                                continue;
                            std::uint32_t adjacent_bees = 0u;
                            for (const auto& [nx, ny] : bee_neighbors) {
                                adjacent_bees += hazard_cells[index_of(
                                    static_cast<std::uint32_t>(
                                        static_cast<std::int32_t>(candidate_x) + nx),
                                    static_cast<std::uint32_t>(
                                        static_cast<std::int32_t>(candidate_y) + ny))].material ==
                                    material_id(Material::bee) ? 1u : 0u;
                            }
                            if (adjacent_bees == 1u) {
                                removed_slot = slot;
                                fire_x = candidate_x;
                                fire_y = candidate_y;
                                break;
                            }
                        }
                    }

                    bool cycle_passed =
                        removed_slot < fix29_bee_formation_count;
                    if (cycle_passed) {
                        removed_slots[removed_slot] = true;
                        hazard_cells[index_of(fire_x, fire_y)] = make_fill_cell(
                            material_id(Material::fire),
                            static_cast<std::uint32_t>(index_of(fire_x, fire_y)));
                        upload_scene_cells(hazard_cells);
                        run_acceptance_tile_pass(
                            active_section_x, active_section_y, true);
                        run_acceptance_chemistry_pass(
                            active_section_x, active_section_y, true);
                    }
                    const auto hazarded = cycle_passed
                        ? download_scene_cells() : std::vector<SceneCell>{};
                    const auto removed_offset = cycle_passed
                        ? fix29_bee_formation_offset(removed_slot)
                        : FormationOffset{};
                    const auto removed_x = cycle_passed
                        ? static_cast<std::uint32_t>(
                            static_cast<std::int32_t>(queen_x) + removed_offset.x)
                        : 0u;
                    const auto removed_y = cycle_passed
                        ? static_cast<std::uint32_t>(
                            static_cast<std::int32_t>(queen_y) + removed_offset.y)
                        : 0u;
                    const bool hazard_death = cycle_passed &&
                        count_material(hazarded, Material::bee) ==
                            fix29_bee_formation_count - 1u &&
                        hazarded[index_of(removed_x, removed_y)].material ==
                            material_id(Material::ash);

                    auto replacement_cells = hazard_death ? hazarded : result;
                    if (hazard_death) {
                        replacement_cells[index_of(fire_x, fire_y)] =
                            make_fill_cell(
                                material_id(Material::atmosphere),
                                static_cast<std::uint32_t>(
                                    index_of(fire_x, fire_y)));
                    }
                    const auto [birth_x, birth_y] =
                        find_birth_candidate(replacement_cells);
                    const bool birth_site_found =
                        birth_x < config.grid_width && birth_y < config.grid_height;
                    const auto [birth_step, birth_step_found] =
                        birth_site_found
                        ? find_birth_step()
                        : std::pair{0u, false};
                    std::uint32_t classified_bees = 0u;
                    if (hazard_death && birth_site_found && birth_step_found) {
                        upload_scene_cells(replacement_cells);
                        run_acceptance_tile_pass(
                            active_section_x, active_section_y, true);
                        const auto classified_tiles = download_tile_states();
                        const auto classified_tile_columns =
                            (config.grid_width + 7u) / 8u;
                        for (std::uint32_t tile_y = district_origin_y / 8u;
                             tile_y <= (district_origin_y + 359u) / 8u;
                             ++tile_y) {
                            for (std::uint32_t tile_x = district_origin_x / 8u;
                                 tile_x <= (district_origin_x + 639u) / 8u;
                                 ++tile_x) {
                                classified_bees +=
                                    (classified_tiles[tile_y * classified_tile_columns + tile_x]
                                         .occupancy >> 25u) & 127u;
                            }
                        }
                        simulation_step = birth_step;
                        run_acceptance_chemistry_pass(
                            active_section_x, active_section_y, true);
                    }
                    const auto born = hazard_death && birth_site_found &&
                                      birth_step_found
                        ? download_scene_cells() : std::vector<SceneCell>{};
                    const auto& newborn = birth_site_found && !born.empty()
                        ? born[index_of(birth_x, birth_y)]
                        : result[index_of(queen_x, queen_y)];
                    const auto born_bee_count = born.empty()
                        ? 0u : count_material(born, Material::bee);
                    const bool replacement_born =
                        !born.empty() &&
                        count_material(born, Material::bee) ==
                            fix29_bee_formation_count &&
                        newborn.material == material_id(Material::bee) &&
                        ((newborn.aux >> 13u) & 127u) == removed_slot &&
                        (newborn.aux & 127u) ==
                            static_cast<std::uint32_t>(queen_x -
                                static_cast<std::int32_t>(district_origin_x)) / 8u &&
                        ((newborn.aux >> 7u) & 63u) ==
                            static_cast<std::uint32_t>(queen_y -
                                static_cast<std::int32_t>(district_origin_y)) / 8u;

                    if (replacement_born) {
                        // Observe the complete animated chamber -> exit ->
                        // outside-lane -> missing-slot return. A replacement
                        // advances every fixed tick, unlike the established
                        // colony's quarter cadence. The route is under 128
                        // cardinal steps; 384 complete production ticks retain
                        // threefold margin plus the correction tick that clears
                        // its reserved newborn target after reaching the slot.
                        for (std::uint32_t tick = 0u; tick < 384u; ++tick)
                            run_acceptance_focused_tick(
                                active_section_x, active_section_y, true, true);
                    }
                    const auto settled = replacement_born
                        ? download_scene_cells() : std::vector<SceneCell>{};
                    std::uint32_t formation_mismatches = 0u;
                    std::int32_t replacement_x = -1;
                    std::int32_t replacement_y = -1;
                    std::uint32_t replacement_age = 0u;
                    std::uint32_t replacement_aux = 0u;
                    std::array<std::int32_t, fix29_bee_formation_count>
                        bee_position_x{};
                    std::array<std::int32_t, fix29_bee_formation_count>
                        bee_position_y{};
                    bee_position_x.fill(-1);
                    bee_position_y.fill(-1);
                    if (!settled.empty()) {
                        for (std::uint32_t y = 0u; y < config.grid_height; ++y) {
                            for (std::uint32_t x = 0u; x < config.grid_width; ++x) {
                                const auto& bee = settled[index_of(x, y)];
                                if (bee.material != material_id(Material::bee))
                                    continue;
                                const auto slot = (bee.aux >> 13u) & 127u;
                                if (slot < fix29_bee_formation_count) {
                                    bee_position_x[slot] = static_cast<std::int32_t>(x);
                                    bee_position_y[slot] = static_cast<std::int32_t>(y);
                                }
                                if (slot == removed_slot) {
                                    replacement_x = static_cast<std::int32_t>(x);
                                    replacement_y = static_cast<std::int32_t>(y);
                                    replacement_age = bee.age;
                                    replacement_aux = bee.aux;
                                }
                            }
                        }
                    }
                    std::string mismatch_detail;
                    if (!settled.empty()) {
                        for (std::size_t slot = 0u;
                             slot < fix29_bee_formation_count; ++slot) {
                            const auto offset = fix29_bee_formation_offset(slot);
                            const auto& bee = settled[index_of(
                                static_cast<std::uint32_t>(
                                    static_cast<std::int32_t>(queen_x) + offset.x),
                                static_cast<std::uint32_t>(
                                    static_cast<std::int32_t>(queen_y) + offset.y))];
                            if (bee.material != material_id(Material::bee) ||
                                ((bee.aux >> 13u) & 127u) != slot) {
                                ++formation_mismatches;
                                if (formation_mismatches <= 8u) {
                                    mismatch_detail += " s" + std::to_string(slot) +
                                        "@" + std::to_string(bee_position_x[slot]) +
                                        "," + std::to_string(bee_position_y[slot]);
                                }
                            }
                        }
                    }
                    const bool exact_formation = replacement_born &&
                        count_material(settled, Material::bee) ==
                            fix29_bee_formation_count &&
                        formation_mismatches == 0u;

                    auto capped_cells = exact_formation ? settled : result;
                    const auto [cap_x, cap_y] =
                        find_birth_candidate(capped_cells);
                    const bool cap_site_found =
                        cap_x < config.grid_width && cap_y < config.grid_height;
                    const auto [cap_step, cap_step_found] = cap_site_found
                        ? find_birth_step()
                        : std::pair{0u, false};
                    if (exact_formation && cap_site_found && cap_step_found) {
                        upload_scene_cells(capped_cells);
                        run_acceptance_tile_pass(
                            active_section_x, active_section_y, true);
                        simulation_step = cap_step;
                        run_acceptance_chemistry_pass(
                            active_section_x, active_section_y, true);
                    }
                    const auto capped = exact_formation && cap_site_found &&
                                        cap_step_found
                        ? download_scene_cells() : std::vector<SceneCell>{};
                    const auto capped_newborns = std::count_if(
                        capped.begin(), capped.end(), [&](const SceneCell& cell) {
                            return cell.material == material_id(Material::bee) &&
                                fix29_bee_target_from_age(cell.age) ==
                                    fix29_bee_target_newborn;
                        });
                    const bool cap_held = !capped.empty() &&
                        count_material(capped, Material::bee) ==
                            fix29_bee_formation_count &&
                        capped_newborns == 0;
                    cycle_passed = hazard_death && replacement_born &&
                        exact_formation && cap_held;

                    // A repeated lifecycle is authoritative only when the
                    // exact recovered colony, not the original seed, survives
                    // schema-2 persistence and becomes the next cycle's input.
                    std::string cycle_save_error;
                    std::string cycle_load_error;
                    bool cycle_save_ok = false;
                    bool cycle_load_ok = false;
                    bool cycle_cells_exact = false;
                    bool cycle_actor_exact = false;
                    bool cycle_gpu_exact = false;
                    bool cycle_homes_exact = false;
                    if (cycle_passed && lifecycle_round_trips_passed) {
                        cycle_save_ok = save_world(
                            lifecycle_save_root, lifecycle_save_metadata,
                            "hive-cycle", capped, lifecycle_saved_owners,
                            cycle_save_error);
                        std::vector<SceneCell> loaded(capped.size());
                        WorldSaveOwners loaded_owners{};
                        WorldSaveMetadata loaded_metadata{};
                        cycle_load_ok = cycle_save_ok && load_world(
                            lifecycle_save_root, config.world_size,
                            config.grid_width, config.grid_height, world_scene,
                            "hive-cycle", loaded, loaded_owners,
                            loaded_metadata, cycle_load_error);
                        cycle_cells_exact = cycle_load_ok &&
                            std::memcmp(capped.data(), loaded.data(),
                                        capped.size() * sizeof(SceneCell)) == 0;
                        cycle_actor_exact = cycle_load_ok &&
                            loaded_owners.actor_present &&
                            std::memcmp(
                                &lifecycle_saved_owners.actor,
                                &loaded_owners.actor,
                                sizeof(lifecycle_saved_owners.actor)) == 0 &&
                            loaded_metadata.format_version ==
                                world_save_format_version &&
                            loaded_metadata.owner_payload_bytes ==
                                world_save_actor_bytes + 16u;
                        if (cycle_cells_exact && cycle_actor_exact) {
                            upload_scene_cells(loaded);
                            upload_actor_state(loaded_owners.actor);
                            auto gpu_loaded = download_scene_cells();
                            const auto gpu_actor = download_actor_state();
                            cycle_gpu_exact =
                                std::memcmp(
                                    loaded.data(), gpu_loaded.data(),
                                    loaded.size() * sizeof(SceneCell)) == 0 &&
                                std::memcmp(
                                    &loaded_owners.actor, &gpu_actor,
                                    sizeof(gpu_actor)) == 0;
                            std::uint32_t loaded_home_mismatches = 0u;
                            for (std::size_t slot = 0u;
                                 slot < fix29_bee_formation_count; ++slot) {
                                const auto offset =
                                    fix29_bee_formation_offset(slot);
                                const auto& bee = gpu_loaded[index_of(
                                    static_cast<std::uint32_t>(
                                        static_cast<std::int32_t>(queen_x) +
                                        offset.x),
                                    static_cast<std::uint32_t>(
                                        static_cast<std::int32_t>(queen_y) +
                                        offset.y))];
                                loaded_home_mismatches +=
                                    bee.material != material_id(Material::bee) ||
                                    ((bee.aux >> 13u) & 127u) != slot ||
                                    (bee.aux & 127u) != expected_home_x ||
                                    ((bee.aux >> 7u) & 63u) != expected_home_y
                                        ? 1u : 0u;
                            }
                            cycle_homes_exact = cycle_gpu_exact &&
                                count_material(gpu_loaded, Material::bee) ==
                                    fix29_bee_formation_count &&
                                loaded_home_mismatches == 0u;
                            if (cycle_homes_exact)
                                chained_cycle_cells = std::move(gpu_loaded);
                        }
                    }
                    const bool cycle_round_trip =
                        cycle_save_ok && cycle_load_ok && cycle_cells_exact &&
                        cycle_actor_exact && cycle_gpu_exact &&
                        cycle_homes_exact;
                    lifecycle_round_trips_passed =
                        lifecycle_round_trips_passed && cycle_round_trip;
                    cycle_passed = cycle_passed && cycle_round_trip;
                    replacement_cycles_passed =
                        replacement_cycles_passed && cycle_passed;
                    lifecycle_round_trip_detail +=
                        " cycle" + std::to_string(cycle) +
                        "[save=" + std::to_string(cycle_save_ok ? 1u : 0u) +
                        " load=" + std::to_string(cycle_load_ok ? 1u : 0u) +
                        " cells=" + std::to_string(cycle_cells_exact ? 1u : 0u) +
                        " actor=" + std::to_string(cycle_actor_exact ? 1u : 0u) +
                        " gpu=" + std::to_string(cycle_gpu_exact ? 1u : 0u) +
                        " homes=" + std::to_string(cycle_homes_exact ? 1u : 0u) +
                        (cycle_save_error.empty()
                            ? "" : " save_error=" + cycle_save_error) +
                        (cycle_load_error.empty()
                            ? "" : " load_error=" + cycle_load_error) + "]";
                    replacement_detail +=
                        " cycle" + std::to_string(cycle) +
                        "[slot=" + std::to_string(removed_slot) +
                        " hazard=" + std::to_string(hazard_death ? 1u : 0u) +
                        " site=" + std::to_string(birth_site_found ? 1u : 0u) +
                        " xy=" + std::to_string(birth_x) + "," +
                            std::to_string(birth_y) +
                        " step=" + std::to_string(birth_step_found ? birth_step : 0u) +
                        " classified=" + std::to_string(classified_bees) +
                        " total=" + std::to_string(born_bee_count) +
                        " material=" + std::to_string(newborn.material) +
                        " packed_slot=" +
                            std::to_string((newborn.aux >> 13u) & 127u) +
                        " born=" + std::to_string(replacement_born ? 1u : 0u) +
                        " settled_xy=" + std::to_string(replacement_x) + "," +
                            std::to_string(replacement_y) +
                        " settled_target=" + std::to_string(
                            fix29_bee_target_from_age(replacement_age)) +
                        " settled_timer=" + std::to_string(
                            fix29_bee_timer_from_age(replacement_age)) +
                        " settled_aux=" + std::to_string(replacement_aux) +
                        " mismatches=" + std::to_string(formation_mismatches) +
                            mismatch_detail +
                        " exact=" + std::to_string(exact_formation ? 1u : 0u) +
                        " cap_newborns=" + std::to_string(capped_newborns) +
                        " cap=" + std::to_string(cap_held ? 1u : 0u) + "]";
                    if (!cycle_passed) break;
                }
                lifecycle_cleanup_error.clear();
                std::filesystem::remove_all(
                    lifecycle_save_root, lifecycle_cleanup_error);
                lifecycle_round_trips_passed =
                    lifecycle_round_trips_passed &&
                    !lifecycle_cleanup_error;
                simulation_step = saved_lifecycle_step;
                append("bee_hazard_replacement_and_strict_60_cap",
                       replacement_cycles_passed, replacement_detail);
                append("bee_repeated_lifecycle_schema2_round_trip",
                       lifecycle_round_trips_passed,
                       lifecycle_round_trip_detail +
                           (lifecycle_cleanup_error
                               ? " cleanup_error=" +
                                   lifecycle_cleanup_error.message()
                               : ""));

                // Bee respiration is an actual packed-Atmosphere transaction:
                // exactly one Oxygen unit becomes one stored CO2 unit while
                // represented pressure remains 54.
                auto respiration_cells = result;
                std::uint32_t respiration_x = config.grid_width;
                std::uint32_t respiration_y = config.grid_height;
                for (std::size_t slot = 0u;
                     slot < fix29_bee_formation_count &&
                     respiration_x == config.grid_width; ++slot) {
                    const auto offset = fix29_bee_formation_offset(slot);
                    const auto bee_x = static_cast<std::uint32_t>(
                        static_cast<std::int32_t>(queen_x) + offset.x);
                    const auto bee_y = static_cast<std::uint32_t>(
                        static_cast<std::int32_t>(queen_y) + offset.y);
                    for (const auto& [dx, dy] : bee_neighbors) {
                        const auto x = static_cast<std::uint32_t>(
                            static_cast<std::int32_t>(bee_x) + dx);
                        const auto y = static_cast<std::uint32_t>(
                            static_cast<std::int32_t>(bee_y) + dy);
                        if (respiration_cells[index_of(x, y)].material !=
                            material_id(Material::atmosphere))
                            continue;
                        std::uint32_t adjacent_life = 0u;
                        for (const auto& [nx, ny] : bee_neighbors) {
                            const auto material = respiration_cells[index_of(
                                static_cast<std::uint32_t>(
                                    static_cast<std::int32_t>(x) + nx),
                                static_cast<std::uint32_t>(
                                    static_cast<std::int32_t>(y) + ny))].material;
                            adjacent_life +=
                                material == material_id(Material::bee) ||
                                material == material_id(Material::queen_bee) ? 1u : 0u;
                        }
                        if (adjacent_life == 1u) {
                            respiration_x = x;
                            respiration_y = y;
                            break;
                        }
                    }
                }
                bool bee_respiration_step_found = false;
                std::uint32_t bee_respiration_step = 0u;
                if (respiration_x < config.grid_width) {
                    respiration_cells[index_of(respiration_x, respiration_y)] =
                        SceneCell{
                            .material = material_id(Material::atmosphere),
                            .age = 0u,
                            .temperature = 20,
                            .aux = 54u,
                        };
                    for (std::uint32_t candidate = 0u;
                         candidate < 2'097'152u &&
                         !bee_respiration_step_found; ++candidate) {
                        const auto random_value = fill_hash(
                            respiration_x * 73856093u ^
                            respiration_y * 19349663u ^
                            candidate * 83492791u ^ random_seed ^ 54u);
                        const auto bee_roll =
                            fill_hash(random_value ^ 0xb33a71u) & 0x0003ffffu;
                        if (bee_roll == 0u) {
                            bee_respiration_step = candidate;
                            bee_respiration_step_found = true;
                        }
                    }
                }
                if (bee_respiration_step_found) {
                    upload_scene_cells(respiration_cells);
                    simulation_step = bee_respiration_step;
                    run_acceptance_chemistry_pass(
                        active_section_x, active_section_y, true);
                }
                const auto respired = bee_respiration_step_found
                    ? download_scene_cells() : std::vector<SceneCell>{};
                simulation_step = saved_lifecycle_step;
                const auto& carrier = !respired.empty()
                    ? respired[index_of(respiration_x, respiration_y)]
                    : result[index_of(queen_x, queen_y)];
                const auto stored_material =
                    (carrier.aux & 0x00007f00u) >> 8u;
                const auto stored_volume =
                    (carrier.aux & 0x007f8000u) >> 15u;
                const auto oxygen_volume = carrier.aux & 255u;
                append("bee_respiration_conserves_packed_atmosphere",
                       bee_respiration_step_found &&
                           carrier.material == material_id(Material::atmosphere) &&
                           oxygen_volume == 53u &&
                           stored_material ==
                               material_id(Material::carbon_dioxide) &&
                           stored_volume == 1u &&
                           oxygen_volume + stored_volume == 54u,
                       "step_found=" +
                           std::to_string(bee_respiration_step_found ? 1u : 0u) +
                           " step=" + std::to_string(bee_respiration_step) +
                           " oxygen=" + std::to_string(oxygen_volume) +
                           " stored_material=" +
                               std::to_string(stored_material) +
                           " stored_volume=" +
                               std::to_string(stored_volume));
            }

            {
                constexpr std::uint32_t water_x = 72u;
                constexpr std::uint32_t stone_x = 73u;
                constexpr std::uint32_t probe_y = 72u;
                auto cells = acceptance_atmosphere_world();
                cells[index_of(water_x, probe_y)] = make_fill_cell(
                    material_id(Material::water),
                    static_cast<std::uint32_t>(index_of(water_x, probe_y)));
                cells[index_of(stone_x, probe_y)] = make_fill_cell(
                    material_id(Material::stone),
                    static_cast<std::uint32_t>(index_of(stone_x, probe_y)));
                constexpr std::uint32_t below_sky_x = 74u;
                constexpr std::uint32_t cloud_x = 75u;
                const auto below_sky_y =
                    (std::min)(config.grid_height - 1u, nuke_high_sky_bottom_y + 8u);
                const auto cloud_y = nuke_high_sky_bottom_y - 1u;
                cells[index_of(cloud_x, cloud_y)] = make_fill_cell(
                    material_id(Material::cloud),
                    static_cast<std::uint32_t>(index_of(cloud_x, cloud_y)));
                const auto atmosphere_before = count_material(cells, Material::atmosphere);
                const auto expected_high_sky_fire =
                    static_cast<std::uint64_t>(config.grid_width) *
                        nuke_high_sky_bottom_y -
                    3u;
                upload_scene_cells(cells);
                immediate_submit([&](const VkCommandBuffer command_buffer) {
                    record_nuke_from_space(command_buffer);
                });
                const auto nuked = download_scene_cells();
                const auto nuke_tiles = download_tile_states();
                const auto tile_columns = divide_round_up(config.grid_width, 8u);
                const auto nuke_probe_x = 1u;
                const auto nuke_probe_y = 1u;
                const auto& nuke_probe =
                    nuke_tiles[nuke_probe_y * tile_columns + nuke_probe_x];
                constexpr std::uint32_t tile_sleeping = 0x00000004u;
                constexpr std::uint32_t tile_active = 0x00000008u;
                constexpr std::uint32_t tile_fine_active = 0x00020000u;
                constexpr std::uint32_t tile_bulk_ready = 0x08000000u;
                const bool hierarchy_is_immediate =
                    nuke_probe.material == material_id(Material::fire) &&
                    nuke_probe.occupancy == 64u &&
                    (nuke_probe.flags & tile_active) != 0u &&
                    (nuke_probe.flags & tile_fine_active) != 0u &&
                    (nuke_probe.flags & (tile_sleeping | tile_bulk_ready)) == 0u;
                append("nuke_from_space_gpu_high_sky_edit",
                       count_material(nuked, Material::atmosphere) ==
                               atmosphere_before - expected_high_sky_fire &&
                           count_material(nuked, Material::fire) ==
                               expected_high_sky_fire &&
                            hierarchy_is_immediate &&
                           nuked[index_of(water_x, probe_y)].material ==
                               material_id(Material::water) &&
                           nuked[index_of(stone_x, probe_y)].material ==
                               material_id(Material::stone) &&
                           nuked[index_of(below_sky_x, below_sky_y)].material ==
                               material_id(Material::atmosphere) &&
                           nuked[index_of(cloud_x, cloud_y)].material ==
                               material_id(Material::cloud),
                       "atmosphere_before=" + std::to_string(atmosphere_before) +
                           " atmosphere_after=" + std::to_string(
                               count_material(nuked, Material::atmosphere)) +
                           " fire=" + std::to_string(
                               count_material(nuked, Material::fire)) +
                           " water=" + std::to_string(
                               nuked[index_of(water_x, probe_y)].material) +
                           " stone=" + std::to_string(
                               nuked[index_of(stone_x, probe_y)].material) +
                            " below_sky=" + std::to_string(
                                nuked[index_of(below_sky_x, below_sky_y)].material) +
                            " cloud=" + std::to_string(
                                nuked[index_of(cloud_x, cloud_y)].material) +
                            " hierarchy=" +
                            std::to_string(hierarchy_is_immediate ? 1u : 0u) +
                            " probe=" + std::to_string(nuke_probe_x) + "," +
                            std::to_string(nuke_probe_y) +
                            " tile=" + std::to_string(nuke_probe.material) + "/" +
                            std::to_string(nuke_probe.occupancy) + "/" +
                            std::to_string(nuke_probe.flags));
            }

            {
                constexpr std::uint32_t full_x = 64u;
                constexpr std::uint32_t partial_x = 80u;
                constexpr std::uint32_t structural_y = 64u;
                constexpr std::uint32_t tile_structural = 0x00000001u;
                constexpr std::uint32_t tile_damaged = 0x00000080u;
                constexpr std::uint32_t tile_bulk_ready = 0x08000000u;
                constexpr std::uint32_t tile_fracture_armed = 0x10000000u;
                auto cells = acceptance_atmosphere_world();
                seed_rect(cells, Material::stone, full_x, structural_y, 8u, 8u);
                seed_rect(cells, Material::stone, partial_x, structural_y, 4u, 4u);
                upload_scene_cells(cells);
                run_acceptance_tile_pass();
                const auto states = download_tile_states();
                const auto columns = divide_round_up(config.grid_width, 8u);
                const auto& full =
                    states[(structural_y / 8u) * columns + full_x / 8u];
                const auto& partial =
                    states[(structural_y / 8u) * columns + partial_x / 8u];
                const bool full_exact =
                    full.material == material_id(Material::stone) &&
                    full.occupancy == 64u &&
                    (full.flags & (tile_structural | tile_fracture_armed)) ==
                        (tile_structural | tile_fracture_armed) &&
                    (full.flags & (tile_damaged | tile_bulk_ready)) == 0u;
                const bool partial_exact =
                    (partial.flags & tile_structural) != 0u &&
                    (partial.flags &
                     (tile_damaged | tile_bulk_ready | tile_fracture_armed)) == 0u;
                append("authored_structures_start_without_false_damage_or_bulk_state",
                       full_exact && partial_exact,
                       "full=" + std::to_string(full.material) + "/" +
                           std::to_string(full.occupancy) + "/" +
                           std::to_string(full.flags) +
                           " partial=" + std::to_string(partial.material) + "/" +
                           std::to_string(partial.occupancy) + "/" +
                           std::to_string(partial.flags));
            }

            {
                constexpr std::uint32_t moved_bit = 0x01000000u;
                constexpr std::uint32_t tile_sleeping = 0x00000004u;
                constexpr std::uint32_t tile_active = 0x00000008u;
                constexpr std::uint32_t tile_fine_active = 0x00020000u;
                constexpr std::uint32_t tile_settled_medium = 0x00040000u;
                auto cells = acceptance_atmosphere_world();
                seed_rect(cells, Material::stone, 56u, 120u, 80u, 8u);
                seed_rect(cells, Material::stone, 56u, 112u, 8u, 8u);
                seed_rect(cells, Material::stone, 128u, 112u, 8u, 8u);
                seed_rect(cells, Material::water, 64u, 114u, 64u, 6u);
                upload_scene_cells(cells);
                for (std::uint32_t tick = 0u; tick < 16u; ++tick)
                    run_acceptance_focused_tick();
                const auto settled = download_scene_cells();
                for (std::uint32_t tick = 0u; tick < 16u; ++tick)
                    run_acceptance_focused_tick();
                const auto observed = download_scene_cells();
                const auto tile_states = download_tile_states();

                bool unchanged_mask = true;
                std::uint32_t moved_water = 0u;
                for (std::size_t index = 0u; index < observed.size(); ++index) {
                    const bool was_water =
                        settled[index].material == material_id(Material::water);
                    const bool is_water =
                        observed[index].material == material_id(Material::water);
                    unchanged_mask = unchanged_mask && was_water == is_water;
                    if (is_water && (observed[index].aux & moved_bit) != 0u)
                        ++moved_water;
                }
                const auto [units, halves] = water_half_units(observed);
                const auto tile_columns = divide_round_up(config.grid_width, 8u);
                std::uint32_t settled_surface_tiles = 0u;
                std::string surface_flags;
                for (std::uint32_t tile_x = 8u; tile_x < 16u; ++tile_x) {
                    const auto flags =
                        tile_states[14u * tile_columns + tile_x].flags;
                    surface_flags += (surface_flags.empty() ? "" : ",") +
                        std::to_string(flags);
                    const bool settled_surface =
                        (flags & tile_sleeping) != 0u &&
                        (flags & tile_settled_medium) != 0u &&
                        (flags & (tile_active | tile_fine_active)) == 0u;
                    settled_surface_tiles += settled_surface ? 1u : 0u;
                }
                append("water_surface_zero_jitter",
                       units == 768u && halves == 0u && unchanged_mask &&
                           moved_water == 0u && settled_surface_tiles == 8u,
                       "units=" + std::to_string(units) +
                           " halves=" + std::to_string(halves) +
                           " unchanged=" + std::to_string(unchanged_mask ? 1u : 0u) +
                           " moved=" + std::to_string(moved_water) +
                           " sleeping_tiles=" +
                           std::to_string(settled_surface_tiles) +
                           " sample_age=" +
                           std::to_string(observed[index_of(64u, 114u)].age) +
                           " flags=" + surface_flags);
            }

            {
                auto cells = acceptance_atmosphere_world();
                seed_rect(cells, Material::stone, 80u, 101u, 41u, 1u);
                for (const auto x : {90u, 94u}) {
                    cells[index_of(x, 100u)] = SceneCell{
                        .material = material_id(Material::water),
                        .age = 0u,
                        .temperature = 20,
                        .aux = water_half_bit,
                    };
                }
                upload_scene_cells(cells);
                for (std::uint32_t tick = 0u; tick < 4u; ++tick)
                    run_acceptance_focused_tick();
                const auto result = download_scene_cells();
                const auto [units, halves] = water_half_units(result);
                append("half_water_bounded_attraction_merge",
                       units == 2u && halves == 0u &&
                           count_material(result, Material::water) == 1u,
                       "half_units=" + std::to_string(units) +
                           " halves=" + std::to_string(halves) +
                           " water_cells=" +
                           std::to_string(count_material(result, Material::water)) +
                           " saltwater=" + std::to_string(count_material(result, Material::saltwater)) +
                           " dirty_water=" + std::to_string(count_material(result, Material::dirty_water)) +
                           " mud=" + std::to_string(count_material(result, Material::mud)));
            }

            {
                constexpr std::uint32_t half_medium_temperature_20 = 121u;
                constexpr std::uint32_t half_atmosphere_aux =
                    water_half_bit |
                    ((material_id(Material::atmosphere) & 0x7fu) << 8u) |
                    half_medium_temperature_20;
                auto cells = acceptance_atmosphere_world();
                seed_rect(cells, Material::stone, 88u, 101u, 16u, 1u);
                cells[index_of(95u, 100u)] = SceneCell{
                    .material = material_id(Material::water),
                    .age = 0u,
                    .temperature = 10,
                    .aux = water_half_bit,
                };
                cells[index_of(96u, 100u)] = SceneCell{
                    .material = material_id(Material::water),
                    .age = 0u,
                    .temperature = 50,
                    .aux = half_atmosphere_aux,
                };
                upload_scene_cells(cells);
                // x=95/96 crosses an 8x8 ownership boundary. One half owns the
                // displaced Atmosphere marker and the other owns no medium.
                run_acceptance_horizontal_pass(1);
                const auto result = download_scene_cells();
                const auto& water = result[index_of(95u, 100u)];
                const auto& atmosphere = result[index_of(96u, 100u)];
                append("half_water_split_merge_heat_ledger",
                       water.material == material_id(Material::water) &&
                           (water.aux & water_half_bit) == 0u &&
                           (water.aux & 0xffu) == 0u && water.temperature == 30 &&
                           atmosphere.material == material_id(Material::atmosphere) &&
                           (atmosphere.aux & 0xffu) == 54u &&
                           atmosphere.temperature == 20,
                       "water_temp=" + std::to_string(water.temperature) +
                           " water_state=" + std::to_string(water.aux & 0xffu) +
                           " medium=" + std::to_string(atmosphere.material) +
                           " medium_temp=" + std::to_string(atmosphere.temperature) +
                           " medium_state=" + std::to_string(atmosphere.aux & 0xffu) +
                           " tile_boundary=1");
            }

            {
                auto cells = acceptance_atmosphere_world();
                cells[index_of(100u, 80u)] = SceneCell{
                    .material = material_id(Material::water),
                    .age = 0u,
                    .temperature = 20,
                    .aux = water_half_bit,
                };
                upload_scene_cells(cells);
                run_acceptance_fine_pass(0, 0);
                const auto result = download_scene_cells();
                const auto [units, halves] = water_half_units(result);
                const bool fell = result[index_of(100u, 81u)].material ==
                                      material_id(Material::water) &&
                                  (result[index_of(100u, 81u)].aux & water_half_bit) != 0u;
                append("half_water_falls_first",
                       units == 1u && halves == 1u && fell,
                       "half_units=" + std::to_string(units) +
                           " halves=" + std::to_string(halves) +
                           " fell=" + std::string{fell ? "true" : "false"});
            }

            {
                auto cells = acceptance_atmosphere_world();
                cells[index_of(100u, 64u)] = SceneCell{
                    .material = material_id(Material::water),
                    .age = 0u,
                    .temperature = 20,
                    .aux = water_half_bit,
                };
                upload_scene_cells(cells);
                for (std::uint32_t tick = 0u; tick < 8u; ++tick)
                    run_acceptance_focused_tick();
                const auto result = download_scene_cells();
                std::uint32_t final_y = 0u;
                std::uint32_t units = 0u;
                std::uint32_t halves = 0u;
                bool moved = false;
                for (std::uint32_t y = 0u; y < 255u; ++y) {
                    const auto cell = result[index_of(100u, y)];
                    if (cell.material != material_id(Material::water) ||
                        (cell.aux & water_half_bit) == 0u) {
                        continue;
                    }
                    if (units == 0u) final_y = y;
                    ++units;
                    ++halves;
                    if (y > 64u) moved = true;
                }
                append("half_water_keeps_dripping",
                       units == 1u && halves == 1u && moved && final_y > 70u,
                       "half_units=" + std::to_string(units) +
                           " halves=" + std::to_string(halves) +
                           " final_y=" + std::to_string(final_y) +
                           " moved=" + std::string{moved ? "true" : "false"});
            }

            {
                constexpr std::uint32_t half_medium_temperature_20 = 121u;
                auto cells = acceptance_atmosphere_world();
                seed_rect(cells, Material::stone, 104u, 161u, 8u, 1u);
                seed_rect(cells, Material::water, 109u, 160u, 2u, 1u);
                cells[index_of(109u, 160u)].temperature = 80;
                cells[index_of(110u, 160u)].temperature = 80;
                upload_scene_cells(cells);
                run_acceptance_horizontal_pass(0);
                const auto result = download_scene_cells();
                const auto [units, halves] = water_half_units(result);
                bool half_heat_exact = true;
                std::uint32_t inspected_halves = 0u;
                std::uint32_t atmosphere_owners = 0u;
                std::uint32_t empty_owners = 0u;
                for (const auto& cell : result) {
                    if (cell.material != material_id(Material::water) ||
                        (cell.aux & water_half_bit) == 0u)
                        continue;
                    ++inspected_halves;
                    const auto medium = (cell.aux >> 8u) & 0x7fu;
                    const bool atmosphere_owner =
                        medium == material_id(Material::atmosphere);
                    const bool empty_owner = medium == material_id(Material::empty);
                    atmosphere_owners += atmosphere_owner ? 1u : 0u;
                    empty_owners += empty_owner ? 1u : 0u;
                    half_heat_exact = half_heat_exact && cell.temperature == 80 &&
                        ((atmosphere_owner &&
                          (cell.aux & 0xffu) == half_medium_temperature_20) ||
                         (empty_owner && (cell.aux & 0xffu) == 0u));
                }
                append("supplied_ledge_creates_half_water",
                       units == 4u && halves == 2u &&
                           count_material(result, Material::water) == 3u &&
                           inspected_halves == 2u && half_heat_exact &&
                           atmosphere_owners == 1u && empty_owners == 1u,
                       "half_units=" + std::to_string(units) +
                           " halves=" + std::to_string(halves) +
                           " water_cells=" +
                           std::to_string(count_material(result, Material::water)) +
                           " atmosphere_owners=" +
                           std::to_string(atmosphere_owners) +
                           " empty_owners=" + std::to_string(empty_owners) +
                           " half_heat_exact=" +
                           std::string{half_heat_exact ? "true" : "false"});
            }

            {
                auto cells = acceptance_atmosphere_world();
                seed_rect(cells, Material::stone, 104u, 161u, 8u, 1u);
                seed_rect(cells, Material::water, 107u, 160u, 4u, 1u);
                upload_scene_cells(cells);
                run_acceptance_horizontal_pass(0);
                const auto result = download_scene_cells();
                const auto [units, halves] = water_half_units(result);
                append("full_water_reservoir_does_not_cascade_to_halves",
                       units == 8u && halves == 0u &&
                           count_material(result, Material::water) == 4u,
                       "half_units=" + std::to_string(units) +
                           " halves=" + std::to_string(halves) +
                           " water_cells=" +
                            std::to_string(count_material(result, Material::water)));
            }

            {
                auto cells = acceptance_atmosphere_world();
                seed_rect(cells, Material::stone, 104u, 161u, 8u, 1u);
                cells[index_of(110u, 160u)] = make_fill_cell(
                    material_id(Material::water), static_cast<std::uint32_t>(index_of(110u, 160u)));
                upload_scene_cells(cells);
                for (std::uint32_t tick = 0u; tick < 4u; ++tick)
                    run_acceptance_focused_tick();
                const auto result = download_scene_cells();
                bool crossed_ledge = false;
                std::uint32_t water_x = 0u;
                std::uint32_t water_y = 0u;
                std::uint32_t water_aux = 0u;
                for (std::uint32_t y = 0u; y < config.grid_height; ++y) {
                    for (std::uint32_t x = 0u; x < config.grid_width; ++x) {
                        const auto cell = result[index_of(x, y)];
                        if (cell.material != material_id(Material::water)) continue;
                        water_x = x;
                        water_y = y;
                        water_aux = cell.aux;
                        crossed_ledge = crossed_ledge ||
                            (x >= 112u && x < 120u && y >= 160u);
                    }
                }
                append("full_water_crosses_unsupported_ledge",
                       count_material(result, Material::water) == 1u && crossed_ledge,
                       "water_cells=" + std::to_string(count_material(result, Material::water)) +
                           " crossed=" + std::string{crossed_ledge ? "true" : "false"} +
                           " final=" + std::to_string(water_x) + "," +
                           std::to_string(water_y) +
                           " aux=" + std::to_string(water_aux));
            }

            immediate_submit([&](const VkCommandBuffer command_buffer) {
                record_reset(command_buffer, static_cast<std::uint32_t>(world_scene));
            });
            state.selected_scene.store(static_cast<std::uint32_t>(world_scene),
                                       std::memory_order_release);
            immediate_submit([&](const VkCommandBuffer command_buffer) {
                record_actor(command_buffer, state, true, false, false);
            });
            const auto actor = download_actor_state();
            const auto world_cells = download_scene_cells();
            const auto expected_spawn =
                persistent_world_spawn(config.grid_width, config.grid_height);
            append("persistent_world_player_spawn",
                   actor.enabled != 0u && actor.x == expected_spawn.x &&
                       actor.y == expected_spawn.y &&
                       actor.scene == static_cast<std::uint32_t>(world_scene) &&
                       actor.health == 255u && actor.oxygen == 255u,
                   "enabled=" + std::to_string(actor.enabled) +
                       " position=" + std::to_string(actor.x) + "," +
                       std::to_string(actor.y) +
                       " expected=" + std::to_string(expected_spawn.x) + "," +
                       std::to_string(expected_spawn.y) +
                       " health=" + std::to_string(actor.health) +
                       " oxygen=" + std::to_string(actor.oxygen));
            std::uint32_t player_clear_cells = 0u;
            for (std::int32_t offset_y = player_top_offset_cells; offset_y <= 0; ++offset_y) {
                for (std::int32_t offset_x = -player_half_width_cells;
                     offset_x <= player_half_width_cells; ++offset_x) {
                    const auto body_cell = world_cells[index_of(
                        static_cast<std::uint32_t>(actor.x + offset_x),
                        static_cast<std::uint32_t>(actor.y + offset_y))];
                    player_clear_cells += body_cell.material == material_id(Material::empty) ||
                        body_cell.material == material_id(Material::atmosphere) ||
                        body_cell.material == material_id(Material::oxygen) ? 1u : 0u;
                }
            }
            std::uint32_t player_support_cells = 0u;
            for (std::int32_t offset_x = -player_half_width_cells;
                 offset_x <= player_half_width_cells; ++offset_x) {
                const auto support = world_cells[index_of(
                    static_cast<std::uint32_t>(actor.x + offset_x),
                    static_cast<std::uint32_t>(actor.y + 1))];
                player_support_cells += support.material != material_id(Material::empty) &&
                    support.material != material_id(Material::atmosphere) &&
                    support.material != material_id(Material::oxygen) ? 1u : 0u;
            }
            const auto head = world_cells[index_of(
                static_cast<std::uint32_t>(actor.x),
                static_cast<std::uint32_t>(actor.y + player_head_center_offset_cells))];
            const bool player_head_breathable =
                head.material == material_id(Material::atmosphere) ||
                head.material == material_id(Material::oxygen);
            constexpr auto player_footprint_cells = static_cast<std::uint32_t>(
                (player_half_width_cells * 2 + 1) * player_body_height_cells);
            append("persistent_world_player_scale_clearance",
                   player_body_height_cells == 23 &&
                       player_body_height_cells <
                           3 * static_cast<std::int32_t>(authored_scene_foundation_cells) &&
                       player_clear_cells == player_footprint_cells &&
                       player_support_cells > 0u && player_head_breathable,
                   "height=" + std::to_string(player_body_height_cells) +
                       " footprint=" + std::to_string(player_footprint_cells) +
                       " clear=" + std::to_string(player_clear_cells) +
                       " support=" + std::to_string(player_support_cells) +
                       " breathable=" +
                       std::string{player_head_breathable ? "true" : "false"});
            const SectionCoordinate startup_center{
                static_cast<std::int32_t>(expected_spawn.x /
                                          static_cast<std::uint32_t>(active_region_width_cells)),
                static_cast<std::int32_t>(expected_spawn.y /
                                          static_cast<std::uint32_t>(active_region_height_cells))};
            const auto startup_origin = active_window_origin(
                startup_center,
                (config.grid_width + static_cast<std::uint32_t>(active_region_width_cells) - 1u) /
                    static_cast<std::uint32_t>(active_region_width_cells),
                (config.grid_height + static_cast<std::uint32_t>(active_region_height_cells) - 1u) /
                    static_cast<std::uint32_t>(active_region_height_cells));
            const auto startup_left = startup_origin.x * active_region_width_cells;
            const auto startup_right = startup_left +
                active_region_width_cells * active_window_columns;
            std::uint32_t startup_districts = 0u;
            for (std::uint32_t district = 0u;
                 district < persistent_world_district_count; ++district) {
                const auto district_left = static_cast<std::int32_t>(
                    persistent_world_district_origin_x(config.grid_width, district));
                const auto district_right = district_left +
                    static_cast<std::int32_t>(pre_expansion_world_width);
                if (district_right > startup_left && district_left < startup_right) {
                    ++startup_districts;
                }
            }
            append("persistent_world_startup_sparse_footprint",
                   config.grid_width <= persistent_world_width || startup_districts <= 3u,
                   "active_authored_districts=" + std::to_string(startup_districts) +
                       " active_x=" + std::to_string(startup_left) + ".." +
                       std::to_string(startup_right));
            for (std::uint32_t district = 0u;
                 district < persistent_world_district_count; ++district) {
                const auto scene = persistent_world_district_scene(district);
                const auto origin_x =
                    persistent_world_district_origin_x(config.grid_width, district);
                const auto origin_y =
                    persistent_world_district_origin_y(config.grid_height, district);
                const auto first_foundation_y =
                    origin_y + pre_expansion_world_height - authored_scene_foundation_cells;
                std::uint32_t stone = 0u;
                std::uint32_t lava = 0u;
                std::uint32_t supported_stone = 0u;
                for (std::uint32_t y = first_foundation_y;
                     y < origin_y + pre_expansion_world_height; ++y) {
                    for (std::uint32_t x = origin_x;
                         x < origin_x + pre_expansion_world_width; ++x) {
                        const auto& cell = world_cells[index_of(x, y)];
                        const bool is_stone =
                            cell.material == material_id(Material::stone);
                        stone += is_stone ? 1u : 0u;
                        lava += cell.material == material_id(Material::lava) ? 1u : 0u;
                        supported_stone += is_stone &&
                            (cell.aux & (fill_aux_structural | fill_aux_supported)) ==
                                (fill_aux_structural | fill_aux_supported) ? 1u : 0u;
                    }
                }
                const auto expected =
                    pre_expansion_world_width * authored_scene_foundation_cells;
                const auto surface_y = origin_y +
                    scene_surface_tile_row(scene) * authored_scene_foundation_cells;
                const auto complete_structural_tile =
                    [&](const std::uint32_t tile_x,
                        const std::uint32_t tile_y,
                        const Material expected_material) {
                        for (std::uint32_t dy = 0u;
                             dy < authored_scene_foundation_cells; ++dy) {
                            for (std::uint32_t dx = 0u;
                                 dx < authored_scene_foundation_cells; ++dx) {
                                const auto& cell = world_cells[index_of(tile_x + dx,
                                                                       tile_y + dy)];
                                if (cell.material != material_id(expected_material) ||
                                    (cell.aux & (fill_aux_structural |
                                                 fill_aux_supported)) !=
                                        (fill_aux_structural | fill_aux_supported)) {
                                    return false;
                                }
                            }
                        }
                        return true;
                    };
                std::uint32_t complete_grass_tiles = 0u;
                std::uint32_t complete_dirt_tiles = 0u;
                for (std::uint32_t local_x = 0u;
                     local_x < pre_expansion_world_width;
                     local_x += authored_scene_foundation_cells) {
                    const auto tile_x = origin_x + local_x;
                    complete_grass_tiles += complete_structural_tile(
                        tile_x, surface_y, Material::grass) ? 1u : 0u;
                    complete_dirt_tiles += complete_structural_tile(
                        tile_x, surface_y + authored_scene_foundation_cells,
                        Material::dirt) ? 1u : 0u;
                }
                const bool aligned_layout =
                    (origin_x % authored_scene_foundation_cells) == 0u &&
                    (origin_y % authored_scene_foundation_cells) == 0u &&
                    surface_y == persistent_world_surface_y(config.grid_height) &&
                    origin_x + pre_expansion_world_width <= config.grid_width;
                append("world_district_" + std::string{scene_name(scene)},
                       aligned_layout && stone == expected && lava == 0u &&
                           supported_stone == expected && complete_grass_tiles > 0u &&
                           complete_dirt_tiles > 0u,
                       "district=" + std::to_string(district) +
                           " origin=" + std::to_string(origin_x) + "," +
                           std::to_string(origin_y) +
                           " surface=" + std::to_string(surface_y) +
                           " stone=" + std::to_string(stone) +
                           " lava=" + std::to_string(lava) +
                           " supported_stone=" + std::to_string(supported_stone) +
                           " complete_grass_tiles=" +
                           std::to_string(complete_grass_tiles) +
                           " complete_dirt_tiles=" +
                           std::to_string(complete_dirt_tiles));
            }
            {
                {
                    const auto district = persistent_world_district_index(Scene::engineering_lab);
                    const auto origin_x = persistent_world_district_origin_x(
                        config.grid_width, district);
                    const auto origin_y = persistent_world_district_origin_y(
                        config.grid_height, district);
                    constexpr std::uint32_t hopper_left = 27u;
                    constexpr std::uint32_t hopper_right = 46u;
                    constexpr std::uint32_t staged_top = 6u;
                    constexpr std::uint32_t staged_bottom = 15u;
                    constexpr std::uint32_t feed_bottom = 19u;
                    std::uint32_t complete_staged_tiles = 0u;
                    std::uint32_t loose_feed_cells = 0u;
                    for (std::uint32_t brick_y = staged_top;
                         brick_y < staged_bottom; ++brick_y) {
                        for (std::uint32_t brick_x = hopper_left;
                             brick_x < hopper_right; ++brick_x) {
                            const auto tile_x = origin_x + brick_x * 8u;
                            const auto tile_y = origin_y + brick_y * 8u;
                            const auto expected_material =
                                world_cells[index_of(tile_x, tile_y)].material;
                            bool complete =
                                expected_material != material_id(Material::atmosphere);
                            for (std::uint32_t dy = 0u; dy < 8u; ++dy) {
                                for (std::uint32_t dx = 0u; dx < 8u; ++dx) {
                                    const auto& cell = world_cells[index_of(
                                        tile_x + dx, tile_y + dy)];
                                    complete = complete &&
                                        cell.material == expected_material &&
                                        (cell.aux & (fill_aux_structural |
                                                     fill_aux_supported)) ==
                                            (fill_aux_structural | fill_aux_supported);
                                }
                            }
                            complete_staged_tiles += complete ? 1u : 0u;
                        }
                    }
                    for (std::uint32_t brick_y = staged_bottom;
                         brick_y < feed_bottom; ++brick_y) {
                        for (std::uint32_t brick_x = hopper_left;
                             brick_x < hopper_right; ++brick_x) {
                            for (std::uint32_t dy = 0u; dy < 8u; ++dy) {
                                for (std::uint32_t dx = 0u; dx < 8u; ++dx) {
                                    const auto& cell = world_cells[index_of(
                                        origin_x + brick_x * 8u + dx,
                                        origin_y + brick_y * 8u + dy)];
                                    if (cell.material !=
                                            material_id(Material::atmosphere) &&
                                        (cell.aux & (fill_aux_structural |
                                                     fill_aux_supported)) == 0u)
                                        ++loose_feed_cells;
                                }
                            }
                        }
                    }
                    append("engineering_authored_stock_and_feed_roles",
                           complete_staged_tiles == 171u &&
                               loose_feed_cells == 4864u,
                           "staged_tiles=" +
                               std::to_string(complete_staged_tiles) +
                               " loose_feed_cells=" +
                               std::to_string(loose_feed_cells));
                    const auto hydrogen_cells = count_rect(
                        world_cells, Material::hydrogen,
                        origin_x + 58u * 8u, origin_y + 6u * 8u,
                        9u * 8u, 14u * 8u);
                    const auto oxygen_cells = count_rect(
                        world_cells, Material::oxygen,
                        origin_x + 68u * 8u, origin_y + 6u * 8u,
                        9u * 8u, 14u * 8u);
                    const auto divider_glass = count_rect(
                        world_cells, Material::glass,
                        origin_x + 67u * 8u, origin_y + 6u * 8u,
                        8u, 14u * 8u);
                    const auto aperture_air = count_rect(
                        world_cells, Material::atmosphere,
                        origin_x + 67u * 8u, origin_y + 13u * 8u,
                        8u, 8u);
                    append("engineering_gas_chamber_controlled_aperture",
                           hydrogen_cells == 8064u && oxygen_cells == 8064u &&
                               divider_glass == 832u && aperture_air == 64u,
                           "hydrogen=" + std::to_string(hydrogen_cells) +
                               " oxygen=" + std::to_string(oxygen_cells) +
                               " divider_glass=" + std::to_string(divider_glass) +
                               " aperture_air=" + std::to_string(aperture_air));
                }

                // Exercise the authored Waterworks bubbles in the same resident
                // World cells and 640x360 active-section dispatch used by play.
                // This closes the gap where an isolated synthetic packet passed
                // while the visible level could still leave complete tiles stuck.
                constexpr std::uint32_t tile_size = 8u;
                constexpr std::uint32_t bubble_count = 3u;
                constexpr std::uint32_t tile_macro_movable = 0x00010000u;
                constexpr std::uint32_t tile_fine_active = 0x00020000u;
                constexpr std::uint32_t tile_medium_breakup = 0x04000000u;
                const auto district = persistent_world_district_index(Scene::waterworks);
                const auto district_x = persistent_world_district_origin_x(
                    config.grid_width, district);
                const auto district_y = persistent_world_district_origin_y(
                    config.grid_height, district);
                const auto world_bricks_x = pre_expansion_world_width / tile_size;
                const auto world_bricks_y = pre_expansion_world_height / tile_size;
                const auto tank_width = (std::max)(8, (static_cast<int>(world_bricks_x) - 10) / 3);
                const auto tank_left = 2;
                const auto tank_right = (std::min)(tank_left + tank_width,
                                                   static_cast<int>(world_bricks_x) - 2);
                const auto bubble_row = static_cast<std::uint32_t>(
                    static_cast<int>(world_bricks_y) - 1 - 9 - 5);
                const auto quarter = (std::max)(2, tank_width / 4);
                const std::array<std::uint32_t, bubble_count> bubble_columns{
                    static_cast<std::uint32_t>(tank_left + quarter),
                    static_cast<std::uint32_t>((tank_left + tank_right) / 2),
                    static_cast<std::uint32_t>(tank_right - quarter)};
                const auto initial_bubble_y = district_y + bubble_row * tile_size;
                const auto initial_water = count_material(world_cells, Material::water);
                const auto initial_water_family = initial_water +
                    count_material(world_cells, Material::steam) +
                    count_material(world_cells, Material::cloud) +
                    count_material(world_cells, Material::dirty_water) +
                    count_material(world_cells, Material::dirty_steam);
                const auto initial_hydrogen = count_material(world_cells, Material::hydrogen);
                bool authored_complete = true;
                for (const auto column : bubble_columns) {
                    authored_complete = authored_complete &&
                        count_rect(world_cells, Material::hydrogen,
                                   district_x + column * tile_size,
                                   initial_bubble_y, tile_size, tile_size) == 64u;
                }
                append("world_waterworks_authored_bubble_packets",
                       authored_complete,
                       "packets=" + std::to_string(bubble_count) +
                           " row=" + std::to_string(bubble_row) +
                           " initial_hydrogen=" + std::to_string(initial_hydrogen));

                std::uint32_t complete_cloud_tiles = 0u;
                std::uint32_t cloud_cells = 0u;
                std::uint32_t low_cloud_cells = 0u;
                std::uint32_t cloud_min_y = config.grid_height;
                std::uint32_t cloud_max_y = 0u;
                std::uint32_t half_water_cells = 0u;
                std::uint32_t waterworks_water_cells = 0u;
                const auto resident_tile_columns =
                    divide_round_up(config.grid_width, tile_size);
                std::vector<bool> cloud_column_covered(resident_tile_columns, false);
                const auto cloud_brick_top =
                    persistent_world_weather_region_top_y / tile_size;
                const auto cloud_brick_bottom =
                    persistent_world_weather_region_bottom_y / tile_size;
                for (std::uint32_t brick_y = cloud_brick_top;
                     brick_y < cloud_brick_bottom; ++brick_y) {
                    for (std::uint32_t brick_x = 0u;
                         brick_x < resident_tile_columns; ++brick_x) {
                        bool complete_cloud = true;
                        for (std::uint32_t dy = 0u; dy < tile_size; ++dy) {
                            for (std::uint32_t dx = 0u; dx < tile_size; ++dx) {
                                const auto world_x = brick_x * tile_size + dx;
                                const auto world_y = brick_y * tile_size + dy;
                                const auto& cell = world_cells[index_of(world_x, world_y)];
                                const bool cloud = cell.material ==
                                    material_id(Material::cloud);
                                complete_cloud = complete_cloud && cloud;
                                if (cloud) {
                                    ++cloud_cells;
                                    cloud_min_y = (std::min)(cloud_min_y, world_y);
                                    cloud_max_y = (std::max)(cloud_max_y, world_y);
                                }
                            }
                        }
                        if (complete_cloud) {
                            ++complete_cloud_tiles;
                            cloud_column_covered[brick_x] = true;
                        }
                    }
                }
                const auto covered_cloud_columns = static_cast<std::uint32_t>(
                    std::count(cloud_column_covered.begin(),
                               cloud_column_covered.end(), true));
                for (std::uint32_t local_y = 0u;
                     local_y < pre_expansion_world_height; ++local_y) {
                    for (std::uint32_t local_x = 0u;
                         local_x < pre_expansion_world_width; ++local_x) {
                        const auto& cell = world_cells[index_of(
                            district_x + local_x, district_y + local_y)];
                        low_cloud_cells += cell.material ==
                            material_id(Material::cloud) ? 1u : 0u;
                        if (cell.material == material_id(Material::water)) {
                            ++waterworks_water_cells;
                            half_water_cells +=
                                (cell.aux & water_half_bit) != 0u ? 1u : 0u;
                        }
                    }
                }
                const auto steam_riser = count_rect(
                    world_cells, Material::steam,
                    district_x + 13u * tile_size,
                    district_y + 9u * tile_size,
                    tile_size, 4u * tile_size);
                const auto boiler_smelter = count_rect(
                    world_cells, Material::smelter,
                    district_x + 12u * tile_size, district_y + 42u * tile_size,
                    tile_size, tile_size);
                const auto boiler_power = count_rect(
                    world_cells, Material::power_cell,
                    district_x + 12u * tile_size, district_y + 43u * tile_size,
                    tile_size, tile_size);
                const auto open_catchment_cells = count_rect(
                    world_cells, Material::atmosphere,
                    district_x + 3u * tile_size, district_y + 40u * tile_size,
                    (world_bricks_x - 6u) * tile_size, tile_size);
                const auto volcano_district =
                    persistent_world_district_index(Scene::volcano);
                const auto volcano_reset_smoke = count_rect(
                    world_cells, Material::smoke,
                    persistent_world_district_origin_x(
                        config.grid_width, volcano_district),
                    persistent_world_district_origin_y(
                        config.grid_height, volcano_district),
                    pre_expansion_world_width, pre_expansion_world_height);
                append("world_wide_high_sky_weather_inventory",
                       resident_tile_columns >= 3u &&
                            !cloud_column_covered.front() &&
                            !cloud_column_covered.back() &&
                            covered_cloud_columns == resident_tile_columns - 2u &&
                            complete_cloud_tiles >=
                                (resident_tile_columns - 2u) * 3u &&
                            cloud_cells == complete_cloud_tiles * tile_size * tile_size &&
                            low_cloud_cells == 0u &&
                            cloud_min_y >= persistent_world_weather_region_top_y &&
                            cloud_max_y < persistent_world_weather_region_bottom_y &&
                            steam_riser == 256u && waterworks_water_cells > 0u &&
                            half_water_cells == 0u && boiler_smelter == 64u &&
                            boiler_power == 64u && open_catchment_cells >= 4096u &&
                            volcano_reset_smoke == 0u,
                       "covered_columns=" +
                            std::to_string(covered_cloud_columns) + "/" +
                            std::to_string(resident_tile_columns) +
                            " complete_cloud_tiles=" +
                            std::to_string(complete_cloud_tiles) +
                            " cloud_cells=" + std::to_string(cloud_cells) +
                            " high_sky_y=" + std::to_string(cloud_min_y) + ".." +
                            std::to_string(cloud_max_y) +
                            " low_cloud_cells=" + std::to_string(low_cloud_cells) +
                            " steam_riser=" + std::to_string(steam_riser) +
                            " boiler=" + std::to_string(boiler_smelter) + "/" +
                            std::to_string(boiler_power) +
                            " open_catchment=" +
                            std::to_string(open_catchment_cells) +
                            " volcano_reset_smoke=" +
                            std::to_string(volcano_reset_smoke) +
                            " full_water_cells=" +
                            std::to_string(waterworks_water_cells) +
                            " half_water_cells=" +
                            std::to_string(half_water_cells));
                const auto active_section_x = static_cast<std::int32_t>(
                    (district_x + bubble_columns[1] * tile_size) /
                    static_cast<std::uint32_t>(active_region_width_cells));
                const auto active_section_y = static_cast<std::int32_t>(
                    (initial_bubble_y - tile_size * 8u) /
                    static_cast<std::uint32_t>(active_region_height_cells));
                run_acceptance_tile_pass(active_section_x, active_section_y, true);
                const auto tile_columns = divide_round_up(config.grid_width, tile_size);
                auto gas_tile_row = initial_bubble_y / tile_size;
                bool retained_first_seven = true;
                bool counted_each_step = true;
                std::uint32_t seventh_progress = 0u;
                for (std::uint32_t step = 1u; step <= 8u; ++step) {
                    const auto water_source_row = gas_tile_row - 1u;
                    run_acceptance_macro_pass(
                        0, static_cast<std::int32_t>(water_source_row & 1u),
                        active_section_x, active_section_y, true);
                    --gas_tile_row;
                    const auto moved_states = download_tile_states();
                    for (const auto column : bubble_columns) {
                        const auto tile_column = (district_x + column * tile_size) / tile_size;
                        const auto& moved_state = moved_states[
                            gas_tile_row * tile_columns + tile_column];
                        counted_each_step = counted_each_step &&
                            (moved_state.counters & 0xffu) == step;
                        if (step < 8u) {
                            retained_first_seven = retained_first_seven &&
                                (moved_state.flags & tile_macro_movable) != 0u &&
                                (moved_state.flags & tile_fine_active) == 0u;
                            seventh_progress = moved_state.counters & 0xffu;
                        }
                    }
                    if (step < 8u) {
                        run_acceptance_chemistry_pass(
                            active_section_x, active_section_y, true);
                        run_acceptance_tile_pass(
                            active_section_x, active_section_y, true);
                    }
                }
                run_acceptance_tile_pass(active_section_x, active_section_y, true);
                const auto final_states = download_tile_states();
                const auto final_cells = download_scene_cells();
                bool all_broke_to_fine = true;
                bool all_packets_conserved = true;
                const auto final_bubble_y = gas_tile_row * tile_size;
                for (const auto column : bubble_columns) {
                    const auto bubble_x = district_x + column * tile_size;
                    const auto tile_column = bubble_x / tile_size;
                    const auto& final_state = final_states[
                        gas_tile_row * tile_columns + tile_column];
                    all_broke_to_fine = all_broke_to_fine &&
                        (final_state.flags & tile_fine_active) != 0u &&
                        (final_state.flags & tile_medium_breakup) != 0u &&
                        (final_state.flags & tile_macro_movable) == 0u &&
                        (final_state.counters & 0xffu) == 8u;
                    all_packets_conserved = all_packets_conserved &&
                        count_rect(final_cells, Material::hydrogen,
                                   bubble_x, final_bubble_y,
                                   tile_size, tile_size) == 64u;
                }
                const auto final_water = count_material(final_cells, Material::water);
                const auto final_water_family = final_water +
                    count_material(final_cells, Material::steam) +
                    count_material(final_cells, Material::cloud) +
                    count_material(final_cells, Material::dirty_water) +
                    count_material(final_cells, Material::dirty_steam);
                const auto final_hydrogen = count_material(final_cells, Material::hydrogen);
                append("world_waterworks_bubbles_eight_step_breakup",
                       authored_complete && retained_first_seven &&
                           counted_each_step && all_broke_to_fine &&
                           all_packets_conserved &&
                           final_water_family == initial_water_family &&
                           final_hydrogen == initial_hydrogen,
                       "packets=" + std::to_string(bubble_count) +
                            " authored=" + std::to_string(authored_complete ? 1u : 0u) +
                            " retained7=" + std::to_string(retained_first_seven ? 1u : 0u) +
                            " counted=" + std::to_string(counted_each_step ? 1u : 0u) +
                            " broke=" + std::to_string(all_broke_to_fine ? 1u : 0u) +
                            " conserved=" + std::to_string(all_packets_conserved ? 1u : 0u) +
                            " water_family_same=" +
                            std::to_string(final_water_family == initial_water_family ? 1u : 0u) +
                            " water_phase_delta=" +
                            std::to_string(
                                static_cast<std::int64_t>(final_water) -
                                static_cast<std::int64_t>(initial_water)) +
                            " hydrogen_same=" +
                            std::to_string(final_hydrogen == initial_hydrogen ? 1u : 0u) +
                           " seventh_progress=" + std::to_string(seventh_progress) +
                           " final_row=" + std::to_string(gas_tile_row) +
                           " water=" + std::to_string(final_water) +
                           " hydrogen=" + std::to_string(final_hydrogen) +
                           " active_section=" + std::to_string(active_section_x) + "," +
                           std::to_string(active_section_y));
            }
            {
                auto cells = acceptance_atmosphere_world();
                {
                    auto thermal_cells = acceptance_atmosphere_world();
                    thermal_cells[index_of(80u, 80u)] = make_fill_cell(
                        material_id(Material::water),
                        static_cast<std::uint32_t>(index_of(80u, 80u)));
                    thermal_cells[index_of(81u, 80u)] = make_fill_cell(
                        material_id(Material::ember),
                        static_cast<std::uint32_t>(index_of(81u, 80u)));
                    thermal_cells[index_of(96u, 80u)] = make_fill_cell(
                        material_id(Material::water),
                        static_cast<std::uint32_t>(index_of(96u, 80u)));
                    upload_scene_cells(thermal_cells);
                    run_acceptance_chemistry_pass();
                    const auto result = download_scene_cells();
                    append("engineering_thermal_treatment_and_control",
                           result[index_of(80u, 80u)].material ==
                                   material_id(Material::steam) &&
                               result[index_of(96u, 80u)].material ==
                                   material_id(Material::water),
                           "treatment=" + std::to_string(
                               result[index_of(80u, 80u)].material) +
                               " control=" + std::to_string(
                               result[index_of(96u, 80u)].material));
                }
                {
                    auto compost_cells = acceptance_atmosphere_world();
                    auto treatment_feed = make_fill_cell(
                        material_id(Material::ash),
                        static_cast<std::uint32_t>(index_of(100u, 100u)));
                    treatment_feed.age = 2000u;
                    compost_cells[index_of(100u, 100u)] = treatment_feed;
                    auto treatment_water = make_fill_cell(
                        material_id(Material::dirty_water),
                        static_cast<std::uint32_t>(index_of(101u, 100u)));
                    treatment_water.age = 2000u;
                    compost_cells[index_of(101u, 100u)] = treatment_water;
                    compost_cells[index_of(99u, 100u)] = make_fill_cell(
                        material_id(Material::silt),
                        static_cast<std::uint32_t>(index_of(99u, 100u)));
                    compost_cells[index_of(100u, 99u)] = make_fill_cell(
                        material_id(Material::waste),
                        static_cast<std::uint32_t>(index_of(100u, 99u)));

                    auto control_feed = make_fill_cell(
                        material_id(Material::ash),
                        static_cast<std::uint32_t>(index_of(120u, 100u)));
                    control_feed.age = 2000u;
                    compost_cells[index_of(120u, 100u)] = control_feed;
                    compost_cells[index_of(121u, 100u)] = make_fill_cell(
                        material_id(Material::water),
                        static_cast<std::uint32_t>(index_of(121u, 100u)));
                    compost_cells[index_of(119u, 100u)] = make_fill_cell(
                        material_id(Material::silt),
                        static_cast<std::uint32_t>(index_of(119u, 100u)));
                    compost_cells[index_of(120u, 99u)] = make_fill_cell(
                        material_id(Material::waste),
                        static_cast<std::uint32_t>(index_of(120u, 99u)));
                    upload_scene_cells(compost_cells);

                    const auto feed_index = static_cast<std::uint32_t>(
                        index_of(100u, 100u));
                    const auto water_index = static_cast<std::uint32_t>(
                        index_of(101u, 100u));
                    const auto pair_key = (std::min)(feed_index, water_index) ^
                        ((std::max)(feed_index, water_index) * 0x9e3779b9u);
                    const auto saved_step = simulation_step;
                    bool event_found = false;
                    std::uint32_t event_step = saved_step;
                    for (std::uint32_t offset = 0u; offset < 65536u; ++offset) {
                        const auto candidate = saved_step + offset;
                        if ((fill_hash(pair_key ^ candidate ^ random_seed ^ 0xc06f057u) &
                             255u) == 0u) {
                            event_found = true;
                            event_step = candidate;
                            break;
                        }
                    }
                    simulation_step = event_step;
                    run_acceptance_chemistry_pass();
                    simulation_step = saved_step;
                    const auto result = download_scene_cells();
                    append("engineering_compost_treatment_and_control",
                           event_found &&
                               result[index_of(100u, 100u)].material ==
                                   material_id(Material::fertilizer) &&
                               result[index_of(101u, 100u)].material ==
                                   material_id(Material::water) &&
                               result[index_of(120u, 100u)].material ==
                                   material_id(Material::ash) &&
                               result[index_of(121u, 100u)].material ==
                                   material_id(Material::water),
                           "event_step=" + std::to_string(event_step) +
                               " treatment=" + std::to_string(
                               result[index_of(100u, 100u)].material) + "/" +
                               std::to_string(result[index_of(101u, 100u)].material) +
                               " control=" + std::to_string(
                               result[index_of(120u, 100u)].material) + "/" +
                               std::to_string(result[index_of(121u, 100u)].material));
                }

                cells[index_of(96u, 100u)] = SceneCell{
                    .material = material_id(Material::iron_ore),
                    .age = 0u,
                    .temperature = 20,
                    .aux = 255u,
                };
                cells[index_of(96u, 108u)] = make_fill_cell(
                    material_id(Material::iron_ore),
                    static_cast<std::uint32_t>(index_of(96u, 108u)));
                cells[index_of(105u, 100u)] = make_fill_cell(
                    material_id(Material::magnet),
                    static_cast<std::uint32_t>(index_of(105u, 100u)));
                cells[index_of(105u, 108u)] = make_fill_cell(
                    material_id(Material::magnet),
                    static_cast<std::uint32_t>(index_of(105u, 108u)));
                upload_scene_cells(cells);
                for (std::uint32_t step = 0u; step < 8u; ++step)
                    run_acceptance_horizontal_pass(static_cast<std::int32_t>((96u + step) & 1u));
                const auto result = download_scene_cells();
                const bool loose_attracted = result[index_of(104u, 100u)].material ==
                    material_id(Material::iron_ore);
                const auto& structural_stock = result[index_of(96u, 108u)];
                const bool stock_retained = structural_stock.material ==
                        material_id(Material::iron_ore) &&
                    (structural_stock.aux & (fill_aux_structural | fill_aux_supported)) ==
                        (fill_aux_structural | fill_aux_supported);
                append("engineering_bounded_magnet_field",
                       loose_attracted && stock_retained &&
                           count_material(result, Material::iron_ore) == 2u,
                       "loose_attracted=" + std::to_string(loose_attracted ? 1u : 0u) +
                           " structural_stock=" + std::to_string(stock_retained ? 1u : 0u) +
                           " iron_ore=" +
                           std::to_string(count_material(result, Material::iron_ore)));
            }

            {
                auto cells = acceptance_atmosphere_world();
                auto steam = make_fill_cell(
                    material_id(Material::steam),
                    static_cast<std::uint32_t>(index_of(96u, 100u)));
                steam.temperature = 140;
                cells[index_of(96u, 100u)] = steam;
                cells[index_of(112u, 100u)] = make_fill_cell(
                    material_id(Material::smoke),
                    static_cast<std::uint32_t>(index_of(112u, 100u)));
                upload_scene_cells(cells);
                run_acceptance_fine_pass(0, 1);
                const auto result = download_scene_cells();
                const bool steam_rose = result[index_of(96u, 99u)].material ==
                    material_id(Material::steam);
                const bool smoke_rose = result[index_of(112u, 99u)].material ==
                    material_id(Material::smoke);
                append("volcano_excess_gas_buoyancy",
                       steam_rose && smoke_rose &&
                           count_material(result, Material::steam) == 1u &&
                           count_material(result, Material::smoke) == 1u,
                       "steam_rose=" + std::to_string(steam_rose ? 1u : 0u) +
                           " smoke_rose=" + std::to_string(smoke_rose ? 1u : 0u));
            }

            {
                auto cells = acceptance_atmosphere_world();
                cells[index_of(100u, 100u)] = make_fill_cell(
                    material_id(Material::hydrogen),
                    static_cast<std::uint32_t>(index_of(100u, 100u)));
                cells[index_of(101u, 100u)] = make_fill_cell(
                    material_id(Material::oxygen),
                    static_cast<std::uint32_t>(index_of(101u, 100u)));
                upload_scene_cells(cells);
                run_acceptance_chemistry_pass();
                const auto result = download_scene_cells();
                const auto& water_product = result[index_of(100u, 100u)];
                const auto& atmosphere_product = result[index_of(101u, 100u)];
                append("waterworks_hydrogen_oxygen_synthesis",
                       water_product.material == material_id(Material::water) &&
                           atmosphere_product.material == material_id(Material::atmosphere) &&
                           (atmosphere_product.aux & 255u) == 54u &&
                           count_material(result, Material::hydrogen) == 0u &&
                           count_material(result, Material::oxygen) == 0u,
                       "products=" + std::to_string(water_product.material) + "/" +
                           std::to_string(atmosphere_product.material) +
                           " atmosphere_pressure=" +
                           std::to_string(atmosphere_product.aux & 255u));
            }

            {
                const auto saved_step = simulation_step;
                constexpr std::uint32_t respiration_x = 100u;
                constexpr std::uint32_t respiration_y = 100u;
                std::uint32_t respiration_step = 0u;
                bool respiration_step_found = false;
                for (std::uint32_t candidate = 0u;
                     candidate < 4096u && !respiration_step_found; ++candidate) {
                    const auto random_value = fill_hash(
                        respiration_x * 73856093u ^
                        respiration_y * 19349663u ^
                        candidate * 83492791u ^ random_seed ^ 54u);
                    if ((random_value & 127u) == 0u) {
                        respiration_step = candidate;
                        respiration_step_found = true;
                    }
                }
                auto respiration_cells = acceptance_atmosphere_world();
                respiration_cells[index_of(respiration_x - 1u, respiration_y)] =
                    make_fill_cell(
                        material_id(Material::fire),
                        static_cast<std::uint32_t>(
                            index_of(respiration_x - 1u, respiration_y)));
                upload_scene_cells(respiration_cells);
                simulation_step = respiration_step;
                run_acceptance_chemistry_pass();
                const auto respired = download_scene_cells();
                simulation_step = saved_step;
                const auto& carrier =
                    respired[index_of(respiration_x, respiration_y)];
                const auto stored_material = (carrier.aux & 0x00007f00u) >> 8u;
                const auto stored_volume = (carrier.aux & 0x007f8000u) >> 15u;
                const auto oxygen_volume = carrier.aux & 255u;
                append("packed_atmosphere_respiration_conserves_pressure",
                       respiration_step_found &&
                           carrier.material == material_id(Material::atmosphere) &&
                           oxygen_volume == 53u &&
                           stored_material == material_id(Material::carbon_dioxide) &&
                           stored_volume == 1u &&
                           oxygen_volume + stored_volume == 54u,
                       "step=" + std::to_string(respiration_step) +
                           " carrier=" + std::to_string(carrier.material) +
                           " oxygen=" + std::to_string(oxygen_volume) +
                           " stored_material=" + std::to_string(stored_material) +
                           " stored_volume=" + std::to_string(stored_volume));
            }

            {
                const auto saved_step = simulation_step;
                constexpr std::uint32_t fertilizer_x = 100u;
                constexpr std::uint32_t fertilizer_y = 100u;
                constexpr std::uint32_t water_x = 99u;
                constexpr std::uint32_t carbon_x = 101u;
                const auto fertilizer_index = static_cast<std::uint32_t>(
                    index_of(fertilizer_x, fertilizer_y));
                const auto crop_step =
                    (512u - ((fertilizer_index * 3u) & 511u)) & 511u;
                struct CropSnapshot final {
                    SceneCell product;
                    SceneCell fertilizer;
                    SceneCell water;
                    SceneCell carbon;
                };
                const auto run_crop_transaction =
                    [&](const bool packed_carbon, const bool collect_debug) {
                        auto crop_cells = acceptance_atmosphere_world();
                        crop_cells[index_of(fertilizer_x, fertilizer_y - 1u)] =
                            make_fill_cell(material_id(Material::empty),
                                           static_cast<std::uint32_t>(index_of(
                                               fertilizer_x, fertilizer_y - 1u)));
                        auto fertilizer = make_fill_cell(
                            material_id(Material::fertilizer), fertilizer_index);
                        fertilizer.age = 1200u;
                        crop_cells[index_of(fertilizer_x, fertilizer_y)] = fertilizer;
                        crop_cells[index_of(water_x, fertilizer_y)] = make_fill_cell(
                            material_id(Material::water),
                            static_cast<std::uint32_t>(index_of(water_x, fertilizer_y)));
                        crop_cells[index_of(fertilizer_x + 2u, fertilizer_y)] =
                            make_fill_cell(
                                material_id(Material::grass),
                                static_cast<std::uint32_t>(index_of(
                                    fertilizer_x + 2u, fertilizer_y)));
                        auto carbon = make_fill_cell(
                            packed_carbon ? material_id(Material::atmosphere)
                                          : material_id(Material::carbon_dioxide),
                            static_cast<std::uint32_t>(index_of(carbon_x, fertilizer_y)));
                        if (packed_carbon) {
                            carbon.aux = 53u | 0x40000000u |
                                ((material_id(Material::carbon_dioxide) & 0x7fu) << 8u) |
                                (1u << 15u);
                        }
                        crop_cells[index_of(carbon_x, fertilizer_y)] = carbon;
                        upload_scene_cells(crop_cells);
                        simulation_step = crop_step;
                        run_acceptance_sunlight_pass();
                        run_acceptance_chemistry_pass(0, 0, false, collect_debug);
                        const auto result = download_scene_cells();
                        return CropSnapshot{
                            .product = result[index_of(fertilizer_x, fertilizer_y - 1u)],
                            .fertilizer = result[index_of(fertilizer_x, fertilizer_y)],
                            .water = result[index_of(water_x, fertilizer_y)],
                            .carbon = result[index_of(carbon_x, fertilizer_y)],
                        };
                    };
                const auto visible = run_crop_transaction(false, false);
                const auto packed = run_crop_transaction(true, false);
                const auto packed_debug = run_crop_transaction(true, true);
                simulation_step = saved_step;
                const auto visible_stored_material =
                    (visible.carbon.aux & 0x00007f00u) >> 8u;
                const auto visible_stored_volume =
                    (visible.carbon.aux & 0x007f8000u) >> 15u;
                append("closed_crop_visible_co2_water_biomass",
                       visible.product.material == material_id(Material::food) &&
                           visible.fertilizer.material == material_id(Material::dirt) &&
                           visible.water.material == material_id(Material::empty) &&
                           visible.carbon.material == material_id(Material::atmosphere) &&
                           (visible.carbon.aux & 255u) == 1u &&
                           visible_stored_material ==
                               material_id(Material::carbon_dioxide) &&
                           visible_stored_volume == 179u,
                       "step=" + std::to_string(crop_step) +
                           " product=" + std::to_string(visible.product.material) +
                           " fertilizer=" +
                           std::to_string(visible.fertilizer.material) +
                           " water=" + std::to_string(visible.water.material) +
                           " oxygen=" +
                           std::to_string(visible.carbon.aux & 255u) +
                           " stored_co2=" +
                           std::to_string(visible_stored_volume));
                const auto same_cell = [](const SceneCell& first,
                                          const SceneCell& second) {
                    return first.material == second.material &&
                           first.age == second.age &&
                           first.temperature == second.temperature &&
                           first.aux == second.aux;
                };
                append("closed_crop_stored_co2_debug_identity",
                       packed.product.material == material_id(Material::food) &&
                           packed.fertilizer.material == material_id(Material::dirt) &&
                           packed.water.material == material_id(Material::empty) &&
                           packed.carbon.material == material_id(Material::atmosphere) &&
                           (packed.carbon.aux & 255u) == 54u &&
                           (packed.carbon.aux & 0x007f8000u) == 0u &&
                           (packed.carbon.aux & 0x40000000u) == 0u &&
                           same_cell(packed.product, packed_debug.product) &&
                           same_cell(packed.fertilizer, packed_debug.fertilizer) &&
                           same_cell(packed.water, packed_debug.water) &&
                           same_cell(packed.carbon, packed_debug.carbon),
                       "oxygen=" + std::to_string(packed.carbon.aux & 255u) +
                           " packed_aux=" + std::to_string(packed.carbon.aux) +
                           " debug_aux=" +
                           std::to_string(packed_debug.carbon.aux));
            }

            {
                const auto saved_step = simulation_step;
                constexpr std::uint32_t water_x = 100u;
                constexpr std::uint32_t water_y = 100u;
                constexpr std::uint32_t donor_x = water_x;
                constexpr std::uint32_t donor_y = water_y - 1u;
                constexpr std::uint32_t moved_bit = 0x01000000u;
                constexpr std::uint32_t dissolved_bit = 0x08000000u;
                const auto water_index = static_cast<std::uint32_t>(
                    index_of(water_x, water_y));
                const auto donor_index = static_cast<std::uint32_t>(
                    index_of(donor_x, donor_y));
                const auto pair = water_index ^ (donor_index * 0x9e3779b9u);
                std::uint32_t aeration_step = 0u;
                bool aeration_step_found = false;
                for (std::uint32_t candidate = 0u;
                     candidate < 4096u && !aeration_step_found; ++candidate) {
                    if ((candidate & 3u) == 3u &&
                        (fill_hash(pair ^ candidate * 0x85ebca6bu ^
                                   random_seed ^ 0xa311u) & 15u) == 0u) {
                        aeration_step = candidate;
                        aeration_step_found = true;
                    }
                }
                struct AerationSnapshot final {
                    SceneCell acquired_water;
                    SceneCell acquired_air;
                    SceneCell held_water;
                    SceneCell released_water;
                    SceneCell released_air;
                };
                const auto run_aeration = [&](const bool collect_debug) {
                    auto aeration_cells = acceptance_atmosphere_world();
                    auto water = make_fill_cell(
                        material_id(Material::water), water_index);
                    water.temperature = 20;
                    water.aux = moved_bit;
                    aeration_cells[index_of(water_x, water_y)] = water;
                    auto donor = make_fill_cell(
                        material_id(Material::atmosphere), donor_index);
                    donor.temperature = 20;
                    donor.aux = 54u;
                    aeration_cells[index_of(donor_x, donor_y)] = donor;
                    upload_scene_cells(aeration_cells);
                    simulation_step = aeration_step;
                    run_acceptance_chemistry_pass(0, 0, false, collect_debug);
                    const auto acquired = download_scene_cells();
                    ++simulation_step;
                    run_acceptance_chemistry_pass(0, 0, false, collect_debug);
                    const auto held = download_scene_cells();
                    ++simulation_step;
                    run_acceptance_chemistry_pass(0, 0, false, collect_debug);
                    const auto released = download_scene_cells();
                    return AerationSnapshot{
                        .acquired_water = acquired[index_of(water_x, water_y)],
                        .acquired_air = acquired[index_of(donor_x, donor_y)],
                        .held_water = held[index_of(water_x, water_y)],
                        .released_water = released[index_of(water_x, water_y)],
                        .released_air = released[index_of(water_x - 1u, water_y)],
                    };
                };
                const auto normal = run_aeration(false);
                const auto debug = run_aeration(true);
                simulation_step = saved_step;
                const auto same_cell = [](const SceneCell& first,
                                          const SceneCell& second) {
                    return first.material == second.material &&
                           first.age == second.age &&
                           first.temperature == second.temperature &&
                           first.aux == second.aux;
                };
                const auto dissolved_material =
                    (normal.acquired_water.aux & 0x00007f00u) >> 8u;
                const auto dissolved_temperature =
                    static_cast<std::int32_t>(
                        (normal.acquired_water.aux & 0x007f8000u) >> 15u) - 100;
                const bool acquisition_conserved = aeration_step_found &&
                    normal.acquired_water.material == material_id(Material::water) &&
                    (normal.acquired_water.aux & dissolved_bit) != 0u &&
                    dissolved_material == material_id(Material::oxygen) &&
                    dissolved_temperature == 20 &&
                    (normal.acquired_air.aux & 255u) == 53u;
                const bool hold_is_unsplit =
                    normal.held_water.material == material_id(Material::water) &&
                    (normal.held_water.aux & dissolved_bit) != 0u &&
                    (normal.held_water.aux & 0x00800000u) == 0u &&
                    (normal.held_water.aux & moved_bit) == 0u;
                const bool release_conserved =
                    normal.released_water.material == material_id(Material::water) &&
                    (normal.released_water.aux &
                        (dissolved_bit | 0x007fff00u | 0x00800000u)) == 0u &&
                    normal.released_air.material == material_id(Material::atmosphere) &&
                    (normal.released_air.aux & 255u) == 55u;
                append("waterfall_dissolved_oxygen_closed_transaction",
                       acquisition_conserved && hold_is_unsplit && release_conserved,
                       "step=" + std::to_string(aeration_step) +
                           " acquired_o2=" +
                           std::to_string(normal.acquired_air.aux & 255u) +
                           "+1 held_temp=" +
                           std::to_string(dissolved_temperature) +
                           " released_o2=" +
                           std::to_string(normal.released_air.aux & 255u));
                append("waterfall_aeration_debug_identity",
                       same_cell(normal.acquired_water, debug.acquired_water) &&
                           same_cell(normal.acquired_air, debug.acquired_air) &&
                           same_cell(normal.held_water, debug.held_water) &&
                           same_cell(normal.released_water, debug.released_water) &&
                           same_cell(normal.released_air, debug.released_air),
                       "normal_released_aux=" +
                           std::to_string(normal.released_air.aux) +
                           " debug_released_aux=" +
                           std::to_string(debug.released_air.aux));
            }

            {
                const auto saved_step = simulation_step;
                constexpr std::uint32_t vent_x = 100u;
                constexpr std::uint32_t vent_y = 101u;
                const auto vent_index = static_cast<std::uint32_t>(
                    index_of(vent_x, vent_y));
                const auto vent_offset =
                    fill_hash(vent_index ^ random_seed ^ 0x76e17u) % 10800u;
                std::uint32_t emission_step = 0u;
                bool emission_step_found = false;
                for (std::uint32_t candidate = 0u;
                     candidate < 10800u && !emission_step_found; ++candidate) {
                    const auto cycle = (candidate + vent_offset) % 10800u;
                    const auto roll = fill_hash(
                        vent_index ^ candidate ^ 0x76e17u);
                    if ((cycle % 900u) < 420u && (roll & 3u) != 0u) {
                        emission_step = candidate;
                        emission_step_found = true;
                    }
                }

                auto converted_cells = acceptance_atmosphere_world();
                auto vent = make_fill_cell(
                    material_id(Material::magma_vent), vent_index);
                vent.aux = (vent.aux & ~255u) | 100u;
                converted_cells[index_of(vent_x, vent_y)] = vent;
                converted_cells[index_of(vent_x, vent_y - 1u)] = make_fill_cell(
                    material_id(Material::lava),
                    static_cast<std::uint32_t>(index_of(vent_x, vent_y - 1u)));
                upload_scene_cells(converted_cells);
                simulation_step = emission_step;
                run_acceptance_chemistry_pass();
                const auto converted = download_scene_cells();
                const auto& ejecta = converted[index_of(vent_x, vent_y - 1u)];
                const bool exact_ejecta =
                    ejecta.material == material_id(Material::ash) ||
                    ejecta.material == material_id(Material::smoke) ||
                    ejecta.material == material_id(Material::steam);
                const auto converted_units = count_material(converted, Material::lava) +
                    count_material(converted, Material::ash) +
                    count_material(converted, Material::smoke) +
                    count_material(converted, Material::steam);

                auto ambient_cells = acceptance_atmosphere_world();
                auto ambient_vent = make_fill_cell(
                    material_id(Material::magma_vent), vent_index);
                ambient_vent.aux = (ambient_vent.aux & ~255u) | 100u;
                ambient_cells[index_of(vent_x, vent_y)] = ambient_vent;
                upload_scene_cells(ambient_cells);
                simulation_step = emission_step;
                run_acceptance_chemistry_pass();
                const auto ambient = download_scene_cells();
                const auto& ambient_outlet =
                    ambient[index_of(vent_x, vent_y - 1u)];
                const auto& recharged_vent = ambient[index_of(vent_x, vent_y)];
                simulation_step = saved_step;
                append("volcano_converts_owned_lava_without_overwriting_ambient",
                       emission_step_found && exact_ejecta && converted_units == 1u &&
                           ambient_outlet.material ==
                               material_id(Material::atmosphere) &&
                           count_material(ambient, Material::ash) == 0u &&
                           count_material(ambient, Material::smoke) == 0u &&
                           count_material(ambient, Material::steam) == 0u &&
                           (recharged_vent.aux & 255u) == 120u,
                       "step=" + std::to_string(emission_step) +
                           " ejecta=" + std::to_string(ejecta.material) +
                           " converted_units=" +
                           std::to_string(converted_units) +
                           " ambient=" +
                           std::to_string(ambient_outlet.material) +
                           " recharge=" +
                           std::to_string(recharged_vent.aux & 255u));
            }

            {
                const auto saved_step = simulation_step;
                auto boiler_cells = acceptance_atmosphere_world();
                boiler_cells[index_of(100u, 100u)] = make_fill_cell(
                    material_id(Material::water),
                    static_cast<std::uint32_t>(index_of(100u, 100u)));
                boiler_cells[index_of(99u, 100u)] = make_fill_cell(
                    material_id(Material::smelter),
                    static_cast<std::uint32_t>(index_of(99u, 100u)));
                boiler_cells[index_of(99u, 101u)] = make_fill_cell(
                    material_id(Material::power_cell),
                    static_cast<std::uint32_t>(index_of(99u, 101u)));
                upload_scene_cells(boiler_cells);
                run_acceptance_chemistry_pass();
                const auto boiled = download_scene_cells();
                const bool powered_boiler_cycle =
                    boiled[index_of(100u, 100u)].material ==
                        material_id(Material::steam) &&
                    count_material(boiled, Material::water) +
                        count_material(boiled, Material::steam) == 1u;

                auto feed_cells = acceptance_atmosphere_world();
                for (std::uint32_t y = 100u; y < 103u; ++y) {
                    for (std::uint32_t x = 100u; x < 103u; ++x) {
                        auto cloud = make_fill_cell(
                            material_id(Material::cloud),
                            static_cast<std::uint32_t>(index_of(x, y)));
                        cloud.age = 300u;
                        cloud.temperature = 12;
                        feed_cells[index_of(x, y)] = cloud;
                    }
                }
                auto steam = make_fill_cell(
                    material_id(Material::steam),
                    static_cast<std::uint32_t>(index_of(103u, 101u)));
                steam.age = 121u;
                steam.temperature = 60;
                feed_cells[index_of(103u, 101u)] = steam;
                simulation_step = 239u;
                upload_scene_cells(feed_cells);
                run_acceptance_chemistry_pass();
                const auto fed = download_scene_cells();
                const auto fed_clouds = count_material(fed, Material::cloud);
                const auto remaining_steam = count_material(fed, Material::steam);

                constexpr std::uint32_t rain_y = 100u;
                constexpr std::uint32_t rain_age = 700u;
                const auto rain_candidate =
                    find_scheduled_rain_candidate(2u, 253u);
                const auto rain_x = rain_candidate ? rain_candidate->x : 2u;
                const auto rain_step = rain_candidate ? rain_candidate->step : 0u;
                auto rain_cells = acceptance_atmosphere_world();
                if (rain_candidate) {
                    for (std::uint32_t y = rain_y - 1u; y <= rain_y; ++y) {
                        for (std::uint32_t x = rain_x - 1u; x <= rain_x + 1u; ++x) {
                            auto cloud = make_fill_cell(
                                material_id(Material::cloud),
                                static_cast<std::uint32_t>(index_of(x, y)));
                            cloud.age = rain_age;
                            cloud.temperature = 12;
                            rain_cells[index_of(x, y)] = cloud;
                        }
                    }
                }
                upload_scene_cells(rain_cells);
                simulation_step = 4799u;
                run_acceptance_chemistry_pass();
                const auto dry = download_scene_cells();
                const auto dry_water = count_material(dry, Material::water) +
                    count_material(dry, Material::dirty_water);
                const auto dry_clouds = count_material(dry, Material::cloud);

                upload_scene_cells(rain_cells);
                simulation_step = rain_step;
                run_acceptance_chemistry_pass();
                const auto rained = download_scene_cells();
                simulation_step = saved_step;
                const auto rain_water = count_material(rained, Material::water) +
                    count_material(rained, Material::dirty_water);
                const auto remaining_clouds = count_material(rained, Material::cloud);
                append("cloud_first_conserved_weather_cycle",
                       powered_boiler_cycle && fed_clouds == 10u &&
                           remaining_steam == 0u && rain_candidate &&
                           dry_water == 0u && dry_clouds == 6u &&
                           rain_water == 1u &&
                           remaining_clouds + rain_water == 6u,
                       "powered_boiler=" +
                            std::to_string(powered_boiler_cycle ? 1u : 0u) +
                            " fed_clouds=" + std::to_string(fed_clouds) +
                           " remaining_steam=" + std::to_string(remaining_steam) +
                           " dry_water/clouds=" + std::to_string(dry_water) + "/" +
                           std::to_string(dry_clouds) +
                           " rain_water=" + std::to_string(rain_water) +
                           " remaining_clouds=" + std::to_string(remaining_clouds) +
                           " rain_cell=" + std::to_string(rain_x) + "," +
                           std::to_string(rain_y) +
                           " step=" + std::to_string(rain_step) +
                           " cloud_age=" + std::to_string(rain_age) +
                           " result=" + std::to_string(
                               rained[index_of(rain_x, rain_y)].material) +
                           "/" + std::to_string(
                               rained[index_of(rain_x, rain_y)].age));
            }

            {
                const auto saved_step = simulation_step;
                constexpr std::uint32_t tracked_rain_bit = 0x10000000u;
                constexpr std::uint32_t moved_bit = 0x01000000u;
                const auto drop_x = (std::min)(config.grid_width - 4u, 252u);
                constexpr std::uint32_t drop_y = 100u;
                constexpr std::uint32_t landing_y = 103u;
                constexpr std::uint32_t isolated_x = 230u;
                constexpr std::uint32_t pool_x = 220u;
                auto cells = acceptance_atmosphere_world();
                auto drop = make_fill_cell(
                    material_id(Material::water),
                    static_cast<std::uint32_t>(index_of(drop_x, drop_y)));
                drop.aux |= tracked_rain_bit;
                drop.temperature = 11;
                cells[index_of(drop_x, drop_y)] = drop;
                cells[index_of(drop_x, landing_y + 1u)] = make_fill_cell(
                    material_id(Material::stone),
                    static_cast<std::uint32_t>(
                        index_of(drop_x, landing_y + 1u)));
                cells[index_of(isolated_x, drop_y)] = make_fill_cell(
                    material_id(Material::water),
                    static_cast<std::uint32_t>(index_of(isolated_x, drop_y)));
                for (std::uint32_t x = pool_x; x < pool_x + 3u; ++x) {
                    cells[index_of(x, landing_y)] = make_fill_cell(
                        material_id(Material::water),
                        static_cast<std::uint32_t>(index_of(x, landing_y)));
                    cells[index_of(x, landing_y + 1u)] = make_fill_cell(
                        material_id(Material::stone),
                        static_cast<std::uint32_t>(index_of(x, landing_y + 1u)));
                }
                upload_scene_cells(cells, true);
                const auto before = download_scene_cells();
                for (std::uint32_t pass = 0u; pass < 16u; ++pass)
                    run_acceptance_rainfall_pass();
                const auto after = download_scene_cells();
                simulation_step = saved_step;
                const auto same_cell = [](const SceneCell& left,
                                          const SceneCell& right) {
                    return left.material == right.material &&
                        left.age == right.age &&
                        left.temperature == right.temperature &&
                        left.aux == right.aux;
                };
                bool pool_unchanged = true;
                for (std::uint32_t x = pool_x; x < pool_x + 3u; ++x)
                    pool_unchanged = pool_unchanged &&
                        same_cell(before[index_of(x, landing_y)],
                                  after[index_of(x, landing_y)]);
                const auto& landed = after[index_of(drop_x, landing_y)];
                append("scheduled_rain_continues_off_window_without_pool_disturbance",
                       drop_x >= 192u &&
                           landed.material == material_id(Material::water) &&
                           landed.temperature == 11 &&
                           (landed.aux & tracked_rain_bit) == 0u &&
                           (landed.aux & moved_bit) != 0u &&
                           same_cell(before[index_of(isolated_x, drop_y)],
                                     after[index_of(isolated_x, drop_y)]) &&
                           pool_unchanged &&
                           count_material(before, Material::water) ==
                               count_material(after, Material::water) &&
                           count_material(after, Material::empty) == 0u,
                       "drop=" + std::to_string(drop_x) + "," +
                           std::to_string(landing_y) +
                           " material/temp/aux=" +
                           std::to_string(landed.material) + "/" +
                           std::to_string(landed.temperature) + "/" +
                           std::to_string(landed.aux) +
                           " pool_unchanged=" +
                           std::to_string(pool_unchanged ? 1u : 0u) +
                           " water_before/after=" +
                           std::to_string(count_material(before, Material::water)) +
                           "/" +
                           std::to_string(count_material(after, Material::water)) +
                           " vacuum=" +
                           std::to_string(count_material(after, Material::empty)));
            }

            {
                const auto saved_step = simulation_step;
                constexpr std::uint32_t water_x = 100u;
                constexpr std::uint32_t water_y = 100u;
                auto cells = acceptance_atmosphere_world();
                auto water = make_fill_cell(
                    material_id(Material::water),
                    static_cast<std::uint32_t>(index_of(water_x, water_y)));
                water.temperature = 47;
                cells[index_of(water_x, water_y)] = water;
                cells[index_of(water_x, water_y + 1u)] = make_fill_cell(
                    material_id(Material::dirt),
                    static_cast<std::uint32_t>(index_of(water_x, water_y + 1u)));
                cells[index_of(water_x, water_y + 2u)] = make_fill_cell(
                    material_id(Material::stone),
                    static_cast<std::uint32_t>(index_of(water_x, water_y + 2u)));
                std::uint32_t percolation_step = 0u;
                for (std::uint32_t candidate = 0u; candidate < 65536u; ++candidate) {
                    const auto roll = fill_hash(
                        water_x * 73856093u ^ water_y * 19349663u ^
                        candidate * 83492791u ^ random_seed ^ 0x57u);
                    if ((roll & 255u) == 0u) {
                        percolation_step = candidate;
                        break;
                    }
                }
                upload_scene_cells(cells);
                simulation_step = percolation_step;
                run_acceptance_tile_pass();
                run_acceptance_fine_pass(0, 0);
                const auto result = download_scene_cells();
                simulation_step = saved_step;
                constexpr std::uint32_t wet_bit = 0x80000000u;
                const auto& wet_earth = result[index_of(water_x, water_y)];
                const auto& descended_water = result[index_of(water_x, water_y + 1u)];
                append("water_percolates_as_exact_owner_until_stone",
                       wet_earth.material == material_id(Material::dirt) &&
                           (wet_earth.aux & wet_bit) != 0u &&
                           descended_water.material == material_id(Material::water) &&
                           descended_water.temperature == 47 &&
                           result[index_of(water_x, water_y + 2u)].material ==
                               material_id(Material::stone) &&
                           count_material(result, Material::water) == 1u &&
                           count_material(result, Material::dirt) == 1u &&
                           count_material(result, Material::empty) == 0u,
                       "step=" + std::to_string(percolation_step) +
                           " earth=" + std::to_string(wet_earth.material) +
                           "/" + std::to_string(wet_earth.aux) +
                           " water=" + std::to_string(descended_water.material) +
                           "/" + std::to_string(descended_water.temperature));
            }
            {
                constexpr std::uint32_t grass_x = 120u;
                constexpr std::uint32_t grass_top = 96u;
                auto cells = acceptance_atmosphere_world();
                for (std::uint32_t y = grass_top; y < grass_top + 8u; ++y) {
                    cells[index_of(grass_x, y)] = make_fill_cell(
                        material_id(Material::grass),
                        static_cast<std::uint32_t>(index_of(grass_x, y)));
                }
                upload_scene_cells(cells);
                run_acceptance_chemistry_pass();
                const auto result = download_scene_cells();
                std::uint32_t shallow_grass = 0u;
                std::uint32_t buried_dirt = 0u;
                for (std::uint32_t y = grass_top; y < grass_top + 8u; ++y) {
                    const auto material = result[index_of(grass_x, y)].material;
                    if (material == material_id(Material::grass)) ++shallow_grass;
                    if (material == material_id(Material::dirt)) ++buried_dirt;
                }
                append("grass_is_three_cell_skylight_skin",
                       shallow_grass == 3u && buried_dirt == 5u,
                       "grass=" + std::to_string(shallow_grass) +
                           " buried_dirt=" + std::to_string(buried_dirt));
            }
            {
                constexpr std::uint32_t thermal_y = 80u;
                constexpr std::uint32_t fire_x = 64u;
                constexpr std::uint32_t ember_x = 80u;
                constexpr std::uint32_t lava_x = 96u;
                auto cells = acceptance_atmosphere_world();
                seed_rect(cells, Material::fire, fire_x, thermal_y, 8u, 8u);
                seed_rect(cells, Material::ember, ember_x, thermal_y, 8u, 8u);
                seed_rect(cells, Material::lava, lava_x, thermal_y, 8u, 8u);
                upload_scene_cells(cells);
                run_acceptance_tile_pass();
                const auto states = download_tile_states();
                const auto columns = divide_round_up(config.grid_width, 8u);
                constexpr std::uint32_t active_flag = 0x00000008u;
                constexpr std::uint32_t sleeping_flag = 0x00000004u;
                constexpr std::uint32_t fine_flag = 0x00020000u;
                const auto active_thermal = [&](const std::uint32_t x) {
                    const auto flags = states[(thermal_y / 8u) * columns + x / 8u].flags;
                    return (flags & active_flag) != 0u &&
                           (flags & fine_flag) != 0u &&
                           (flags & sleeping_flag) == 0u;
                };
                const bool fire_active = active_thermal(fire_x);
                const bool ember_active = active_thermal(ember_x);
                const bool lava_active = active_thermal(lava_x);
                append("fire_ember_lava_are_always_active_thermal_owners",
                       fire_active && ember_active && lava_active,
                       "fire/ember/lava=" +
                           std::to_string(fire_active ? 1u : 0u) + "/" +
                           std::to_string(ember_active ? 1u : 0u) + "/" +
                           std::to_string(lava_active ? 1u : 0u));
            }

            {
                const auto saved_step = simulation_step;
                constexpr std::uint32_t acid_x = 100u;
                constexpr std::uint32_t acid_y = 100u;
                auto acid_cells = acceptance_atmosphere_world();
                auto acid = make_fill_cell(
                    material_id(Material::acid),
                    static_cast<std::uint32_t>(index_of(acid_x, acid_y)));
                acid_cells[index_of(acid_x, acid_y)] = acid;
                acid_cells[index_of(acid_x + 1u, acid_y)] = make_fill_cell(
                    material_id(Material::water),
                    static_cast<std::uint32_t>(index_of(acid_x + 1u, acid_y)));
                std::uint32_t acid_trigger_step = 0u;
                for (std::uint32_t candidate = 0u; candidate < 8192u; ++candidate) {
                    const auto roll = fill_hash(
                        acid_x * 73856093u ^ acid_y * 19349663u ^
                        candidate * 83492791u ^ random_seed ^ acid.aux);
                    if ((roll & 511u) == 0u) {
                        acid_trigger_step = candidate;
                        break;
                    }
                }
                upload_scene_cells(acid_cells);
                simulation_step = acid_trigger_step;
                run_acceptance_chemistry_pass();
                const auto acid_result = download_scene_cells();

                constexpr std::uint32_t waste_x = 112u;
                constexpr std::uint32_t waste_y = 100u;
                auto waste_cells = acceptance_atmosphere_world();
                auto waste = make_fill_cell(
                    material_id(Material::waste),
                    static_cast<std::uint32_t>(index_of(waste_x, waste_y)));
                waste_cells[index_of(waste_x, waste_y)] = waste;
                waste_cells[index_of(waste_x + 1u, waste_y)] = make_fill_cell(
                    material_id(Material::water),
                    static_cast<std::uint32_t>(index_of(waste_x + 1u, waste_y)));
                std::uint32_t waste_trigger_step = 0u;
                for (std::uint32_t candidate = 0u; candidate < 8192u; ++candidate) {
                    const auto roll = fill_hash(
                        waste_x * 73856093u ^ waste_y * 19349663u ^
                        candidate * 83492791u ^ random_seed ^ waste.aux);
                    if ((roll & 255u) == 0u) {
                        waste_trigger_step = candidate;
                        break;
                    }
                }
                upload_scene_cells(waste_cells);
                simulation_step = waste_trigger_step;
                run_acceptance_chemistry_pass();
                const auto waste_result = download_scene_cells();
                simulation_step = saved_step;

                append("acid_and_moist_waste_never_manufacture_water",
                       acid_result[index_of(acid_x, acid_y)].material ==
                               material_id(Material::acid) &&
                           count_material(acid_result, Material::water) == 1u &&
                           count_material(acid_result, Material::dirty_water) == 0u &&
                           waste_result[index_of(waste_x, waste_y)].material ==
                               material_id(Material::fertilizer) &&
                           count_material(waste_result, Material::water) == 1u &&
                           count_material(waste_result, Material::dirty_water) == 0u,
                       "acid_step=" + std::to_string(acid_trigger_step) +
                           " acid_result=" + std::to_string(
                               acid_result[index_of(acid_x, acid_y)].material) +
                           " acid_water=" +
                           std::to_string(count_material(acid_result, Material::water)) +
                           " waste_step=" + std::to_string(waste_trigger_step) +
                           " waste_result=" + std::to_string(
                               waste_result[index_of(waste_x, waste_y)].material) +
                           " waste_water=" +
                           std::to_string(count_material(waste_result, Material::water)));
            }

            {
                constexpr std::uint32_t charged_bit = 0x40000000u;
                constexpr std::uint32_t stored_material_mask = 0x00007f00u;
                constexpr std::uint32_t stored_volume_mask = 0x007f8000u;
                auto cells = acceptance_atmosphere_world();
                auto smoke = make_fill_cell(
                    material_id(Material::smoke),
                    static_cast<std::uint32_t>(index_of(100u, 100u)));
                smoke.age = 64u;
                cells[index_of(100u, 100u)] = smoke;
                upload_scene_cells(cells);
                run_acceptance_horizontal_pass(0);
                const auto result = download_scene_cells();
                const auto& first = result[index_of(100u, 100u)];
                const auto& second = result[index_of(101u, 100u)];
                const auto first_stored_material =
                    (first.aux & stored_material_mask) >> 8u;
                const auto second_stored_material =
                    (second.aux & stored_material_mask) >> 8u;
                const auto first_stored_volume =
                    (first.aux & stored_volume_mask) >> 15u;
                const auto second_stored_volume =
                    (second.aux & stored_volume_mask) >> 15u;
                const auto pressure = (first.aux & 255u) + (second.aux & 255u);
                const auto stored_volume = first_stored_volume + second_stored_volume;
                const bool stored_materials_valid =
                    (first_stored_volume == 0u ||
                     first_stored_material == material_id(Material::smoke)) &&
                    (second_stored_volume == 0u ||
                     second_stored_material == material_id(Material::smoke));
                append("atmosphere_delayed_excess_reabsorption",
                       first.material == material_id(Material::atmosphere) &&
                           second.material == material_id(Material::atmosphere) &&
                           ((first.aux | second.aux) & charged_bit) != 0u &&
                           pressure == 54u && stored_volume == 1u &&
                           stored_materials_valid,
                       "first_material=" + std::to_string(first.material) +
                           " second_material=" + std::to_string(second.material) +
                           " pressure=" + std::to_string(pressure) +
                           " stored_volume=" + std::to_string(stored_volume) +
                           " first_aux=" + std::to_string(first.aux) +
                           " second_aux=" + std::to_string(second.aux));
            }

            {
                auto cells = acceptance_atmosphere_world();
                seed_rect(cells, Material::water, 80u, 80u, 8u, 8u);
                cells[index_of(100u, 100u)] = make_fill_cell(
                    material_id(Material::hydrogen),
                    static_cast<std::uint32_t>(index_of(100u, 100u)));
                upload_scene_cells(cells);
                const auto before_debug = download_scene_cells();
                immediate_submit([&](const VkCommandBuffer command_buffer) {
                    reset_debug_stats(command_buffer);
                    record_debug_stats(command_buffer, state, 0u);
                });
                const auto after_debug = download_scene_cells();
                const bool identical = before_debug.size() == after_debug.size() &&
                    std::equal(before_debug.begin(), before_debug.end(),
                               after_debug.begin(),
                               [](const SceneCell& before, const SceneCell& after) {
                                   return before.material == after.material &&
                                       before.age == after.age &&
                                       before.temperature == after.temperature &&
                                       before.aux == after.aux;
                               });
                append("debug_collection_is_read_only", identical,
                       "cells_compared=" + std::to_string(before_debug.size()));
            }

            {
                const auto saved_step = simulation_step;

                auto hot_water_cells = acceptance_atmosphere_world();
                auto hot_water = make_fill_cell(
                    material_id(Material::water),
                    static_cast<std::uint32_t>(index_of(100u, 100u)));
                hot_water.temperature = 125;
                hot_water_cells[index_of(100u, 100u)] = hot_water;
                upload_scene_cells(hot_water_cells);
                run_acceptance_chemistry_pass();
                const auto boiled = download_scene_cells();
                const auto& carried_steam = boiled[index_of(100u, 100u)];

                auto condense_cells = acceptance_atmosphere_world();
                auto cooling_steam = make_fill_cell(
                    material_id(Material::steam),
                    static_cast<std::uint32_t>(index_of(100u, 100u)));
                cooling_steam.age = 121u;
                cooling_steam.temperature = 60;
                condense_cells[index_of(100u, 100u)] = cooling_steam;
                auto cloud_neighbor = make_fill_cell(
                    material_id(Material::cloud),
                    static_cast<std::uint32_t>(index_of(101u, 100u)));
                cloud_neighbor.temperature = 12;
                condense_cells[index_of(101u, 100u)] = cloud_neighbor;
                upload_scene_cells(condense_cells);
                run_acceptance_chemistry_pass();
                const auto condensed = download_scene_cells();
                const auto& carried_cloud = condensed[index_of(100u, 100u)];

                constexpr std::uint32_t rain_y = 100u;
                constexpr std::uint32_t rain_age = 700u;
                const auto rain_candidate =
                    find_scheduled_rain_candidate(2u, 253u);
                const auto rain_x = rain_candidate ? rain_candidate->x : 2u;
                const auto rain_step = rain_candidate ? rain_candidate->step : 0u;
                auto rain_cells = acceptance_atmosphere_world();
                if (rain_candidate) {
                    for (std::uint32_t y = rain_y - 1u;
                         y <= rain_y; ++y) {
                        for (std::uint32_t x = rain_x - 1u;
                             x <= rain_x + 1u; ++x) {
                            auto cloud = make_fill_cell(
                                material_id(Material::cloud),
                                static_cast<std::uint32_t>(index_of(x, y)));
                            cloud.age = rain_age;
                            cloud.temperature = 13;
                            rain_cells[index_of(x, y)] = cloud;
                        }
                    }
                }
                upload_scene_cells(rain_cells);
                simulation_step = rain_step;
                run_acceptance_chemistry_pass();
                const auto rained = download_scene_cells();
                const auto& carried_rain =
                    rained[index_of(rain_x, rain_y)];

                auto extinguish_cells = acceptance_atmosphere_world();
                extinguish_cells[index_of(100u, 100u)] = make_fill_cell(
                    material_id(Material::fire),
                    static_cast<std::uint32_t>(index_of(100u, 100u)));
                extinguish_cells[index_of(101u, 100u)] = make_fill_cell(
                    material_id(Material::water),
                    static_cast<std::uint32_t>(index_of(101u, 100u)));
                upload_scene_cells(extinguish_cells);
                run_acceptance_chemistry_pass();
                const auto extinguished = download_scene_cells();
                const auto water_family_after_extinguish =
                    count_material(extinguished, Material::water) +
                    count_material(extinguished, Material::steam) +
                    count_material(extinguished, Material::cloud);

                auto cooling_cells = acceptance_atmosphere_world();
                auto isolated_lava = make_fill_cell(
                    material_id(Material::lava),
                    static_cast<std::uint32_t>(index_of(100u, 100u)));
                isolated_lava.temperature = 900;
                cooling_cells[index_of(100u, 100u)] = isolated_lava;
                cooling_cells[index_of(101u, 100u)] = make_fill_cell(
                    material_id(Material::water),
                    static_cast<std::uint32_t>(index_of(101u, 100u)));
                upload_scene_cells(cooling_cells);
                run_acceptance_chemistry_pass();
                const auto cooled = download_scene_cells();
                const auto& cooled_stone = cooled[index_of(100u, 100u)];
                const auto cooled_rock_family =
                    count_material(cooled, Material::stone) +
                    count_material(cooled, Material::lava);
                const auto cooled_water_family =
                    count_material(cooled, Material::water) +
                    count_material(cooled, Material::steam) +
                    count_material(cooled, Material::cloud);

                auto reheating_cells = acceptance_atmosphere_world();
                auto reheated_stone = make_fill_cell(
                    material_id(Material::stone),
                    static_cast<std::uint32_t>(index_of(100u, 100u)));
                reheated_stone.temperature = 950;
                reheating_cells[index_of(100u, 100u)] = reheated_stone;
                for (const auto& offset : std::array{
                         std::pair{-1, 0}, std::pair{1, 0},
                         std::pair{0, -1}, std::pair{0, 1}}) {
                    reheating_cells[index_of(
                        static_cast<std::uint32_t>(100 + offset.first),
                        static_cast<std::uint32_t>(100 + offset.second))] =
                        make_fill_cell(
                            material_id(Material::lava),
                            static_cast<std::uint32_t>(index_of(
                                static_cast<std::uint32_t>(100 + offset.first),
                                static_cast<std::uint32_t>(100 + offset.second))));
                }
                upload_scene_cells(reheating_cells);
                run_acceptance_chemistry_pass();
                const auto reheated = download_scene_cells();
                const auto& reheated_lava = reheated[index_of(100u, 100u)];
                const auto reheated_rock_family =
                    count_material(reheated, Material::stone) +
                    count_material(reheated, Material::lava);
                simulation_step = saved_step;

                append("water_weather_phase_temperature_ownership",
                       carried_steam.material == material_id(Material::steam) &&
                           carried_steam.temperature > 110 &&
                           carried_cloud.material == material_id(Material::cloud) &&
                           carried_cloud.temperature == 60 &&
                           rain_candidate &&
                           carried_rain.material == material_id(Material::water) &&
                           carried_rain.temperature == 13,
                       "steam_temp=" +
                           std::to_string(carried_steam.temperature) +
                           " cloud_temp=" +
                           std::to_string(carried_cloud.temperature) +
                       " rain_temp=" +
                           std::to_string(carried_rain.temperature) +
                           " rain_material=" +
                           std::to_string(carried_rain.material) +
                           " rain_step=" + std::to_string(rain_step) +
                           " cloud_age=" + std::to_string(rain_age));
                append("fire_extinguish_does_not_duplicate_water",
                       extinguished[index_of(100u, 100u)].material ==
                               material_id(Material::empty) &&
                           water_family_after_extinguish == 1u,
                       "fire_result=" +
                           std::to_string(
                               extinguished[index_of(100u, 100u)].material) +
                           " water_family=" +
                           std::to_string(water_family_after_extinguish));
                append("renewable_lava_stone_family_balance",
                       cooled_stone.material == material_id(Material::stone) &&
                           cooled_stone.temperature > 20 &&
                           cooled_rock_family == 1u &&
                           cooled_water_family == 1u &&
                           reheated_lava.material == material_id(Material::lava) &&
                           reheated_lava.temperature >= 900 &&
                           reheated_rock_family == 5u,
                       "cooled_material=" +
                           std::to_string(cooled_stone.material) +
                           " cooled_temp=" +
                           std::to_string(cooled_stone.temperature) +
                           " cooled_families=" +
                           std::to_string(cooled_rock_family) + "/" +
                           std::to_string(cooled_water_family) +
                           " reheated_material=" +
                           std::to_string(reheated_lava.material) +
                           " reheated_temp=" +
                           std::to_string(reheated_lava.temperature) +
                           " reheated_family=" +
                           std::to_string(reheated_rock_family));
            }
            {
                constexpr std::uint32_t charged_bit = 0x40000000u;
                constexpr std::uint32_t wet_bit = 0x80000000u;
                auto cells = acceptance_atmosphere_world();
                const auto controller = SceneCell{
                    .material = material_id(Material::sluice_box),
                    .age = 0u,
                    .temperature = 20,
                    .aux = charged_bit | fill_aux_structural |
                           fill_aux_supported | 255u,
                };
                cells[index_of(99u, 99u)] = controller;
                cells[index_of(96u, 99u)] = SceneCell{
                    .material = material_id(Material::sand),
                    .age = 0u,
                    .temperature = 20,
                    .aux = wet_bit | 255u,
                };
                cells[index_of(98u, 94u)] = make_fill_cell(
                    material_id(Material::water),
                    static_cast<std::uint32_t>(index_of(98u, 94u)));
                cells[index_of(98u, 95u)] = make_fill_cell(
                    material_id(Material::water),
                    static_cast<std::uint32_t>(index_of(98u, 95u)));
                upload_scene_cells(cells);
                for (std::uint32_t pass = 0u; pass < 24u; ++pass)
                    run_acceptance_chemistry_pass();
                const auto before_output = download_scene_cells();
                const auto displaced_pressure =
                    (before_output[index_of(104u, 99u)].aux & 255u) +
                    (before_output[index_of(104u, 98u)].aux & 255u);
                for (std::uint32_t pass = 0u; pass < 2u; ++pass)
                    run_acceptance_chemistry_pass();
                const auto result = download_scene_cells();
                const auto& output = result[index_of(104u, 99u)];
                const auto& vent = result[index_of(104u, 98u)];
                const bool valid_output = output.material == material_id(Material::gold) ||
                    output.material == material_id(Material::sand) ||
                    output.material == material_id(Material::silt);
                append("engineering_sluice_outputs_through_atmosphere",
                       valid_output && vent.material == material_id(Material::atmosphere) &&
                           (vent.aux & 255u) == displaced_pressure &&
                           (result[index_of(99u, 99u)].aux & fill_aux_random_mask) == 0u,
                       "output=" + std::to_string(output.material) +
                           " displaced_pressure=" + std::to_string(displaced_pressure) +
                           " vent_pressure=" + std::to_string(vent.aux & 255u) +
                           " inventory=" + std::to_string(
                               result[index_of(99u, 99u)].aux & fill_aux_random_mask));
            }
            {
                auto cells = acceptance_atmosphere_world();
                const auto target_x = static_cast<std::uint32_t>(actor.x + 24);
                const auto target_y = static_cast<std::uint32_t>(
                    actor.y + player_tool_origin_offset_cells);
                cells[index_of(target_x, target_y)] = make_fill_cell(
                    material_id(Material::stone),
                    static_cast<std::uint32_t>(index_of(target_x, target_y)));
                upload_scene_cells(cells);
                const ActorPush push{
                    .width = config.grid_width,
                    .height = config.grid_height,
                    .step = simulation_step,
                    .seed = random_seed,
                    .aim_x = static_cast<std::int32_t>(target_x),
                    .aim_y = static_cast<std::int32_t>(target_y),
                    .fire = 1u,
                    .scene = static_cast<std::uint32_t>(world_scene),
                    .simulate = 0u,
                    .active_mode = 0u,
                };
                immediate_submit([&](const VkCommandBuffer command_buffer) {
                    bind_compute(command_buffer, actor_pipeline, current_set);
                    vkCmdPushConstants(command_buffer, compute_pipeline_layout,
                                       VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                       sizeof(push), &push);
                    vkCmdDispatch(command_buffer, 1, 1, 1);
                    buffer_barrier(command_buffer, cell_buffers[current_set],
                                   VK_ACCESS_SHADER_WRITE_BIT,
                                   VK_ACCESS_SHADER_READ_BIT |
                                       VK_ACCESS_SHADER_WRITE_BIT,
                                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
                    buffer_barrier(command_buffer, actor_buffer,
                                   VK_ACCESS_SHADER_WRITE_BIT,
                                   VK_ACCESS_SHADER_READ_BIT |
                                       VK_ACCESS_SHADER_WRITE_BIT,
                                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
                });
                const auto fired_actor = download_actor_state();
                const auto fired_cells = download_scene_cells();
                const auto& damaged = fired_cells[index_of(target_x, target_y)];
                append("inventory_player_laser_produces_visible_hit_state",
                       fired_actor.shot_timer == 4u &&
                           fired_actor.hit_x == static_cast<std::int32_t>(target_x) &&
                           fired_actor.hit_y == static_cast<std::int32_t>(target_y) &&
                           damaged.material == material_id(Material::stone) &&
                           (damaged.aux & 255u) == 111u,
                       "shot_timer=" + std::to_string(fired_actor.shot_timer) +
                           " hit=" + std::to_string(fired_actor.hit_x) + "," +
                           std::to_string(fired_actor.hit_y) +
                           " target=" + std::to_string(target_x) + "," +
                           std::to_string(target_y) +
                           " target_health=" +
                           std::to_string(damaged.aux & 255u));
            }
            const auto fire_acceptance_laser = [&](const std::uint32_t target_x,
                                                    const std::uint32_t target_y) {
                const ActorPush push{
                    .width = config.grid_width,
                    .height = config.grid_height,
                    .step = simulation_step,
                    .seed = random_seed,
                    .aim_x = static_cast<std::int32_t>(target_x),
                    .aim_y = static_cast<std::int32_t>(target_y),
                    .fire = 1u,
                    .scene = static_cast<std::uint32_t>(world_scene),
                    .simulate = 0u,
                    .active_mode = 0u,
                };
                immediate_submit([&](const VkCommandBuffer command_buffer) {
                    bind_compute(command_buffer, actor_pipeline, current_set);
                    vkCmdPushConstants(command_buffer, compute_pipeline_layout,
                                       VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                       sizeof(push), &push);
                    vkCmdDispatch(command_buffer, 1, 1, 1);
                    buffer_barrier(command_buffer, cell_buffers[current_set],
                                   VK_ACCESS_SHADER_WRITE_BIT,
                                   VK_ACCESS_SHADER_READ_BIT |
                                       VK_ACCESS_SHADER_WRITE_BIT,
                                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
                    buffer_barrier(command_buffer, actor_buffer,
                                   VK_ACCESS_SHADER_WRITE_BIT,
                                   VK_ACCESS_SHADER_READ_BIT |
                                       VK_ACCESS_SHADER_WRITE_BIT,
                                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
                });
            };
            {
                auto mining_actor = actor;
                mining_actor.gold = 0u;
                mining_actor.shot_timer = 0u;
                const auto target_x = static_cast<std::uint32_t>(mining_actor.x + 24);
                const auto target_y = static_cast<std::uint32_t>(
                    mining_actor.y + player_tool_origin_offset_cells);
                auto cells = acceptance_atmosphere_world();
                cells[index_of(target_x, target_y)] = make_fill_cell(
                    material_id(Material::gold),
                    static_cast<std::uint32_t>(index_of(target_x, target_y)));
                const std::uint64_t before_units = count_material(cells, Material::gold);
                upload_scene_cells(cells);
                upload_actor_state(mining_actor);
                fire_acceptance_laser(target_x, target_y);
                auto rearmed = download_actor_state();
                rearmed.shot_timer = 0u;
                upload_actor_state(rearmed);
                fire_acceptance_laser(target_x, target_y);
                const auto result_actor = download_actor_state();
                const auto result = download_scene_cells();
                const std::uint64_t after_units = count_material(result, Material::gold);
                const auto& fragment = result[index_of(target_x - 2u, target_y)];
                append("player_laser_world_transfer_conserves_without_collection",
                       result_actor.gold == 0u && before_units == after_units &&
                           count_material(result, Material::gold) == 1u &&
                           result[index_of(target_x, target_y)].material ==
                               material_id(Material::atmosphere) &&
                           fragment.material == material_id(Material::gold) &&
                           (fragment.aux & fill_aux_structural) == 0u &&
                           (fragment.aux & 255u) == 1u,
                       "before_units=" + std::to_string(before_units) +
                           " after_units=" + std::to_string(after_units) +
                           " inventory=" + std::to_string(result_actor.gold) +
                           " world_gold=" + std::to_string(
                               count_material(result, Material::gold)) +
                           " fragment=" + std::to_string(fragment.material));
            }
            {
                auto moving_shield_actor = actor;
                moving_shield_actor.gold = 5u;
                moving_shield_actor.shot_timer = 0u;
                const auto target_x = static_cast<std::uint32_t>(
                    moving_shield_actor.x + 28);
                const auto moving_x = target_x - 12u;
                const auto target_y = static_cast<std::uint32_t>(
                    moving_shield_actor.y + player_tool_origin_offset_cells);
                auto cells = acceptance_atmosphere_world();
                auto moving_sand = make_fill_cell(
                    material_id(Material::sand),
                    static_cast<std::uint32_t>(index_of(moving_x, target_y)));
                moving_sand.aux |= fill_aux_moved;
                cells[index_of(moving_x, target_y)] = moving_sand;
                cells[index_of(target_x, target_y)] = make_fill_cell(
                    material_id(Material::gold),
                    static_cast<std::uint32_t>(index_of(target_x, target_y)));
                upload_scene_cells(cells);
                upload_actor_state(moving_shield_actor);
                fire_acceptance_laser(target_x, target_y);
                auto rearmed = download_actor_state();
                rearmed.shot_timer = 0u;
                upload_actor_state(rearmed);
                fire_acceptance_laser(target_x, target_y);
                const auto result_actor = download_actor_state();
                const auto result = download_scene_cells();
                append("player_laser_ignores_moving_cells",
                       result_actor.gold == moving_shield_actor.gold &&
                           result[index_of(moving_x, target_y)].material ==
                               material_id(Material::sand) &&
                           result[index_of(target_x, target_y)].material ==
                               material_id(Material::atmosphere) &&
                           result[index_of(target_x - 2u, target_y)].material ==
                               material_id(Material::gold) &&
                           count_material(result, Material::gold) == 1u,
                       "inventory=" + std::to_string(result_actor.gold) +
                           " moving=" + std::to_string(
                               result[index_of(moving_x, target_y)].material) +
                           " target=" + std::to_string(
                               result[index_of(target_x, target_y)].material));
            }
            {
                auto underside_actor = actor;
                underside_actor.x = 124;
                underside_actor.y = 150;
                underside_actor.gold = 9u;
                underside_actor.shot_timer = 0u;
                constexpr std::uint32_t target_x = 124u;
                constexpr std::uint32_t target_y = 110u;
                auto cells = acceptance_atmosphere_world();
                cells[index_of(target_x, target_y - 1u)] = make_fill_cell(
                    material_id(Material::stone),
                    static_cast<std::uint32_t>(index_of(target_x, target_y - 1u)));
                cells[index_of(target_x, target_y)] = make_fill_cell(
                    material_id(Material::gold),
                    static_cast<std::uint32_t>(index_of(target_x, target_y)));
                // Make the struck owner terminal on the first pulse while the
                // solid immediately above remains an independent owner.
                // This isolates the underside-release direction instead of
                // letting a second pulse open the preferred upward slot first.
                cells[index_of(target_x, target_y)].aux =
                    (cells[index_of(target_x, target_y)].aux & ~fill_aux_state_mask) | 1u;
                upload_scene_cells(cells);
                upload_actor_state(underside_actor);
                fire_acceptance_laser(target_x, target_y);
                const auto result_actor = download_actor_state();
                const auto result = download_scene_cells();
                append("player_laser_accepts_exposed_undersides",
                       result_actor.gold == underside_actor.gold &&
                           result[index_of(target_x, target_y - 1u)].material ==
                               material_id(Material::stone) &&
                           result[index_of(target_x, target_y)].material ==
                               material_id(Material::atmosphere) &&
                           result[index_of(target_x, target_y + 2u)].material ==
                               material_id(Material::gold) &&
                           count_material(result, Material::gold) == 1u,
                       "inventory=" + std::to_string(result_actor.gold) +
                           " source=" + std::to_string(
                               result[index_of(target_x, target_y)].material) +
                           " below=" + std::to_string(
                               result[index_of(target_x, target_y + 2u)].material));
            }
            {
                auto full_actor = actor;
                full_actor.gold = 9999u;
                full_actor.shot_timer = 0u;
                const auto target_x = static_cast<std::uint32_t>(full_actor.x + 24);
                const auto target_y = static_cast<std::uint32_t>(
                    full_actor.y + player_tool_origin_offset_cells);
                auto cells = acceptance_atmosphere_world();
                cells[index_of(target_x, target_y)] = make_fill_cell(
                    material_id(Material::gold),
                    static_cast<std::uint32_t>(index_of(target_x, target_y)));
                const auto seed_guard = [&](const std::uint32_t x,
                                            const std::uint32_t y) {
                    cells[index_of(x, y)] = make_fill_cell(
                        material_id(Material::stone),
                        static_cast<std::uint32_t>(index_of(x, y)));
                };
                seed_guard(target_x, target_y - 1u);
                seed_guard(target_x, target_y + 1u);
                seed_guard(target_x + 1u, target_y);
                const auto guard_above = cells[index_of(target_x, target_y - 1u)];
                const auto guard_below = cells[index_of(target_x, target_y + 1u)];
                const auto guard_behind = cells[index_of(target_x + 1u, target_y)];
                upload_scene_cells(cells);
                upload_actor_state(full_actor);
                fire_acceptance_laser(target_x, target_y);
                auto rearmed = download_actor_state();
                rearmed.shot_timer = 0u;
                upload_actor_state(rearmed);
                fire_acceptance_laser(target_x, target_y);
                const auto result_actor = download_actor_state();
                const auto result = download_scene_cells();
                const auto& fragment = result[index_of(target_x - 2u, target_y)];
                const auto cell_unchanged = [](const SceneCell& lhs,
                                               const SceneCell& rhs) {
                    return lhs.material == rhs.material && lhs.age == rhs.age &&
                           lhs.temperature == rhs.temperature && lhs.aux == rhs.aux;
                };
                const bool adjacent_owners_unchanged =
                    cell_unchanged(result[index_of(target_x, target_y - 1u)], guard_above) &&
                    cell_unchanged(result[index_of(target_x, target_y + 1u)], guard_below) &&
                    cell_unchanged(result[index_of(target_x + 1u, target_y)], guard_behind);
                append("player_laser_releases_exact_loose_fragment",
                       result_actor.gold == 9999u &&
                            count_material(result, Material::gold) == 1u &&
                            adjacent_owners_unchanged &&
                            result[index_of(target_x, target_y)].material ==
                               material_id(Material::atmosphere) &&
                           fragment.material == material_id(Material::gold) &&
                           (fragment.aux & fill_aux_structural) == 0u &&
                           (fragment.aux & 255u) == 1u,
                       "inventory=" + std::to_string(result_actor.gold) +
                            " world_gold=" + std::to_string(
                                count_material(result, Material::gold)) +
                            " adjacent_unchanged=" +
                            std::to_string(adjacent_owners_unchanged ? 1u : 0u) +
                            " fragment=" + std::to_string(fragment.material) +
                           "/" + std::to_string(fragment.aux & 255u));
            }
            {
                auto broad_actor = actor;
                broad_actor.gold = 7u;
                broad_actor.iron = 11u;
                broad_actor.copper = 13u;
                broad_actor.aluminum = 17u;
                broad_actor.shot_timer = 0u;
                const auto water_x = static_cast<std::uint32_t>(broad_actor.x + 24);
                const auto grass_x = water_x + 8u;
                const auto cloud_x = grass_x + 8u;
                const auto bee_x = cloud_x + 8u;
                const auto target_y = static_cast<std::uint32_t>(
                    broad_actor.y + player_tool_origin_offset_cells);
                auto cells = acceptance_atmosphere_world();
                cells[index_of(water_x, target_y)] = make_fill_cell(
                    material_id(Material::water),
                    static_cast<std::uint32_t>(index_of(water_x, target_y)));
                cells[index_of(grass_x, target_y)] = make_fill_cell(
                    material_id(Material::grass),
                    static_cast<std::uint32_t>(index_of(grass_x, target_y)));
                cells[index_of(cloud_x, target_y)] = make_fill_cell(
                    material_id(Material::cloud),
                    static_cast<std::uint32_t>(index_of(cloud_x, target_y)));
                cells[index_of(bee_x, target_y)] = make_fill_cell(
                    material_id(Material::bee),
                    static_cast<std::uint32_t>(index_of(bee_x, target_y)));
                upload_scene_cells(cells);
                upload_actor_state(broad_actor);
                fire_acceptance_laser(water_x, target_y);
                auto rearmed = download_actor_state();
                rearmed.shot_timer = 0u;
                upload_actor_state(rearmed);
                fire_acceptance_laser(grass_x, target_y);
                rearmed = download_actor_state();
                rearmed.shot_timer = 0u;
                upload_actor_state(rearmed);
                fire_acceptance_laser(cloud_x, target_y);
                rearmed = download_actor_state();
                rearmed.shot_timer = 0u;
                upload_actor_state(rearmed);
                fire_acceptance_laser(bee_x, target_y);
                const auto result_actor = download_actor_state();
                const auto result = download_scene_cells();
                const bool inventory_unchanged =
                    result_actor.gold == broad_actor.gold &&
                    result_actor.iron == broad_actor.iron &&
                    result_actor.copper == broad_actor.copper &&
                    result_actor.aluminum == broad_actor.aluminum;
                append("player_laser_targets_condensed_weather_and_life",
                       inventory_unchanged &&
                           count_material(result, Material::water) == 1u &&
                           count_material(result, Material::grass) == 1u &&
                           result[index_of(water_x, target_y)].material ==
                               material_id(Material::atmosphere) &&
                           result[index_of(water_x - 2u, target_y)].material ==
                               material_id(Material::water) &&
                           result[index_of(grass_x, target_y)].material ==
                               material_id(Material::atmosphere) &&
                           result[index_of(grass_x - 2u, target_y)].material ==
                               material_id(Material::grass) &&
                           result[index_of(cloud_x, target_y)].material ==
                               material_id(Material::atmosphere) &&
                           result[index_of(cloud_x - 2u, target_y)].material ==
                               material_id(Material::cloud) &&
                           result[index_of(bee_x, target_y)].material ==
                               material_id(Material::atmosphere) &&
                           result[index_of(bee_x - 2u, target_y)].material ==
                               material_id(Material::bee),
                       "inventory_unchanged=" +
                           std::to_string(inventory_unchanged ? 1u : 0u) +
                           " water=" +
                           std::to_string(count_material(result, Material::water)) +
                           " grass=" +
                           std::to_string(count_material(result, Material::grass)) +
                           " cloud=" +
                           std::to_string(count_material(result, Material::cloud)) +
                           " bee=" +
                           std::to_string(count_material(result, Material::bee)));
            }
            {
                // Reproduce the packaged failure path that the former static
                // fire-only acceptance missed: a released resource later enters
                // the normal actor pickup volume on a simulated tick.
                auto pickup_actor = actor;
                pickup_actor.gold = 23u;
                pickup_actor.shot_timer = 0u;
                auto cells = acceptance_atmosphere_world();
                const auto pickup_x = static_cast<std::uint32_t>(pickup_actor.x);
                const auto pickup_y = static_cast<std::uint32_t>(pickup_actor.y);
                constexpr std::uint32_t laser_world_only = 0x20000000u;
                auto fragment = make_fill_cell(
                    material_id(Material::gold),
                    static_cast<std::uint32_t>(index_of(pickup_x, pickup_y)));
                fragment.aux &= ~(fill_aux_structural | fill_aux_supported);
                fragment.aux |= laser_world_only | fill_aux_moved;
                fragment.aux = (fragment.aux & ~255u) | 1u;
                cells[index_of(pickup_x, pickup_y)] = fragment;
                upload_scene_cells(cells);
                upload_actor_state(pickup_actor);
                const ActorPush push{
                    .width = config.grid_width,
                    .height = config.grid_height,
                    .step = simulation_step,
                    .seed = random_seed,
                    .scene = static_cast<std::uint32_t>(world_scene),
                    .simulate = 1u,
                    .active_mode = 0u,
                };
                immediate_submit([&](const VkCommandBuffer command_buffer) {
                    bind_compute(command_buffer, actor_pipeline, current_set);
                    vkCmdPushConstants(command_buffer, compute_pipeline_layout,
                                       VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                       sizeof(push), &push);
                    vkCmdDispatch(command_buffer, 1, 1, 1);
                    buffer_barrier(command_buffer, cell_buffers[current_set],
                                   VK_ACCESS_SHADER_WRITE_BIT,
                                   VK_ACCESS_SHADER_READ_BIT |
                                       VK_ACCESS_SHADER_WRITE_BIT,
                                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
                });
                const auto result_actor = download_actor_state();
                const auto result = download_scene_cells();
                const auto& retained = result[index_of(pickup_x, pickup_y)];
                append("laser_fragment_survives_normal_pickup_without_vacuum",
                       result_actor.gold == pickup_actor.gold &&
                           retained.material == material_id(Material::gold) &&
                           (retained.aux & laser_world_only) != 0u &&
                           count_material(result, Material::gold) == 1u &&
                           count_material(result, Material::empty) == 0u,
                       "inventory=" + std::to_string(result_actor.gold) +
                           " retained=" + std::to_string(retained.material) +
                           " world_gold=" +
                           std::to_string(count_material(result, Material::gold)) +
                           " vacuum=" +
                           std::to_string(count_material(result, Material::empty)));
            }
            {
                auto blocked_actor = actor;
                blocked_actor.x = 124;
                blocked_actor.y = 14;
                blocked_actor.gold = 9999u;
                blocked_actor.shot_timer = 0u;
                constexpr std::uint32_t target_x = 124u;
                constexpr std::uint32_t target_y = 0u;
                auto cells = acceptance_atmosphere_world();
                cells[index_of(target_x, target_y)] = make_fill_cell(
                    material_id(Material::gold),
                    static_cast<std::uint32_t>(index_of(target_x, target_y)));
                upload_scene_cells(cells);
                upload_actor_state(blocked_actor);
                fire_acceptance_laser(target_x, target_y);
                auto rearmed = download_actor_state();
                rearmed.shot_timer = 0u;
                upload_actor_state(rearmed);
                fire_acceptance_laser(target_x, target_y);
                const auto result_actor = download_actor_state();
                const auto result = download_scene_cells();
                const auto& retained = result[index_of(target_x, target_y)];
                append("blocked_player_laser_retains_exact_world_unit",
                       result_actor.gold == 9999u &&
                           count_material(result, Material::gold) == 1u &&
                           retained.material == material_id(Material::gold) &&
                           (retained.aux & fill_aux_structural) != 0u &&
                           (retained.aux & 255u) == 111u,
                       "inventory=" + std::to_string(result_actor.gold) +
                           " world_gold=" + std::to_string(
                               count_material(result, Material::gold)) +
                           " retained_health=" +
                           std::to_string(retained.aux & 255u));
            }
            {
                const auto save_root = executable_directory() /
                    "runtime-acceptance-save-owner";
                std::error_code cleanup_error;
                std::filesystem::remove_all(save_root, cleanup_error);

                auto saved_cells = download_scene_cells();
                constexpr std::uint32_t saved_x = 144u;
                constexpr std::uint32_t saved_y = 144u;
                const auto saved_index = index_of(saved_x, saved_y);
                saved_cells[saved_index] = SceneCell{
                    .material = material_id(Material::water),
                    .age = 37u,
                    .temperature = 73,
                    .aux = 0x00800000u |
                        ((material_id(Material::atmosphere) & 0x7fu) << 8u) | 121u,
                };
                auto saved_actor = download_actor_state();
                saved_actor.gold = 17u;
                saved_actor.iron = 23u;
                saved_actor.aluminum = 5u;
                saved_actor.copper = 6u;
                saved_actor.health = 201u;
                saved_actor.oxygen = 187u;
                saved_actor.unlocks = 15u;
                saved_actor.drill_level = 2u;
                const WorldSaveOwners saved_owners{
                    .actor_present = true,
                    .actor = saved_actor,
                };
                const WorldSaveMetadata save_metadata{
                    .world_size = config.world_size,
                    .width = config.grid_width,
                    .height = config.grid_height,
                    .scene = world_scene,
                };
                std::string save_error;
                const bool save_ok = save_world(
                    save_root, save_metadata, "acceptance", saved_cells,
                    saved_owners, save_error);

                std::vector<SceneCell> loaded_cells(saved_cells.size());
                WorldSaveOwners loaded_owners{};
                WorldSaveMetadata loaded_metadata{};
                std::string load_error;
                const bool load_ok = save_ok && load_world(
                    save_root, config.world_size, config.grid_width,
                    config.grid_height, world_scene, "acceptance", loaded_cells,
                    loaded_owners, loaded_metadata, load_error);
                const bool exact_cells = load_ok &&
                    std::memcmp(saved_cells.data(), loaded_cells.data(),
                                saved_cells.size() * sizeof(SceneCell)) == 0;
                const bool exact_actor = load_ok && loaded_owners.actor_present &&
                    std::memcmp(&saved_actor, &loaded_owners.actor,
                                sizeof(saved_actor)) == 0;
                if (load_ok) {
                    upload_scene_cells(loaded_cells);
                    upload_actor_state(loaded_owners.actor);
                }
                const auto gpu_actor = load_ok ? download_actor_state()
                                               : ActorStateReadback{};
                const auto gpu_cells = load_ok ? download_scene_cells()
                                               : std::vector<SceneCell>{};
                const bool gpu_exact = load_ok &&
                    std::memcmp(&saved_actor, &gpu_actor, sizeof(saved_actor)) == 0 &&
                    gpu_cells.size() == loaded_cells.size() &&
                    gpu_cells[saved_index].material == material_id(Material::water) &&
                    gpu_cells[saved_index].age == 37u &&
                    gpu_cells[saved_index].temperature == 73 &&
                    gpu_cells[saved_index].aux == saved_cells[saved_index].aux;
                append("persistent_world_save_preserves_actor_owner_and_half_water",
                       save_ok && load_ok && exact_cells && exact_actor && gpu_exact &&
                           loaded_metadata.format_version == world_save_format_version &&
                           loaded_metadata.owner_payload_bytes ==
                               world_save_actor_bytes + 16u,
                       "save=" + std::to_string(save_ok ? 1u : 0u) +
                           " load=" + std::to_string(load_ok ? 1u : 0u) +
                           " cells=" + std::to_string(exact_cells ? 1u : 0u) +
                           " actor=" + std::to_string(exact_actor ? 1u : 0u) +
                           " gpu=" + std::to_string(gpu_exact ? 1u : 0u) +
                           " save_error=" + save_error +
                           " load_error=" + load_error);
                cleanup_error.clear();
                std::filesystem::remove_all(save_root, cleanup_error);
            }
            check_pre_pr19_hive("sandbox_hard_coded_hive", Scene::sandbox);
            check_pre_pr19_hive("sandbox_hard_coded_hive_delayed", Scene::sandbox, 120u);
            check_pre_pr19_hive("ecosystem_hard_coded_hive", Scene::ecosystem);
            check_pre_pr19_hive("ecosystem_hard_coded_hive_delayed", Scene::ecosystem, 120u);
        }

        const bool passed = std::all_of(checks.begin(), checks.end(),
                                        [](const RuntimeAcceptanceCheck& check) {
                                            return check.passed;
                                        });
        const std::filesystem::path report_path{config.runtime_acceptance_report};
        if (!report_path.parent_path().empty()) {
            std::error_code error;
            std::filesystem::create_directories(report_path.parent_path(), error);
            if (error) {
                throw std::runtime_error("Unable to create runtime acceptance report directory: " +
                                         error.message());
            }
        }
        std::ofstream report{report_path, std::ios::binary | std::ios::trunc};
        if (!report) {
            throw std::runtime_error("Unable to open runtime acceptance report: " +
                                     report_path.string());
        }
        report << "{\n  \"schema\": 1,\n"
               << "  \"backend\": \"vulkan\",\n"
               << "  \"world_width\": " << config.grid_width << ",\n"
               << "  \"world_height\": " << config.grid_height << ",\n"
               << "  \"passed\": " << (passed ? "true" : "false") << ",\n"
               << "  \"checks\": [\n";
        for (std::size_t index = 0u; index < checks.size(); ++index) {
            const auto& check = checks[index];
            report << "    {\"name\": \"" << json_escape(check.name)
                   << "\", \"passed\": " << (check.passed ? "true" : "false")
                   << ", \"details\": \"" << json_escape(check.details) << "\"}"
                   << (index + 1u == checks.size() ? "\n" : ",\n");
        }
        report << "  ]\n}\n";
        if (!report) {
            throw std::runtime_error("Unable to write runtime acceptance report: " +
                                     report_path.string());
        }
        startup_log(std::string{"Packaged Vulkan state acceptance "} +
                    (passed ? "passed: " : "failed: ") + report_path.string());
        return passed ? 0 : 3;
    }
    [[nodiscard]] int run_long_cycle_acceptance() {
        startup_log("Running repeated finite-ledger and save-cycle acceptance...");
        constexpr std::uint32_t requested_cycles = 12u;
        constexpr std::uint32_t fixture_rows = 192u;
        constexpr std::uint32_t water_half_bit = 0x00800000u;
        constexpr std::uint32_t half_medium_temperature_20 = 121u;
        constexpr std::uint32_t weather_cycle_ticks = 7200u;
        constexpr std::uint32_t rain_start_tick = 4800u;
        constexpr std::uint32_t rain_duration_ticks = 600u;
        constexpr std::uint32_t emission_cadence = 360u;
        constexpr std::uint32_t sector_width = 256u;

        const std::filesystem::path report_path{
            config.long_cycle_acceptance_report};
        if (!report_path.parent_path().empty()) {
            std::error_code directory_error;
            std::filesystem::create_directories(report_path.parent_path(),
                                                directory_error);
            if (directory_error) {
                throw std::runtime_error(
                    "Unable to create long-cycle report directory: " +
                    directory_error.message());
            }
        }
        const auto report_parent = report_path.parent_path().empty()
            ? std::filesystem::current_path()
            : report_path.parent_path();
        const auto save_root = report_parent /
            (report_path.stem().string() + "-save-fixture");
        struct CleanupDirectory final {
            std::filesystem::path path;
            ~CleanupDirectory() {
                std::error_code ignored;
                std::filesystem::remove_all(path, ignored);
            }
        } cleanup{save_root};
        std::error_code initial_cleanup_error;
        std::filesystem::remove_all(save_root, initial_cleanup_error);

        struct RestoreStep final {
            std::uint32_t& step;
            std::uint32_t saved;
            ~RestoreStep() { step = saved; }
        } restore_step{simulation_step, simulation_step};

        const auto material_id = [](const Material material) {
            return static_cast<std::uint32_t>(material);
        };
        const auto index_of = [&](const std::uint32_t x,
                                  const std::uint32_t y) {
            return static_cast<std::size_t>(y) * config.grid_width + x;
        };
        const auto fixture_cell_count =
            static_cast<std::size_t>(config.grid_width) * fixture_rows;
        const auto make_atmosphere_fixture = [&]() {
            std::vector<SceneCell> cells(fixture_cell_count);
            for (std::size_t index = 0u; index < cells.size(); ++index) {
                cells[index] = make_fill_cell(
                    material_id(Material::atmosphere),
                    static_cast<std::uint32_t>(index));
            }
            return cells;
        };
        const auto count_material = [&](const std::vector<SceneCell>& cells,
                                        const Material material) {
            return static_cast<std::uint32_t>(std::count_if(
                cells.begin(), cells.end(), [&](const SceneCell& cell) {
                    return cell.material == material_id(material);
                }));
        };
        const auto water_family = [&](const std::vector<SceneCell>& cells) {
            return count_material(cells, Material::water) +
                count_material(cells, Material::dirty_water) +
                count_material(cells, Material::steam) +
                count_material(cells, Material::dirty_steam) +
                count_material(cells, Material::cloud);
        };
        const auto rock_family = [&](const std::vector<SceneCell>& cells) {
            return count_material(cells, Material::stone) +
                count_material(cells, Material::lava);
        };
        const auto water_half_units = [&](const std::vector<SceneCell>& cells) {
            std::pair<std::uint32_t, std::uint32_t> result{};
            for (const auto& cell : cells) {
                if (cell.material != material_id(Material::water)) continue;
                const bool half = (cell.aux & water_half_bit) != 0u;
                result.first += half ? 1u : 2u;
                result.second += half ? 1u : 0u;
            }
            return result;
        };
        const auto scheduled_rain_candidate = [&]()
            -> std::optional<std::pair<std::uint32_t, std::uint32_t>> {
            for (std::uint32_t step = rain_start_tick;
                 step < rain_start_tick + rain_duration_ticks; ++step) {
                const auto rain_tick = step - rain_start_tick;
                for (std::uint32_t x = 2u; x < 190u; ++x) {
                    const auto sector = x / sector_width;
                    const auto sector_phase =
                        (sector * 37u) % emission_cadence;
                    if ((rain_tick % emission_cadence) != sector_phase) continue;
                    const auto event_index = rain_tick / emission_cadence;
                    const auto lane = fill_hash(
                        sector ^ event_index * 0x9e3779b9u ^
                        (step / weather_cycle_ticks) * 0x85ebca6bu) %
                        sector_width;
                    if ((x % sector_width) == lane)
                        return std::pair{x, step};
                }
            }
            return std::nullopt;
        }();

        const WorldSaveActorState actor{
            .x = 32,
            .y = 32,
            .velocity_y = 0,
            .enabled = 1u,
            .gold = 17u,
            .iron = 23u,
            .ammo = 91u,
            .shot_timer = 0u,
            .move_cooldown = 0u,
            .grounded = 1u,
            .health = 201u,
            .oxygen = 187u,
            .hit_x = -1,
            .hit_y = -1,
            .scene = static_cast<std::uint32_t>(world_scene),
            .exposure_ticks = 3u,
            .aluminum = 5u,
            .copper = 6u,
            .unlocks = 15u,
            .drill_level = 2u,
        };
        const WorldSaveOwners owners{.actor_present = true, .actor = actor};
        const WorldSaveMetadata metadata{
            .world_size = config.world_size,
            .width = config.grid_width,
            .height = fixture_rows,
            .scene = world_scene,
        };

        std::vector<RuntimeAcceptanceCheck> cycles;
        std::optional<std::vector<SceneCell>> baseline_cells;
        std::optional<WorldSaveActorState> baseline_actor;
        std::uint32_t completed_cycles = 0u;
        for (std::uint32_t cycle = 0u; cycle < requested_cycles; ++cycle) {
            bool passed = scheduled_rain_candidate.has_value();
            std::string failure;
            const auto fail = [&](const std::string_view reason) {
                if (passed) failure = reason;
                passed = false;
            };

            auto halves = make_atmosphere_fixture();
            for (std::uint32_t x = 88u; x < 96u; ++x)
                halves[index_of(x, 101u)] = make_fill_cell(
                    material_id(Material::stone),
                    static_cast<std::uint32_t>(index_of(x, 101u)));
            const auto half_aux = water_half_bit |
                ((material_id(Material::atmosphere) & 0x7fu) << 8u) |
                half_medium_temperature_20;
            halves[index_of(90u, 100u)] = SceneCell{
                .material = material_id(Material::water), .age = 0u,
                .temperature = 10, .aux = half_aux};
            halves[index_of(91u, 100u)] = SceneCell{
                .material = material_id(Material::water), .age = 0u,
                .temperature = 50, .aux = half_aux};
            halves[index_of(120u, 100u)] = SceneCell{
                .material = material_id(Material::water), .age = 37u,
                .temperature = 73, .aux = half_aux};
            halves[index_of(120u, 101u)] = make_fill_cell(
                material_id(Material::stone),
                static_cast<std::uint32_t>(index_of(120u, 101u)));
            simulation_step = 100u;
            upload_acceptance_cell_prefix(halves);
            run_acceptance_horizontal_pass(0);
            const auto merged = download_scene_cell_prefix(fixture_cell_count);
            const auto [half_units, half_cells] = water_half_units(merged);
            const auto& merged_water = merged[index_of(90u, 100u)];
            const auto& restored_air = merged[index_of(91u, 100u)];
            const auto& stored_half = merged[index_of(120u, 100u)];
            if (half_units != 3u || half_cells != 1u ||
                merged_water.material != material_id(Material::water) ||
                (merged_water.aux & water_half_bit) != 0u ||
                merged_water.temperature != 30 ||
                restored_air.material != material_id(Material::atmosphere) ||
                (restored_air.aux & 0xffu) != 54u ||
                restored_air.temperature != 20 ||
                stored_half.material != material_id(Material::water) ||
                stored_half.temperature != 73 || stored_half.aux != half_aux ||
                count_material(merged, Material::empty) != 0u)
                fail("Half Water/displaced-Atmosphere ledger drift");

            auto boiling = make_atmosphere_fixture();
            auto hot_water = make_fill_cell(
                material_id(Material::water),
                static_cast<std::uint32_t>(index_of(100u, 100u)));
            hot_water.temperature = 125;
            boiling[index_of(100u, 100u)] = hot_water;
            simulation_step = 120u;
            upload_acceptance_cell_prefix(boiling);
            run_acceptance_chemistry_pass();
            const auto boiled = download_scene_cell_prefix(fixture_cell_count);
            const auto carried_steam = boiled[index_of(100u, 100u)];
            if (carried_steam.material != material_id(Material::steam) ||
                carried_steam.temperature <= 110 || water_family(boiled) != 1u ||
                count_material(boiled, Material::empty) != 0u)
                fail("Water-to-Steam ledger drift");

            auto condensing = make_atmosphere_fixture();
            auto cooling_steam = carried_steam;
            cooling_steam.age = 121u;
            cooling_steam.temperature = 60;
            condensing[index_of(100u, 100u)] = cooling_steam;
            auto cloud_neighbor = make_fill_cell(
                material_id(Material::cloud),
                static_cast<std::uint32_t>(index_of(101u, 100u)));
            cloud_neighbor.temperature = 12;
            condensing[index_of(101u, 100u)] = cloud_neighbor;
            simulation_step = 240u;
            upload_acceptance_cell_prefix(condensing);
            run_acceptance_chemistry_pass();
            const auto condensed = download_scene_cell_prefix(fixture_cell_count);
            const auto carried_cloud = condensed[index_of(100u, 100u)];
            if (carried_cloud.material != material_id(Material::cloud) ||
                carried_cloud.temperature != 60 || water_family(condensed) != 2u ||
                count_material(condensed, Material::empty) != 0u)
                fail("Steam-to-Cloud ledger drift");

            auto raining = make_atmosphere_fixture();
            std::uint32_t rain_x = 2u;
            std::uint32_t rain_step = 0u;
            if (scheduled_rain_candidate) {
                rain_x = scheduled_rain_candidate->first;
                rain_step = scheduled_rain_candidate->second;
                for (std::uint32_t y = 79u; y <= 80u; ++y) {
                    for (std::uint32_t x = rain_x - 1u;
                         x <= rain_x + 1u; ++x) {
                        auto cloud = make_fill_cell(
                            material_id(Material::cloud),
                            static_cast<std::uint32_t>(index_of(x, y)));
                        cloud.age = 700u;
                        cloud.temperature = 13;
                        raining[index_of(x, y)] = cloud;
                    }
                }
            }
            simulation_step = rain_step;
            upload_acceptance_cell_prefix(raining);
            run_acceptance_chemistry_pass();
            const auto rained = download_scene_cell_prefix(fixture_cell_count);
            const auto carried_rain = rained[index_of(rain_x, 80u)];
            if (!scheduled_rain_candidate ||
                carried_rain.material != material_id(Material::water) ||
                carried_rain.temperature != 13 || water_family(rained) != 6u ||
                count_material(rained, Material::empty) != 0u)
                fail("Cloud-to-rain ledger drift");

            auto cooling = make_atmosphere_fixture();
            auto isolated_lava = make_fill_cell(
                material_id(Material::lava),
                static_cast<std::uint32_t>(index_of(100u, 100u)));
            isolated_lava.temperature = 900;
            cooling[index_of(100u, 100u)] = isolated_lava;
            cooling[index_of(101u, 100u)] = make_fill_cell(
                material_id(Material::water),
                static_cast<std::uint32_t>(index_of(101u, 100u)));
            simulation_step = 360u;
            upload_acceptance_cell_prefix(cooling);
            run_acceptance_chemistry_pass();
            const auto cooled = download_scene_cell_prefix(fixture_cell_count);
            const auto cooled_stone = cooled[index_of(100u, 100u)];
            if (cooled_stone.material != material_id(Material::stone) ||
                cooled_stone.temperature < 800 || rock_family(cooled) != 1u ||
                water_family(cooled) != 1u ||
                count_material(cooled, Material::empty) != 0u)
                fail("Lava-to-Stone ledger drift");

            auto reheating = make_atmosphere_fixture();
            auto hot_stone = make_fill_cell(
                material_id(Material::stone),
                static_cast<std::uint32_t>(index_of(100u, 100u)));
            hot_stone.temperature = 950;
            reheating[index_of(100u, 100u)] = hot_stone;
            for (const auto& offset : std::array{
                     std::pair{-1, 0}, std::pair{1, 0},
                     std::pair{0, -1}, std::pair{0, 1}}) {
                const auto x = static_cast<std::uint32_t>(100 + offset.first);
                const auto y = static_cast<std::uint32_t>(100 + offset.second);
                reheating[index_of(x, y)] = make_fill_cell(
                    material_id(Material::lava),
                    static_cast<std::uint32_t>(index_of(x, y)));
            }
            simulation_step = 480u;
            upload_acceptance_cell_prefix(reheating);
            run_acceptance_chemistry_pass();
            const auto reheated = download_scene_cell_prefix(fixture_cell_count);
            const auto reheated_lava = reheated[index_of(100u, 100u)];
            if (reheated_lava.material != material_id(Material::lava) ||
                reheated_lava.temperature < 900 || rock_family(reheated) != 5u ||
                count_material(reheated, Material::empty) != 0u)
                fail("Stone-to-Lava ledger drift");

            auto serialized = make_atmosphere_fixture();
            serialized[index_of(10u, 10u)] = merged_water;
            serialized[index_of(11u, 10u)] = restored_air;
            serialized[index_of(12u, 10u)] = stored_half;
            serialized[index_of(13u, 10u)] = carried_steam;
            serialized[index_of(14u, 10u)] = carried_cloud;
            serialized[index_of(15u, 10u)] = carried_rain;
            serialized[index_of(16u, 10u)] = cooled_stone;
            serialized[index_of(17u, 10u)] = cooled[index_of(101u, 100u)];
            serialized[index_of(20u, 10u)] = reheated_lava;
            serialized[index_of(19u, 10u)] = reheated[index_of(99u, 100u)];
            serialized[index_of(21u, 10u)] = reheated[index_of(101u, 100u)];
            serialized[index_of(20u, 9u)] = reheated[index_of(100u, 99u)];
            serialized[index_of(20u, 11u)] = reheated[index_of(100u, 101u)];

            std::string save_error;
            const bool save_ok = save_world(save_root, metadata, "cycle",
                                            serialized, owners, save_error);
            std::vector<SceneCell> loaded(serialized.size());
            WorldSaveOwners loaded_owners{};
            WorldSaveMetadata loaded_metadata{};
            std::string load_error;
            const bool load_ok = save_ok && load_world(
                save_root, config.world_size, config.grid_width, fixture_rows,
                world_scene, "cycle", loaded, loaded_owners, loaded_metadata,
                load_error);
            const bool exact_round_trip = load_ok &&
                std::memcmp(serialized.data(), loaded.data(),
                            serialized.size() * sizeof(SceneCell)) == 0 &&
                loaded_owners.actor_present &&
                std::memcmp(&actor, &loaded_owners.actor, sizeof(actor)) == 0 &&
                loaded_metadata.format_version == world_save_format_version &&
                loaded_metadata.owner_payload_bytes ==
                    world_save_actor_bytes + 16u;
            if (!exact_round_trip) fail("schema-2 save/load byte drift");
            if (exact_round_trip && baseline_cells) {
                if (std::memcmp(baseline_cells->data(), loaded.data(),
                                loaded.size() * sizeof(SceneCell)) != 0 ||
                    !baseline_actor ||
                    std::memcmp(&*baseline_actor, &loaded_owners.actor,
                                sizeof(actor)) != 0)
                    fail("cross-cycle accepted-state byte drift");
            }
            if (exact_round_trip && !baseline_cells) {
                baseline_cells = loaded;
                baseline_actor = loaded_owners.actor;
            }

            const auto details =
                "cycle=" + std::to_string(cycle + 1u) +
                " half_units=" + std::to_string(half_units) +
                " half_cells=" + std::to_string(half_cells) +
                " steam_temp=" + std::to_string(carried_steam.temperature) +
                " cloud_temp=" + std::to_string(carried_cloud.temperature) +
                " rain_temp=" + std::to_string(carried_rain.temperature) +
                " cooled_temp=" + std::to_string(cooled_stone.temperature) +
                " reheated_temp=" + std::to_string(reheated_lava.temperature) +
                " save=" + std::to_string(save_ok ? 1u : 0u) +
                " load=" + std::to_string(load_ok ? 1u : 0u) +
                (failure.empty() ? "" : " failure=" + failure) +
                (save_error.empty() ? "" : " save_error=" + save_error) +
                (load_error.empty() ? "" : " load_error=" + load_error);
            startup_log(std::string{passed ? "PASS " : "FAIL "} +
                        "finite_cycle_" + std::to_string(cycle + 1u) +
                        ": " + details);
            cycles.push_back({"finite_cycle_" + std::to_string(cycle + 1u),
                              passed, details});
            if (!passed) break;
            ++completed_cycles;
        }

        const bool passed = completed_cycles == requested_cycles;
        std::ofstream report{report_path, std::ios::binary | std::ios::trunc};
        if (!report)
            throw std::runtime_error("Unable to open long-cycle report: " +
                                     report_path.string());
        report << "{\n  \"schema\": 1,\n"
               << "  \"backend\": \"vulkan\",\n"
               << "  \"requested_cycles\": " << requested_cycles << ",\n"
               << "  \"completed_cycles\": " << completed_cycles << ",\n"
               << "  \"fixture_width\": " << config.grid_width << ",\n"
               << "  \"fixture_height\": " << fixture_rows << ",\n"
               << "  \"passed\": " << (passed ? "true" : "false") << ",\n"
               << "  \"cycles\": [\n";
        for (std::size_t index = 0u; index < cycles.size(); ++index) {
            const auto& cycle = cycles[index];
            report << "    {\"name\": \"" << json_escape(cycle.name)
                   << "\", \"passed\": "
                   << (cycle.passed ? "true" : "false")
                   << ", \"details\": \"" << json_escape(cycle.details)
                   << "\"}" << (index + 1u == cycles.size() ? "\n" : ",\n");
        }
        report << "  ]\n}\n";
        if (!report)
            throw std::runtime_error("Unable to write long-cycle report: " +
                                     report_path.string());
        startup_log(std::string{"Repeated finite-ledger acceptance "} +
                    (passed ? "passed: " : "failed: ") + report_path.string());
        return passed ? 0 : 4;
    }

    void run(const std::atomic_bool& stop_requested, SharedState& state) {
        startup_log("Entering render loop...");
        if (!config.long_cycle_acceptance_report.empty()) {
            const auto exit_code = run_long_cycle_acceptance();
            state.runtime_acceptance_exit_code.store(exit_code,
                                                     std::memory_order_release);
            state.quit.store(true, std::memory_order_release);
            return;
        }
        if (!config.runtime_acceptance_report.empty()) {
            const auto exit_code = run_runtime_acceptance(state);
            state.runtime_acceptance_exit_code.store(exit_code, std::memory_order_release);
            state.quit.store(true, std::memory_order_release);
            return;
        }
        using Clock = std::chrono::steady_clock;
        const bool interactive_acceptance = !config.interactive_acceptance_report.empty();
        constexpr std::array<std::string_view, 8u> interactive_phase_names{
            "normal_world", "region_debug", "world_totals", "map_overlay",
            "inventory_blueprints", "designer_blueprints", "hive_ecology",
            "nuke_warning_and_edit",
        };
        std::array<std::uint32_t, interactive_phase_names.size()>
            interactive_phase_frame_counts{};
        interactive_phase_frame_counts.fill(240u);
        if (cpu_physical_device) {
            interactive_phase_frame_counts.fill(1u);
            interactive_phase_frame_counts.back() = 8u;
        }
        std::uint32_t interactive_total_frames = 0u;
        for (const auto count : interactive_phase_frame_counts)
            interactive_total_frames += count;
        constexpr std::uint32_t interactive_hardware_warmup_frames = 60u;
        const auto locate_interactive_phase =
            [&interactive_phase_frame_counts](std::uint32_t frame) {
                for (std::uint32_t phase = 0u;
                     phase < interactive_phase_frame_counts.size(); ++phase) {
                    if (frame < interactive_phase_frame_counts[phase])
                        return std::pair{phase, frame};
                    frame -= interactive_phase_frame_counts[phase];
                }
                return std::pair{
                    static_cast<std::uint32_t>(interactive_phase_frame_counts.size() - 1u),
                    interactive_phase_frame_counts.back() - 1u};
            };
        struct InteractiveCapture final {
            std::string phase;
            std::filesystem::path path;
            std::uint32_t width{};
            std::uint32_t height{};
        };
        std::array<std::vector<double>, interactive_phase_names.size()> interactive_samples{};
        std::array<std::vector<double>, 2u> interactive_debug_frame_intervals{};
        std::vector<InteractiveCapture> interactive_captures{};
        std::filesystem::path interactive_capture_directory{};
        std::uint32_t interactive_phase = std::numeric_limits<std::uint32_t>::max();
        std::uint32_t interactive_phase_offset = 0u;
        bool interactive_debug_pair_region = false;
        bool interactive_debug_pair_measurement = false;
        std::uint32_t interactive_presented_frames = 0u;
        std::uint64_t interactive_ticks = 0u;
        const auto interactive_start = Clock::now();
        if (interactive_acceptance) {
            state.presentation_limit.store(1u, std::memory_order_relaxed);
            const std::filesystem::path report_path{config.interactive_acceptance_report};
            interactive_capture_directory = report_path.parent_path() /
                (report_path.stem().string() + "-frames");
        }
        constexpr auto simulation_interval = std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<double>{1.0 / 60.0});
        auto next_frame = Clock::now();
        auto next_simulation = next_frame;
        auto fps_window_start = next_frame;
        std::uint32_t rendered_frames = 0;
        std::uint32_t thirty_fps_divider = 0;
        std::uint32_t active_limit = state.presentation_limit.load(std::memory_order_relaxed) & 3u;

        while (!stop_requested.load(std::memory_order_acquire) &&
               !state.quit.load(std::memory_order_acquire)) {
            if (interactive_acceptance) {
                const auto [phase, phase_offset] =
                    locate_interactive_phase(interactive_presented_frames);
                interactive_phase_offset = phase_offset;
                if (phase != interactive_phase) {
                    interactive_phase = phase;
                    state.debug_visualization.store(false, std::memory_order_relaxed);
                    state.debug_page.store(0u, std::memory_order_relaxed);
                    state.map_view.store(false, std::memory_order_relaxed);
                    state.selected_workspace.store(1u, std::memory_order_relaxed);
                    if (phase == 1u) {
                        state.debug_page.store(0u, std::memory_order_relaxed);
                        state.debug_visualization.store(true, std::memory_order_release);
                    } else if (phase == 2u) {
                        state.debug_page.store(1u, std::memory_order_relaxed);
                        state.debug_visualization.store(true, std::memory_order_release);
                    } else if (phase == 3u) {
                        state.map_view.store(true, std::memory_order_release);
                    } else if (phase == 4u) {
                        state.selected_workspace.store(0u, std::memory_order_relaxed);
                        state.inventory_pane.store(1u, std::memory_order_relaxed);
                    } else if (phase == 5u) {
                        state.selected_workspace.store(3u, std::memory_order_relaxed);
                        state.designer_pane.store(1u, std::memory_order_relaxed);
                    } else if (phase == 6u) {
                        const auto ecosystem_district =
                            persistent_world_district_index(Scene::ecosystem);
                        state.camera_center_x.store(
                            static_cast<int>(persistent_world_district_origin_x(
                                config.grid_width, ecosystem_district) + 512u),
                            std::memory_order_relaxed);
                        state.camera_center_y.store(
                            static_cast<int>(persistent_world_district_origin_y(
                                config.grid_height, ecosystem_district) + 232u),
                            std::memory_order_relaxed);
                        state.camera_zoom.store(camera_zoom_max,
                                                std::memory_order_relaxed);
                    } else if (phase == 7u) {
                        state.camera_zoom.store(camera_zoom_default, std::memory_order_relaxed);
                        state.camera_center_x.store(
                            static_cast<int>(config.grid_width / 2u),
                            std::memory_order_relaxed);
                        state.camera_center_y.store(360, std::memory_order_relaxed);
                        state.ignite_air.store(true, std::memory_order_release);
                    }
                }
                interactive_debug_pair_measurement = false;
                if (!cpu_physical_device && interactive_phase < 2u) {
                    const auto pair_frame =
                        interactive_phase * interactive_phase_frame_counts[0] +
                        interactive_phase_offset;
                    const bool capture_frame =
                        interactive_phase_offset + 1u ==
                            interactive_phase_frame_counts[interactive_phase];
                    if (capture_frame) {
                        interactive_debug_pair_region = interactive_phase == 1u;
                        state.paused.store(false, std::memory_order_relaxed);
                    } else {
                        const auto pair_index = pair_frame / 2u;
                        const bool second_in_pair = (pair_frame & 1u) != 0u;
                        interactive_debug_pair_region =
                            (pair_index & 1u) == 0u
                                ? second_in_pair
                                : !second_in_pair;
                        interactive_debug_pair_measurement =
                            pair_frame >= interactive_hardware_warmup_frames &&
                            interactive_phase_offset + 2u <
                                interactive_phase_frame_counts[interactive_phase];
                        // Freeze canonical simulation state while alternating
                        // N/R and R/N pair order. REGION still forces its
                        // observational stats dispatch in draw_frame().
                        state.paused.store(true, std::memory_order_relaxed);
                    }
                    state.debug_page.store(0u, std::memory_order_relaxed);
                    state.debug_visualization.store(
                        interactive_debug_pair_region,
                        std::memory_order_release);
                }
            }
            const auto width = state.window_width.load(std::memory_order_relaxed);
            const auto height = state.window_height.load(std::memory_order_relaxed);
            if (width == 0 || height == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds{16});
                next_frame = Clock::now();
                next_simulation = next_frame;
                fps_window_start = next_frame;
                rendered_frames = 0;
                continue;
            }

            if (state.resized.exchange(false, std::memory_order_acq_rel)) {
                recreate_swapchain(width, height);
            }

            const auto selected_limit =
                state.presentation_limit.load(std::memory_order_relaxed) & 3u;
            if (selected_limit != active_limit) {
                active_limit = selected_limit;
                next_frame = Clock::now();
                thirty_fps_divider = 0u;
            }
            const std::uint32_t internal_hz =
                active_limit <= 1u ? 60u : (active_limit == 2u ? 120u : 0u);
            const auto frame_interval = internal_hz == 0u
                ? Clock::duration::zero()
                : std::chrono::duration_cast<Clock::duration>(
                    std::chrono::duration<double>{
                        1.0 / static_cast<double>(internal_hz)});

            const auto before_draw = Clock::now();
            const bool simulation_due = before_draw >= next_simulation;
            const std::uint32_t simulation_ticks = simulation_due ? 1u : 0u;
            if (simulation_due) {
                next_simulation += simulation_interval;
                constexpr std::uint32_t max_time_debt_ticks = 2u;
                if (before_draw > next_simulation + simulation_interval * max_time_debt_ticks) {
                    // Discard stale wall-clock debt instead of creating a GPU catch-up spiral.
                    next_simulation = before_draw + simulation_interval;
                }
            }

            // Always present requested frames. Fixed ticks still submit at most
            // once and stale debt is shed, but a late tick no longer creates a
            // visible skip/jitter cadence.
            const bool present_requested = active_limit != 0u ||
                ((thirty_fps_divider++ & 1u) == 0u);
            const bool present_frame = present_requested;
            std::optional<InteractiveCapture> scheduled_capture{};
            if (interactive_acceptance && present_frame &&
                interactive_phase < interactive_phase_names.size()) {
                const bool final_phase_frame =
                    interactive_phase_offset + 1u ==
                        interactive_phase_frame_counts[interactive_phase];
                const auto nuke_warning_presentations = 6u *
                    (cpu_physical_device ? 1u : 8u);
                const bool final_bright_nuke_warning =
                    interactive_phase + 1u == interactive_phase_names.size() &&
                    interactive_phase_offset + 1u == nuke_warning_presentations;
                if (final_phase_frame || final_bright_nuke_warning) {
                    const std::string capture_name = final_bright_nuke_warning
                        ? "nuke_warning_bright"
                        : std::string{interactive_phase_names[interactive_phase]};
                    scheduled_capture = InteractiveCapture{
                        .phase = capture_name,
                        .path = interactive_capture_directory / (capture_name + ".bmp"),
                        .width = swapchain_extent.width,
                        .height = swapchain_extent.height,
                    };
                    pending_frame_capture = scheduled_capture->path;
                }
            }
            const bool frame_presented = draw_frame(
                state, simulation_ticks, present_frame,
                interactive_debug_pair_measurement &&
                    interactive_debug_pair_region);
            if (!frame_presented) {
                recreate_swapchain(width, height);
            } else {
                interactive_ticks += simulation_ticks;
                if (present_frame) {
                    ++rendered_frames;
                    ++interactive_presented_frames;
                    if (scheduled_capture.has_value())
                        interactive_captures.push_back(std::move(*scheduled_capture));
                }
            }
            const auto after_draw = Clock::now();
            if (interactive_acceptance && present_frame && frame_presented &&
                interactive_phase < interactive_samples.size()) {
                const double draw_ms =
                    std::chrono::duration<double, std::milli>(
                        after_draw - before_draw).count();
                if (interactive_debug_pair_measurement) {
                    interactive_samples[
                        interactive_debug_pair_region ? 1u : 0u].push_back(draw_ms);
                } else if (cpu_physical_device ||
                           (interactive_phase >= 2u &&
                            !scheduled_capture.has_value() &&
                            interactive_phase_offset >
                                interactive_hardware_warmup_frames)) {
                    interactive_samples[interactive_phase].push_back(draw_ms);
                }
            }
#if SANDHYBRID_ENABLE_VALIDATION
            log_conservation_if_due(state);
#endif

            const auto now = Clock::now();
            const auto elapsed = now - fps_window_start;
            if (elapsed >= std::chrono::milliseconds{500}) {
                const auto seconds = std::chrono::duration<double>(elapsed).count();
                const auto fps = seconds > 0.0
                    ? static_cast<std::uint32_t>(
                        static_cast<double>(rendered_frames) / seconds + 0.5)
                    : 0u;
                state.frames_per_second.store(fps, std::memory_order_relaxed);
                fps_window_start = now;
                rendered_frames = 0;
            }

            if (internal_hz != 0u) {
                next_frame += frame_interval;
                const auto frame_end = Clock::now();
                if (frame_end < next_frame) {
                    std::this_thread::sleep_until(next_frame);
                } else if (frame_end - next_frame > frame_interval * 4) {
                    next_frame = frame_end;
                }
            } else {
                next_frame = Clock::now();
                std::this_thread::yield();
            }
            if (interactive_debug_pair_measurement && frame_presented) {
                interactive_debug_frame_intervals[
                    interactive_debug_pair_region ? 1u : 0u].push_back(
                        std::chrono::duration<double, std::milli>(
                            Clock::now() - before_draw).count());
            }

            if (interactive_acceptance &&
                interactive_presented_frames >= interactive_total_frames) {
                const auto elapsed_seconds = std::chrono::duration<double>(
                    Clock::now() - interactive_start).count();
                bool passed = !gpu_stalled &&
                    interactive_ticks <= interactive_presented_frames;
                std::array<double, interactive_phase_names.size()> means{};
                std::array<double, interactive_phase_names.size()> p95s{};
                std::array<double, interactive_phase_names.size()> maxima{};
                std::array<double, 2u> debug_interval_means{};
                std::array<double, 2u> debug_interval_p95s{};
                std::array<double, 2u> debug_interval_maxima{};
                for (std::size_t phase = 0u; phase < interactive_samples.size(); ++phase) {
                    auto& samples = interactive_samples[phase];
                    if (samples.empty()) {
                        passed = false;
                        continue;
                    }
                    std::ranges::sort(samples);
                    double total = 0.0;
                    for (const double value : samples) total += value;
                    means[phase] = total / static_cast<double>(samples.size());
                    const auto p95_index = (std::min)(
                        samples.size() - 1u, (samples.size() * 95u) / 100u);
                    p95s[phase] = samples[p95_index];
                    maxima[phase] = samples.back();
                    if (!cpu_physical_device)
                        passed = passed && p95s[phase] <= 33.34;
                }
                const double debug_mean_delta = means[1] - means[0];
                const double debug_mean_overhead = means[0] > 0.0
                    ? (debug_mean_delta / means[0]) * 100.0 : 100.0;
                const double debug_p95_delta = p95s[1] - p95s[0];
                const double debug_p95_overhead = p95s[0] > 0.0
                    ? (debug_p95_delta / p95s[0]) * 100.0 : 100.0;
                constexpr double ten_fps_tail_loss_ms =
                    (1000.0 / 50.0) - (1000.0 / 60.0);
                constexpr double fifty_fps_interval_ms = 1000.0 / 50.0;
                if (!cpu_physical_device) {
                    passed = passed &&
                        interactive_samples[0].size() ==
                            interactive_samples[1].size() &&
                        interactive_debug_frame_intervals[0].size() ==
                            interactive_debug_frame_intervals[1].size();
                    for (std::size_t bucket = 0u;
                         bucket < interactive_debug_frame_intervals.size();
                         ++bucket) {
                        auto& samples = interactive_debug_frame_intervals[bucket];
                        if (samples.empty()) {
                            passed = false;
                            continue;
                        }
                        std::ranges::sort(samples);
                        double total = 0.0;
                        for (const double value : samples) total += value;
                        debug_interval_means[bucket] =
                            total / static_cast<double>(samples.size());
                        const auto p95_index = (std::min)(
                            samples.size() - 1u,
                            (samples.size() * 95u) / 100u);
                        debug_interval_p95s[bucket] = samples[p95_index];
                        debug_interval_maxima[bucket] = samples.back();
                    }
                    const double debug_present_p95_delta =
                        debug_interval_p95s[1] - debug_interval_p95s[0];
                    // Absolute paired tail/cadence loss owns the gate.
                    // Relative percent remains diagnostic only because
                    // sub-millisecond baselines amplify scheduler noise.
                    passed = passed &&
                        debug_p95_delta <= ten_fps_tail_loss_ms &&
                        debug_present_p95_delta <= ten_fps_tail_loss_ms &&
                        debug_interval_p95s[0] <= fifty_fps_interval_ms &&
                        debug_interval_p95s[1] <= fifty_fps_interval_ms;
                }
                passed = passed && interactive_captures.size() ==
                    interactive_phase_names.size() + 1u;
                for (const auto& capture : interactive_captures)
                    passed = passed && std::filesystem::is_regular_file(capture.path) &&
                        std::filesystem::file_size(capture.path) > 54u;

                const std::filesystem::path report_path{config.interactive_acceptance_report};
                if (!report_path.parent_path().empty())
                    std::filesystem::create_directories(report_path.parent_path());
                std::ofstream report{report_path, std::ios::binary | std::ios::trunc};
                if (!report)
                    throw std::runtime_error("Unable to create interactive acceptance report: " +
                                             report_path.string());
                report << "{\n"
                       << "  \"schema\": 3,\n"
                       << "  \"backend\": \"vulkan-presented\",\n"
                       << "  \"device_class\": \""
                       << (cpu_physical_device ? "cpu-software" : "hardware") << "\",\n"
                       << "  \"performance_gate\": "
                       << (cpu_physical_device ? "false" : "true") << ",\n"
                       << "  \"phase_frame_counts\": [";
                for (std::size_t phase = 0u;
                     phase < interactive_phase_frame_counts.size(); ++phase) {
                    report << interactive_phase_frame_counts[phase]
                           << (phase + 1u == interactive_phase_frame_counts.size()
                               ? "],\n" : ", ");
                }
                report
                       << "  \"passed\": " << (passed ? "true" : "false") << ",\n"
                       << "  \"presented_frames\": " << interactive_presented_frames << ",\n"
                       << "  \"simulation_ticks\": " << interactive_ticks << ",\n"
                       << "  \"elapsed_seconds\": " << elapsed_seconds << ",\n"
                       << "  \"debug_overhead_percent\": " << debug_p95_overhead << ",\n"
                       << "  \"debug_mean_overhead_percent\": "
                       << debug_mean_overhead << ",\n"
                       << "  \"debug_p95_overhead_percent\": "
                       << debug_p95_overhead << ",\n"
                       << "  \"debug_mean_delta_ms\": " << debug_mean_delta << ",\n"
                       << "  \"debug_p95_delta_ms\": " << debug_p95_delta << ",\n"
                       << "  \"debug_pairing\": "
                          "\"interleaved-frozen-state-balanced-order\",\n"
                       << "  \"debug_present_p95_delta_ms\": "
                       << (debug_interval_p95s[1] - debug_interval_p95s[0])
                       << ",\n"
                       << "  \"captures\": [\n";
                for (std::size_t index = 0u; index < interactive_captures.size(); ++index) {
                    const auto& capture = interactive_captures[index];
                    report << "    {\"phase\": \"" << json_escape(capture.phase)
                           << "\", \"file\": \""
                           << json_escape(capture.path.generic_string())
                           << "\", \"width\": " << capture.width
                           << ", \"height\": " << capture.height << "}"
                           << (index + 1u == interactive_captures.size() ? "\n" : ",\n");
                }
                report << "  ],\n"
                       << "  \"debug_present_intervals\": [\n";
                constexpr std::array<std::string_view, 2u> debug_interval_names{
                    "normal_world", "region_debug"};
                for (std::size_t bucket = 0u;
                     bucket < debug_interval_names.size(); ++bucket) {
                    report << "    {\"name\": \"" << debug_interval_names[bucket]
                           << "\", \"samples\": "
                           << interactive_debug_frame_intervals[bucket].size()
                           << ", \"mean_ms\": " << debug_interval_means[bucket]
                           << ", \"p95_ms\": " << debug_interval_p95s[bucket]
                           << ", \"max_ms\": " << debug_interval_maxima[bucket] << "}"
                           << (bucket + 1u == debug_interval_names.size()
                               ? "\n" : ",\n");
                }
                report << "  ],\n"
                       << "  \"phases\": [\n";
                for (std::size_t phase = 0u; phase < interactive_phase_names.size(); ++phase) {
                    report << "    {\"name\": \"" << interactive_phase_names[phase]
                           << "\", \"samples\": " << interactive_samples[phase].size()
                           << ", \"mean_ms\": " << means[phase]
                           << ", \"p95_ms\": " << p95s[phase]
                           << ", \"max_ms\": " << maxima[phase] << "}"
                           << (phase + 1u == interactive_phase_names.size() ? "\n" : ",\n");
                }
                report << "  ]\n}\n";
                if (!report)
                    throw std::runtime_error("Unable to write interactive acceptance report: " +
                                             report_path.string());
                state.runtime_acceptance_exit_code.store(passed ? 0 : 4,
                                                         std::memory_order_release);
                state.quit.store(true, std::memory_order_release);
            }
        }
        startup_log("Render loop stopped.");
        if (!gpu_stalled) {
            check_vk(vkDeviceWaitIdle(device), "vkDeviceWaitIdle(shutdown)");
        }
    }
};

VulkanRenderer::VulkanRenderer(const NativeWindow& window,
                     const SimulationConfig config,
                     std::string save_slot)
    : impl_(std::make_unique<Impl>(window, config, std::move(save_slot))) {}

VulkanRenderer::~VulkanRenderer() = default;

void VulkanRenderer::run(const std::atomic_bool& stop_requested, SharedState& shared_state) {
    impl_->run(stop_requested, shared_state);
}

} // namespace sandhybrid
