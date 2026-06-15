#include "spsc.hpp"
#include <benchmark/benchmark.h>
#include <emmintrin.h>

template <std::size_t N> struct SpscFixture : public benchmark::Fixture {
  inline static auto queue = std::shared_ptr<spsc::LockfreeSpscQueue<int, N>>{};
  inline static auto itemsProcessed = std::size_t{0};

  void SetUp(benchmark::State &state) override {
    if (state.thread_index() == 0) {
      queue = std::make_shared<spsc::LockfreeSpscQueue<int, N>>();
    }
  }

  void TearDown(benchmark::State &state) override {
    if (state.thread_index() == 0) {
      state.SetItemsProcessed(itemsProcessed);
      queue.reset();
    }
  }
};

template <std::size_t N>
static void BM_Throughput_Single(benchmark::State &state,
                                 SpscFixture<N> *fixture) {
  for (auto _ : state) {
    if (state.thread_index() == 0) {
      constexpr auto itemToEnqueue = 42;
      while (!fixture->queue->enqueue(itemToEnqueue)) {
        _mm_pause();
      }
    } else if (state.thread_index() == 1) {
      while (!fixture->queue->dequeue()) {
        _mm_pause();
      }

      ++fixture->itemsProcessed;
    }
  }
}

BENCHMARK_TEMPLATE_DEFINE_F(SpscFixture, BM_Throughput_Single_100,
                            100)(benchmark::State &state) {
  BM_Throughput_Single<100>(state, this);
}

BENCHMARK_TEMPLATE_DEFINE_F(SpscFixture, BM_Throughput_Single_128,
                            128)(benchmark::State &state) {
  BM_Throughput_Single<128>(state, this);
}

BENCHMARK_TEMPLATE_DEFINE_F(SpscFixture, BM_Throughput_Single_500,
                            500)(benchmark::State &state) {
  BM_Throughput_Single<500>(state, this);
}

BENCHMARK_TEMPLATE_DEFINE_F(SpscFixture, BM_Throughput_Single_512,
                            512)(benchmark::State &state) {
  BM_Throughput_Single<512>(state, this);
}

BENCHMARK_TEMPLATE_DEFINE_F(SpscFixture, BM_Throughput_Single_1000,
                            1000)(benchmark::State &state) {
  BM_Throughput_Single<1000>(state, this);
}

BENCHMARK_TEMPLATE_DEFINE_F(SpscFixture, BM_Throughput_Single_1024,
                            1024)(benchmark::State &state) {
  BM_Throughput_Single<1024>(state, this);
}

BENCHMARK_REGISTER_F(SpscFixture, BM_Throughput_Single_100)
    ->Threads(2)
    ->UseRealTime();

BENCHMARK_REGISTER_F(SpscFixture, BM_Throughput_Single_128)
    ->Threads(2)
    ->UseRealTime();

BENCHMARK_REGISTER_F(SpscFixture, BM_Throughput_Single_500)
    ->Threads(2)
    ->UseRealTime();

BENCHMARK_REGISTER_F(SpscFixture, BM_Throughput_Single_512)
    ->Threads(2)
    ->UseRealTime();

BENCHMARK_REGISTER_F(SpscFixture, BM_Throughput_Single_1000)
    ->Threads(2)
    ->UseRealTime();

BENCHMARK_REGISTER_F(SpscFixture, BM_Throughput_Single_1024)
    ->Threads(2)
    ->UseRealTime();

BENCHMARK_MAIN();
