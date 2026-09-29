#pragma once

#include <array>
#include <cstdint>
#include <cstddef>

const uint64_t BUCKET_COUNT = 256;
const uint64_t BUCKET_COVERED_RANGE_MS = 4;

struct Snapshot {
    std::array<uint64_t, BUCKET_COUNT> buckets;
    uint64_t count;
    uint64_t sum;
    uint64_t min;
    uint64_t max;
    uint64_t p50;
    uint64_t p99;
};

class MetricsCollector {
public:
    virtual ~MetricsCollector() = default;
    virtual void record(uint64_t value) = 0;
    virtual Snapshot snapshot() = 0;
};

size_t get_bucket_index(uint64_t value);

size_t find_percentile_value(
    const std::array<uint64_t, BUCKET_COUNT>& buckets,
    uint64_t count,
    uint8_t percentile
);
