cmake_minimum_required(VERSION 3.28)
project(EpochSimEngineIncompatibleLegacyTarget LANGUAGES NONE)

# Configure-only negative contract. Supply the fresh install via
# CMAKE_PREFIX_PATH; no compiler, executable, graphics dependency or build runs.
add_library(SandHybrid::SandHybrid INTERFACE IMPORTED)
find_package(EpochSimEngine CONFIG REQUIRED)
message(FATAL_ERROR "Expected the incompatible legacy target to be rejected")
