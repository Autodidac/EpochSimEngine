cmake_minimum_required(VERSION 3.28)

project(EpochSimEngine VERSION 2.5.29 LANGUAGES CXX)

include(CTest)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(EPOCHSIMENGINE_VALIDATION_DEFAULT OFF)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(EPOCHSIMENGINE_VALIDATION_DEFAULT ON)
endif()

# Legacy cache variables remain accepted for existing consumers and presets.
# Conflicting spellings are rejected instead of silently changing build scope.
function(epochsimengine_compat_option canonical legacy description default_value)
    if(DEFINED ${canonical} AND DEFINED ${legacy})
        if((${canonical} AND NOT ${legacy}) OR (${legacy} AND NOT ${canonical}))
            message(FATAL_ERROR "${canonical} conflicts with legacy ${legacy}; set both to the same value or unset the legacy cache entry.")
        endif()
    elseif(DEFINED ${legacy})
        set(default_value "${${legacy}}")
    endif()
    option(${canonical} "${description}" ${default_value})
    # A normal variable mirrors the canonical value without creating a second
    # cache entry that would become stale when a canonical option is changed.
    set(${legacy} "${${canonical}}" PARENT_SCOPE)
endfunction()

epochsimengine_compat_option(EPOCHSIMENGINE_BUILD_DEMO SANDHYBRID_BUILD_APP
    "Build SandHybrid, the native EpochSimEngine Vulkan demo" ON)
epochsimengine_compat_option(EPOCHSIMENGINE_BUILD_VULKAN_RUNTIME SANDHYBRID_BUILD_VULKAN_RUNTIME
    "Build the optional EpochSimEngine Vulkan simulation and presentation runtime"
    ${EPOCHSIMENGINE_BUILD_DEMO})
epochsimengine_compat_option(EPOCHSIMENGINE_ENABLE_VALIDATION SANDHYBRID_ENABLE_VALIDATION
    "Enable Vulkan validation layers" ${EPOCHSIMENGINE_VALIDATION_DEFAULT})
epochsimengine_compat_option(EPOCHSIMENGINE_WARNINGS_AS_ERRORS SANDHYBRID_WARNINGS_AS_ERRORS
    "Treat compiler warnings as errors" OFF)

if(SANDHYBRID_BUILD_APP AND NOT SANDHYBRID_BUILD_VULKAN_RUNTIME)
    message(FATAL_ERROR
        "EPOCHSIMENGINE_BUILD_DEMO requires EPOCHSIMENGINE_BUILD_VULKAN_RUNTIME. "
        "Disable the demo for a pure headless library build.")
endif()

function(sandhybrid_configure_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive- /Zc:preprocessor /EHsc /utf-8)
        if(SANDHYBRID_WARNINGS_AS_ERRORS)
            target_compile_options(${target_name} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target_name} PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wno-missing-field-initializers)
        if(SANDHYBRID_WARNINGS_AS_ERRORS)
            target_compile_options(${target_name} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# Reusable, platform-neutral library. Native startup, windowing, Vulkan, EpochGui,
# and application event handling are intentionally absent from this target.
add_library(EpochSimEngine STATIC
    src/scene_image.cpp
    src/hive_recovery.cpp
    src/world_save.cpp
    src/section_grid.cpp
    src/section_scheduler.cpp
)
add_library(EpochSimEngine::EpochSimEngine ALIAS EpochSimEngine)
# Legacy target names are aliases of the same library, never a second archive.
add_library(SandHybrid ALIAS EpochSimEngine)
add_library(SandHybrid::SandHybrid ALIAS EpochSimEngine)
add_library(Autodidac::SandHybrid ALIAS EpochSimEngine)
set_target_properties(EpochSimEngine PROPERTIES
    EXPORT_NAME EpochSimEngine
    POSITION_INDEPENDENT_CODE ON
)
target_include_directories(EpochSimEngine PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(EpochSimEngine PUBLIC cxx_std_23)
sandhybrid_configure_warnings(EpochSimEngine)

if(SANDHYBRID_BUILD_VULKAN_RUNTIME)
    add_subdirectory(third_party/EpochGui)
    find_package(Vulkan REQUIRED)
    find_package(Threads REQUIRED)

    if(NOT Vulkan_GLSLC_EXECUTABLE)
        set(SANDHYBRID_GLSLC_HINTS
            "$ENV{VULKAN_SDK}/Bin"
            "$ENV{VULKAN_SDK}/bin"
        )
        foreach(VCPKG_INSTALL_ROOT IN ITEMS
                "${VCPKG_INSTALLED_DIR}"
                "${CMAKE_BINARY_DIR}/vcpkg_installed")
            foreach(VCPKG_TOOL_TRIPLET IN ITEMS
                    "${VCPKG_HOST_TRIPLET}"
                    "${VCPKG_TARGET_TRIPLET}")
                if(VCPKG_INSTALL_ROOT AND VCPKG_TOOL_TRIPLET)
                    list(APPEND SANDHYBRID_GLSLC_HINTS
                        "${VCPKG_INSTALL_ROOT}/${VCPKG_TOOL_TRIPLET}/tools/shaderc")
                endif()
            endforeach()
        endforeach()

        find_program(Vulkan_GLSLC_EXECUTABLE
            NAMES glslc glslc.exe
            HINTS ${SANDHYBRID_GLSLC_HINTS}
        )
    endif()

    if(NOT Vulkan_GLSLC_EXECUTABLE)
        message(FATAL_ERROR
            "glslc was not found. The vcpkg shaderc host tool should provide it; "
            "alternatively install the Vulkan SDK and put glslc on PATH.")
    endif()

    set(SANDHYBRID_VULKAN_LIBRARY Vulkan::Vulkan)
    if(UNIX AND NOT APPLE)
        find_library(SANDHYBRID_SYSTEM_VULKAN_LIBRARY
            NAMES vulkan
            PATHS /usr/lib/x86_64-linux-gnu /usr/lib64 /usr/lib
            NO_DEFAULT_PATH
            NO_CMAKE_FIND_ROOT_PATH
        )
        if(SANDHYBRID_SYSTEM_VULKAN_LIBRARY)
            set(SANDHYBRID_VULKAN_LIBRARY "${SANDHYBRID_SYSTEM_VULKAN_LIBRARY}")
        endif()
    endif()

    add_library(EpochSimEngine_VulkanRuntime STATIC src/vulkan_renderer.cpp)
    add_library(EpochSimEngine::VulkanRuntime ALIAS EpochSimEngine_VulkanRuntime)
    add_library(SandHybrid_VulkanRuntime ALIAS EpochSimEngine_VulkanRuntime)
    add_library(SandHybrid::VulkanRuntime ALIAS EpochSimEngine_VulkanRuntime)
    target_include_directories(EpochSimEngine_VulkanRuntime SYSTEM PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(EpochSimEngine_VulkanRuntime PUBLIC
        EpochSimEngine::EpochSimEngine
        ${SANDHYBRID_VULKAN_LIBRARY}
        Threads::Threads
        Autodidac::EpochGui
    )
    target_compile_definitions(EpochSimEngine_VulkanRuntime PRIVATE
        SANDHYBRID_VERSION_STRING="${PROJECT_VERSION}"
        $<$<PLATFORM_ID:Windows>:VK_USE_PLATFORM_WIN32_KHR>
        $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
        $<$<PLATFORM_ID:Windows>:NOMINMAX>
        $<$<PLATFORM_ID:Windows>:UNICODE>
        $<$<PLATFORM_ID:Windows>:_UNICODE>
        $<$<PLATFORM_ID:Linux>:VK_USE_PLATFORM_XCB_KHR>
        $<$<BOOL:${SANDHYBRID_ENABLE_VALIDATION}>:SANDHYBRID_ENABLE_VALIDATION=1>
        $<$<NOT:$<BOOL:${SANDHYBRID_ENABLE_VALIDATION}>>:SANDHYBRID_ENABLE_VALIDATION=0>
    )
    sandhybrid_configure_warnings(EpochSimEngine_VulkanRuntime)

    if(UNIX AND NOT APPLE)
        find_path(XCB_INCLUDE_DIR xcb/xcb.h REQUIRED)
        find_library(XCB_LIBRARY NAMES xcb REQUIRED)
        target_include_directories(EpochSimEngine_VulkanRuntime PRIVATE ${XCB_INCLUDE_DIR})
    endif()

    set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
    set(SHADER_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/shaders")
    file(MAKE_DIRECTORY "${SHADER_BINARY_DIR}")

    set(SANDHYBRID_SHADER_SOURCES
        reset.comp
        paint.comp
        sunlight.comp
        tiles.comp
        chunks.comp
        copy_cells.comp
        chemistry.comp
        chemistry_bees.comp
        chemistry_machinery.comp
        chemistry_ecology_donors.comp
        chemistry_controllers.comp
        chemistry_harvest_donors.comp
        chemistry_destinations.comp
        chemistry_phases.comp
        conservation_corrections.comp
        bee_move.comp
        rainfall.comp
        macro_move.comp
        structural_repair.comp
        move.comp
        actor.comp
        debug_stats.comp
        fullscreen.vert
        fullscreen.frag
    )

    set(SHADER_OPTIMIZATION -O)
    set(SANDHYBRID_SHADER_BINARIES)
    foreach(SHADER_FILE IN LISTS SANDHYBRID_SHADER_SOURCES)
        set(SHADER_INPUT "${SHADER_SOURCE_DIR}/${SHADER_FILE}")
        set(SHADER_DEFINITIONS)
        # One maintained rule body, compiled into disjoint immutable-source
        # owners so software Vulkan does not compile one oversized function.
        if(SHADER_FILE STREQUAL "chemistry_bees.comp")
            set(SHADER_INPUT "${SHADER_SOURCE_DIR}/chemistry.comp")
            set(SHADER_DEFINITIONS -DEPOCHSIM_CHEMISTRY_OWNER=1)
        elseif(SHADER_FILE STREQUAL "chemistry_machinery.comp")
            set(SHADER_INPUT "${SHADER_SOURCE_DIR}/chemistry.comp")
            set(SHADER_DEFINITIONS -DEPOCHSIM_CHEMISTRY_OWNER=2)
        elseif(SHADER_FILE STREQUAL "chemistry_destinations.comp")
            set(SHADER_INPUT "${SHADER_SOURCE_DIR}/chemistry.comp")
            set(SHADER_DEFINITIONS -DEPOCHSIM_CHEMISTRY_OWNER=3)
        elseif(SHADER_FILE STREQUAL "chemistry_phases.comp")
            set(SHADER_INPUT "${SHADER_SOURCE_DIR}/chemistry.comp")
            set(SHADER_DEFINITIONS -DEPOCHSIM_CHEMISTRY_OWNER=4)
        elseif(SHADER_FILE STREQUAL "chemistry_ecology_donors.comp")
            set(SHADER_INPUT "${SHADER_SOURCE_DIR}/chemistry.comp")
            set(SHADER_DEFINITIONS -DEPOCHSIM_CHEMISTRY_OWNER=5)
        elseif(SHADER_FILE STREQUAL "chemistry_controllers.comp")
            set(SHADER_INPUT "${SHADER_SOURCE_DIR}/chemistry.comp")
            set(SHADER_DEFINITIONS -DEPOCHSIM_CHEMISTRY_OWNER=6)
        elseif(SHADER_FILE STREQUAL "chemistry_harvest_donors.comp")
            set(SHADER_INPUT "${SHADER_SOURCE_DIR}/chemistry.comp")
            set(SHADER_DEFINITIONS -DEPOCHSIM_CHEMISTRY_OWNER=7)
        endif()
        set(SHADER_OUTPUT "${SHADER_BINARY_DIR}/${SHADER_FILE}.spv")
        add_custom_command(
            OUTPUT "${SHADER_OUTPUT}"
            COMMAND "${Vulkan_GLSLC_EXECUTABLE}"
                    --target-env=vulkan1.2
                    ${SHADER_OPTIMIZATION}
                    ${SHADER_DEFINITIONS}
                    -I "${SHADER_SOURCE_DIR}"
                    "${SHADER_INPUT}"
                    -o "${SHADER_OUTPUT}"
            DEPENDS "${SHADER_INPUT}" "${SHADER_SOURCE_DIR}/materials.glsl"
                    "${SHADER_SOURCE_DIR}/bee_swarm.glsl"
                    "${SHADER_SOURCE_DIR}/bee_swarm_chemistry_legacy.glsl"
                    "${SHADER_SOURCE_DIR}/beehive.glsl"
                    "${SHADER_SOURCE_DIR}/material_ids.glsl"
                    "${SHADER_SOURCE_DIR}/material_physics.glsl"
                    "${SHADER_SOURCE_DIR}/tiles.glsl"
                    "${SHADER_SOURCE_DIR}/chunks.glsl"
                    "${SHADER_SOURCE_DIR}/conservation.glsl"
                    "${SHADER_SOURCE_DIR}/rain_membership.glsl"
                    "${SHADER_SOURCE_DIR}/debug_stats.glsl"
                    "${SHADER_SOURCE_DIR}/debug_geometry.glsl"
                    "${SHADER_SOURCE_DIR}/paint_schedule.glsl"
                    "${SHADER_SOURCE_DIR}/actor.glsl"
                    "${SHADER_SOURCE_DIR}/terrain_generation.glsl"
                    "${SHADER_SOURCE_DIR}/frontier_layout.glsl"
                    "${SHADER_SOURCE_DIR}/queen_ventilation.glsl"
                    "${SHADER_SOURCE_DIR}/machine_controller_lattice.glsl"
                    "${SHADER_SOURCE_DIR}/machine_input_credit.glsl"
                    "${SHADER_SOURCE_DIR}/chemistry_ownership.glsl"
                    "${SHADER_SOURCE_DIR}/epochgui_font.glsl"
                    "${SHADER_SOURCE_DIR}/ui_text.glsl"
                    "${SHADER_SOURCE_DIR}/material_appearance.glsl"
                    "${SHADER_SOURCE_DIR}/material_visual_phase.glsl"
                    "${SHADER_SOURCE_DIR}/text_geometry.glsl"
            VERBATIM
            COMMENT "Compiling ${SHADER_FILE}"
        )
        list(APPEND SANDHYBRID_SHADER_BINARIES "${SHADER_OUTPUT}")
    endforeach()

    add_custom_target(sandhybrid_shaders ALL DEPENDS ${SANDHYBRID_SHADER_BINARIES})
    add_dependencies(EpochSimEngine_VulkanRuntime sandhybrid_shaders)
endif()

if(SANDHYBRID_BUILD_APP)
    set(SANDHYBRID_DEMO_SOURCES src/main.cpp src/app.cpp)
    if(WIN32)
        list(APPEND SANDHYBRID_DEMO_SOURCES src/window_win32.cpp)
    elseif(UNIX AND NOT APPLE)
        list(APPEND SANDHYBRID_DEMO_SOURCES src/window_xcb.cpp)
    else()
        message(FATAL_ERROR "SandHybrid currently supports Windows and Linux/XCB only.")
    endif()

    add_executable(SandHybrid_Demo ${SANDHYBRID_DEMO_SOURCES})
    add_executable(sandhybrid ALIAS SandHybrid_Demo)
    set_target_properties(SandHybrid_Demo PROPERTIES OUTPUT_NAME sandhybrid)
    target_link_libraries(SandHybrid_Demo PRIVATE EpochSimEngine::VulkanRuntime)
    target_compile_definitions(SandHybrid_Demo PRIVATE
        SANDHYBRID_VERSION_STRING="${PROJECT_VERSION}"
        $<$<PLATFORM_ID:Windows>:VK_USE_PLATFORM_WIN32_KHR>
        $<$<PLATFORM_ID:Linux>:VK_USE_PLATFORM_XCB_KHR>
    )
    sandhybrid_configure_warnings(SandHybrid_Demo)

    if(MSVC)
        target_link_libraries(SandHybrid_Demo PRIVATE user32 gdi32)
    else()
        target_include_directories(SandHybrid_Demo PRIVATE ${XCB_INCLUDE_DIR})
        target_link_libraries(SandHybrid_Demo PRIVATE ${XCB_LIBRARY} dl)
    endif()

    add_custom_target(sandhybrid_runtime_shaders ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:SandHybrid_Demo>/shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${SANDHYBRID_SHADER_BINARIES}
                "$<TARGET_FILE_DIR:SandHybrid_Demo>/shaders"
        DEPENDS sandhybrid_shaders
        VERBATIM
    )
    add_dependencies(SandHybrid_Demo sandhybrid_runtime_shaders)
    add_custom_command(TARGET SandHybrid_Demo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:SandHybrid_Demo>/scenes"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/scenes/README.txt"
                "$<TARGET_FILE_DIR:SandHybrid_Demo>/scenes/README.txt"
    )

    install(TARGETS SandHybrid_Demo RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES ${SANDHYBRID_SHADER_BINARIES} DESTINATION ${CMAKE_INSTALL_BINDIR}/shaders)
    install(FILES scenes/README.txt DESTINATION ${CMAKE_INSTALL_BINDIR}/scenes)
    if(WIN32)
        install(FILES run.bat run-compact.bat run-standard.bat run-large.bat DESTINATION .)
        # vcpkg app-local deployment populates the build tree, not install().
        # Package the matching loader explicitly instead of borrowing an old
        # demo install or accidentally shipping the Debug DLL in Release.
        if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
            set(EPOCHSIMENGINE_RELEASE_VULKAN_LOADER
                "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/vulkan-1.dll")
            set(EPOCHSIMENGINE_DEBUG_VULKAN_LOADER
                "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin/vulkan-1.dll")
            if(EXISTS "${EPOCHSIMENGINE_RELEASE_VULKAN_LOADER}")
                install(FILES "${EPOCHSIMENGINE_RELEASE_VULKAN_LOADER}"
                    DESTINATION ${CMAKE_INSTALL_BINDIR}
                    CONFIGURATIONS Release RelWithDebInfo MinSizeRel)
            endif()
            if(EXISTS "${EPOCHSIMENGINE_DEBUG_VULKAN_LOADER}")
                install(FILES "${EPOCHSIMENGINE_DEBUG_VULKAN_LOADER}"
                    DESTINATION ${CMAKE_INSTALL_BINDIR} CONFIGURATIONS Debug)
            endif()
        endif()
    else()
        install(PROGRAMS run-compact.sh run-standard.sh run-large.sh DESTINATION .)
    endif()
endif()

install(TARGETS EpochSimEngine
    EXPORT EpochSimEngineTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
set(SANDHYBRID_CORE_PUBLIC_HEADERS
    include/sandhybrid/actor_medium.hpp
    include/sandhybrid/atmosphere.hpp
    include/sandhybrid/blueprint.hpp
    include/sandhybrid/camera_policy.hpp
    include/sandhybrid/inventory.hpp
    include/sandhybrid/machinery.hpp
    include/sandhybrid/packet_transaction.hpp
    include/sandhybrid/input_routing.hpp
    include/sandhybrid/library.hpp
    include/sandhybrid/material.hpp
    include/sandhybrid/material_color.hpp
    include/sandhybrid/scene.hpp
    include/sandhybrid/scene_spawn.hpp
    include/sandhybrid/scene_image.hpp
    include/sandhybrid/hive_recovery.hpp
    include/sandhybrid/bee_colony.hpp
    include/sandhybrid/world_save.hpp
    include/sandhybrid/section_grid.hpp
    include/sandhybrid/section_scheduler.hpp
    include/sandhybrid/simulation_policy.hpp
    include/sandhybrid/terrain_generation.hpp
    include/sandhybrid/world_layout.hpp
)
install(FILES ${SANDHYBRID_CORE_PUBLIC_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/sandhybrid)
# Canonical public paths forward to the ABI-compatible implementation headers.
# Runtime-only headers are intentionally absent from both installed trees.
set(EPOCHSIMENGINE_CORE_PUBLIC_HEADERS include/epochsimengine/namespace.hpp)
foreach(legacy_header IN LISTS SANDHYBRID_CORE_PUBLIC_HEADERS)
    get_filename_component(header_name "${legacy_header}" NAME)
    list(APPEND EPOCHSIMENGINE_CORE_PUBLIC_HEADERS "include/epochsimengine/${header_name}")
endforeach()
install(FILES ${EPOCHSIMENGINE_CORE_PUBLIC_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/epochsimengine)

set(SANDHYBRID_CMAKE_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/SandHybrid")
configure_package_config_file(
    cmake/SandHybridConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/SandHybridConfig.cmake"
    INSTALL_DESTINATION "${SANDHYBRID_CMAKE_INSTALL_DIR}"
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/SandHybridConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
set(EPOCHSIMENGINE_CMAKE_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/EpochSimEngine")
configure_package_config_file(
    cmake/EpochSimEngineConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/EpochSimEngineConfig.cmake"
    INSTALL_DESTINATION "${EPOCHSIMENGINE_CMAKE_INSTALL_DIR}"
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/EpochSimEngineConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(EXPORT EpochSimEngineTargets
    FILE EpochSimEngineTargets.cmake
    NAMESPACE EpochSimEngine::
    DESTINATION "${EPOCHSIMENGINE_CMAKE_INSTALL_DIR}"
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/SandHybridConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/SandHybridConfigVersion.cmake"
    cmake/SandHybridTargets.cmake
    DESTINATION "${SANDHYBRID_CMAKE_INSTALL_DIR}"
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/EpochSimEngineConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/EpochSimEngineConfigVersion.cmake"
    cmake/EpochSimEngineCompatibility.cmake
    DESTINATION "${EPOCHSIMENGINE_CMAKE_INSTALL_DIR}"
)
install(FILES README.md LICENSE CHANGELOG.md DESTINATION .)
install(FILES docs/library.md docs/sandhybrid.md docs/development.md DESTINATION docs)
if(SANDHYBRID_BUILD_VULKAN_RUNTIME)
    install(FILES third_party/EpochGui/LICENSE
        DESTINATION docs/licenses RENAME EpochGui.txt)
    if(WIN32 AND DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
        set(SANDHYBRID_LOADER_NOTICE
            "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/vulkan-loader/copyright")
        if(EXISTS "${SANDHYBRID_LOADER_NOTICE}")
            install(FILES "${SANDHYBRID_LOADER_NOTICE}"
                DESTINATION docs/licenses RENAME VulkanLoader.txt)
        endif()
    endif()
endif()

if(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME epochsimengine_project_branding_contract
       COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_project_branding.py)
        add_test(NAME epochsimengine_designer_clear_label_contract
       COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_designer_clear_label.py)
        add_test(NAME epochsimengine_machinery_edits_contract
       COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_machinery_edits.py)
        if(SANDHYBRID_BUILD_VULKAN_RUNTIME)
            add_test(NAME epochsimengine_chemistry_partition_contract
                COMMAND ${Python3_EXECUTABLE} -B
                    ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_chemistry_partition.py
                    --glslc ${Vulkan_GLSLC_EXECUTABLE})
        endif()
        add_test(NAME sandhybrid_mission_cache_contract
       COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_mission_cache.py)
        add_test(NAME sandhybrid_mission_registry_contract
       COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_mission_cache.py)
        add_test(NAME sandhybrid_v2510_source_contract
       COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_v2510_contract.py)
        add_test(NAME sandhybrid_v2511_source_contract
       COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_v2511_contract.py)
        add_test(NAME sandhybrid_v2512_source_contract
       COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_v2512_contract.py)
        add_test(NAME sandhybrid_v2513_source_contract
       COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_v2513_contract.py)
        add_test(NAME sandhybrid_v2515_source_contract
       COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_v2515_contract.py)
        add_test(NAME sandhybrid_v2516_source_contract
       COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_v2516_contract.py)
        add_test(NAME sandhybrid_v2517_source_contract
       COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_v2517_contract.py)
        add_test(NAME sandhybrid_v2519_source_contract
       COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_v2519_contract.py)
        add_test(NAME sandhybrid_v2521_source_contract
       COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_v2521_contract.py)
        add_test(NAME sandhybrid_v2522_source_contract
       COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_v2522_contract.py)
        add_test(NAME sandhybrid_v2524_source_contract
       COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_v2524_contract.py)
        add_test(NAME sandhybrid_v2525_source_contract
       COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_v2525_contract.py)
        add_test(NAME sandhybrid_v2527_source_contract
       COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_v2527_contract.py)
    endif()

    add_executable(sandhybrid_input_routing_contract tests/input_routing_contract.cpp)
    add_executable(epochsimengine_paint_schedule_contract tests/paint_schedule_contract.cpp)
    target_compile_features(epochsimengine_paint_schedule_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(epochsimengine_paint_schedule_contract)
    add_test(NAME epochsimengine_paint_schedule_contract COMMAND epochsimengine_paint_schedule_contract)

    add_executable(epochsimengine_material_appearance_contract tests/material_appearance_contract.cpp)
    target_include_directories(epochsimengine_material_appearance_contract PRIVATE include shaders)
    target_compile_features(epochsimengine_material_appearance_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(epochsimengine_material_appearance_contract)
    add_test(NAME epochsimengine_material_appearance_contract COMMAND epochsimengine_material_appearance_contract)

    add_executable(epochsimengine_text_geometry_contract tests/text_geometry_contract.cpp)
    target_include_directories(epochsimengine_text_geometry_contract PRIVATE shaders include third_party/EpochGui/include)
    target_compile_features(epochsimengine_text_geometry_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(epochsimengine_text_geometry_contract)
    add_test(NAME epochsimengine_text_geometry_contract COMMAND epochsimengine_text_geometry_contract)

    add_executable(epochsimengine_frontier_layout_contract tests/frontier_layout_contract.cpp)
    target_include_directories(epochsimengine_frontier_layout_contract PRIVATE shaders)
    target_compile_features(epochsimengine_frontier_layout_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(epochsimengine_frontier_layout_contract)
    add_test(NAME epochsimengine_frontier_layout_contract COMMAND epochsimengine_frontier_layout_contract)

    add_executable(epochsimengine_queen_ventilation_contract tests/queen_ventilation_contract.cpp)
    target_include_directories(epochsimengine_queen_ventilation_contract PRIVATE shaders)
    target_compile_features(epochsimengine_queen_ventilation_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(epochsimengine_queen_ventilation_contract)
    add_test(NAME epochsimengine_queen_ventilation_contract COMMAND epochsimengine_queen_ventilation_contract)

    add_executable(epochsimengine_machine_controller_lattice_contract tests/machine_controller_lattice_contract.cpp)
    target_include_directories(epochsimengine_machine_controller_lattice_contract PRIVATE shaders)
    target_compile_features(epochsimengine_machine_controller_lattice_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(epochsimengine_machine_controller_lattice_contract)
    add_test(NAME epochsimengine_machine_controller_lattice_contract COMMAND epochsimengine_machine_controller_lattice_contract)

    add_executable(epochsimengine_machine_input_credit_contract tests/machine_input_credit_contract.cpp)
    target_include_directories(epochsimengine_machine_input_credit_contract PRIVATE shaders)
    target_compile_features(epochsimengine_machine_input_credit_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(epochsimengine_machine_input_credit_contract)
    add_test(NAME epochsimengine_machine_input_credit_contract COMMAND epochsimengine_machine_input_credit_contract)

    add_executable(epochsimengine_chemistry_ownership_contract tests/chemistry_ownership_contract.cpp)
    target_include_directories(epochsimengine_chemistry_ownership_contract PRIVATE shaders)
    target_compile_features(epochsimengine_chemistry_ownership_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(epochsimengine_chemistry_ownership_contract)
    add_test(NAME epochsimengine_chemistry_ownership_contract COMMAND epochsimengine_chemistry_ownership_contract)

    target_include_directories(sandhybrid_input_routing_contract PRIVATE include)
    target_compile_features(sandhybrid_input_routing_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_input_routing_contract)
    add_test(NAME sandhybrid_input_routing_contract COMMAND sandhybrid_input_routing_contract)

    add_executable(sandhybrid_debug_geometry_contract tests/debug_geometry_contract.cpp)
    target_include_directories(sandhybrid_debug_geometry_contract PRIVATE include shaders)
    target_compile_features(sandhybrid_debug_geometry_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_debug_geometry_contract)
    add_test(NAME sandhybrid_debug_geometry_contract COMMAND sandhybrid_debug_geometry_contract)

    add_executable(sandhybrid_material_contract tests/material_contract.cpp)
    target_include_directories(sandhybrid_material_contract PRIVATE include)
    target_compile_features(sandhybrid_material_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_material_contract)
    add_test(NAME sandhybrid_material_contract COMMAND sandhybrid_material_contract)

    add_executable(sandhybrid_material_color_contract tests/material_color_contract.cpp)
    target_link_libraries(sandhybrid_material_color_contract PRIVATE SandHybrid::SandHybrid)
    target_compile_features(sandhybrid_material_color_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_material_color_contract)
    add_test(NAME sandhybrid_material_color_contract COMMAND sandhybrid_material_color_contract)

    add_executable(sandhybrid_behavior_contract tests/behavior_contract.cpp)
    target_include_directories(sandhybrid_behavior_contract PRIVATE include)
    target_compile_features(sandhybrid_behavior_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_behavior_contract)
    add_test(NAME sandhybrid_behavior_contract COMMAND sandhybrid_behavior_contract)
    add_executable(sandhybrid_blueprint_contract tests/blueprint_contract.cpp)
    target_include_directories(sandhybrid_blueprint_contract PRIVATE include)
    target_compile_features(sandhybrid_blueprint_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_blueprint_contract)
    add_test(NAME sandhybrid_blueprint_contract COMMAND sandhybrid_blueprint_contract)

    add_executable(sandhybrid_scene_spawn_contract tests/scene_spawn_contract.cpp)
    target_link_libraries(sandhybrid_scene_spawn_contract PRIVATE SandHybrid::SandHybrid)
    target_compile_features(sandhybrid_scene_spawn_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_scene_spawn_contract)
    add_test(NAME sandhybrid_scene_spawn_contract COMMAND sandhybrid_scene_spawn_contract)

    add_executable(sandhybrid_scene_image_contract tests/scene_image_contract.cpp)
    target_link_libraries(sandhybrid_scene_image_contract PRIVATE SandHybrid)
    target_compile_features(sandhybrid_scene_image_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_scene_image_contract)
    add_test(NAME sandhybrid_scene_image_contract COMMAND sandhybrid_scene_image_contract)

    add_executable(sandhybrid_hive_recovery_contract tests/hive_recovery_contract.cpp)
    target_link_libraries(sandhybrid_hive_recovery_contract PRIVATE SandHybrid)
    target_compile_features(sandhybrid_hive_recovery_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_hive_recovery_contract)
    add_test(NAME sandhybrid_hive_recovery_contract COMMAND sandhybrid_hive_recovery_contract)

    add_executable(epochsimengine_bee_colony_contract tests/bee_colony_contract.cpp)
    target_link_libraries(epochsimengine_bee_colony_contract PRIVATE EpochSimEngine::EpochSimEngine)
    target_compile_features(epochsimengine_bee_colony_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(epochsimengine_bee_colony_contract)
    add_test(NAME epochsimengine_bee_colony_contract COMMAND epochsimengine_bee_colony_contract)

    add_executable(sandhybrid_bee_tile_search_contract tests/bee_tile_search_contract.cpp)
    target_compile_features(sandhybrid_bee_tile_search_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_bee_tile_search_contract)
    add_test(NAME sandhybrid_bee_tile_search_contract COMMAND sandhybrid_bee_tile_search_contract)

    add_executable(sandhybrid_chemistry_gate_contract tests/chemistry_gate_contract.cpp)
    target_compile_features(sandhybrid_chemistry_gate_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_chemistry_gate_contract)
    add_test(NAME sandhybrid_chemistry_gate_contract COMMAND sandhybrid_chemistry_gate_contract)

    add_executable(sandhybrid_rain_membership_contract tests/rain_membership_contract.cpp)
    target_compile_features(sandhybrid_rain_membership_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_rain_membership_contract)
    add_test(NAME sandhybrid_rain_membership_contract COMMAND sandhybrid_rain_membership_contract)

    add_executable(sandhybrid_world_save_contract tests/world_save_contract.cpp)
    target_link_libraries(sandhybrid_world_save_contract PRIVATE SandHybrid)
    target_compile_features(sandhybrid_world_save_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_world_save_contract)
    add_test(NAME sandhybrid_world_save_contract COMMAND sandhybrid_world_save_contract)

    add_executable(sandhybrid_section_scheduler_contract tests/section_scheduler_contract.cpp)
    target_link_libraries(sandhybrid_section_scheduler_contract PRIVATE SandHybrid)
    target_compile_features(sandhybrid_section_scheduler_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_section_scheduler_contract)
    add_test(NAME sandhybrid_section_scheduler_contract COMMAND sandhybrid_section_scheduler_contract)

    add_executable(sandhybrid_section_grid_contract tests/section_grid_contract.cpp)
    target_link_libraries(sandhybrid_section_grid_contract PRIVATE SandHybrid)
    target_compile_features(sandhybrid_section_grid_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_section_grid_contract)
    add_test(NAME sandhybrid_section_grid_contract COMMAND sandhybrid_section_grid_contract)

    add_executable(sandhybrid_public_api_contract tests/public_api_contract.cpp)
    target_link_libraries(sandhybrid_public_api_contract PRIVATE EpochSimEngine::EpochSimEngine)
    target_compile_features(sandhybrid_public_api_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_public_api_contract)
    add_test(NAME sandhybrid_public_api_contract COMMAND sandhybrid_public_api_contract)

    add_executable(sandhybrid_terrain_generation_contract tests/terrain_generation_contract.cpp)
    target_include_directories(sandhybrid_terrain_generation_contract PRIVATE include)
    target_compile_features(sandhybrid_terrain_generation_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_terrain_generation_contract)
    add_test(NAME sandhybrid_terrain_generation_contract COMMAND sandhybrid_terrain_generation_contract)

    add_executable(sandhybrid_world_layout_contract tests/world_layout_contract.cpp)
    target_link_libraries(sandhybrid_world_layout_contract PRIVATE SandHybrid::SandHybrid)
    target_compile_features(sandhybrid_world_layout_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_world_layout_contract)
    add_test(NAME sandhybrid_world_layout_contract COMMAND sandhybrid_world_layout_contract)


    add_executable(sandhybrid_atmosphere_contract tests/atmosphere_contract.cpp)
    target_link_libraries(sandhybrid_atmosphere_contract PRIVATE SandHybrid::SandHybrid)
    target_compile_features(sandhybrid_atmosphere_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_atmosphere_contract)
    add_test(NAME sandhybrid_atmosphere_contract COMMAND sandhybrid_atmosphere_contract)

    add_executable(sandhybrid_packet_transaction_contract tests/packet_transaction_contract.cpp)
    target_link_libraries(sandhybrid_packet_transaction_contract PRIVATE SandHybrid::SandHybrid)
    target_compile_features(sandhybrid_packet_transaction_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_packet_transaction_contract)
    add_test(NAME sandhybrid_packet_transaction_contract COMMAND sandhybrid_packet_transaction_contract)

    add_executable(sandhybrid_actor_medium_contract tests/actor_medium_contract.cpp)
    target_link_libraries(sandhybrid_actor_medium_contract PRIVATE SandHybrid::SandHybrid)
    target_compile_features(sandhybrid_actor_medium_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_actor_medium_contract)
    add_test(NAME sandhybrid_actor_medium_contract COMMAND sandhybrid_actor_medium_contract)

    add_executable(sandhybrid_machinery_contract tests/machinery_contract.cpp)
    target_link_libraries(sandhybrid_machinery_contract PRIVATE SandHybrid::SandHybrid)
    target_compile_features(sandhybrid_machinery_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_machinery_contract)
    add_test(NAME sandhybrid_machinery_contract COMMAND sandhybrid_machinery_contract)

    if(SANDHYBRID_BUILD_APP)
        # These argument contracts must return before native/Vulkan startup.
        # Use a wrapper to require the exact exit code and diagnostic rather
        # than treating an unrelated crash as a successful WILL_FAIL test.
        foreach(profile_cli_case IN ITEMS
                missing_path empty_equals flag_as_path existing_path existing_path_help
                runtime_separate runtime_equals
                long_cycle_separate long_cycle_equals
                interactive_separate interactive_equals
                separate_help equals_help
                stage_trace_standalone stage_trace_existing_path
                stage_trace_runtime stage_trace_long_cycle stage_trace_interactive
                stage_trace_runtime_profile stage_trace_long_cycle_profile
                stage_trace_interactive_profile stage_trace_help stage_trace_profile_help)
            add_test(NAME sandhybrid_profile_cli_${profile_cli_case}
                COMMAND ${CMAKE_COMMAND}
                    "-DSANDHYBRID_EXECUTABLE=$<TARGET_FILE:SandHybrid_Demo>"
                    "-DSANDHYBRID_CLI_CASE=${profile_cli_case}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_profile_cli.cmake")
            set_tests_properties(sandhybrid_profile_cli_${profile_cli_case}
                PROPERTIES TIMEOUT 10 LABELS "cli;no-gpu")
        endforeach()
    endif()

    if(SANDHYBRID_BUILD_VULKAN_RUNTIME)
        add_executable(sandhybrid_vulkan_barrier_contract tests/vulkan_barrier_contract.cpp)
        target_include_directories(sandhybrid_vulkan_barrier_contract PRIVATE src)
        target_link_libraries(sandhybrid_vulkan_barrier_contract PRIVATE Vulkan::Vulkan)
        target_compile_features(sandhybrid_vulkan_barrier_contract PRIVATE cxx_std_23)
        sandhybrid_configure_warnings(sandhybrid_vulkan_barrier_contract)
        add_test(NAME sandhybrid_vulkan_barrier_contract COMMAND sandhybrid_vulkan_barrier_contract)
        set_tests_properties(sandhybrid_vulkan_barrier_contract
            PROPERTIES TIMEOUT 10 LABELS "cpu;no-gpu")

        add_executable(sandhybrid_ui_layout_contract tests/ui_layout_contract.cpp)
        target_link_libraries(sandhybrid_ui_layout_contract PRIVATE Autodidac::EpochGui)
        target_include_directories(sandhybrid_ui_layout_contract PRIVATE include)
        target_compile_features(sandhybrid_ui_layout_contract PRIVATE cxx_std_23)
        sandhybrid_configure_warnings(sandhybrid_ui_layout_contract)
        add_test(NAME sandhybrid_ui_layout_contract COMMAND sandhybrid_ui_layout_contract)
    endif()

    add_test(NAME sandhybrid_downstream_package_contract
        COMMAND ${CMAKE_COMMAND}
            "-DSANDHYBRID_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DSANDHYBRID_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
            "-DSANDHYBRID_GENERATOR=${CMAKE_GENERATOR}"
            "-DSANDHYBRID_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
            "-DSANDHYBRID_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
            "-DSANDHYBRID_CONFIG=$<CONFIG>"
            "-DSANDHYBRID_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
            "-DSANDHYBRID_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_downstream_smoke.cmake"
    )
    set_tests_properties(sandhybrid_downstream_package_contract PROPERTIES RUN_SERIAL TRUE)
endif()
