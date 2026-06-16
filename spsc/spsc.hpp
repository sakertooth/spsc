#ifndef SPSC_H
#define SPSC_H

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace spsc {

namespace {
constexpr auto DynamicQueueSize = static_cast<std::size_t>(-1);
}

template <typename T, std::size_t N = DynamicQueueSize>
class LockfreeSpscQueue {
  static_assert(N > 1, "Queue size must be greater than 1");

public:
  //! @brief Create a queue with a fixed capacity.
  LockfreeSpscQueue()
    requires(N != DynamicQueueSize)
  = default;

  //! @brief Create a queue with a capacity of @a capacity elements.
  explicit LockfreeSpscQueue(std::size_t capacity)
    requires(N == DynamicQueueSize)
      : m_buffer(capacity) {}

  /**
   * @brief Enqueues @a value into the queue.
   *
   * @param value
   * @returns true if @a value was sucessfully enqueued (enough space)
   * @returns false if @a value was not enqueued (queue was full)
   */
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

  /**
   * @brief Dequeues a value from the front of the queue.
   *
   * @returns the value at the front of the queue, or std::nullopt if the queue
   * was empty.
   */
  auto dequeue() -> std::optional<T> {
    const auto readIndex = m_readIndex.load(std::memory_order_relaxed);
    const auto writeIndex = m_writeIndex.load(std::memory_order_acquire);

    if (readIndex == writeIndex) {
      return std::nullopt;
    }

    auto value = std::move(m_buffer[readIndex]);
    m_readIndex.store(index(readIndex + 1), std::memory_order_release);
    return value;
  }

  /**
   * @brief Enqueues all @a count elements that were supplied by the callback @a
   fn. This function is all or nothing: It either enqueues all @a count
   elements, or it doesn't enqueue any of them. @a fn will continue to be
   executed untill all @a count slots have been filled in the queue.
   *
   * @param fn - the callback to supply elements. It is given a std::span<T>,
   specifying the region to enqueue elements in. It returns the number of
   elements enqueued by this invocation of @a fn.
   *
   * @param count - the total number of elements that are to be enqueued.
   * @return true - all @a count elements were enqueued successfully.
   * @return false - failed to enqueue all @a count elements (not enough space).
   */
  template <typename Fn>
  auto enqueueAll(Fn &&fn, std::size_t count) -> bool
    requires std::is_invocable_v<Fn, std::span<T>> &&
             std::is_convertible_v<std::invoke_result_t<Fn, std::span<T>>,
                                   std::size_t>
  {
    const auto readIndex = m_readIndex.load(std::memory_order_acquire);
    const auto writeIndex = m_writeIndex.load(std::memory_order_relaxed);
    const auto available = calculateSpace(readIndex, writeIndex);

    if (available < count || count == 0) {
      return false;
    }

    auto nextWriteIndex = writeIndex;
    while (count > 0) {
      const auto spaceToEnd = calculateSpaceToEnd(readIndex, nextWriteIndex);
      const auto amountEnqueued = fn({&m_buffer[nextWriteIndex], spaceToEnd});
      nextWriteIndex = index(nextWriteIndex + amountEnqueued);
      count -= amountEnqueued;
    }

    m_writeIndex.store(nextWriteIndex, std::memory_order_release);
    return true;
  }

  /**
   * @brief Dequeues all @a count elements that were supplied by the callback @a
   * fn. This function is all or nothing: It either dequeues all @a count
   * elements, or it doesn't dequeue any of them. @a fn will continue to be
   *  executed untill all @a count slots have been moved out of the queue.
   *
   * @param fn - the callback to transfer elements to. It is given a
   * std::span<const T>, specifying the region to dequeue elements from. It
   * returns the number of elements dequeued by this invocation of @a fn.
   *
   * @param count - the total number of elements that are to be dequeued.
   * @return true - all @a count elements were dequeued successfully.
   * @return false - failed to dequeue all @a count elements (not enough
   * elements).
   */
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
      const auto sizeToEnd = calculateSizeToEnd(nextReadIndex, writeIndex);
      const auto amountDequeued = fn({&m_buffer[nextReadIndex], sizeToEnd});
      nextReadIndex = index(nextReadIndex + amountDequeued);
      count -= amountDequeued;
    }

    m_readIndex.store(nextReadIndex, std::memory_order_release);
    return true;
  }

  /**
   * @brief Enqueues at most @a count elements into the queue that were supplied
   by the callback @a fn.

   * Less than @a count elements being enqueued is allowed here. @a fn will
   * continue to execute until either there is no more available space to
   * enqueue more elements, or if all @a counts were enqueued, whichever happens
   * first.
   *
   * @param fn - the callback to supply elements in. Takes in a `std::span<T>`,
   * specifying the region to enqueue elements in, and returns the number of
   * elements enqueued by this invocation of @a fn.
   *
   * @param count - the maximum number of elements to enqueue
   * @returns the number of elements enqueued
   */
  template <typename Fn>
  auto enqueueSome(Fn &&fn, std::size_t count) -> std::size_t
    requires std::is_invocable_v<Fn, std::span<T>> &&
             std::is_convertible_v<std::invoke_result_t<Fn, std::span<T>>,
                                   std::size_t>
  {
    const auto readIndex = m_readIndex.load(std::memory_order_acquire);
    const auto writeIndex = m_writeIndex.load(std::memory_order_relaxed);

    auto available = calculateSpace(readIndex, writeIndex);
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

  /**
   * @brief Dequeues at most @a count elements into the queue that were consumed
   * by the callback @a fn.

   * Less than @a count elements being dequeued is allowed here. @a fn will
   * continue to execute until either there are no more elements to
   * dequeue, or if @a count elements were dequeued, whichever happens
   * first.
   *
   * @param fn - Takes in a `std::span<const T>`, specifying the region to
   * consume elements from, and returns the number of elements dequeued by this
   * invocation of @a fn.
   *
   * @param count - The maximum number of elements to dequeue
   * @returns The number of elements dequeued
   */
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

    m_readIndex.store(nextReadIndex, std::memory_order_release);
    return totalDequeued;
  }

  //! Tries to enqueue all values in @a values. @see enqueueAll(Fn, std::size_t)
  auto enqueueAll(std::span<const T> values) -> std::size_t {
    return enqueueAll(
        [&](std::span<T> dst) {
          const auto amount = std::min(values.size(), dst.size());
          std::copy_n(values.begin(), amount, dst.begin());
          values = values.subspan(amount);
          return amount;
        },
        values.size());
  }

  //! Tries to dequeue all values in @a values. @see enqueueAll(Fn, std::size_t)
  auto dequeueAll(std::span<T> values) -> bool {
    return dequeueAll(
        [&](std::span<const T> src) {
          const auto amount = std::min(values.size(), src.size());
          std::copy_n(src.begin(), amount, values.begin());
          values = values.subspan(amount);
          return amount;
        },
        values.size());
  }

  //! Tries to enqueue at most `values.size()` values in @a values. @see
  //! enqueueSome(Fn, std::size_t)
  auto enqueueSome(std::span<const T> values) -> std::size_t {
    return enqueueSome(
        [&](std::span<T> dst) {
          const auto amount = std::min(values.size(), dst.size());
          std::copy_n(values.begin(), amount, dst.begin());
          values = values.subspan(amount);
          return amount;
        },
        values.size());
  }

  //! Tries to dequeue at most `values.size()` values in @a values. @see
  //! dequeueSome(Fn, std::size_t)
  auto dequeueSome(std::span<T> values) -> std::size_t {
    return dequeueSome(
        [&](std::span<const T> src) {
          const auto amount = std::min(values.size(), src.size());
          std::copy_n(src.begin(), amount, values.begin());
          values = values.subspan(amount);
          return amount;
        },
        values.size());
  }

  //! @returns a const reference to the element at the front of the queue.
  auto peek() const -> const T & {
    const auto readIndex = m_readIndex.load(std::memory_order_relaxed);
    return m_buffer[readIndex];
  }

  //! @returns whether or not the queue is empty
  auto empty() const -> bool {
    const auto readIndex = m_readIndex.load(std::memory_order_relaxed);
    const auto writeIndex = m_writeIndex.load(std::memory_order_acquire);
    return calculateEmpty(readIndex, writeIndex);
  }

  //! @returns true if the queue is full
  //! @returns false is the queue is not full
  auto full() const -> bool {
    const auto readIndex = m_readIndex.load(std::memory_order_acquire);
    const auto writeIndex = m_writeIndex.load(std::memory_order_relaxed);
    return calculateFull(readIndex, writeIndex);
  }

  //! @returns the number of elements in the queue
  auto size() const -> std::size_t {
    const auto readIndex = m_readIndex.load(std::memory_order_acquire);
    const auto writeIndex = m_writeIndex.load(std::memory_order_acquire);
    return calculateSize(readIndex, writeIndex);
  }

  //! @returns the number of free slots in the queue
  auto free() const -> std::size_t { return capacity() - size(); }

  //! @returns the maximum number of elements that can be stored in the queue
  //! @note this will be one less than the capacity specified upon construction
  //! of the queue to accomodate for an empty slot.
  constexpr auto capacity() const -> std::size_t {
    if constexpr (N == DynamicQueueSize) {
      return m_buffer.size() - 1;
    } else {
      return N - 1;
    }
  }

private:
  constexpr auto index(std::size_t index) const -> std::size_t {
    if constexpr (N == DynamicQueueSize) {
      if (std::has_single_bit(m_buffer.size())) {
        return index & (m_buffer.size() - 1);
      } else {
        return index % m_buffer.size();
      }
    } else {
      if constexpr (std::has_single_bit(N)) {
        return index & (N - 1);
      } else {
        return index % N;
      }
    }
  }

  constexpr auto calculateSize(std::size_t readIndex,
                               std::size_t writeIndex) const {
    if (readIndex <= writeIndex) {
      return writeIndex - readIndex;
    }

    if constexpr (N == DynamicQueueSize) {
      return index(writeIndex + m_buffer.size() - readIndex);
    } else {
      return index(writeIndex + N - readIndex);
    }
  }

  constexpr auto calculateSpace(std::size_t readIndex,
                                std::size_t writeIndex) const {
    return capacity() - calculateSize(readIndex, writeIndex);
  }

  constexpr auto calculateSizeToEnd(std::size_t readIndex,
                                    std::size_t writeIndex) const
      -> std::size_t {
    if (readIndex <= writeIndex) {
      return writeIndex - readIndex;
    }

    if constexpr (N == DynamicQueueSize) {
      return m_buffer.size() - readIndex;
    } else {
      return N - readIndex;
    }
  }

  constexpr auto calculateSpaceToEnd(std::size_t readIndex,
                                     std::size_t writeIndex) const
      -> std::size_t {
    if (writeIndex < readIndex) {
      return readIndex - writeIndex - 1;
    }

    if constexpr (N == DynamicQueueSize) {
      return m_buffer.size() - writeIndex - (readIndex == 0 ? 1 : 0);
    } else {
      return N - writeIndex - (readIndex == 0 ? 1 : 0);
    }
  }

  constexpr auto calculateEmpty(std::size_t readIndex,
                                std::size_t writeIndex) const -> bool {
    return readIndex == writeIndex;
  }

  constexpr auto calculateFull(std::size_t readIndex,
                               std::size_t writeIndex) const -> bool {
    return index(writeIndex + 1) == readIndex;
  }

  alignas(std::hardware_constructive_interference_size) std::conditional_t<
      N == DynamicQueueSize, std::vector<T>, std::array<T, N>> m_buffer;
  alignas(std::hardware_destructive_interference_size) std::atomic_size_t
      m_readIndex = 0;
  alignas(std::hardware_destructive_interference_size) std::atomic_size_t
      m_writeIndex = 0;
};

} // namespace spsc

#endif // SPSC_H
