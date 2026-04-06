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
    dr_libs_impl.cpp
    resources/resource.h
  RESOURCES
    resources/fonts/Roboto-Regular.ttf
  FORMATS
    MINIMAL_PLUGINS APP
)

# dr_libs headers live in RVRSE/libs/dr_libs
target_include_directories(_${PROJECT_NAME}-base INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/libs/dr_libs
)

# signalsmith-stretch: spectral time-stretcher (fetched in top-level CMakeLists)
target_link_libraries(_${PROJECT_NAME}-base INTERFACE signalsmith-stretch)