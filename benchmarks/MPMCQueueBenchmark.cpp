#include <benchmark/benchmark.h>
#include <glimpse/MPMCQueue.h>

static void BM_MPMCQueue_EnqueueDequeue(benchmark::State& state) {
    glimpse::MPMCQueue<int> queue(1024);

    for (auto _ : state) {
        queue.write(42);
        int val = queue.read();
        benchmark::DoNotOptimize(val);
    }
}
BENCHMARK(BM_MPMCQueue_EnqueueDequeue);

static void BM_MPMCQueue_Contention(benchmark::State& state) {
    static constexpr int N = 1024;
    glimpse::MPMCQueue<int> queue(N);

    std::atomic<bool> start_flag{false};
    std::atomic<bool> ready_flag{false};

    std::vector<std::thread> threads;
    int thread_id = 0;

    for (int i = 0; i < state.threads(); ++i) {
        threads.emplace_back([&, id = thread_id++] {
            ready_flag.store(true, std::memory_order_release);
            while (!start_flag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            for (auto _ : state) {
                queue.write(123);
                benchmark::DoNotOptimize(queue.read());
            }
        });
    }

    // Wait until all threads are ready
    while (!ready_flag.load(std::memory_order_acquire))
        std::this_thread::yield();

    start_flag.store(true, std::memory_order_release);
    for (auto& t : threads) t.join();
}
BENCHMARK(BM_MPMCQueue_Contention)
    ->Threads(2)->UseRealTime()
    ->Threads(4)->UseRealTime()
    ->Threads(8)->UseRealTime()
    ->Threads(16)->UseRealTime()
    ->Unit(benchmark::kNanosecond);

BENCHMARK_MAIN();