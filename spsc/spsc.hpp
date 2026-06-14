#ifndef SPSC_H
#define SPSC_H

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <cstddef>
#include <span>

namespace spsc {
template <typename T, std::size_t N> class LockfreeSpscQueue {
  static_assert(N > 1, "Queue size must be greater than 1");

public:
  auto enqueue(T value) -> bool {
    const auto readIndex = m_readIndex.load(std::memory_order_acquire);
    const auto writeIndex = m_writeIndex.load(std::memory_order_relaxed);
    const auto nextWriteIndex = index(writeIndex + 1);

    if (nextWriteIndex == readIndex) {
      return false;
    }

    m_buffer[writeIndex] = std::move(value);
    m_writeIndex.store(nextWriteIndex, std::memory_order_release);
    return true;
  }

  auto dequeue(T &value) -> bool {
    const auto readIndex = m_readIndex.load(std::memory_order_relaxed);
    const auto writeIndex = m_writeIndex.load(std::memory_order_acquire);

    if (readIndex == writeIndex) {
      return false;
    }

    value = std::move(m_buffer[readIndex]);
    m_readIndex.store(index(readIndex + 1), std::memory_order_release);
    return true;
  }

  template <typename Fn>
  auto enqueueAll(Fn &&fn, std::size_t count) -> bool
    requires std::is_invocable_v<Fn, std::span<T>> &&
             std::is_convertible_v<std::invoke_result_t<Fn, std::span<T>>,
                                   std::size_t>
  {
    const auto readIndex = m_readIndex.load(std::memory_order_acquire);
    const auto writeIndex = m_writeIndex.load(std::memory_order_relaxed);
    const auto available = calculateFree(readIndex, writeIndex);

    if (available < count || count == 0) {
      return false;
    }

    auto nextWriteIndex = writeIndex;
    while (count > 0) {
      const auto spaceToEnd = calculateSpaceToEnd(readIndex, nextWriteIndex);
      const auto amountEnqueued = fn({&m_buffer[nextWriteIndex], spaceToEnd});
      nextWriteIndex = index(nextWriteIndex + amountEnqueued);
    }

    m_writeIndex.store(nextWriteIndex, std::memory_order_release);
    return true;
  }

  template <typename Fn>
  auto dequeueAll(Fn &&fn, std::size_t count) -> bool
    requires std::is_invocable_v<Fn, std::span<const T>> &&
             std::is_convertible_v<std::invoke_result_t<Fn, std::span<const T>>,
                                   std::size_t>
  {
    const auto readIndex = m_readIndex.load(std::memory_order_relaxed);
    const auto writeIndex = m_writeIndex.load(std::memory_order_acquire);
    const auto available = calculateSize(readIndex, writeIndex);

    if (available < count || count == 0) {
      return false;
    }

    auto nextReadIndex = readIndex;
    while (count > 0) {
      const auto sizeToEnd = calculateSizeToEnd(readIndex, nextReadIndex);
      const auto amountDequeued = fn({&m_buffer[nextReadIndex], sizeToEnd});
      nextReadIndex = index(nextReadIndex + amountDequeued);
    }

    m_readIndex.store(nextReadIndex, std::memory_order_release);
    return true;
  }

  template <typename Fn>
  auto enqueueSome(Fn &&fn, std::size_t count) -> std::size_t
    requires std::is_invocable_v<Fn, std::span<T>> &&
             std::is_convertible_v<std::invoke_result_t<Fn, std::span<T>>,
                                   std::size_t>
  {
    const auto readIndex = m_readIndex.load(std::memory_order_acquire);
    const auto writeIndex = m_writeIndex.load(std::memory_order_relaxed);

    auto available = calculateFree(readIndex, writeIndex);
    if (available == 0 || count == 0) {
      return 0;
    }

    auto nextWriteIndex = writeIndex;
    auto totalEnqueued = 0;
    while (available > 0 && count > 0) {
      const auto spaceToEnd = calculateSpaceToEnd(readIndex, nextWriteIndex);
      const auto amountEnqueued = fn({&m_buffer[nextWriteIndex], spaceToEnd});

      if (amountEnqueued == 0) {
        break;
      }

      nextWriteIndex = index(nextWriteIndex + amountEnqueued);
      available -= amountEnqueued;
      count -= amountEnqueued;
      totalEnqueued += amountEnqueued;
    }

    m_writeIndex.store(nextWriteIndex, std::memory_order_release);
    return totalEnqueued;
  }

  template <typename Fn>
  auto dequeueSome(Fn &&fn, std::size_t count) -> std::size_t
    requires std::is_invocable_v<Fn, std::span<const T>> &&
             std::is_convertible_v<std::invoke_result_t<Fn, std::span<const T>>,
                                   std::size_t>
  {
    const auto readIndex = m_readIndex.load(std::memory_order_relaxed);
    const auto writeIndex = m_writeIndex.load(std::memory_order_acquire);

    auto available = calculateSize(readIndex, writeIndex);
    if (available == 0 || count == 0) {
      return 0;
    }

    auto nextReadIndex = readIndex;
    auto totalDequeued = 0;
    while (available > 0 && count > 0) {
      const auto sizeToEnd = calculateSizeToEnd(nextReadIndex, writeIndex);
      const auto amountDequeued = fn({&m_buffer[nextReadIndex], sizeToEnd});

      if (amountDequeued == 0) {
        break;
      }

      nextReadIndex = index(nextReadIndex + amountDequeued);
      available -= amountDequeued;
      count -= amountDequeued;
      totalDequeued += amountDequeued;
    }

    return totalDequeued;
  }

  auto enqueueAll(std::span<const T> values) -> std::size_t {
    return enqueueAll(
        [&](std::span<T> dst) {
          assert(dst.size() >= values.size());

          std::copy(values.begin(), values.end(), dst.begin());
          values = values.subspan(dst.size());

          return dst.size();
        },
        values.size());
  }

  auto dequeueAll(std::span<T> values) -> bool {
    return dequeueAll(
        [&](std::span<const T> src) {
          assert(src.size() <= values.size());

          std::copy(src.begin(), src.end(), values.begin());
          values = values.subspan(src.size());

          return src.size();
        },
        values.size());
  }

  auto enqueueSome(std::span<const T> values) -> std::size_t {
    return enqueueSome(
        [&](std::span<T> dst) {
          assert(dst.size() >= values.size());

          std::copy(values.begin(), values.end(), dst.begin());
          values = values.subspan(dst.size());

          return dst.size();
        },
        values.size());
  }

  auto dequeueSome(std::span<T> values) -> std::size_t {
    return dequeueSome(
        [&](std::span<const T> src) {
          assert(src.size() <= values.size());

          std::copy(src.begin(), src.end(), values.begin());
          values = values.subspan(src.size());

          return src.size();
        },
        values.size());
  }

  auto peek() const -> const T & {
    const auto readIndex = m_readIndex.load(std::memory_order_relaxed);
    return m_buffer[readIndex];
  }

  auto empty() const -> bool {
    const auto readIndex = m_readIndex.load(std::memory_order_relaxed);
    const auto writeIndex = m_writeIndex.load(std::memory_order_acquire);
    return calculateEmpty(readIndex, writeIndex);
  }

  auto full() const -> bool {
    const auto readIndex = m_readIndex.load(std::memory_order_acquire);
    const auto writeIndex = m_writeIndex.load(std::memory_order_relaxed);
    return calculateFull(readIndex, writeIndex);
  }

  auto size() const -> std::size_t {
    const auto readIndex = m_readIndex.load(std::memory_order_acquire);
    const auto writeIndex = m_writeIndex.load(std::memory_order_acquire);
    return calculateSize(readIndex, writeIndex);
  }

  auto free() const -> std::size_t { return capacity() - size(); }

  constexpr auto capacity() const -> std::size_t { return N - 1; }

private:
  constexpr auto index(std::size_t index) const -> std::size_t {
    if constexpr (std::has_single_bit(N)) {
      return index & (N - 1);
    } else {
      return index % N;
    }
  }

  constexpr auto calculateSize(std::size_t readIndex,
                               std::size_t writeIndex) const {
    return index(writeIndex + N - readIndex);
  }

  constexpr auto calculateFree(std::size_t readIndex,
                               std::size_t writeIndex) const {
    return capacity() - calculateSize(readIndex, writeIndex);
  }

  constexpr auto calculateSizeToEnd(std::size_t readIndex,
                                    std::size_t writeIndex) const
      -> std::size_t {
    return std::min(calculateSize(readIndex, writeIndex), N - readIndex);
  }

  constexpr auto calculateSpaceToEnd(std::size_t readIndex,
                                     std::size_t writeIndex) const
      -> std::size_t {
    return std::min(calculateFree(readIndex, writeIndex),
                    N - writeIndex - (readIndex == 0 ? 1 : 0));
  }

  constexpr auto calculateEmpty(std::size_t readIndex,
                                std::size_t writeIndex) const -> bool {
    return readIndex == writeIndex;
  }

  constexpr auto calculateFull(std::size_t readIndex,
                               std::size_t writeIndex) const -> bool {
    return (writeIndex + 1) % N == readIndex;
  }

  std::array<T, N> m_buffer;
  alignas(std::hardware_destructive_interference_size) std::atomic_size_t
      m_readIndex = 0;
  alignas(std::hardware_destructive_interference_size) std::atomic_size_t
      m_writeIndex = 0;
};

} // namespace spsc

#endif // SPSC_H
