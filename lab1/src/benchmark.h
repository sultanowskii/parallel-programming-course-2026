#pragma once

#include "collectors/metrics_collector.h"

#include <vector>

struct BenchmarkOptions {
    unsigned int threads = 1;
    unsigned int warmup = 5;
    unsigned int seconds = 5;
    unsigned int repeats = 5;
};

struct BenchmarkResult {
    std::vector<double> ops_per_second;
    double median_ops_per_second = 0;
    uint64_t total_operations = 0;
};

BenchmarkResult run_benchmark(
    MetricsCollector& collector,
    const std::vector<uint64_t>& values,
    const BenchmarkOptions& options
);
