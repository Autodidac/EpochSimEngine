if(NOT DEFINED SANDHYBRID_SOURCE_DIR OR NOT DEFINED SANDHYBRID_BINARY_DIR)
    message(FATAL_ERROR "EpochSimEngine source and binary directories are required.")
endif()

if(NOT DEFINED SANDHYBRID_CONFIG OR SANDHYBRID_CONFIG STREQUAL "")
    set(SANDHYBRID_CONFIG Release)
endif()

if(NOT IS_ABSOLUTE "${SANDHYBRID_BINARY_DIR}" OR
   NOT EXISTS "${SANDHYBRID_BINARY_DIR}/CMakeCache.txt")
    message(FATAL_ERROR "An existing absolute CMake build directory is required")
endif()
file(REAL_PATH "${SANDHYBRID_BINARY_DIR}" resolved_binary)
# Never recursively delete a computed prefix: prior fixtures may be in use or
# contain user data/reparse points. Each run owns a fresh, nonexisting child of
# the resolved build directory, retained for explicit audited cleanup afterward.
string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef fixture_id)
set(fixture_root "${resolved_binary}/epochsimengine-downstream-identity-${fixture_id}")
if(EXISTS "${fixture_root}" OR IS_SYMLINK "${fixture_root}")
    message(FATAL_ERROR "Downstream fixture path already exists; refusing to reuse it")
endif()
file(MAKE_DIRECTORY "${fixture_root}")
set(prefix "${fixture_root}/prefix")
set(consumer_root "${fixture_root}/consumers")
message(STATUS "Retained downstream fixture: ${fixture_root}")

if(NOT DEFINED SANDHYBRID_INSTALL_LIBDIR OR SANDHYBRID_INSTALL_LIBDIR STREQUAL "")
    set(SANDHYBRID_INSTALL_LIBDIR lib)
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${SANDHYBRID_BINARY_DIR}"
            --config "${SANDHYBRID_CONFIG}" --prefix "${prefix}"
    RESULT_VARIABLE install_result)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "Installing the EpochSimEngine package failed: ${install_result}")
endif()

foreach(runtime_header IN ITEMS app.hpp shared_state.hpp ui_layout.hpp ui_text_data.hpp
                                vulkan_renderer.hpp window.hpp)
    foreach(public_namespace IN ITEMS epochsimengine sandhybrid)
        if(EXISTS "${prefix}/include/${public_namespace}/${runtime_header}")
            message(FATAL_ERROR
                "Headless package leaked runtime-only header: ${public_namespace}/${runtime_header}")
        endif()
    endforeach()
endforeach()

# A different already-loaded legacy library must fail explicitly instead of
# allowing two archives with the same C++ symbols into a consumer process.
set(conflict_command "${CMAKE_COMMAND}"
    -S "${SANDHYBRID_SOURCE_DIR}/tests/downstream/target_conflict"
    -B "${consumer_root}/target-conflict"
    "-DCMAKE_PREFIX_PATH=${prefix}")
if(DEFINED SANDHYBRID_GENERATOR AND NOT SANDHYBRID_GENERATOR STREQUAL "")
    list(APPEND conflict_command -G "${SANDHYBRID_GENERATOR}")
endif()
execute_process(COMMAND ${conflict_command}
    RESULT_VARIABLE conflict_result OUTPUT_VARIABLE conflict_output ERROR_VARIABLE conflict_error)
string(CONCAT conflict_diagnostic "${conflict_output}" "${conflict_error}")
if(conflict_result EQUAL 0 OR NOT conflict_diagnostic MATCHES
   "EpochSimEngine compatibility target conflict: SandHybrid::SandHybrid")
    message(FATAL_ERROR "Expected the exact incompatible-library rejection: ${conflict_diagnostic}")
endif()
message(STATUS "Incompatible preloaded legacy target correctly rejected")
if(EXISTS "${prefix}/include/gui")
    message(FATAL_ERROR "Headless package leaked the EpochGui header tree")
endif()

if(NOT DEFINED SANDHYBRID_CTEST_COMMAND OR SANDHYBRID_CTEST_COMMAND STREQUAL "")
    get_filename_component(cmake_program_dir "${CMAKE_COMMAND}" DIRECTORY)
    find_program(SANDHYBRID_CTEST_COMMAND NAMES ctest
        PATHS "${cmake_program_dir}" NO_DEFAULT_PATH REQUIRED)
endif()

# Separate configure trees prove that neither package requires the other to
# have been loaded first, and that loading both is idempotent in either order.
foreach(package_order IN ITEMS canonical_only legacy_only canonical_first legacy_first legacy_targets)
set(consumer_binary "${consumer_root}/${package_order}")
set(configure_command
    "${CMAKE_COMMAND}"
    -S "${SANDHYBRID_SOURCE_DIR}/tests/downstream"
    -B "${consumer_binary}"
    "-DCMAKE_PREFIX_PATH=${prefix}"
    "-DEPOCHSIMENGINE_PACKAGE_ORDER=${package_order}"
    "-DEPOCHSIMENGINE_LEGACY_TARGETS_FILE=${prefix}/${SANDHYBRID_INSTALL_LIBDIR}/cmake/SandHybrid/SandHybridTargets.cmake"
    "-DCMAKE_BUILD_TYPE=${SANDHYBRID_CONFIG}")
if(DEFINED SANDHYBRID_GENERATOR AND NOT SANDHYBRID_GENERATOR STREQUAL "")
    list(APPEND configure_command -G "${SANDHYBRID_GENERATOR}")
endif()
if(DEFINED SANDHYBRID_GENERATOR_PLATFORM AND NOT SANDHYBRID_GENERATOR_PLATFORM STREQUAL "")
    list(APPEND configure_command -A "${SANDHYBRID_GENERATOR_PLATFORM}")
endif()
if(DEFINED SANDHYBRID_GENERATOR_TOOLSET AND NOT SANDHYBRID_GENERATOR_TOOLSET STREQUAL "")
    list(APPEND configure_command -T "${SANDHYBRID_GENERATOR_TOOLSET}")
endif()

execute_process(COMMAND ${configure_command} RESULT_VARIABLE configure_result)
if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR "Configuring the downstream EpochSimEngine consumer failed: ${configure_result}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${consumer_binary}"
            --config "${SANDHYBRID_CONFIG}" --parallel
    RESULT_VARIABLE build_result)
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR "Building the downstream EpochSimEngine consumer failed: ${build_result}")
endif()

execute_process(
    COMMAND "${SANDHYBRID_CTEST_COMMAND}" --test-dir "${consumer_binary}"
            -C "${SANDHYBRID_CONFIG}" --output-on-failure --no-tests=error
    RESULT_VARIABLE runtime_result)
if(NOT runtime_result EQUAL 0)
    message(FATAL_ERROR "Running the installed EpochSimEngine consumers failed: ${runtime_result}")
endif()
endforeach()
