cmake_minimum_required(VERSION 3.25)

include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.7.1
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

# Test executable — links Catch2 and includes the RVRSE DSP headers directly.
# The DSP modules (BufferUtils, Reverb, TimeStretch, Stutter, Constants) are
# pure C++17 with no iPlug2 dependency, so they compile standalone.
add_executable(rvrse_tests
  test_smoke.cpp
)

target_compile_features(rvrse_tests PRIVATE cxx_std_17)

target_include_directories(rvrse_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/RVRSE           # Constants.h, BufferUtils.h, Reverb.h, etc.
  ${CMAKE_SOURCE_DIR}/RVRSE/libs/dr_libs  # dr_wav.h (for SampleLoader tests later)
)

target_link_libraries(rvrse_tests PRIVATE Catch2::Catch2WithMain)

# Register with CTest so `ctest` discovers and runs the tests
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(rvrse_tests)
