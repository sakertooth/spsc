FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest
    GIT_TAG        v2.5.2
)

FetchContent_MakeAvailable(doctest)

add_executable(spsc_tests ${CMAKE_SOURCE_DIR}/tests/tests.cpp)
target_link_libraries(spsc_tests PRIVATE spsc doctest::doctest)

add_test(NAME spsc_tests COMMAND spsc_tests)