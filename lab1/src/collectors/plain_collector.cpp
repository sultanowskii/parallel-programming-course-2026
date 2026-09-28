#include "plain_collector.h"

#include <algorithm>

void PlainCollector::record(uint64_t value) {
    size_t bucket_index = get_bucket_index(value);

    buckets_[bucket_index]++;
    count_++;
    sum_ += value;
    max_ = std::max(max_, value);
    min_ = std::min(min_, value);
}

Snapshot PlainCollector::snapshot() {
    Snapshot result{
        .buckets = {},
        .count = count_,
        .sum = sum_,
        .min = min_,
        .max = max_,
        .p50 = 0,
        .p99 = 0,
    };

    std::copy(buckets_.begin(), buckets_.end(), result.buckets.begin());

    result.p50 = find_percentile_value(result.buckets, result.count, 50);
    result.p99 = find_percentile_value(result.buckets, result.count, 99);

    return result;
}
