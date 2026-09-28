#pragma once

#include "collectors/metrics_collector.h"

#include <vector>

struct StressOptions {
    unsigned int threads = 4;
    unsigned int snapshots = 10000;
};

struct StressResult {
    unsigned int snapshots = 0;
    unsigned int bucket_sum_less_than_count = 0;
    unsigned int bucket_sum_greater_than_count = 0;
    uint64_t total_operations = 0;
    uint64_t final_count = 0;
};

StressResult run_stress(
    MetricsCollector& collector,
    const std::vector<uint64_t>& values,
    const StressOptions& options
);
