cmake_minimum_required(VERSION 3.23)
project(spsc CXX)

include(CTest)
include(FetchContent)
enable_testing()

add_library(spsc INTERFACE)
target_include_directories(spsc INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/spsc)
target_compile_features(spsc INTERFACE cxx_std_20)

# tests
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest
    GIT_TAG        v2.5.2
)

FetchContent_MakeAvailable(doctest)

add_executable(spsc_tests tests/tests.cpp)
target_link_libraries(spsc_tests PRIVATE spsc doctest::doctest)

add_test(NAME spsc_tests COMMAND spsc_tests)

# benchmarks
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark
    GIT_TAG        v1.9.5
)

FetchContent_MakeAvailable(benchmark)

add_executable(spsc_benchmarks benchmarks/benchmarks.cpp)
target_link_libraries(spsc_benchmarks PRIVATE spsc benchmark::benchmark)