#include "striped_collector.h"

void StripedCollector::record(uint64_t value) {
    size_t bucket_index = get_bucket_index(value);

    {
        std::lock_guard<std::mutex> lock(mutexes_[bucket_index % STRIPE_COUNT]);
        buckets_[bucket_index]++;
    }

    count_.fetch_add(1);
    sum_.fetch_add(value);

    uint64_t current_min = min_.load();
    while (value < current_min && !min_.compare_exchange_weak(current_min, value)) {
    }

    uint64_t current_max = max_.load();
    while (value > current_max && !max_.compare_exchange_weak(current_max, value)) {
    }
}

Snapshot StripedCollector::snapshot() {
    Snapshot result{};

    for (size_t stripe = 0; stripe < STRIPE_COUNT; stripe++) {
        std::lock_guard<std::mutex> lock(mutexes_[stripe]);

        for (size_t bucket = stripe; bucket < BUCKET_COUNT; bucket += STRIPE_COUNT) {
            result.buckets[bucket] = buckets_[bucket];
        }
    }

    result.count = count_.load();
    result.sum = sum_.load();
    result.min = min_.load();
    result.max = max_.load();
    result.p50 = find_percentile_value(result.buckets, result.count, 50);
    result.p99 = find_percentile_value(result.buckets, result.count, 99);

    return result;
}
