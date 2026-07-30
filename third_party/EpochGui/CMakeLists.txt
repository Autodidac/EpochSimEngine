add_library(EpochGui INTERFACE)
add_library(Autodidac::EpochGui ALIAS EpochGui)
target_include_directories(EpochGui INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(EpochGui INTERFACE cxx_std_23)
