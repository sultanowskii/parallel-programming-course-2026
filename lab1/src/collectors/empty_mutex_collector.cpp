#include "empty_mutex_collector.h"

void EmptyMutexCollector::record(uint64_t) {
    std::lock_guard<std::mutex> lock(mutex_);
}
