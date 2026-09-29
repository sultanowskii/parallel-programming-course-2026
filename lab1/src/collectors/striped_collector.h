#pragma once

#include "metrics_collector.h"

#include <atomic>
#include <mutex>

const size_t STRIPE_COUNT = 16;

class StripedCollector : public MetricsCollector {
public:
    void record(uint64_t value) override;
    Snapshot snapshot() override;

private:
    std::array<std::mutex, STRIPE_COUNT> mutexes_;
    std::array<uint64_t, BUCKET_COUNT> buckets_{};
    std::atomic<uint64_t> count_{0};
    std::atomic<uint64_t> sum_{0};
    std::atomic<uint64_t> min_{UINT64_MAX};
    std::atomic<uint64_t> max_{0};
};
