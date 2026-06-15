#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "spsc.hpp"

TEST_CASE("Correct initial state on construction")
{
    constexpr auto capacity = 32;
    auto queue = spsc::LockfreeSpscQueue<int, capacity>{};

    CHECK(queue.size() == 0);
    CHECK(queue.empty());
    CHECK(!queue.full());
    CHECK(queue.capacity() == capacity - 1);
}

TEST_CASE("Enqueue operations")
{
    SUBCASE("Enqueuing single element")
    {
        constexpr auto element = 7;
        auto queue = spsc::LockfreeSpscQueue<int, 32>{};

        CHECK(queue.enqueue(element));
        CHECK(queue.size() == 1);
        CHECK(queue.peek() == element);
    }

    SUBCASE("Enqueuing single element (not enough space)")
    {
        auto queue = spsc::LockfreeSpscQueue<int, 3>{};

        CHECK(queue.enqueue(1));
        CHECK(queue.enqueue(2));
        CHECK(!queue.enqueue(3));
    }

    SUBCASE("Enqueuing all items in batch")
    {
        constexpr auto elements = std::array{1, 2, 3, 4, 5, 6};
        auto queue = spsc::LockfreeSpscQueue<int, 32>{};

        CHECK(queue.enqueueAll(elements));
        CHECK(queue.size() == elements.size());
        CHECK(queue.peek() == elements.front());
    }

    SUBCASE("Enqueuing all items in batch (not enough space)")
    {
        constexpr auto elements = std::array{1, 2, 3, 4, 5, 6};
        auto queue = spsc::LockfreeSpscQueue<int, 3>{};

        CHECK(!queue.enqueueAll(elements));
        CHECK(queue.size() == 0);
    }

    SUBCASE("Enqueuing some items in batch")
    {
        constexpr auto elements = std::array{1, 2, 3, 4, 5, 6};
        auto queue = spsc::LockfreeSpscQueue<int, 32>{};

        CHECK(queue.enqueueSome(elements) == elements.size());
        CHECK(queue.size() == elements.size());
        CHECK(queue.peek() == elements.front());
    }


    SUBCASE("Enqueuing some items in batch (not enough space)")
    {
        constexpr auto elements = std::array{1, 2, 3, 4, 5, 6};
        auto queue = spsc::LockfreeSpscQueue<int, 3>{};

        CHECK(queue.enqueueSome(elements) == queue.capacity());
        CHECK(queue.size() == queue.capacity());
        CHECK(queue.peek() == elements.front());
    }
}

TEST_CASE("Dequeue operations")
{
    SUBCASE("Dequeuing single element")
    {
        auto queue = spsc::LockfreeSpscQueue<int, 32>{};
        auto value = 0;

        queue.enqueue(1);

        CHECK(queue.dequeue(value));
        CHECK(value == 1);
    }

    SUBCASE("Dequeuing single element when queue is empty")
    {
        auto queue = spsc::LockfreeSpscQueue<int, 32>{};
        auto value = 0;

        CHECK(!queue.dequeue(value));
        CHECK(value == 0);
    }

    SUBCASE("Dequeuing all batch of elements requested")
    {
        auto queue = spsc::LockfreeSpscQueue<int, 3>{};
        auto values = std::array<int, 2>{};

        queue.enqueueAll(std::array{1, 2});

        CHECK(queue.dequeueAll(values));
        CHECK(values[0] == 1);
        CHECK(values[1] == 2);
    }

    SUBCASE("Dequeuing all batch of elements (not enough elements)")
    {
        auto queue = spsc::LockfreeSpscQueue<int, 4>{};
        auto values = std::array<int, 4>{};

        queue.enqueueAll(std::array{1, 2});

        CHECK(!queue.dequeueAll(values));
        CHECK(values[0] == 0);
        CHECK(values[1] == 0);
        CHECK(values[2] == 0);
        CHECK(values[3] == 0);
    }

    SUBCASE("Dequeuing some batch of elements")
    {
        auto queue = spsc::LockfreeSpscQueue<int, 4>{};
        auto values = std::array<int, 3>{};

        queue.enqueueAll(std::array{1, 2, 3});

        CHECK(queue.dequeueSome(values) == values.size());
        CHECK(values[0] == 1);
        CHECK(values[1] == 2);
        CHECK(values[2] == 3);
    }

    SUBCASE("Dequeuing some batch of elements (not enough elements)")
    {
        auto queue = spsc::LockfreeSpscQueue<int, 4>{};
        auto values = std::array<int, 4>{};

        queue.enqueueAll(std::array{1, 2});

        CHECK(queue.dequeueSome(values) == 2);
        CHECK(values[0] == 1);
        CHECK(values[1] == 2);
        CHECK(values[2] == 0);
        CHECK(values[3] == 0);
    }
}

TEST_CASE("Enqueue + Dequeue operations")
{
    SUBCASE("Enqueue and dequeue, no wrap-around")
    {
        auto queue = spsc::LockfreeSpscQueue<int, 3>{};
        auto value = 0;

        CHECK(queue.enqueue(1));
        CHECK(queue.enqueue(2));

        CHECK(queue.dequeue(value));
        CHECK(value == 1);

        CHECK(queue.dequeue(value));
        CHECK(value == 2);
    }

    SUBCASE("Enqueue and dequeue, wrap around")
    {
        auto queue = spsc::LockfreeSpscQueue<int, 3>{};
        auto value = 0;

        CHECK(queue.enqueue(1));
        CHECK(queue.enqueue(2));

        CHECK(queue.dequeue(value));
        CHECK(value == 1);

        CHECK(queue.enqueue(3));

        CHECK(queue.dequeue(value));
        CHECK(value == 2);

        CHECK(queue.dequeue(value));
        CHECK(value == 3);
    }
}