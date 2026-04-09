cmake_minimum_required(VERSION 3.25)
project(RVRSE VERSION 1.0.0)

set(IPLUG2_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../iPlug2 CACHE PATH "iPlug2 root directory")
include(${IPLUG2_DIR}/iPlug2.cmake)

find_package(iPlug2 REQUIRED)

iplug_add_plugin(${PROJECT_NAME}
  SOURCES
    RVRSE.cpp
    RVRSE.h
    Constants.h
    SampleData.h
    SampleLoader.h
    SampleLoader.cpp
    WaveformControl.h
    TimeStretch.cpp
    dr_libs_impl.cpp
    resources/resource.h
  RESOURCES
    resources/fonts/Roboto-Regular.ttf
    resources/fonts/Roboto-Bold.ttf
    resources/img/logo.png
    resources/img/logo@2x.png
    resources/img/logo@3x.png
  FORMATS
    MINIMAL_PLUGINS APP
)

# Workaround: iPlug2's CMake only adds main.rc to the APP target on Windows.
# Plugin MODULE targets (VST3, CLAP) need it too for Win32 resource embedding
# (fonts, images). See rverse-1qa for the upstream fix.
if(WIN32)
  set(_rc_file "${CMAKE_CURRENT_SOURCE_DIR}/resources/main.rc")
  if(EXISTS "${_rc_file}")
    set(_rc_flags "/I\"${CMAKE_CURRENT_SOURCE_DIR}/resources/fonts\" /I\"${CMAKE_CURRENT_SOURCE_DIR}/resources/img\" /I\"${CMAKE_CURRENT_SOURCE_DIR}/resources\"")
    foreach(_fmt vst3 clap)
      if(TARGET ${PROJECT_NAME}-${_fmt})
        target_sources(${PROJECT_NAME}-${_fmt} PRIVATE "${_rc_file}")
        set_source_files_properties("${_rc_file}" TARGET_DIRECTORY ${PROJECT_NAME}-${_fmt}
          PROPERTIES COMPILE_FLAGS "${_rc_flags}")
      endif()
    endforeach()
  endif()
endif()

# Spectral stretcher FFT is orders of magnitude slower at -O0.
# Force -O2 for TimeStretch.cpp even in Debug builds.
# MSVC Debug: skipped — /O2 conflicts with /RTC1 (runtime checks) and there's no
# clean per-file way to remove it. Release builds already use /O2 on all compilers.
if(NOT MSVC)
  set_source_files_properties(TimeStretch.cpp PROPERTIES COMPILE_OPTIONS
    "$<$<CONFIG:Debug>:-O2>"
  )
endif()

# dr_libs headers live in RVRSE/libs/dr_libs
target_include_directories(_${PROJECT_NAME}-base INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/libs/dr_libs
)

# signalsmith-stretch: spectral time-stretcher (fetched in top-level CMakeLists)
target_link_libraries(_${PROJECT_NAME}-base INTERFACE signalsmith-stretch)