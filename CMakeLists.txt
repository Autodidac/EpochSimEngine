cmake_minimum_required(VERSION 3.28)

project(SandHybrid VERSION 2.5.14 LANGUAGES CXX)

include(CTest)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SANDHYBRID_VALIDATION_DEFAULT OFF)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(SANDHYBRID_VALIDATION_DEFAULT ON)
endif()

option(SANDHYBRID_BUILD_APP "Build the native SandHybrid Vulkan demo" ON)
set(SANDHYBRID_RUNTIME_DEFAULT ${SANDHYBRID_BUILD_APP})
option(SANDHYBRID_BUILD_VULKAN_RUNTIME
       "Build the optional Vulkan simulation and presentation runtime"
       ${SANDHYBRID_RUNTIME_DEFAULT})
option(SANDHYBRID_ENABLE_VALIDATION "Enable Vulkan validation layers" ${SANDHYBRID_VALIDATION_DEFAULT})
option(SANDHYBRID_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

if(SANDHYBRID_BUILD_APP AND NOT SANDHYBRID_BUILD_VULKAN_RUNTIME)
    message(FATAL_ERROR
        "SANDHYBRID_BUILD_APP requires SANDHYBRID_BUILD_VULKAN_RUNTIME. "
        "Disable the app for a pure headless library build.")
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
add_library(SandHybrid STATIC
    src/scene_image.cpp
    src/world_save.cpp
    src/section_grid.cpp
    src/section_scheduler.cpp
)
add_library(SandHybrid::SandHybrid ALIAS SandHybrid)
add_library(Autodidac::SandHybrid ALIAS SandHybrid)
set_target_properties(SandHybrid PROPERTIES
    EXPORT_NAME SandHybrid
    POSITION_INDEPENDENT_CODE ON
)
target_include_directories(SandHybrid PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(SandHybrid PUBLIC cxx_std_23)
sandhybrid_configure_warnings(SandHybrid)

if(SANDHYBRID_BUILD_VULKAN_RUNTIME)
    add_subdirectory(third_party/EpochGui EXCLUDE_FROM_ALL)
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

    add_library(SandHybrid_VulkanRuntime STATIC src/vulkan_renderer.cpp)
    add_library(SandHybrid::VulkanRuntime ALIAS SandHybrid_VulkanRuntime)
    target_include_directories(SandHybrid_VulkanRuntime SYSTEM PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(SandHybrid_VulkanRuntime PUBLIC
        SandHybrid
        ${SANDHYBRID_VULKAN_LIBRARY}
        Threads::Threads
        Autodidac::EpochGui
    )
    target_compile_definitions(SandHybrid_VulkanRuntime PRIVATE
        $<$<PLATFORM_ID:Windows>:VK_USE_PLATFORM_WIN32_KHR>
        $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
        $<$<PLATFORM_ID:Windows>:NOMINMAX>
        $<$<PLATFORM_ID:Windows>:UNICODE>
        $<$<PLATFORM_ID:Windows>:_UNICODE>
        $<$<PLATFORM_ID:Linux>:VK_USE_PLATFORM_XCB_KHR>
        $<$<BOOL:${SANDHYBRID_ENABLE_VALIDATION}>:SANDHYBRID_ENABLE_VALIDATION=1>
        $<$<NOT:$<BOOL:${SANDHYBRID_ENABLE_VALIDATION}>>:SANDHYBRID_ENABLE_VALIDATION=0>
    )
    sandhybrid_configure_warnings(SandHybrid_VulkanRuntime)

    if(UNIX AND NOT APPLE)
        find_path(XCB_INCLUDE_DIR xcb/xcb.h REQUIRED)
        find_library(XCB_LIBRARY NAMES xcb REQUIRED)
        target_include_directories(SandHybrid_VulkanRuntime PRIVATE ${XCB_INCLUDE_DIR})
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
        chemistry.comp
        macro_move.comp
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
        set(SHADER_OUTPUT "${SHADER_BINARY_DIR}/${SHADER_FILE}.spv")
        add_custom_command(
            OUTPUT "${SHADER_OUTPUT}"
            COMMAND "${Vulkan_GLSLC_EXECUTABLE}"
                    --target-env=vulkan1.2
                    ${SHADER_OPTIMIZATION}
                    -I "${SHADER_SOURCE_DIR}"
                    "${SHADER_INPUT}"
                    -o "${SHADER_OUTPUT}"
            DEPENDS "${SHADER_INPUT}" "${SHADER_SOURCE_DIR}/materials.glsl"
                    "${SHADER_SOURCE_DIR}/bee_swarm.glsl"
                    "${SHADER_SOURCE_DIR}/beehive.glsl"
                    "${SHADER_SOURCE_DIR}/material_ids.glsl"
                    "${SHADER_SOURCE_DIR}/material_physics.glsl"
                    "${SHADER_SOURCE_DIR}/tiles.glsl"
                    "${SHADER_SOURCE_DIR}/chunks.glsl"
                    "${SHADER_SOURCE_DIR}/conservation.glsl"
                    "${SHADER_SOURCE_DIR}/debug_stats.glsl"
                    "${SHADER_SOURCE_DIR}/actor.glsl"
                    "${SHADER_SOURCE_DIR}/terrain_generation.glsl"
                    "${SHADER_SOURCE_DIR}/epochgui_font.glsl"
                    "${SHADER_SOURCE_DIR}/ui_text.glsl"
                    "${SHADER_SOURCE_DIR}/material_appearance.glsl"
            VERBATIM
            COMMENT "Compiling ${SHADER_FILE}"
        )
        list(APPEND SANDHYBRID_SHADER_BINARIES "${SHADER_OUTPUT}")
    endforeach()

    add_custom_target(sandhybrid_shaders ALL DEPENDS ${SANDHYBRID_SHADER_BINARIES})
    add_dependencies(SandHybrid_VulkanRuntime sandhybrid_shaders)
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
    target_link_libraries(SandHybrid_Demo PRIVATE SandHybrid_VulkanRuntime)
    target_compile_definitions(SandHybrid_Demo PRIVATE
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

    add_dependencies(SandHybrid_Demo sandhybrid_shaders)
    add_custom_command(TARGET SandHybrid_Demo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:SandHybrid_Demo>/shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${SANDHYBRID_SHADER_BINARIES}
                "$<TARGET_FILE_DIR:SandHybrid_Demo>/shaders"
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
    else()
        install(PROGRAMS run-compact.sh run-standard.sh run-large.sh DESTINATION .)
    endif()
endif()

install(TARGETS SandHybrid
    EXPORT SandHybridTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
set(SANDHYBRID_CORE_PUBLIC_HEADERS
    include/sandhybrid/actor_medium.hpp
    include/sandhybrid/atmosphere.hpp
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
    include/sandhybrid/world_save.hpp
    include/sandhybrid/section_grid.hpp
    include/sandhybrid/section_scheduler.hpp
    include/sandhybrid/simulation_policy.hpp
    include/sandhybrid/terrain_generation.hpp
    include/sandhybrid/world_layout.hpp
)
install(FILES ${SANDHYBRID_CORE_PUBLIC_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/sandhybrid)

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
install(EXPORT SandHybridTargets
    FILE SandHybridTargets.cmake
    NAMESPACE SandHybrid::
    DESTINATION "${SANDHYBRID_CMAKE_INSTALL_DIR}"
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/SandHybridConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/SandHybridConfigVersion.cmake"
    DESTINATION "${SANDHYBRID_CMAKE_INSTALL_DIR}"
)

if(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME sandhybrid_mission_cache_contract
       COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_mission_cache.py)
        add_test(NAME sandhybrid_v2510_source_contract
       COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_v2510_contract.py)
        add_test(NAME sandhybrid_v2511_source_contract
       COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_v2511_contract.py)
        add_test(NAME sandhybrid_v2512_source_contract
       COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_v2512_contract.py)
        add_test(NAME sandhybrid_v2513_source_contract
       COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_v2513_contract.py)
    endif()

    add_executable(sandhybrid_input_routing_contract tests/input_routing_contract.cpp)
    target_include_directories(sandhybrid_input_routing_contract PRIVATE include)
    target_compile_features(sandhybrid_input_routing_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_input_routing_contract)
    add_test(NAME sandhybrid_input_routing_contract COMMAND sandhybrid_input_routing_contract)

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
    target_link_libraries(sandhybrid_public_api_contract PRIVATE SandHybrid::SandHybrid)
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

    if(SANDHYBRID_BUILD_VULKAN_RUNTIME)
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
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_downstream_smoke.cmake"
    )
    set_tests_properties(sandhybrid_downstream_package_contract PROPERTIES RUN_SERIAL TRUE)
endif()
