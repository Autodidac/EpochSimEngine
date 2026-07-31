cmake_minimum_required(VERSION 3.28)

project(EpochSimEngine VERSION 2.4.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_TESTING "Build EpochSimEngine tests" OFF)
include(CTest)

set(EPOCH_SAND_VALIDATION_DEFAULT OFF)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(EPOCH_SAND_VALIDATION_DEFAULT ON)
endif()

option(EPOCH_SAND_BUILD_APP "Build the EpochSimEngine Vulkan demo" ON)
option(EPOCH_SAND_ENABLE_VALIDATION "Enable Vulkan validation layers" ${EPOCH_SAND_VALIDATION_DEFAULT})
option(EPOCH_SAND_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

function(epoch_sand_configure_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive- /Zc:preprocessor /EHsc /utf-8)
        if(EPOCH_SAND_WARNINGS_AS_ERRORS)
            target_compile_options(${target_name} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wno-missing-field-initializers)
        if(EPOCH_SAND_WARNINGS_AS_ERRORS)
            target_compile_options(${target_name} PRIVATE -Werror)
        endif()
    endif()
endfunction()

add_subdirectory(third_party/EpochGui EXCLUDE_FROM_ALL)

add_library(EpochSimEngine STATIC
    src/scene_image.cpp
    src/section_scheduler.cpp
)
add_library(Autodidac::EpochSimEngine ALIAS EpochSimEngine)
target_include_directories(EpochSimEngine PUBLIC include)
target_compile_features(EpochSimEngine PUBLIC cxx_std_23)
epoch_sand_configure_warnings(EpochSimEngine)

if(EPOCH_SAND_BUILD_APP)
    find_package(Vulkan REQUIRED)
    find_package(Threads REQUIRED)

    if(NOT Vulkan_GLSLC_EXECUTABLE)
        set(EPOCH_SAND_GLSLC_HINTS
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
                    list(APPEND EPOCH_SAND_GLSLC_HINTS
                        "${VCPKG_INSTALL_ROOT}/${VCPKG_TOOL_TRIPLET}/tools/shaderc")
                endif()
            endforeach()
        endforeach()

        find_program(Vulkan_GLSLC_EXECUTABLE
            NAMES glslc glslc.exe
            HINTS ${EPOCH_SAND_GLSLC_HINTS}
        )
    endif()

    if(NOT Vulkan_GLSLC_EXECUTABLE)
        message(FATAL_ERROR
            "glslc was not found. The vcpkg shaderc host tool should provide it; "
            "alternatively install the Vulkan SDK and put glslc on PATH.")
    endif()

    target_sources(EpochSimEngine PRIVATE src/vulkan_renderer.cpp)

    set(EPOCH_SAND_VULKAN_LIBRARY Vulkan::Vulkan)
    if(UNIX AND NOT APPLE)
        # vcpkg supplies headers/tools, while the distribution loader provides
        # the host-window-system entry points needed for XCB presentation.
        find_library(EPOCH_SAND_SYSTEM_VULKAN_LIBRARY
            NAMES vulkan
            PATHS /usr/lib/x86_64-linux-gnu /usr/lib64 /usr/lib
            NO_DEFAULT_PATH
            NO_CMAKE_FIND_ROOT_PATH
        )
        if(EPOCH_SAND_SYSTEM_VULKAN_LIBRARY)
            set(EPOCH_SAND_VULKAN_LIBRARY "${EPOCH_SAND_SYSTEM_VULKAN_LIBRARY}")
        endif()
    endif()

    target_include_directories(EpochSimEngine SYSTEM PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(EpochSimEngine PUBLIC
        ${EPOCH_SAND_VULKAN_LIBRARY} Threads::Threads Autodidac::EpochGui)
    target_compile_definitions(EpochSimEngine PRIVATE
        $<$<PLATFORM_ID:Windows>:VK_USE_PLATFORM_WIN32_KHR>
        $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
        $<$<PLATFORM_ID:Windows>:NOMINMAX>
        $<$<PLATFORM_ID:Windows>:UNICODE>
        $<$<PLATFORM_ID:Windows>:_UNICODE>
        $<$<PLATFORM_ID:Linux>:VK_USE_PLATFORM_XCB_KHR>
        $<$<BOOL:${EPOCH_SAND_ENABLE_VALIDATION}>:EPOCH_SAND_ENABLE_VALIDATION=1>
        $<$<NOT:$<BOOL:${EPOCH_SAND_ENABLE_VALIDATION}>>:EPOCH_SAND_ENABLE_VALIDATION=0>
    )

    set(EPOCH_SIM_DEMO_SOURCES src/main.cpp src/app.cpp)
    if(WIN32)
        list(APPEND EPOCH_SIM_DEMO_SOURCES src/window_win32.cpp)
    elseif(UNIX AND NOT APPLE)
        list(APPEND EPOCH_SIM_DEMO_SOURCES src/window_xcb.cpp)
    else()
        message(FATAL_ERROR "EpochSimEngine currently supports Windows and Linux/XCB only.")
    endif()

    add_executable(EpochSimEngine_Demo ${EPOCH_SIM_DEMO_SOURCES})
    add_executable(epoch_sand ALIAS EpochSimEngine_Demo)
    set_target_properties(EpochSimEngine_Demo PROPERTIES OUTPUT_NAME epoch_sand)
    target_include_directories(EpochSimEngine_Demo PRIVATE include)
    target_link_libraries(EpochSimEngine_Demo PRIVATE EpochSimEngine)
    target_compile_definitions(EpochSimEngine_Demo PRIVATE
        $<$<PLATFORM_ID:Windows>:VK_USE_PLATFORM_WIN32_KHR>
        $<$<PLATFORM_ID:Linux>:VK_USE_PLATFORM_XCB_KHR>
    )
    epoch_sand_configure_warnings(EpochSimEngine_Demo)

    if(MSVC)
        target_link_libraries(EpochSimEngine_Demo PRIVATE user32 gdi32)
    else()
        find_path(XCB_INCLUDE_DIR xcb/xcb.h REQUIRED)
        find_library(XCB_LIBRARY NAMES xcb REQUIRED)
        target_include_directories(EpochSimEngine PRIVATE ${XCB_INCLUDE_DIR})
        target_include_directories(EpochSimEngine_Demo PRIVATE ${XCB_INCLUDE_DIR})
        target_link_libraries(EpochSimEngine_Demo PRIVATE ${XCB_LIBRARY} dl)
    endif()

    set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
    set(SHADER_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/shaders")
    file(MAKE_DIRECTORY "${SHADER_BINARY_DIR}")

    set(EPOCH_SAND_SHADER_SOURCES
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

    set(EPOCH_SAND_SHADER_BINARIES)
    foreach(SHADER_FILE IN LISTS EPOCH_SAND_SHADER_SOURCES)
        set(SHADER_INPUT "${SHADER_SOURCE_DIR}/${SHADER_FILE}")
        set(SHADER_OUTPUT "${SHADER_BINARY_DIR}/generated/shaders/${SHADER_FILE}.spv")
        set(SHADER_OUTPUT "${SHADER_BINARY_DIR}/${SHADER_FILE}.spv")
        # Optimize every shader offline. Leaving move.comp unoptimized shifts
        # expensive legalization and dead-code removal into the Vulkan driver
        # during vkCreateComputePipelines, which caused the startup hang.
        set(SHADER_OPTIMIZATION -O)
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
                    "${SHADER_SOURCE_DIR}/material_ids.glsl"
                    "${SHADER_SOURCE_DIR}/material_physics.glsl"
                    "${SHADER_SOURCE_DIR}/tiles.glsl"
                    "${SHADER_SOURCE_DIR}/chunks.glsl"
                    "${SHADER_SOURCE_DIR}/conservation.glsl"
                    "${SHADER_SOURCE_DIR}/debug_stats.glsl"
                    "${SHADER_SOURCE_DIR}/actor.glsl" "${SHADER_SOURCE_DIR}/epochgui_font.glsl"
                    "${SHADER_SOURCE_DIR}/ui_text.glsl"
            VERBATIM
            COMMENT "Compiling ${SHADER_FILE}"
        )
        list(APPEND EPOCH_SAND_SHADER_BINARIES "${SHADER_OUTPUT}")
    endforeach()

    add_custom_target(epoch_sand_shaders ALL DEPENDS ${EPOCH_SAND_SHADER_BINARIES})
    add_dependencies(EpochSimEngine_Demo epoch_sand_shaders)

    add_custom_command(TARGET EpochSimEngine_Demo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:EpochSimEngine_Demo>/shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${EPOCH_SAND_SHADER_BINARIES}
                "$<TARGET_FILE_DIR:EpochSimEngine_Demo>/shaders"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:EpochSimEngine_Demo>/scenes"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/scenes/README.txt"
                "$<TARGET_FILE_DIR:EpochSimEngine_Demo>/scenes/README.txt"
    )

    install(TARGETS EpochSimEngine_Demo RUNTIME DESTINATION bin)
    install(FILES ${EPOCH_SAND_SHADER_BINARIES} DESTINATION bin/shaders)
    install(FILES scenes/README.txt DESTINATION bin/scenes)
endif()

install(TARGETS EpochSimEngine
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)

if(BUILD_TESTING)
    add_executable(epoch_sand_material_contract tests/material_contract.cpp)
    target_include_directories(epoch_sand_material_contract PRIVATE include)
    target_link_libraries(epoch_sand_material_contract PRIVATE Autodidac::EpochGui)
    target_compile_features(epoch_sand_material_contract PRIVATE cxx_std_23)
    epoch_sand_configure_warnings(epoch_sand_material_contract)
    add_test(NAME epoch_sand_material_contract COMMAND epoch_sand_material_contract)

    add_executable(epoch_sand_behavior_contract tests/behavior_contract.cpp)
    target_include_directories(epoch_sand_behavior_contract PRIVATE include)
    target_compile_features(epoch_sand_behavior_contract PRIVATE cxx_std_23)
    epoch_sand_configure_warnings(epoch_sand_behavior_contract)
    add_test(NAME epoch_sand_behavior_contract COMMAND epoch_sand_behavior_contract)

    add_executable(epoch_sand_scene_image_contract tests/scene_image_contract.cpp)
    target_link_libraries(epoch_sand_scene_image_contract PRIVATE EpochSimEngine)
    target_compile_features(epoch_sand_scene_image_contract PRIVATE cxx_std_23)
    epoch_sand_configure_warnings(epoch_sand_scene_image_contract)
    add_test(NAME epoch_sand_scene_image_contract COMMAND epoch_sand_scene_image_contract)

    add_executable(epoch_sand_section_scheduler_contract tests/section_scheduler_contract.cpp)
    target_link_libraries(epoch_sand_section_scheduler_contract PRIVATE EpochSimEngine)
    target_compile_features(epoch_sand_section_scheduler_contract PRIVATE cxx_std_23)
    epoch_sand_configure_warnings(epoch_sand_section_scheduler_contract)
    add_test(NAME epoch_sand_section_scheduler_contract COMMAND epoch_sand_section_scheduler_contract)
endif()
