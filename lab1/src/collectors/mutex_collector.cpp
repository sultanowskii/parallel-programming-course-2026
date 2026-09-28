#include "mutex_collector.h"

void MutexCollector::record(uint64_t value) {
    std::lock_guard<std::mutex> lock(mutex_);

    PlainCollector::record(value);
}

Snapshot MutexCollector::snapshot() {
    std::lock_guard<std::mutex> lock(mutex_);

    return PlainCollector::snapshot();
}
