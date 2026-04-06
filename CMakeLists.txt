cmake_minimum_required(VERSION 3.25)
project(RVRSE-OOS)

set(IPLUG2_DIR ${CMAKE_SOURCE_DIR}/iPlug2)
include(${IPLUG2_DIR}/iPlug2.cmake)

find_package(iPlug2 REQUIRED)

# signalsmith-stretch: MIT-licensed spectral time-stretcher (brings signalsmith-linear via FetchContent)
include(FetchContent)
FetchContent_Declare(
  signalsmith-stretch
  GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
  GIT_TAG        57b93f4e9206a089a45387eaa39bdc9f310d3308  # main @ 2026-04 (v1.3.2)
  GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(signalsmith-stretch)

add_subdirectory(RVRSE)

# Unit tests (Catch2) — opt-in via BUILD_TESTING (ON by default)
option(BUILD_TESTING "Build unit tests" ON)
if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()
