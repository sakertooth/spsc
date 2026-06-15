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

        CHECK(queue.enqueueSome(elements));
        CHECK(queue.size() == elements.size());
        CHECK(queue.peek() == elements.front());
    }


    SUBCASE("Enqueuing some items in batch (not enough space)")
    {
        constexpr auto elements = std::array{1, 2, 3, 4, 5, 6};
        auto queue = spsc::LockfreeSpscQueue<int, 3>{};

        CHECK(queue.enqueueSome(elements));
        CHECK(queue.size() == queue.capacity());
        CHECK(queue.peek() == elements.front());
    }
}