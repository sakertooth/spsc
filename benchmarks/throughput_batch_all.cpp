#include "spsc.hpp"
#include <benchmark/benchmark.h>
#include <emmintrin.h>

template <std::size_t QueueSize, std::size_t BatchSize>
struct SpscFixture : public benchmark::Fixture {
  static_assert(QueueSize % BatchSize == 0,
                "Batch size must be a multiple of the queue size");

  inline static auto queue =
      std::shared_ptr<spsc::LockfreeSpscQueue<int, QueueSize>>{};
  inline static auto batch = std::array<int, BatchSize>{};
  inline static auto itemsProcessed = std::size_t{0};

  void SetUp(benchmark::State &state) override {
    if (state.thread_index() == 0) {
      queue = std::make_shared<spsc::LockfreeSpscQueue<int, QueueSize>>();
    }
  }

  void TearDown(benchmark::State &state) override {
    if (state.thread_index() == 0) {
      state.SetItemsProcessed(itemsProcessed);
      queue.reset();
      itemsProcessed = 0;
    }
  }
};

template <std::size_t QueueSize, std::size_t BatchSize>
static void
BM_Throughput_Batch_All(benchmark::State &state,
                        SpscFixture<QueueSize, BatchSize> *fixture) {
  for (auto _ : state) {
    if (state.thread_index() == 0) {
      while (!fixture->queue->enqueueAll(fixture->batch)) {
        _mm_pause();
      }
    } else if (state.thread_index() == 1) {
      while (!fixture->queue->dequeueAll(fixture->batch)) {
        _mm_pause();
      }

      fixture->itemsProcessed += BatchSize;
    }
  }
}

BENCHMARK_TEMPLATE_DEFINE_F(SpscFixture, BM_Throughput_Batch_All_128_64, 128,
                            64)(benchmark::State &state) {
  BM_Throughput_Batch_All<128, 64>(state, this);
}

BENCHMARK_TEMPLATE_DEFINE_F(SpscFixture, BM_Throughput_Batch_All_128_32, 128,
                            32)(benchmark::State &state) {
  BM_Throughput_Batch_All<128, 32>(state, this);
}

BENCHMARK_TEMPLATE_DEFINE_F(SpscFixture, BM_Throughput_Batch_All_128_16, 128,
                            16)(benchmark::State &state) {
  BM_Throughput_Batch_All<128, 16>(state, this);
}

BENCHMARK_TEMPLATE_DEFINE_F(SpscFixture, BM_Throughput_Batch_All_512_256, 512,
                            256)(benchmark::State &state) {
  BM_Throughput_Batch_All<512, 256>(state, this);
}

BENCHMARK_TEMPLATE_DEFINE_F(SpscFixture, BM_Throughput_Batch_All_512_128, 512,
                            128)(benchmark::State &state) {
  BM_Throughput_Batch_All<512, 128>(state, this);
}

BENCHMARK_TEMPLATE_DEFINE_F(SpscFixture, BM_Throughput_Batch_All_512_64, 512,
                            64)(benchmark::State &state) {
  BM_Throughput_Batch_All<512, 64>(state, this);
}

BENCHMARK_TEMPLATE_DEFINE_F(SpscFixture, BM_Throughput_Batch_All_1024_512, 1024,
                            512)(benchmark::State &state) {
  BM_Throughput_Batch_All<1024, 512>(state, this);
}

BENCHMARK_TEMPLATE_DEFINE_F(SpscFixture, BM_Throughput_Batch_All_1024_256, 1024,
                            256)(benchmark::State &state) {
  BM_Throughput_Batch_All<1024, 256>(state, this);
}

BENCHMARK_TEMPLATE_DEFINE_F(SpscFixture, BM_Throughput_Batch_All_1024_128, 1024,
                            128)(benchmark::State &state) {
  BM_Throughput_Batch_All<1024, 128>(state, this);
}

BENCHMARK_REGISTER_F(SpscFixture, BM_Throughput_Batch_All_128_64)
    ->Threads(2)
    ->UseRealTime();

BENCHMARK_REGISTER_F(SpscFixture, BM_Throughput_Batch_All_128_32)
    ->Threads(2)
    ->UseRealTime();

BENCHMARK_REGISTER_F(SpscFixture, BM_Throughput_Batch_All_128_16)
    ->Threads(2)
    ->UseRealTime();

BENCHMARK_REGISTER_F(SpscFixture, BM_Throughput_Batch_All_512_256)
    ->Threads(2)
    ->UseRealTime();

BENCHMARK_REGISTER_F(SpscFixture, BM_Throughput_Batch_All_512_128)
    ->Threads(2)
    ->UseRealTime();

BENCHMARK_REGISTER_F(SpscFixture, BM_Throughput_Batch_All_512_64)
    ->Threads(2)
    ->UseRealTime();

BENCHMARK_REGISTER_F(SpscFixture, BM_Throughput_Batch_All_1024_512)
    ->Threads(2)
    ->UseRealTime();

BENCHMARK_REGISTER_F(SpscFixture, BM_Throughput_Batch_All_1024_256)
    ->Threads(2)
    ->UseRealTime();

BENCHMARK_REGISTER_F(SpscFixture, BM_Throughput_Batch_All_1024_128)
    ->Threads(2)
    ->UseRealTime();

BENCHMARK_MAIN();
