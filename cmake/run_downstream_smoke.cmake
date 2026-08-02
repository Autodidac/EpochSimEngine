if(NOT DEFINED SANDHYBRID_SOURCE_DIR OR NOT DEFINED SANDHYBRID_BINARY_DIR)
    message(FATAL_ERROR "SandHybrid source and binary directories are required.")
endif()

if(NOT DEFINED SANDHYBRID_CONFIG OR SANDHYBRID_CONFIG STREQUAL "")
    set(SANDHYBRID_CONFIG Release)
endif()

set(prefix "${SANDHYBRID_BINARY_DIR}/downstream-prefix")
set(consumer_binary "${SANDHYBRID_BINARY_DIR}/downstream-consumer")
file(REMOVE_RECURSE "${prefix}" "${consumer_binary}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${SANDHYBRID_BINARY_DIR}"
            --config "${SANDHYBRID_CONFIG}" --prefix "${prefix}"
    RESULT_VARIABLE install_result)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "Installing the SandHybrid package failed: ${install_result}")
endif()

set(configure_command
    "${CMAKE_COMMAND}"
    -S "${SANDHYBRID_SOURCE_DIR}/tests/downstream"
    -B "${consumer_binary}"
    "-DCMAKE_PREFIX_PATH=${prefix}"
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
    message(FATAL_ERROR "Configuring the downstream SandHybrid consumer failed: ${configure_result}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${consumer_binary}"
            --config "${SANDHYBRID_CONFIG}" --parallel
    RESULT_VARIABLE build_result)
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR "Building the downstream SandHybrid consumer failed: ${build_result}")
endif()
