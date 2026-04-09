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
# SampleLoader + dr_wav are also standalone (header-only codec, no iPlug2).
add_executable(rvrse_tests
  test_smoke.cpp
  test_constants.cpp
  test_buffer_utils.cpp
  test_reverb.cpp
  test_time_stretch.cpp
  test_stutter.cpp
  test_sample_loader.cpp
  ${CMAKE_SOURCE_DIR}/RVRSE/SampleLoader.cpp
  ${CMAKE_SOURCE_DIR}/RVRSE/dr_libs_impl.cpp
  ${CMAKE_SOURCE_DIR}/RVRSE/TimeStretch.cpp
)

target_compile_features(rvrse_tests PRIVATE cxx_std_17)

target_include_directories(rvrse_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/RVRSE           # Constants.h, BufferUtils.h, Reverb.h, etc.
  ${CMAKE_SOURCE_DIR}/RVRSE/libs/dr_libs  # dr_wav.h (for SampleLoader tests later)
)

target_link_libraries(rvrse_tests PRIVATE Catch2::Catch2WithMain signalsmith-stretch)

# Spectral stretcher FFT is orders of magnitude slower at -O0.
# Force -O2 for TimeStretch.cpp even in Debug test builds.
# MSVC Debug: skipped (see RVRSE/CMakeLists.txt for rationale).
if(NOT MSVC)
  set_source_files_properties(${CMAKE_SOURCE_DIR}/RVRSE/TimeStretch.cpp PROPERTIES COMPILE_OPTIONS
    "$<$<CONFIG:Debug>:-O2>"
  )
endif()

# Register with CTest so `ctest` discovers and runs the tests
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(rvrse_tests)
