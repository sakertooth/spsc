set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark
    GIT_TAG        v1.9.5
)

FetchContent_MakeAvailable(benchmark)

function(add_benchmark name)
    add_executable(spsc_${name} ${CMAKE_SOURCE_DIR}/benchmarks/${name}.cpp)
    target_link_libraries(spsc_${name} PRIVATE spsc benchmark::benchmark)
endfunction()

add_benchmark(throughput_single)
add_benchmark(throughput_batch_all)
add_benchmark(throughput_batch_some)