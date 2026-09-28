#include "stress.h"

#include <atomic>
#include <cassert>
#include <functional>
#include <latch>
#include <thread>

void stress_record_values(
    MetricsCollector& collector,
    const std::vector<uint64_t>& values,
    size_t worker_index,
    std::latch& ready,
    std::latch& start,
    const std::atomic<bool>& stop,
    uint64_t& operations
) {
    uint64_t local_count = 0;
    size_t i = (worker_index * 1000) % values.size();

    ready.count_down();
    start.wait();

    while (!stop.load()) {
        collector.record(values[i]);
        local_count++;
        i++;

        if (i == values.size()) {
            i = 0;
        }
    }

    operations = local_count;
}

StressResult run_stress(
    MetricsCollector& collector,
    const std::vector<uint64_t>& values,
    const StressOptions& options
) {
    assert(!values.empty());
    assert(options.threads > 0 && options.snapshots > 0);

    std::latch ready(options.threads);
    std::latch start(1);
    std::atomic<bool> stop{false};
    std::vector<uint64_t> operations(options.threads);
    std::vector<std::thread> threads;
    threads.reserve(options.threads);

    for (size_t k = 0; k < options.threads; k++) {
        threads.push_back(std::thread(
            stress_record_values,
            std::ref(collector),
            std::cref(values),
            k,
            std::ref(ready),
            std::ref(start),
            std::cref(stop),
            std::ref(operations[k])
        ));
    }

    StressResult result;
    result.snapshots = options.snapshots;

    ready.wait();
    start.count_down();

    for (size_t i = 0; i < options.snapshots; i++) {
        Snapshot snapshot = collector.snapshot();
        uint64_t bucket_sum = 0;

        for (uint64_t bucket : snapshot.buckets) {
            bucket_sum += bucket;
        }

        if (bucket_sum < snapshot.count) {
            result.bucket_sum_less_than_count++;
        } else if (bucket_sum > snapshot.count) {
            result.bucket_sum_greater_than_count++;
        }
    }

    stop.store(true);

    for (std::thread& thread : threads) {
        thread.join();
    }

    for (uint64_t count : operations) {
        result.total_operations += count;
    }

    result.final_count = collector.snapshot().count;

    return result;
}
