cmake_minimum_required(VERSION 3.28)

project(SandHybrid VERSION 2.4.7 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_TESTING "Build SandHybrid tests" OFF)
include(CTest)

set(SANDHYBRID_VALIDATION_DEFAULT OFF)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(SANDHYBRID_VALIDATION_DEFAULT ON)
endif()

option(SANDHYBRID_BUILD_APP "Build the SandHybrid Vulkan demo" ON)
option(SANDHYBRID_ENABLE_VALIDATION "Enable Vulkan validation layers" ${SANDHYBRID_VALIDATION_DEFAULT})
option(SANDHYBRID_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

function(sandhybrid_configure_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive- /Zc:preprocessor /EHsc /utf-8)
        if(SANDHYBRID_WARNINGS_AS_ERRORS)
            target_compile_options(${target_name} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wno-missing-field-initializers)
        if(SANDHYBRID_WARNINGS_AS_ERRORS)
            target_compile_options(${target_name} PRIVATE -Werror)
        endif()
    endif()
endfunction()

add_subdirectory(third_party/EpochGui EXCLUDE_FROM_ALL)

add_library(SandHybrid STATIC
    src/scene_image.cpp
    src/section_scheduler.cpp
)
add_library(Autodidac::SandHybrid ALIAS SandHybrid)
target_include_directories(SandHybrid PUBLIC include)
target_compile_features(SandHybrid PUBLIC cxx_std_23)
sandhybrid_configure_warnings(SandHybrid)

if(SANDHYBRID_BUILD_APP)
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

    target_sources(SandHybrid PRIVATE src/vulkan_renderer.cpp)

    set(SANDHYBRID_VULKAN_LIBRARY Vulkan::Vulkan)
    if(UNIX AND NOT APPLE)
        # vcpkg supplies headers/tools, while the distribution loader provides
        # the host-window-system entry points needed for XCB presentation.
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

    target_include_directories(SandHybrid SYSTEM PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(SandHybrid PUBLIC
        ${SANDHYBRID_VULKAN_LIBRARY} Threads::Threads Autodidac::EpochGui)
    target_compile_definitions(SandHybrid PRIVATE
        $<$<PLATFORM_ID:Windows>:VK_USE_PLATFORM_WIN32_KHR>
        $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
        $<$<PLATFORM_ID:Windows>:NOMINMAX>
        $<$<PLATFORM_ID:Windows>:UNICODE>
        $<$<PLATFORM_ID:Windows>:_UNICODE>
        $<$<PLATFORM_ID:Linux>:VK_USE_PLATFORM_XCB_KHR>
        $<$<BOOL:${SANDHYBRID_ENABLE_VALIDATION}>:SANDHYBRID_ENABLE_VALIDATION=1>
        $<$<NOT:$<BOOL:${SANDHYBRID_ENABLE_VALIDATION}>>:SANDHYBRID_ENABLE_VALIDATION=0>
    )

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
    target_include_directories(SandHybrid_Demo PRIVATE include)
    target_link_libraries(SandHybrid_Demo PRIVATE SandHybrid)
    target_compile_definitions(SandHybrid_Demo PRIVATE
        $<$<PLATFORM_ID:Windows>:VK_USE_PLATFORM_WIN32_KHR>
        $<$<PLATFORM_ID:Linux>:VK_USE_PLATFORM_XCB_KHR>
    )
    sandhybrid_configure_warnings(SandHybrid_Demo)

    if(MSVC)
        target_link_libraries(SandHybrid_Demo PRIVATE user32 gdi32)
    else()
        find_path(XCB_INCLUDE_DIR xcb/xcb.h REQUIRED)
        find_library(XCB_LIBRARY NAMES xcb REQUIRED)
        target_include_directories(SandHybrid PRIVATE ${XCB_INCLUDE_DIR})
        target_include_directories(SandHybrid_Demo PRIVATE ${XCB_INCLUDE_DIR})
        target_link_libraries(SandHybrid_Demo PRIVATE ${XCB_LIBRARY} dl)
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

    set(SANDHYBRID_SHADER_BINARIES)
    foreach(SHADER_FILE IN LISTS SANDHYBRID_SHADER_SOURCES)
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
        list(APPEND SANDHYBRID_SHADER_BINARIES "${SHADER_OUTPUT}")
    endforeach()

    add_custom_target(sandhybrid_shaders ALL DEPENDS ${SANDHYBRID_SHADER_BINARIES})
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

    install(TARGETS SandHybrid_Demo RUNTIME DESTINATION bin)
    install(FILES ${SANDHYBRID_SHADER_BINARIES} DESTINATION bin/shaders)
    install(FILES scenes/README.txt DESTINATION bin/scenes)
endif()

install(TARGETS SandHybrid
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)

if(BUILD_TESTING)
    add_executable(sandhybrid_material_contract tests/material_contract.cpp)
    target_include_directories(sandhybrid_material_contract PRIVATE include)
    target_link_libraries(sandhybrid_material_contract PRIVATE Autodidac::EpochGui)
    target_compile_features(sandhybrid_material_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_material_contract)
    add_test(NAME sandhybrid_material_contract COMMAND sandhybrid_material_contract)

    add_executable(sandhybrid_behavior_contract tests/behavior_contract.cpp)
    target_include_directories(sandhybrid_behavior_contract PRIVATE include)
    target_compile_features(sandhybrid_behavior_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_behavior_contract)
    add_test(NAME sandhybrid_behavior_contract COMMAND sandhybrid_behavior_contract)

    add_executable(sandhybrid_scene_image_contract tests/scene_image_contract.cpp)
    target_link_libraries(sandhybrid_scene_image_contract PRIVATE SandHybrid)
    target_compile_features(sandhybrid_scene_image_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_scene_image_contract)
    add_test(NAME sandhybrid_scene_image_contract COMMAND sandhybrid_scene_image_contract)

    add_executable(sandhybrid_section_scheduler_contract tests/section_scheduler_contract.cpp)
    target_link_libraries(sandhybrid_section_scheduler_contract PRIVATE SandHybrid)
    target_compile_features(sandhybrid_section_scheduler_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_section_scheduler_contract)
    add_test(NAME sandhybrid_section_scheduler_contract COMMAND sandhybrid_section_scheduler_contract)
endif()
