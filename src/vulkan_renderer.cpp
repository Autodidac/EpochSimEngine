#include "epoch/sand/vulkan_renderer.hpp"

#include "epoch/sand/material.hpp"
#include "epoch/sand/scene.hpp"
#include "epoch/sand/scene_image.hpp"
#include "epoch/sand/ui_layout.hpp"
#include "epoch/sand/ui_text_data.hpp"

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
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace epoch::sand {

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
constexpr std::uint32_t fill_aux_random_mask = 0x00ffff00u;

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
    else if (material == Material::carbon_dioxide) cell.aux |= 180u;
    else if (material == Material::hydrogen) cell.aux |= 210u;

    if (is_block_material(material)) {
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
    std::fprintf(stderr, "[EpochSand] %.*s\n", static_cast<int>(message.size()), message.data());
    std::fflush(stderr);
#ifdef _WIN32
    const std::string line = std::string{"[EpochSand] "} + std::string{message} + "\n";
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
};
static_assert(sizeof(SimulationPush) == 32);

struct MovementPush final {
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint32_t step{};
    std::uint32_t seed{};
    std::int32_t phase{};
    std::int32_t parity{};
    std::uint32_t reserved0{};
    std::uint32_t reserved1{};
};
static_assert(sizeof(MovementPush) == 32);

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
    std::uint32_t reserved0{};
    std::uint32_t reserved1{};
    std::uint32_t reserved2{};
};
static_assert(sizeof(ActorPush) == 64);

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
};
static_assert(sizeof(RenderPush) == 144);

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
    Buffer sunlight_buffer{};
    Buffer actor_buffer{};
    Buffer tile_buffer{};
    Buffer chunk_buffer{};
    Buffer conservation_buffer{};
    Buffer ui_text_buffer{};
    Buffer scene_staging_buffer{};
    std::uint32_t current_set{};
    std::uint32_t simulation_step{};
    std::uint32_t random_seed{0xD17A5EEDu};
    bool needs_reset{true};
    bool gpu_stalled{false};
    bool first_submission_logged{false};
    bool first_present_logged{false};
    std::optional<std::uint32_t> pending_scene_export{};
#if EPOCH_SAND_ENABLE_VALIDATION
    std::chrono::steady_clock::time_point next_conservation_log{};
#endif

    explicit Impl(const NativeWindow& native_window, const SimulationConfig simulation_config)
        : window(native_window), config(simulation_config) {
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
            destroy_buffer(ui_text_buffer);
            destroy_buffer(conservation_buffer);
            destroy_buffer(chunk_buffer);
            destroy_buffer(tile_buffer);
            destroy_buffer(actor_buffer);
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
#if EPOCH_SAND_ENABLE_VALIDATION
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
            .pEngineName = "Epoch",
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
        sunlight_buffer = create_buffer(light_size, storage_usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        actor_buffer = create_buffer(sizeof(std::uint32_t) * 20u, storage_usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        tile_buffer = create_buffer(tile_size, storage_usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        chunk_buffer = create_buffer(chunk_size, storage_usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        conservation_buffer = create_buffer(sizeof(std::uint32_t) * debug_stat_word_count, storage_usage,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        const auto ui_text_size = static_cast<VkDeviceSize>(ui::text_storage.size() * sizeof(std::uint32_t));
        ui_text_buffer = create_buffer(ui_text_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
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
            .descriptorCount = 16,
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

    void fill_connected_region(SharedState& state) {
        auto cells = download_scene_cells();
        const auto [cursor_x, cursor_y] = grid_cursor(state);
        if (cursor_x < 0 || cursor_y < 0 ||
            cursor_x >= static_cast<std::int32_t>(config.grid_width) ||
            cursor_y >= static_cast<std::int32_t>(config.grid_height)) return;

        const auto replacement_id =
            state.selected_material.load(std::memory_order_relaxed) % material_count;
        const auto replacement = static_cast<Material>(replacement_id);
        if (replacement == Material::bee || replacement == Material::queen_bee ||
            replacement == Material::bee_nest) {
            startup_log("Fill ignored for colony agents/prefab; place Bee nest to build a live colony.");
            return;
        }

        const auto start = static_cast<std::uint32_t>(cursor_y) * config.grid_width +
                           static_cast<std::uint32_t>(cursor_x);
        const auto target = cells[start].material;
        if (target == replacement_id) return;

        std::vector<std::uint8_t> visited(cells.size(), 0u);
        std::vector<std::uint32_t> queue(cells.size());
        std::size_t head = 0;
        std::size_t tail = 0;
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

        std::size_t changed = 0;
        if (is_block_material(replacement)) {
            constexpr std::uint32_t block = 8u;
            const auto tile_columns = divide_round_up(config.grid_width, block);
            const auto tile_rows = divide_round_up(config.grid_height, block);
            std::vector<std::uint8_t> touched(
                static_cast<std::size_t>(tile_columns) * tile_rows, 0u);
            for (std::uint32_t index = 0; index < visited.size(); ++index) {
                if (visited[index] == 0u) continue;
                const auto x = index % config.grid_width;
                const auto y = index / config.grid_width;
                touched[(y / block) * tile_columns + (x / block)] = 1u;
            }
            for (std::uint32_t tile_y = 0; tile_y < tile_rows; ++tile_y) {
                for (std::uint32_t tile_x = 0; tile_x < tile_columns; ++tile_x) {
                    if (touched[tile_y * tile_columns + tile_x] == 0u) continue;
                    bool complete = true;
                    for (std::uint32_t local_y = 0; local_y < block && complete; ++local_y) {
                        for (std::uint32_t local_x = 0; local_x < block; ++local_x) {
                            const auto x = tile_x * block + local_x;
                            const auto y = tile_y * block + local_y;
                            if (x >= config.grid_width || y >= config.grid_height ||
                                visited[y * config.grid_width + x] == 0u) {
                                complete = false;
                                break;
                            }
                        }
                    }
                    if (!complete) continue;
                    for (std::uint32_t local_y = 0; local_y < block; ++local_y) {
                        for (std::uint32_t local_x = 0; local_x < block; ++local_x) {
                            const auto index = (tile_y * block + local_y) * config.grid_width +
                                               tile_x * block + local_x;
                            cells[index] = make_fill_cell(replacement_id, index);
                            ++changed;
                        }
                    }
                }
            }
        } else {
            for (std::uint32_t index = 0; index < visited.size(); ++index) {
                if (visited[index] == 0u) continue;
                cells[index] = make_fill_cell(replacement_id, index);
                ++changed;
            }
        }

        if (changed == 0u) {
            startup_log("Fill found no complete aligned bricks in the connected region.");
            return;
        }
        upload_scene_cells(cells);
        startup_log("Filled connected region: " + std::to_string(changed) + " cells.");
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
        });
        current_set = 0u;
        simulation_step = 0u;
        needs_reset = false;
    }

    [[nodiscard]] bool load_scene_image(const std::uint32_t scene_index) {
        const auto scene = static_cast<Scene>(scene_index % scene_count);
        const auto path = scene_image_path(scene_directory(), scene);
        std::vector<SceneCell> cells(
            static_cast<std::size_t>(config.grid_width) * config.grid_height);
        std::string error;
        if (!load_scene_ppm(path, config.grid_width, config.grid_height, cells, error)) {
            startup_log("Scene image load skipped: " + error);
            return false;
        }
        upload_scene_cells(cells);
        std::string key_error;
        if (!write_scene_material_key(scene_directory(), key_error))
            startup_log("Scene material-key warning: " + key_error);
        startup_log("Loaded moddable scene image: " + path.string());
        return true;
    }

    void save_scene_image(const std::uint32_t scene_index) {
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
                 "vkMapMemory(scene save)");
        std::memcpy(cells.data(), mapped, cells.size() * sizeof(SceneCell));
        vkUnmapMemory(device, scene_staging_buffer.memory);

        const auto scene = static_cast<Scene>(scene_index % scene_count);
        const auto path = scene_image_path(scene_directory(), scene);
        std::string error;
        if (!save_scene_ppm(path, config.grid_width, config.grid_height, cells, error))
            throw std::runtime_error("Unable to save scene image: " + error);
        if (!write_scene_material_key(scene_directory(), error))
            throw std::runtime_error("Unable to save scene material key: " + error);
        startup_log("Saved moddable scene image: " + path.string());
    }

    void record_reset(const VkCommandBuffer command_buffer, const std::uint32_t scene_index) {
        SimulationPush push{
            .width = config.grid_width,
            .height = config.grid_height,
            .step = simulation_step,
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
        buffer_barrier(command_buffer, chunk_buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        buffer_barrier(command_buffer, tile_buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        for (const auto& buffer : cell_buffers) {
            buffer_barrier(command_buffer, buffer, VK_ACCESS_SHADER_WRITE_BIT,
                           VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        }
        current_set = 0;
        simulation_step = 0;
        needs_reset = false;
    }

    struct GridView final {
        std::uint32_t origin_x{};
        std::uint32_t origin_y{};
        std::uint32_t width{};
        std::uint32_t height{};
    };

    [[nodiscard]] GridView grid_view(const SharedState& state) const {
        const auto zoom = std::clamp(state.camera_zoom.load(std::memory_order_relaxed), 1u, 8u);
        const auto visible_width = (std::max)(8u, config.grid_width / zoom);
        const auto visible_height = (std::max)(8u, config.grid_height / zoom);
        const auto center_x = std::clamp(state.camera_center_x.load(std::memory_order_relaxed),
                                         0, static_cast<int>(config.grid_width - 1u));
        const auto center_y = std::clamp(state.camera_center_y.load(std::memory_order_relaxed),
                                         0, static_cast<int>(config.grid_height - 1u));
        const auto origin_x = static_cast<std::uint32_t>(std::clamp(
            center_x - static_cast<int>(visible_width / 2u), 0,
            static_cast<int>(config.grid_width - visible_width)));
        const auto origin_y = static_cast<std::uint32_t>(std::clamp(
            center_y - static_cast<int>(visible_height / 2u), 0,
            static_cast<int>(config.grid_height - visible_height)));
        return {origin_x, origin_y, visible_width, visible_height};
    }

    std::pair<std::int32_t, std::int32_t> grid_cursor(const SharedState& state) const {
        const auto width = (std::max)(state.window_width.load(std::memory_order_relaxed), 1u);
        const auto height = (std::max)(state.window_height.load(std::memory_order_relaxed), 1u);
        const auto layout = ui::make_layout(width, height);
        const auto viewport = ui::make_simulation_viewport(layout, config.grid_width, config.grid_height);
        const auto viewport_width = (std::max)(static_cast<std::uint32_t>(viewport.rect.size.x), 1u);
        const auto viewport_height = (std::max)(static_cast<std::uint32_t>(viewport.rect.size.y), 1u);
        const auto mouse_x = std::clamp(
            state.mouse_x.load(std::memory_order_relaxed) - static_cast<int>(viewport.rect.position.x),
            0, static_cast<int>(viewport_width - 1u));
        const auto mouse_y = std::clamp(
            state.mouse_y.load(std::memory_order_relaxed) - static_cast<int>(viewport.rect.position.y),
            0, static_cast<int>(viewport_height - 1u));
        const auto view = grid_view(state);
        const auto grid_x = static_cast<std::int32_t>(view.origin_x +
            static_cast<std::uint64_t>(mouse_x) * view.width / viewport_width);
        const auto grid_y = static_cast<std::int32_t>(view.origin_y +
            static_cast<std::uint64_t>(mouse_y) * view.height / viewport_height);
        return {grid_x, grid_y};
    }

    void record_paint(const VkCommandBuffer command_buffer, const SharedState& state) {
        const bool erase = state.secondary_down.load(std::memory_order_relaxed);
        const bool paint = state.primary_down.load(std::memory_order_relaxed);
        if (!erase && !paint) return;

        const auto [grid_x, grid_y] = grid_cursor(state);
        const auto requested_radius = state.brush_radius.load(std::memory_order_relaxed);
        const auto material = erase ? static_cast<std::uint32_t>(Material::empty)
                                    : state.selected_material.load(std::memory_order_relaxed);
        const auto selected = static_cast<Material>(material < material_count ? material : 0u);
        const auto radius = material == static_cast<std::uint32_t>(Material::bee_nest)
            ? 64u
            : (is_block_material(selected) ? 8u : requested_radius);
        const auto shape = is_block_material(selected)
            ? 1u : state.brush_shape.load(std::memory_order_relaxed) % 4u;
        const auto packed_material = material | (shape << 16u);
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
                                const bool collect_debug_stats) {
        SimulationPush simulation_push{
            .width = config.grid_width,
            .height = config.grid_height,
            .step = simulation_step,
            .seed = random_seed,
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
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
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
        const std::array<std::int32_t, 5> macro_phases = (simulation_step & 1u) == 0u
  ? std::array<std::int32_t, 5>{0, 1, 2, 3, 4}
  : std::array<std::int32_t, 5>{0, 2, 1, 4, 3};
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
  };
  vkCmdPushConstants(command_buffer, compute_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                     0, sizeof(macro_push), &macro_push);
  if (phase <= 2) {
      vkCmdDispatch(command_buffer, tile_columns, divide_round_up(tile_rows, 2u), 1);
  } else {
      vkCmdDispatch(command_buffer, divide_round_up(tile_columns, 2u), tile_rows, 1);
  }
  buffer_barrier(command_buffer, cell_buffers[current_set], VK_ACCESS_SHADER_WRITE_BIT,
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
        const std::array<std::int32_t, 13> phases = (simulation_step & 1u) == 0u
            ? std::array<std::int32_t, 13>{0, 1, 2, 3, 4, 5, 5, 5, 5, 5, 5, 5, 5}
            : std::array<std::int32_t, 13>{0, 2, 1, 4, 3, 5, 5, 5, 5, 5, 5, 5, 5};
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
                .reserved1 = phase_index >= 9u ? 1u : 0u,
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

    void record_render(const VkCommandBuffer command_buffer, const std::uint32_t image_index,
                       const SharedState& state) const {
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
        const auto layout = ui::make_layout(swapchain_extent.width, swapchain_extent.height);
        const auto simulation_viewport = ui::make_simulation_viewport(
            layout, config.grid_width, config.grid_height);
        const auto view = grid_view(state);
        const epochengine::gui_lib::Vec2 pointer{
            static_cast<float>(state.mouse_x.load(std::memory_order_relaxed)),
            static_cast<float>(state.mouse_y.load(std::memory_order_relaxed))
        };
        const bool inspect_visible = state.inspect_material.load(std::memory_order_relaxed) &&
                                     epochengine::gui_lib::contains(simulation_viewport.rect, pointer);
        const RenderPush push{
            .grid_width = config.grid_width,
            .grid_height = config.grid_height,
            .window_width = swapchain_extent.width,
            .window_height = swapchain_extent.height,
            .selected_material = state.selected_material.load(std::memory_order_relaxed),
            .material_count = material_count,
            .cursor_x = cursor_x,
            .cursor_y = cursor_y,
            .brush_radius = [&state]() {
                const auto material_id = state.selected_material.load(std::memory_order_relaxed);
                const auto material = static_cast<Material>(material_id < material_count ? material_id : 0u);
                if (material == Material::bee_nest) return 12u;
                return is_block_material(material) ? 8u : state.brush_radius.load(std::memory_order_relaxed);
            }(),
            .status_height = static_cast<std::uint32_t>(layout.status.size.y),
            // Existing push slot carries compact sidebar width.
            .palette_height = static_cast<std::uint32_t>(layout.status.size.x),
            .group_tabs_height = static_cast<std::uint32_t>(layout.group_tabs.size.y),
            .material_slots = material_slots_per_group,
            .frames_per_second = state.frames_per_second.load(std::memory_order_relaxed),
            .paused = state.paused.load(std::memory_order_relaxed) ? 1u : 0u,
            .steps_per_frame = state.steps_per_frame.load(std::memory_order_relaxed),
            .selected_group = state.selected_group.load(std::memory_order_relaxed) % material_group_count,
            .hovered_group = state.hovered_group.load(std::memory_order_relaxed),
            .hovered_material = state.hovered_material.load(std::memory_order_relaxed),
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
            .brush_shape = [&state]() {
                const auto material_id = state.selected_material.load(std::memory_order_relaxed);
                const auto material = static_cast<Material>(material_id < material_count ? material_id : 0u);
                return is_block_material(material) ? 1u : state.brush_shape.load(std::memory_order_relaxed) % 4u;
            }(),
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
                "update the GPU driver and inspect the last EpochSand startup line.");
        }
        check_vk(fence_result, "vkWaitForFences");

        const auto selected_scene = state.selected_scene.load(std::memory_order_relaxed) % scene_count;
        if (state.save_scene_image.exchange(false, std::memory_order_acq_rel)) {
            if (!needs_reset) save_scene_image(selected_scene);
            else startup_log("Scene save skipped until the initial scene exists.");
        }
        const bool explicit_load = state.load_scene_image.exchange(false, std::memory_order_acq_rel);
        if (state.fill_region.exchange(false, std::memory_order_acq_rel)) {
            if (!needs_reset) fill_connected_region(state);
            else startup_log("Fill skipped until the initial scene exists.");
        }
        const bool reset_requested = needs_reset || state.reset.exchange(false, std::memory_order_acq_rel);
        bool image_loaded = false;
        if (explicit_load) {
            const auto selected = static_cast<Scene>(selected_scene);
            if (scene_image_exists(scene_directory(), selected)) image_loaded = load_scene_image(selected_scene);
            else startup_log("No saved PPM exists for the selected scene.");
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
        record_paint(frame.command_buffer, state);

        const bool debug_stats = state.debug_visualization.load(std::memory_order_relaxed);
        if (debug_stats) reset_debug_stats(frame.command_buffer);
        const bool step_once = state.single_step.exchange(false, std::memory_order_acq_rel);
        const bool run_simulation = !reset_this_frame && (step_once ||
            (simulation_tick && !state.paused.load(std::memory_order_relaxed)));
        if (run_simulation) record_simulation_step(frame.command_buffer, debug_stats);
        const bool actor_action = state.fire_tool.load(std::memory_order_relaxed) ||
                                  state.deposit_resource.load(std::memory_order_relaxed) ||
                                  state.fire_tool_pressed.load(std::memory_order_acquire) ||
                                  state.deposit_resource_pressed.load(std::memory_order_acquire);
        const bool actor_motion = state.move_x.load(std::memory_order_relaxed) != 0 ||
                                  state.move_y.load(std::memory_order_relaxed) != 0 ||
                                  state.jump.load(std::memory_order_relaxed);
        const bool actor_simulation = run_simulation ||
                                      (actor_motion && !state.paused.load(std::memory_order_relaxed));
        if (run_simulation || reset_actor || actor_action || actor_motion)
            record_actor(frame.command_buffer, state, reset_actor, actor_simulation);

        if (debug_stats) {
            const auto movement_pair_tests = run_simulation
                ? config.grid_width * config.grid_height * 9u / 2u
                : 0u;
            record_debug_stats(frame.command_buffer, state, movement_pair_tests);
        }
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
            save_scene_image(scene_to_export);
        }

        frame_index = (frame_index + 1u) % static_cast<std::uint32_t>(frames.size());
        return acquire_result != VK_SUBOPTIMAL_KHR && present_result != VK_SUBOPTIMAL_KHR &&
               present_result != VK_ERROR_OUT_OF_DATE_KHR;
    }


#if EPOCH_SAND_ENABLE_VALIDATION
    void log_conservation_if_due(const SharedState& state) {
        if (!state.debug_visualization.load(std::memory_order_relaxed)) return;
        const auto now = std::chrono::steady_clock::now();
        if (next_conservation_log.time_since_epoch().count() != 0 && now < next_conservation_log) return;
        next_conservation_log = now + std::chrono::seconds{5};

        check_vk(vkDeviceWaitIdle(device), "vkDeviceWaitIdle(conservation log)");
        void* mapped = nullptr;
        check_vk(vkMapMemory(device, conservation_buffer.memory, 0, conservation_buffer.size, 0, &mapped),
                 "vkMapMemory(conservation)");
        std::array<std::uint32_t, 8> counters{};
        std::memcpy(counters.data(), mapped, sizeof(counters));
        vkUnmapMemory(device, conservation_buffer.memory);
        std::fprintf(stderr,
            "[EpochSand conservation] created=%u destroyed=%u converted=%u boundary=%u "
            "phase=%u rebuilt=%u broken=%u errors=%u\n",
            counters[0], counters[1], counters[2], counters[3],
            counters[4], counters[5], counters[6], counters[7]);
    }
#endif

    void run(const std::atomic_bool& stop_requested, SharedState& state) {
        startup_log("Entering render loop...");
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
#if EPOCH_SAND_ENABLE_VALIDATION
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

VulkanRenderer::VulkanRenderer(const NativeWindow& window, const SimulationConfig config)
    : impl_(std::make_unique<Impl>(window, config)) {}

VulkanRenderer::~VulkanRenderer() = default;

void VulkanRenderer::run(const std::atomic_bool& stop_requested, SharedState& shared_state) {
    impl_->run(stop_requested, shared_state);
}

} // namespace epoch::sand
