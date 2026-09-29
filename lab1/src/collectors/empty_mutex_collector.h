#pragma once

#include "plain_collector.h"

#include <mutex>

class EmptyMutexCollector : public PlainCollector {
public:
    void record(uint64_t value) override;

private:
    std::mutex mutex_;
};
