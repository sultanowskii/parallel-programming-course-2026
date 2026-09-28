#include "metrics_collector.h"

#include <algorithm>

size_t get_bucket_index(uint64_t value) {
    return std::min(value / BUCKET_COVERED_RANGE_MS, BUCKET_COUNT - 1);
}

size_t find_percentile_value(
    const std::array<uint64_t, BUCKET_COUNT>& buckets,
    uint64_t count,
    uint8_t percentile
) {
    if (count == 0) {
        return 0;
    }

    double border = count * (percentile / 100.0);

    size_t result = BUCKET_COUNT - 1;

    uint64_t acc = 0;
    for (size_t i = 0; i < BUCKET_COUNT; i++) {
        acc += buckets[i];
        if (acc >= border) {
            result = i;
            break;
        }
    }

    return result * BUCKET_COVERED_RANGE_MS;
}
