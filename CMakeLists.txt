cmake_minimum_required(VERSION 3.28)

project(FastFreddyWaterTestbed VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(FASTFREDDY_BUILD_APP "Build the SDL3 Vulkan water testbed" ON)
option(FASTFREDDY_BUILD_TESTS "Build deterministic water simulation tests" ON)
option(FASTFREDDY_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

function(fastfreddy_configure_target target_name)
    target_compile_features(${target_name} PUBLIC cxx_std_23)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive- /Zc:preprocessor /EHsc /utf-8)
        if(FASTFREDDY_WARNINGS_AS_ERRORS)
            target_compile_options(${target_name} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
        if(FASTFREDDY_WARNINGS_AS_ERRORS)
            target_compile_options(${target_name} PRIVATE -Werror)
        endif()
    endif()
endfunction()

add_library(fastfreddy_water_core STATIC
    src/simulation.cpp
)
target_include_directories(fastfreddy_water_core PUBLIC include)
fastfreddy_configure_target(fastfreddy_water_core)

if(FASTFREDDY_BUILD_APP)
    add_library(epochgui_snapshot STATIC
        third_party/EpochGuiSnapshot/src/epochgui_snapshot.cpp
    )
    target_include_directories(epochgui_snapshot PUBLIC
        third_party/EpochGuiSnapshot/include
    )
    fastfreddy_configure_target(epochgui_snapshot)
    add_library(Autodidac::EpochGuiSnapshot ALIAS epochgui_snapshot)

    find_package(SDL3 CONFIG REQUIRED)
    find_package(Vulkan REQUIRED)

    add_executable(fastfreddy_testbed
        src/main.cpp
        src/canvas.cpp
        src/ui_bridge.cpp
    )
    target_include_directories(fastfreddy_testbed PRIVATE include)
    target_link_libraries(fastfreddy_testbed PRIVATE
        fastfreddy_water_core
        Autodidac::EpochGuiSnapshot
        SDL3::SDL3
        Vulkan::Vulkan
    )
    fastfreddy_configure_target(fastfreddy_testbed)

    if(WIN32)
        set_target_properties(fastfreddy_testbed PROPERTIES
            WIN32_EXECUTABLE FALSE
            VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:fastfreddy_testbed>"
        )
    endif()

    install(TARGETS fastfreddy_testbed RUNTIME DESTINATION bin)
endif()

if(FASTFREDDY_BUILD_TESTS)
    enable_testing()
    add_executable(fastfreddy_simulation_tests tests/simulation_tests.cpp)
    target_link_libraries(fastfreddy_simulation_tests PRIVATE fastfreddy_water_core)
    fastfreddy_configure_target(fastfreddy_simulation_tests)
    add_test(NAME FastFreddy.WaterContracts COMMAND fastfreddy_simulation_tests)
endif()
