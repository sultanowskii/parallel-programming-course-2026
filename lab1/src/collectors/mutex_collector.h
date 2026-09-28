#pragma once

#include "plain_collector.h"

#include <mutex>

class MutexCollector : public PlainCollector {
public:
    void record(uint64_t value) override;
    Snapshot snapshot() override;

private:
    std::mutex mutex_;
};
