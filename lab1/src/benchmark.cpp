#include "benchmark.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <functional>
#include <latch>
#include <thread>

struct RunResult {
    uint64_t operations = 0;
    double seconds = 0;
};

void record_values(
    MetricsCollector& collector,
    const std::vector<uint64_t>& values,
    size_t worker_index,
    std::latch& ready,
    std::latch& start,
    const std::atomic<bool>& stop,
    uint64_t& operations
) {
    uint64_t local_count = 0; // строго локальная переменная!
    size_t i = (worker_index * 1000) % values.size(); // разносим точки старта по массиву значений

    ready.count_down();
    start.wait(); // ждём команды "старт"

    while (!stop.load()) {
        collector.record(values[i]); // <-- ВЕСЬ ЗАМЕР ЗДЕСЬ
        local_count++;
        i++;

        if (i == values.size()) {
            i = 0;
        }
    }

    operations = local_count; // сохраняем итог только при выходе
}

// Один забег: thread_count потоков крутят цикл до сигнала остановки через seconds секунд.
RunResult run(
    MetricsCollector& collector,
    const std::vector<uint64_t>& values,
    unsigned int thread_count,
    unsigned int seconds
) {
    std::latch ready(thread_count);
    std::latch start(1); // общий стартовый выстрел
    std::atomic<bool> stop{false}; // флаг остановки

    std::vector<uint64_t> operations(thread_count); // результаты потоков
    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    for (size_t k = 0; k < thread_count; k++) {
        threads.push_back(std::thread(
            record_values,
            std::ref(collector),
            std::cref(values),
            k,
            std::ref(ready),
            std::ref(start),
            std::cref(stop),
            std::ref(operations[k])
        ));
    }

    ready.wait(); // ждём готовности всех потоков перед началом замера

    const auto begin = std::chrono::steady_clock::now();
    start.count_down(); // погнали!
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
    stop.store(true, std::memory_order_relaxed); // стоп!

    // TODO: а это точно верно? Не после ли join()-ов? (взял из README).
    const auto end = std::chrono::steady_clock::now();

    // Дожидаемся завершения всех потоков.
    for (std::thread& thread : threads) {
        thread.join();
    }

    double result_seconds = std::chrono::duration<double>(end - begin).count();

    uint64_t result_operations = 0;
    for (uint64_t count : operations) {
        result_operations += count;
    }

    return RunResult{
        .operations=result_operations,
        .seconds=result_seconds,
    };
}

BenchmarkResult run_benchmark(
    MetricsCollector& collector,
    const std::vector<uint64_t>& values,
    const BenchmarkOptions& options
) {
    assert(!values.empty());
    assert(options.threads > 0 && options.warmup > 0 && options.seconds > 0 && options.repeats > 0);

    BenchmarkResult result;
    // ПРОГРЕВ: по умолчанию 5 секунд; скорость прогрева не включаем в результаты.
    result.total_operations = run(collector, values, options.threads, options.warmup).operations;
    result.ops_per_second.reserve(options.repeats);

    for (unsigned int i = 0; i < options.repeats; i++) {
        const RunResult measured = run(collector, values, options.threads, options.seconds);

        result.total_operations += measured.operations;
        result.ops_per_second.push_back(measured.operations / measured.seconds); // реальное число оп/сек
    }

    std::vector<double> sorted = result.ops_per_second;
    std::sort(sorted.begin(), sorted.end());

    size_t middle = sorted.size() / 2;

    if (sorted.size() % 2 == 0) {
        result.median_ops_per_second = (sorted[middle - 1] + sorted[middle]) / 2;
    } else {
        result.median_ops_per_second = sorted[middle];
    }

    return result; // возвращаем медиану вместе с результатами забегов
}
