#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <glimpse/MPMCQueue.h>

using namespace glimpse;

TEST(MPMCQueueTest, SPSC_Basic) {
    MPMCQueue<int> queue(8);
    std::thread producer([&]() {
        for (int i = 0; i < 10; ++i) {
            queue.write(i);
        }
    });

    std::thread consumer([&]() {
        for (int i = 0; i < 10; ++i) {
            int val = queue.read();
            EXPECT_EQ(val, i);
        }
    });

    producer.join();
    consumer.join();
}

TEST(MPMCQueueTest, MPSC_MultipleProducers) {
    MPMCQueue<int> queue(64);
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};

    std::thread consumer([&]() {
        for (int i = 0; i < 100; ++i) {
            queue.read();
            consumed++;
        }
    });

    std::vector<std::thread> producers;
    for (int p = 0; p < 4; ++p) {
        producers.emplace_back([&]() {
            for (int i = 0; i < 25; ++i) {
                queue.write(i);
                produced++;
            }
        });
    }

    for (auto& t : producers) t.join();
    consumer.join();

    EXPECT_EQ(produced.load(), 100);
    EXPECT_EQ(consumed.load(), 100);
}

TEST(MPMCQueueTest, SPMC_MultipleConsumers) {
    MPMCQueue<int> queue(64);
    std::atomic<int> consumed{0};

    for (int i = 0; i < 60; ++i) {
        queue.write(i);
    }

    std::vector<std::thread> consumers;
    for (int c = 0; c < 4; ++c) {
        consumers.emplace_back([&]() {
            while (true) {
                int index = consumed.fetch_add(1, std::memory_order_relaxed);
                if (index >= 60) break;

                int val = queue.read(); 
                (void)val; 
            }
        });
    }

    for (auto& t : consumers) t.join();
    EXPECT_EQ(consumed.load(), 60 + 4);
}


TEST(MPMCQueueTest, MPMC_MultipleProducersConsumers) {
    MPMCQueue<int> queue(1024);
    constexpr int total_items = 1000;
    constexpr int num_producers = 4;
    constexpr int num_consumers = 4;

    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};

    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([&]() {
            while (true) {
                int i = produced.fetch_add(1);
                if (i >= total_items) break;
                queue.write(i);
            }
        });
    }

    std::vector<std::thread> consumers;
    for (int c = 0; c < num_consumers; ++c) {
        consumers.emplace_back([&]() {
            while (true) {
                int i = consumed.load(std::memory_order_relaxed);
                if (i >= total_items) break;
                int val = queue.read();
                consumed.fetch_add(1);
                (void)val;
            }
        });
    }

    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    EXPECT_EQ(consumed.load(), total_items);
}

TEST(MPMCQueueTest, WrapAroundCheck) {
    glimpse::MPMCQueue<int> queue(8);

    for (int i = 0; i < 10; ++i) {
        queue.write(i);
        int val = queue.read();
        EXPECT_EQ(val, i);
    }
}


