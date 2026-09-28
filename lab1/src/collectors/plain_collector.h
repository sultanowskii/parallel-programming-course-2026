#pragma once

#include "metrics_collector.h"

class PlainCollector : public MetricsCollector {
public:
    void record(uint64_t value) override;
    Snapshot snapshot() override;

private:
    std::array<uint64_t, BUCKET_COUNT> buckets_{};
    uint64_t count_ = 0;
    uint64_t sum_ = 0;
    uint64_t min_ = UINT64_MAX;
    uint64_t max_ = 0;
};
