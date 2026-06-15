#include "spsc.hpp"
#include <benchmark/benchmark.h>
#include <emmintrin.h>

template <std::size_t N>
static void BM_Throughput_Single(benchmark::State &state) {
  static auto queue = std::unique_ptr<spsc::LockfreeSpscQueue<int, N>>{};
  static auto itemsProcessed = 0;
  const auto itemToEnqueue = 42;

  if (state.thread_index() == 0) {
    queue = std::make_unique<spsc::LockfreeSpscQueue<int, N>>();
  }

  for (auto _ : state) {
    if (state.thread_index() == 0) {
      while (!queue->enqueue(itemToEnqueue)) {
        _mm_pause();
      }
    }

    if (state.thread_index() == 1) {
      while (!queue->dequeue()) {
        _mm_pause();
      }

      ++itemsProcessed;
    }
  }

  if (state.thread_index() == 0) {
    queue.reset();
    state.SetItemsProcessed(itemsProcessed);
  }
}

BENCHMARK(BM_Throughput_Single<128>)->Threads(2)->UseRealTime();
BENCHMARK(BM_Throughput_Single<512>)->Threads(2)->UseRealTime();
BENCHMARK(BM_Throughput_Single<1024>)->Threads(2)->UseRealTime();

BENCHMARK(BM_Throughput_Single<100>)->Threads(2)->UseRealTime();
BENCHMARK(BM_Throughput_Single<500>)->Threads(2)->UseRealTime();
BENCHMARK(BM_Throughput_Single<1000>)->Threads(2)->UseRealTime();

BENCHMARK_MAIN();
