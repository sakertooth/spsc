cmake_minimum_required(VERSION 3.23)
project(spsc CXX)

include(CTest)
include(FetchContent)
enable_testing()

add_library(spsc INTERFACE)
target_include_directories(spsc INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/spsc)
target_compile_features(spsc INTERFACE cxx_std_20)

option(BUILD_TESTS "Build the test suite", ON)
option(BUILD_BENCHMARKS "Build the benchmarks" ON)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(BUILD_BENCHMAKRS)
    add_subdirectory(benchmarks)
endif()