cmake_minimum_required(VERSION 3.25)
project(RVRSE-OOS)

set(IPLUG2_DIR ${CMAKE_SOURCE_DIR}/iPlug2)
include(${IPLUG2_DIR}/iPlug2.cmake)

find_package(iPlug2 REQUIRED)

add_subdirectory(RVRSE)

# Unit tests (Catch2) — opt-in via BUILD_TESTING (ON by default)
option(BUILD_TESTING "Build unit tests" ON)
if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()
