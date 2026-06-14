cmake_minimum_required(VERSION 3.23)
project(spsc CXX)

include(CTest)
include(FetchContent)
enable_testing()

add_library(spsc INTERFACE)
target_include_directories(spsc INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/spsc)
target_compile_features(spsc INTERFACE cxx_std_20)

FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest
    GIT_TAG        v2.5.2
)

FetchContent_MakeAvailable(doctest)

add_executable(spsc_tests tests/tests.cpp)
target_link_libraries(spsc_tests PRIVATE spsc PRIVATE doctest::doctest)

add_test(NAME spsc_tests COMMAND spsc_tests)