cmake_minimum_required(VERSION 3.28)
project(EpochSimEngineDownstreamConsumer LANGUAGES CXX)

enable_testing()

set(EPOCHSIMENGINE_PACKAGE_ORDER canonical_only CACHE STRING "Package discovery contract")
if(EPOCHSIMENGINE_PACKAGE_ORDER STREQUAL "canonical_only")
    find_package(EpochSimEngine CONFIG REQUIRED)
elseif(EPOCHSIMENGINE_PACKAGE_ORDER STREQUAL "legacy_only")
    find_package(SandHybrid CONFIG REQUIRED)
elseif(EPOCHSIMENGINE_PACKAGE_ORDER STREQUAL "canonical_first")
    find_package(EpochSimEngine CONFIG REQUIRED)
    find_package(SandHybrid CONFIG REQUIRED)
elseif(EPOCHSIMENGINE_PACKAGE_ORDER STREQUAL "legacy_first")
    find_package(SandHybrid CONFIG REQUIRED)
    find_package(EpochSimEngine CONFIG REQUIRED)
elseif(EPOCHSIMENGINE_PACKAGE_ORDER STREQUAL "legacy_targets")
    if(NOT EXISTS "${EPOCHSIMENGINE_LEGACY_TARGETS_FILE}")
        message(FATAL_ERROR "The direct legacy targets-file path is required")
    endif()
    include("${EPOCHSIMENGINE_LEGACY_TARGETS_FILE}")
else()
    message(FATAL_ERROR "Unknown package discovery contract")
endif()

# The canonical target owns a real installed library, not an interface facade
# pointing back to the demo's historical name. Legacy consumers share it.
get_target_property(canonical_type EpochSimEngine::EpochSimEngine TYPE)
get_target_property(legacy_target SandHybrid::SandHybrid ALIASED_TARGET)
if(NOT canonical_type STREQUAL "STATIC_LIBRARY" OR
   NOT legacy_target STREQUAL "EpochSimEngine::EpochSimEngine")
    message(FATAL_ERROR "Canonical library ownership or legacy target alias is incorrect")
endif()
if(TARGET Vulkan::Vulkan OR TARGET Autodidac::EpochGui OR
   TARGET EpochSimEngine::VulkanRuntime OR TARGET SandHybrid::VulkanRuntime)
    message(FATAL_ERROR "The installed headless core must not configure a graphics dependency/runtime")
endif()

add_executable(epochsimengine_downstream_consumer main.cpp)
target_link_libraries(epochsimengine_downstream_consumer PRIVATE EpochSimEngine::EpochSimEngine)
target_compile_features(epochsimengine_downstream_consumer PRIVATE cxx_std_23)

add_executable(sandhybrid_compat_downstream_consumer legacy.cpp)
target_link_libraries(sandhybrid_compat_downstream_consumer PRIVATE SandHybrid::SandHybrid)
target_compile_features(sandhybrid_compat_downstream_consumer PRIVATE cxx_std_23)

if(EPOCHSIMENGINE_PACKAGE_ORDER STREQUAL "canonical_only")
    # Compile each installed forwarding header as the first and only include
    # in its own translation unit. An umbrella include must not hide missing
    # prerequisites or an omitted install rule. No graphics SDK is configured.
    set(header_sources)
    foreach(header IN ITEMS actor_medium atmosphere blueprint camera_policy
            hive_recovery input_routing inventory library machinery material
            material_color namespace packet_transaction scene scene_image
            scene_spawn section_grid section_scheduler simulation_policy
            terrain_generation world_layout world_save)
        set(header_source "${CMAKE_CURRENT_BINARY_DIR}/header_${header}.cpp")
        file(GENERATE OUTPUT "${header_source}"
            CONTENT "#include <epochsimengine/${header}.hpp>\n")
        list(APPEND header_sources "${header_source}")
    endforeach()
    add_library(epochsimengine_installed_headers OBJECT ${header_sources})
    target_link_libraries(epochsimengine_installed_headers PRIVATE EpochSimEngine::EpochSimEngine)
endif()

add_test(NAME epochsimengine_installed_consumer COMMAND epochsimengine_downstream_consumer)
add_test(NAME sandhybrid_compat_installed_consumer COMMAND sandhybrid_compat_downstream_consumer)
