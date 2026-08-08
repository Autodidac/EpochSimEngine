#include "sandhybrid/vulkan_renderer.hpp"

#include "sandhybrid/actor_medium.hpp"
#include "sandhybrid/material.hpp"
#include "sandhybrid/scene.hpp"
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
        .aux = fill_hash(index ^ material_id * 0x9e3779b9u) & fill_aux_random_mask,
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
    std::uint32_t steps_per_frame{};
    std::uint32_t selected_group{};
    std::uint32_t hovered_group{};
    std::uint32_t hovered_material{};
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
    VkPipeline chemistry_pipeline{};
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
    Buffer ui_text_buffer{};
    Buffer designer_buffer{};
    Buffer scene_staging_buffer{};
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
    bool map_was_visible{};
    std::optional<std::uint32_t> pending_scene_export{};
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
            if (chemistry_pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, chemistry_pipeline, nullptr);
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
            .descriptorCount = 20,
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
        chemistry_pipeline = create_compute_pipeline("chemistry.comp.spv");
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
        VkSwapchainCreateInfoKHR create_info{
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = surface,
            .minImageCount = image_count,
            .imageFormat = surface_format.format,
            .imageColorSpace = surface_format.colorSpace,
            .imageExtent = extent,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
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

    [[nodiscard]] std::vector<SceneCell> download_scene_cells() {
        std::vector<SceneCell> cells(
            static_cast<std::size_t>(config.grid_width) * config.grid_height);
        immediate_submit([&](const VkCommandBuffer command_buffer) {
            buffer_barrier(command_buffer, cell_buffers[current_set],
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_TRANSFER_READ_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT);
            const VkBufferCopy copy{.size = scene_staging_buffer.size};
            vkCmdCopyBuffer(command_buffer, cell_buffers[current_set].handle,
                            scene_staging_buffer.handle, 1, &copy);
            buffer_barrier(command_buffer, scene_staging_buffer,
                           VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT);
        });
        void* mapped = nullptr;
        check_vk(vkMapMemory(device, scene_staging_buffer.memory, 0,
                             scene_staging_buffer.size, 0, &mapped),
                 "vkMapMemory(fill readback)");
        std::memcpy(cells.data(), mapped, cells.size() * sizeof(SceneCell));
        vkUnmapMemory(device, scene_staging_buffer.memory);
        return cells;
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
    std::size_t flood_replace_connected(std::vector<SceneCell>& cells,
                                        const std::uint32_t start,
                                        const std::uint32_t target,
                                        const std::uint32_t replacement) {
        if (static_cast<std::size_t>(start) >= cells.size() || target == replacement ||
            cells[start].material != target)
            return 0u;
        std::vector<std::uint8_t> visited(cells.size(), 0u);
        std::vector<std::uint32_t> queue(cells.size());
        std::size_t head = 0u;
        std::size_t tail = 0u;
        visited[start] = 1u;
        queue[tail++] = start;
        while (head < tail) {
            const auto index = queue[head++];
            const auto x = index % config.grid_width;
            const auto y = index / config.grid_width;
            const auto enqueue = [&](const std::uint32_t candidate) {
                if (visited[candidate] == 0u && cells[candidate].material == target) {
                    visited[candidate] = 1u;
                    queue[tail++] = candidate;
                }
            };
            if (x > 0u) enqueue(index - 1u);
            if (x + 1u < config.grid_width) enqueue(index + 1u);
            if (y > 0u) enqueue(index - config.grid_width);
            if (y + 1u < config.grid_height) enqueue(index + config.grid_width);
        }
        for (std::size_t index = 0u; index < visited.size(); ++index) {
            if (visited[index] != 0u)
                cells[index] = make_fill_cell(replacement, static_cast<std::uint32_t>(index));
        }
        return tail;
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
        const auto changed = flood_replace_connected(cells, start, target, replacement);
        if (changed == 0u) {
            startup_log("Air fill found no different connected cells.");
            return;
        }
        upload_scene_cells(cells);
        startup_log("Filled connected region with Air: " + std::to_string(changed) + " cells.");
    }

    void ignite_air_region() {
        auto cells = download_scene_cells();
        const auto air = static_cast<std::uint32_t>(Material::atmosphere);
        const auto fire = static_cast<std::uint32_t>(Material::fire);
        const auto iterator = std::find_if(cells.begin(), cells.end(), [air](const SceneCell& cell) {
            return cell.material == air;
        });
        if (iterator == cells.end()) {
            startup_log("Ignite Air found no Air cell.");
            return;
        }
        const auto start = static_cast<std::uint32_t>(std::distance(cells.begin(), iterator));
        const auto changed = flood_replace_connected(cells, start, air, fire);
        if (changed == 0u) {
            startup_log("Ignite Air found no connected Air region.");
            return;
        }
        upload_scene_cells(cells);
        startup_log("Ignited upper-left connected Air region: " +
                    std::to_string(changed) + " cells.");
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
            if (config.runtime_acceptance_report.empty()) {
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

    void upload_scene_cells(const std::span<const SceneCell> cells) {
        if (cells.size_bytes() != scene_staging_buffer.size)
            throw std::runtime_error("Scene image produced an unexpected cell count.");
        void* mapped = nullptr;
        check_vk(vkMapMemory(device, scene_staging_buffer.memory, 0,
                             scene_staging_buffer.size, 0, &mapped),
                 "vkMapMemory(scene upload)");
        std::memcpy(mapped, cells.data(), cells.size_bytes());
        vkUnmapMemory(device, scene_staging_buffer.memory);

        immediate_submit([&](const VkCommandBuffer command_buffer) {
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
        map_was_visible = false;
        needs_reset = false;
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
                    config.grid_width, config.grid_height, x, y);
                if (material == Material::empty) continue;
                const auto index = static_cast<std::size_t>(y) * config.grid_width + x;
                const bool inside_authored = x >= origin_x && x < origin_x + map_width &&
                                             y >= origin_y && y < origin_y + map_height;
                const auto existing = static_cast<Material>(world_cells[index].material);
                if (inside_authored && existing != Material::empty &&
                    existing != Material::atmosphere)
                    continue;
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
        WorldSaveMetadata metadata{};
        std::string error;
        if (!load_world(executable_directory(), config.world_size,
              config.grid_width, config.grid_height, scene,
              save_slot, cells, metadata, error)) {
  startup_log("World load skipped: " + error);
  return false;
        }
        if (scene == Scene::ecosystem || scene == Scene::sandbox) {
            std::vector<std::uint32_t> materials(static_cast<std::size_t>(cells.size()));
            for (std::size_t index = 0u; index < cells.size(); ++index)
                materials[index] = cells[index].material;
            normalize_pre_pr19_hives(materials, config.grid_width, config.grid_height, authored_map_origin_x(), authored_map_origin_y(), scene);
            for (std::size_t index = 0u; index < cells.size(); ++index) {
                cells[index].material = materials[index];
            }
        }
        upload_scene_cells(cells);
        if (!error.empty()) startup_log("World load recovery: " + error);
        startup_log("Loaded exact world save: " +
          world_save_path(executable_directory(), config.world_size,
                          scene, save_slot).string());
        return true;
    }

    void save_world_slot(const std::uint32_t scene_index) {
        const auto scene = static_cast<Scene>(scene_index % scene_count);
        const auto cells = download_scene_cells();
        const WorldSaveMetadata metadata{
  .world_size = config.world_size,
  .width = config.grid_width,
  .height = config.grid_height,
  .scene = scene,
        };
        std::string error;
        if (!save_world(executable_directory(), metadata, save_slot, cells, error))
  throw std::runtime_error("Unable to save world: " + error);
        startup_log("Saved exact world state: " +
          world_save_path(executable_directory(), config.world_size,
                          scene, save_slot).string());
    }

    void record_reset(const VkCommandBuffer command_buffer, const std::uint32_t scene_index) {
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

    void record_paint(const VkCommandBuffer command_buffer, const SharedState& state) {
        const bool erase = state.secondary_down.load(std::memory_order_relaxed);
        const bool paint = state.primary_down.load(std::memory_order_relaxed);
        if (!erase && !paint) return;

        const auto [grid_x, grid_y] = grid_cursor(state);
        const auto requested_radius = state.brush_radius.load(std::memory_order_relaxed);
        const auto material = erase ? static_cast<std::uint32_t>(Material::oxygen)
                                    : state.selected_material.load(std::memory_order_relaxed);
        const bool tile_mode = state.placement_mode.load(std::memory_order_relaxed) != 0u;
        const auto radius = policy::effective_world_brush_radius(
            material == static_cast<std::uint32_t>(Material::beehive),
            tile_mode, requested_radius);
        const auto shape = policy::effective_world_brush_shape(
            tile_mode, state.brush_shape.load(std::memory_order_relaxed));
        const auto packed_material = material | (shape << 16u) | (tile_mode ? (1u << 18u) : 0u);
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
            .reserved = 1u,
        };
        bind_compute(command_buffer, debug_stats_pipeline, current_set);
        vkCmdPushConstants(command_buffer, compute_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(push), &push);
        const auto cell_count = config.grid_width * config.grid_height;
        vkCmdDispatch(command_buffer, divide_round_up(cell_count, debug_stats_local_size), 1, 1);
        buffer_barrier(command_buffer, conservation_buffer, VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    }

    void record_simulation_step(const VkCommandBuffer command_buffer,
                                const SharedState& state,
                                const bool collect_debug_stats) {
        const auto active_section_x = state.active_window_origin_x.load(std::memory_order_relaxed);
        const auto active_section_y = state.active_window_origin_y.load(std::memory_order_relaxed);
        SimulationPush simulation_push{
            .width = config.grid_width,
            .height = config.grid_height,
            .step = simulation_step,
            .seed = random_seed,
            .active_section_x = active_section_x,
            .active_section_y = active_section_y,
            .active_mode = 1u,
        };

        if ((simulation_step & 3u) == 0u) {
            buffer_barrier(command_buffer, sunlight_buffer, VK_ACCESS_SHADER_READ_BIT,
                           VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            bind_compute(command_buffer, sunlight_pipeline, current_set);
            vkCmdPushConstants(command_buffer, compute_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                               0, sizeof(simulation_push), &simulation_push);
            vkCmdDispatch(command_buffer, divide_round_up(config.grid_width, sunlight_local_size), 1, 1);
            buffer_barrier(command_buffer, sunlight_buffer, VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        }

        bind_compute(command_buffer, tile_pipeline, current_set);
        vkCmdPushConstants(command_buffer, compute_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(simulation_push), &simulation_push);
        vkCmdDispatch(command_buffer, divide_round_up(divide_round_up(config.grid_width, 8u), 8u),
                      divide_round_up(divide_round_up(config.grid_height, 8u), 8u), 1);
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
        vkCmdDispatch(command_buffer, divide_round_up(config.grid_width, simulation_local_size),
                      divide_round_up(config.grid_height, simulation_local_size), 1);
        buffer_barrier(command_buffer, cell_buffers[next_set], VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        current_set = next_set;

        // Full uniform 8x8 regions use the same fall/diagonal/spread decisions
        // as cells, but transfer all 64 canonical cells in parallel. Mixed,
        // partial, structural, reacting, or half-water regions fall through to
        // the ordinary fine-grained movement passes below.
        bind_compute(command_buffer, macro_movement_pipeline, current_set);
        const std::array<std::int32_t, 6> macro_phases = (simulation_step & 1u) == 0u
  ? std::array<std::int32_t, 6>{0, 5, 1, 2, 3, 4}
  : std::array<std::int32_t, 6>{0, 5, 2, 1, 4, 3};
        const auto tile_columns = divide_round_up(config.grid_width, 8u);
        const auto tile_rows = divide_round_up(config.grid_height, 8u);
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
      .reserved0 = collect_debug_stats ? 1u : 0u,
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

        // Freeze the post-chemistry and macro-movement state for all neighborhood decisions in
        // the movement passes. Pair endpoints still use the writable current
        // buffer, while pressure, support, and bee attraction read this exact
        // immutable snapshot, eliminating cross-invocation read/write races.
        const auto snapshot_set = current_set ^ 1u;
        buffer_barrier(command_buffer, cell_buffers[current_set], VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT);
        buffer_barrier(command_buffer, cell_buffers[snapshot_set],
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT);
        const VkBufferCopy snapshot_copy{.srcOffset = 0, .dstOffset = 0, .size = cell_buffers[current_set].size};
        vkCmdCopyBuffer(command_buffer, cell_buffers[current_set].handle,
                        cell_buffers[snapshot_set].handle, 1, &snapshot_copy);
        buffer_barrier(command_buffer, cell_buffers[current_set], VK_ACCESS_TRANSFER_READ_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        buffer_barrier(command_buffer, cell_buffers[snapshot_set], VK_ACCESS_TRANSFER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        bind_compute(command_buffer, movement_pipeline, current_set);
        const std::array<std::int32_t, 15> phases = (simulation_step & 1u) == 0u
  ? std::array<std::int32_t, 15>{0, 1, 2, 3, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5}
  : std::array<std::int32_t, 15>{0, 2, 1, 4, 3, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5};
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
                .reserved0 = collect_debug_stats ? 1u : 0u,
                .reserved1 = (phase_index >= 11u ? 1u : 0u) |
                             (((simulation_step & 3u) == 0u) ? 2u : 0u),
                .active_section_x = active_section_x,
                .active_section_y = active_section_y,
                .active_mode = 1u,
                .worker_count = state.section_worker_count.load(std::memory_order_relaxed),
            };
            vkCmdPushConstants(command_buffer, compute_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                               0, sizeof(movement_push), &movement_push);
            if (phase >= 5) {
                vkCmdDispatch(command_buffer,
                              divide_round_up(divide_round_up(config.grid_width, 2u), simulation_local_size),
                              divide_round_up(config.grid_height, simulation_local_size), 1);
            } else {
                vkCmdDispatch(command_buffer, divide_round_up(config.grid_width, simulation_local_size),
                              divide_round_up(divide_round_up(config.grid_height, 2u), simulation_local_size), 1);
            }
            buffer_barrier(command_buffer, cell_buffers[current_set], VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            buffer_barrier(command_buffer, chunk_buffer, VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        }
        ++simulation_step;
    }

    void record_actor(const VkCommandBuffer command_buffer, SharedState& state,
                      const bool reset_actor, const bool simulate_actor) {
        const auto [aim_x, aim_y] = grid_cursor(state);
        const bool fire = state.fire_tool.load(std::memory_order_relaxed) ||
                          state.fire_tool_pressed.exchange(false, std::memory_order_acq_rel);
        const bool deposit = state.deposit_resource.load(std::memory_order_relaxed) ||
                             state.deposit_resource_pressed.exchange(false, std::memory_order_acq_rel);
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
        const VkBufferCopy copy{.size = map_snapshot_buffer.size};
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
        const auto designer_flags =
            (state.designer_placement_mode.load(std::memory_order_relaxed) & 1u) |
            ((state.designer_brush_shape.load(std::memory_order_relaxed) & 3u) << 1u) |
            ((state.designer_mode.load(std::memory_order_relaxed) & 1u) << 3u) |
            ((state.designer_pane.load(std::memory_order_relaxed) & 1u) << 4u) |
            ((state.inventory_pane.load(std::memory_order_relaxed) & 1u) << 5u) |
            ((state.designer_zoom.load(std::memory_order_relaxed) & 0xffu) << 8u) |
            ((state.designer_brush_radius.load(std::memory_order_relaxed) & 0xffu) << 16u);
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
        const RenderPush push{
            .grid_width = config.grid_width,
            .grid_height = config.grid_height,
            .window_width = logical_width,
            .window_height = logical_height,
            .selected_material = active_selected_material,
            .material_count = material_count,
            .cursor_x = pointer_over_world ? cursor_x : -1'000'000,
            .cursor_y = pointer_over_world ? cursor_y : -1'000'000,
            .brush_radius = [&state, designer_workspace, active_selected_material]() {
                if (designer_workspace)
                    return state.designer_brush_radius.load(std::memory_order_relaxed);
                return policy::effective_world_brush_radius(
                    active_selected_material == static_cast<std::uint32_t>(Material::beehive),
                    state.placement_mode.load(std::memory_order_relaxed) != 0u,
                    state.brush_radius.load(std::memory_order_relaxed));
            }(),
            .status_height = static_cast<std::uint32_t>(layout.status.size.y),
            // Existing push slot carries compact sidebar width.
            .palette_height = static_cast<std::uint32_t>(layout.status.size.x),
            .group_tabs_height = static_cast<std::uint32_t>(layout.group_tabs.size.y),
            .material_slots = material_slots_per_group,
            .frames_per_second = state.frames_per_second.load(std::memory_order_relaxed),
            .paused = state.paused.load(std::memory_order_relaxed) ? 1u : 0u,
            .steps_per_frame = state.steps_per_frame.load(std::memory_order_relaxed),
            .selected_group = active_selected_group % material_group_count,
            .hovered_group = active_hovered_group,
            .hovered_material = active_hovered_material,
            .selected_scene = state.selected_scene.load(std::memory_order_relaxed) % scene_count,
            .group_count = material_group_count,
            .scene_count = scene_count,
            .mining_mode = state.mining_mode.load(std::memory_order_relaxed) ? 1u : 0u,
            .inspect_mode = inspect_visible ? 1u : 0u,
            .debug_mode = state.debug_visualization.load(std::memory_order_relaxed) ? 1u : 0u,
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
                    state.placement_mode.load(std::memory_order_relaxed) != 0u,
                    state.brush_shape.load(std::memory_order_relaxed)),
            .placement_mode = designer_workspace
                ? (state.designer_placement_mode.load(std::memory_order_relaxed) & 1u)
                : (state.placement_mode.load(std::memory_order_relaxed) != 0u ? 1u : 0u),
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

    bool draw_frame(SharedState& state, const bool simulation_tick) {
        auto& frame = frames[frame_index];
        constexpr std::uint64_t gpu_timeout_ns = 5'000'000'000ull;
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
            if (!needs_reset) ignite_air_region();
            else startup_log("Ignite Air skipped until the initial scene exists.");
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
        const auto acquire_result = vkAcquireNextImageKHR(device, swapchain, gpu_timeout_ns,
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
            pending_scene_export = selected_scene;
            reset_actor = true;
            reset_this_frame = true;
        }
        if (policy::editor_mutation_allowed(paused, reset_this_frame))
            record_paint(frame.command_buffer, state);

        const bool debug_visible = state.debug_visualization.load(std::memory_order_relaxed);
        const bool step_once = state.single_step.exchange(false, std::memory_order_acq_rel);
        const bool run_simulation = !reset_this_frame && (step_once ||
            (simulation_tick && !paused));
        bool collect_debug_stats = false;
        if (debug_visible && run_simulation) {
            collect_debug_stats = !debug_was_visible || (debug_sample_frame % 16u) == 0u;
            ++debug_sample_frame;
        } else if (!debug_visible) {
            debug_sample_frame = 0u;
        }
        debug_was_visible = debug_visible;
        if (collect_debug_stats) reset_debug_stats(frame.command_buffer);
        if (run_simulation) record_simulation_step(frame.command_buffer, state, collect_debug_stats);
        if (reset_this_frame) {
            // Reset is a hard epoch boundary: no held/queued edit or actor action
            // may leak into the freshly rebuilt scene on the next frame.
            state.primary_down.store(false, std::memory_order_release);
            state.fill_region.store(false, std::memory_order_release);
            state.ignite_air.store(false, std::memory_order_release);
            state.fire_tool_pressed.store(false, std::memory_order_release);
            state.deposit_resource_pressed.store(false, std::memory_order_release);
            state.blueprint_place_requested.store(false, std::memory_order_release);
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
        if (run_simulation || reset_actor || actor_action || actor_motion)
            record_actor(frame.command_buffer, state, reset_actor, actor_simulation);

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
        constexpr std::uint32_t map_refresh_steps = 15u;
        if (map_visible && !paused && !reset_this_frame &&
            (!map_was_visible || simulation_step - map_snapshot_step >= map_refresh_steps))
            record_map_snapshot(frame.command_buffer);
        map_was_visible = map_visible;
        record_designer_snapshot(frame.command_buffer, state);
        record_render(frame.command_buffer, image_index, state);
        check_vk(vkEndCommandBuffer(frame.command_buffer), "vkEndCommandBuffer");

        constexpr VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        const VkSubmitInfo submit_info{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &frame.image_available,
            .pWaitDstStageMask = &wait_stage,
            .commandBufferCount = 1,
            .pCommandBuffers = &frame.command_buffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &frame.render_finished,
        };
        check_vk(vkQueueSubmit(graphics_queue, 1, &submit_info, frame.fence), "vkQueueSubmit");
        if (!first_submission_logged) {
            startup_log("First GPU submission queued.");
            first_submission_logged = true;
        }

        const VkPresentInfoKHR present_info{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &frame.render_finished,
            .swapchainCount = 1,
            .pSwapchains = &swapchain,
            .pImageIndices = &image_index,
        };
        const auto present_result = vkQueuePresentKHR(present_queue, &present_info);
        if (present_result != VK_SUCCESS && present_result != VK_SUBOPTIMAL_KHR &&
            present_result != VK_ERROR_OUT_OF_DATE_KHR) {
            throw_vk("vkQueuePresentKHR", present_result);
        }
        if (!first_present_logged) {
            startup_log("First frame presented.");
            first_present_logged = true;
        }
        if (pending_scene_export.has_value()) {
            const auto scene_to_export = *pending_scene_export;
            pending_scene_export.reset();
            export_authored_scene_ppm(scene_to_export);
        }

        frame_index = (frame_index + 1u) % static_cast<std::uint32_t>(frames.size());
        return acquire_result != VK_SUBOPTIMAL_KHR && present_result != VK_SUBOPTIMAL_KHR &&
               present_result != VK_ERROR_OUT_OF_DATE_KHR;
    }


#if SANDHYBRID_ENABLE_VALIDATION
    void log_conservation_if_due(const SharedState& state) {
        if (!state.debug_visualization.load(std::memory_order_relaxed)) return;
        const auto now = std::chrono::steady_clock::now();
        if (next_conservation_log != std::chrono::steady_clock::time_point{} && now < next_conservation_log) return;
        next_conservation_log = now + std::chrono::seconds{5};

        check_vk(vkDeviceWaitIdle(device), "vkDeviceWaitIdle(conservation log)");
        void* mapped = nullptr;
        check_vk(vkMapMemory(device, conservation_buffer.memory, 0, conservation_buffer.size, 0, &mapped),
                 "vkMapMemory(conservation)");
        std::array<std::uint32_t, 8> counters{};
        std::memcpy(counters.data(), mapped, sizeof(counters));
        vkUnmapMemory(device, conservation_buffer.memory);
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

    void run_acceptance_tile_pass() {
        immediate_submit([&](const VkCommandBuffer command_buffer) {
            const auto acceptance_width = (std::min)(config.grid_width, 192u);
            const auto acceptance_height = (std::min)(config.grid_height, 192u);
            const SimulationPush push{
                .width = config.grid_width,
                .height = acceptance_height,
                .step = simulation_step,
                .seed = random_seed,
                .active_mode = 0u,
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
                                   const std::int32_t parity) {
        immediate_submit([&](const VkCommandBuffer command_buffer) {
            const auto acceptance_width = (std::min)(config.grid_width, 192u);
            const auto acceptance_height = (std::min)(config.grid_height, 192u);
            const MovementPush push{
                .width = config.grid_width,
                .height = acceptance_height,
                .step = simulation_step,
                .seed = random_seed,
                .phase = phase,
                .parity = parity,
                .active_mode = 0u,
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

    void run_acceptance_fine_pass(const std::int32_t phase, const std::int32_t parity) {
        immediate_submit([&](const VkCommandBuffer command_buffer) {
            const auto acceptance_width = (std::min)(config.grid_width, 192u);
            const auto acceptance_height = (std::min)(config.grid_height, 192u);
            const auto snapshot_set = current_set ^ 1u;
            buffer_barrier(command_buffer, cell_buffers[current_set],
                           VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT);
            buffer_barrier(command_buffer, cell_buffers[snapshot_set],
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_TRANSFER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT);
            const VkBufferCopy copy{.size = cell_buffers[current_set].size};
            vkCmdCopyBuffer(command_buffer, cell_buffers[current_set].handle,
                            cell_buffers[snapshot_set].handle, 1, &copy);
            buffer_barrier(command_buffer, cell_buffers[current_set],
                           VK_ACCESS_TRANSFER_READ_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            buffer_barrier(command_buffer, cell_buffers[snapshot_set],
                           VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT,
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

    void run_acceptance_horizontal_pass(const SharedState& state,
                                        const std::int32_t parity) {
        immediate_submit([&](const VkCommandBuffer command_buffer) {
            const auto acceptance_width = (std::min)(config.grid_width, 192u);
            const auto acceptance_height = (std::min)(config.grid_height, 192u);
            const auto snapshot_set = current_set ^ 1u;
            buffer_barrier(command_buffer, cell_buffers[current_set],
                           VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT);
            buffer_barrier(command_buffer, cell_buffers[snapshot_set],
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_TRANSFER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT);
            const VkBufferCopy copy{.size = cell_buffers[current_set].size};
            vkCmdCopyBuffer(command_buffer, cell_buffers[current_set].handle,
                            cell_buffers[snapshot_set].handle, 1, &copy);
            buffer_barrier(command_buffer, cell_buffers[current_set],
                           VK_ACCESS_TRANSFER_READ_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            buffer_barrier(command_buffer, cell_buffers[snapshot_set],
                           VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            const MovementPush push{
                .width = config.grid_width,
                .height = acceptance_height,
                .step = simulation_step,
                .seed = random_seed,
                .phase = 5,
                .parity = parity,
                .active_section_x = state.active_window_origin_x.load(std::memory_order_relaxed),
                .active_section_y = state.active_window_origin_y.load(std::memory_order_relaxed),
                .active_mode = 1u,
                .worker_count = state.section_worker_count.load(std::memory_order_relaxed),
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

        if (config.grid_width < 256u || config.grid_height < 256u) {
            append("world_dimensions", false, "acceptance requires at least 256x256 cells");
        } else {
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
                auto cells = acceptance_atmosphere_world();
                seed_rect(cells, Material::water, 64u, 64u, 8u, 8u);
                const auto obstacle = index_of(64u, 72u);
                cells[obstacle] = make_fill_cell(material_id(Material::stone),
                                                 static_cast<std::uint32_t>(obstacle));
                upload_scene_cells(cells);
                run_acceptance_tile_pass();
                run_acceptance_macro_pass(0, 0);
                run_acceptance_fine_pass(0, 0);
                run_acceptance_fine_pass(1, 1);
                run_acceptance_fine_pass(2, 0);
                const auto result = download_scene_cells();
                const auto total = count_material(result, Material::water);
                const auto source = count_rect(result, Material::water, 64u, 64u, 8u, 8u);
                const auto outside = total - source;
                append("macro_blocked_fine_fallback",
                       total == 64u && source > 0u && source < 64u && outside > 0u,
                       "water=" + std::to_string(total) +
                           " source=" + std::to_string(source) +
                           " moved_out=" + std::to_string(outside));
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

            const auto pre_pr19_hive_entropy = [&](const Scene scene,
                                                   const std::int32_t dx,
                                                   const std::int32_t dy) {
                const auto queen_y = scene == Scene::sandbox ? 234u : 232u;
                const auto x = static_cast<std::uint32_t>(static_cast<std::int32_t>(512) + dx);
                const auto y = static_cast<std::uint32_t>(static_cast<std::int32_t>(queen_y) + dy);
                return pre_pr19_hive_hash((y * pre_pr19_hive_canonical_width + x) ^
                                          pre_pr19_hive_canonical_seed);
            };
            const auto check_pre_pr19_hive = [&](const std::string_view name,
                                                 const Scene scene) {
                immediate_submit([&](const VkCommandBuffer command_buffer) {
                    record_reset(command_buffer, static_cast<std::uint32_t>(scene));
                });
                const auto cells = download_scene_cells();
                const std::uint32_t queen_x = 512u;
                const std::uint32_t queen_y = scene == Scene::sandbox ? 234u : 232u;
                std::uint32_t mismatches = 0u;
                std::uint32_t shell = 0u;
                std::uint32_t support = 0u;
                std::uint32_t honey = 0u;
                std::uint32_t pollen = 0u;
                std::uint32_t empty_chamber = 0u;
                for (std::int32_t dy = -16; dy <= 11; ++dy) {
                    for (std::int32_t dx = -37; dx <= 29; ++dx) {
                        const auto part = classify_pre_pr19_hive_cell(
                            dx, dy, pre_pr19_hive_entropy(scene, dx, dy));
                        const auto x = static_cast<std::uint32_t>(
                            static_cast<std::int32_t>(queen_x) + dx);
                        const auto y = static_cast<std::uint32_t>(
                            static_cast<std::int32_t>(queen_y) + dy);
                        const auto actual = static_cast<Material>(cells[index_of(x, y)].material);
                        bool matches = true;
                        switch (part) {
                        case HivePart::support:
                            matches = actual == Material::wood;
                            ++support;
                            break;
                        case HivePart::shell:
                            matches = actual == Material::beehive;
                            ++shell;
                            break;
                        case HivePart::queen:
                            matches = actual == Material::queen_bee;
                            break;
                        case HivePart::honey:
                            matches = actual == Material::honey;
                            ++honey;
                            break;
                        case HivePart::pollen:
                            matches = actual == Material::pollen;
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
                append(std::string{name},
                       mismatches == 0u && shell > 0u && support == 268u &&
                           honey > 0u && pollen > 0u && empty_chamber > 0u,
                       "mismatches=" + std::to_string(mismatches) +
                           " support=" + std::to_string(support) +
                           " shell=" + std::to_string(shell) +
                           " honey=" + std::to_string(honey) +
                           " pollen=" + std::to_string(pollen) +
                           " chamber_empty=" + std::to_string(empty_chamber));
            };

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
                for (std::int32_t pass = 0; pass < 4; ++pass)
                    run_acceptance_horizontal_pass(state, pass & 1);
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
                for (std::int32_t step = 0; step < 8; ++step) {
                    run_acceptance_fine_pass(0, 0);
                    run_acceptance_fine_pass(0, 1);
                }
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
                auto cells = acceptance_atmosphere_world();
                seed_rect(cells, Material::stone, 104u, 161u, 8u, 1u);
                seed_rect(cells, Material::water, 108u, 160u, 3u, 1u);
                upload_scene_cells(cells);
                run_acceptance_horizontal_pass(state, 0);
                const auto result = download_scene_cells();
                const auto [units, halves] = water_half_units(result);
                append("supplied_ledge_creates_half_water",
                       units == 6u && halves == 2u &&
                           count_material(result, Material::water) == 4u,
                       "half_units=" + std::to_string(units) +
                           " halves=" + std::to_string(halves) +
                           " water_cells=" +
                           std::to_string(count_material(result, Material::water)));
            }

            check_pre_pr19_hive("sandbox_hard_coded_hive", Scene::sandbox);
            check_pre_pr19_hive("ecosystem_hard_coded_hive", Scene::ecosystem);
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
    void run(const std::atomic_bool& stop_requested, SharedState& state) {
        startup_log("Entering render loop...");
        if (!config.runtime_acceptance_report.empty()) {
            const auto exit_code = run_runtime_acceptance(state);
            state.runtime_acceptance_exit_code.store(exit_code, std::memory_order_release);
            state.quit.store(true, std::memory_order_release);
            return;
        }
        using Clock = std::chrono::steady_clock;
        const auto frame_interval = std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<double>{1.0 / static_cast<double>(config.max_frames_per_second)});
        auto next_frame = Clock::now();
        constexpr auto simulation_interval = std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<double>{1.0 / 60.0});
        auto next_simulation = next_frame;
        auto fps_window_start = next_frame;
        std::uint32_t rendered_frames = 0;

        while (!stop_requested.load(std::memory_order_acquire) &&
               !state.quit.load(std::memory_order_acquire)) {
            const auto width = state.window_width.load(std::memory_order_relaxed);
            const auto height = state.window_height.load(std::memory_order_relaxed);
            if (width == 0 || height == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds{16});
                next_frame = Clock::now();
                fps_window_start = next_frame;
                rendered_frames = 0;
                continue;
            }

            if (state.resized.exchange(false, std::memory_order_acq_rel)) {
                recreate_swapchain(width, height);
            }

            const auto before_draw = Clock::now();
            const bool simulation_tick = before_draw >= next_simulation;
            if (simulation_tick) {
                next_simulation += simulation_interval;
                if (before_draw - next_simulation > simulation_interval * 2) next_simulation = before_draw;
            }

            if (!draw_frame(state, simulation_tick)) {
                recreate_swapchain(width, height);
            } else {
                ++rendered_frames;
            }
#if SANDHYBRID_ENABLE_VALIDATION
            log_conservation_if_due(state);
#endif

            const auto now = Clock::now();
            const auto elapsed = now - fps_window_start;
            if (elapsed >= std::chrono::milliseconds{500}) {
                const auto seconds = std::chrono::duration<double>(elapsed).count();
                const auto fps = seconds > 0.0
                    ? static_cast<std::uint32_t>(static_cast<double>(rendered_frames) / seconds + 0.5)
                    : 0u;
                state.frames_per_second.store(fps, std::memory_order_relaxed);
                fps_window_start = now;
                rendered_frames = 0;
            }

            next_frame += frame_interval;
            const auto frame_end = Clock::now();
            if (frame_end < next_frame) {
                std::this_thread::sleep_until(next_frame);
            } else if (frame_end - next_frame > frame_interval * 4) {
                next_frame = frame_end;
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
