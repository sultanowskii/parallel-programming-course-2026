#include "thread_local_collector.h"

#include <algorithm>
#include <utility>

uint64_t next_collector_id() {
    static std::atomic<uint64_t> next_id{1};
    return next_id.fetch_add(1);
}

ThreadLocalCollector::ThreadLocalCollector() : id_(next_collector_id()) {
}

ThreadLocalCollector::ThreadState* ThreadLocalCollector::get_my_state() {
    struct Slot {
        uint64_t collector_id = 0;
        ThreadState* state = nullptr;
    };

    static thread_local Slot slot;

    if (slot.collector_id != id_) {
        auto state = std::make_unique<ThreadState>();
        ThreadState* pointer = state.get();

        {
            std::lock_guard<std::mutex> lock(list_mutex_);
            states_.push_back(std::move(state));
        }

        slot.collector_id = id_;
        slot.state = pointer;
    }

    return slot.state;
}

void relaxed_add(std::atomic<uint64_t>& counter, uint64_t value) {
    counter.store(counter.load(std::memory_order_relaxed) + value, std::memory_order_relaxed);
}

void ThreadLocalCollector::record(uint64_t value) {
    ThreadState* state = get_my_state();
    size_t bucket = get_bucket_index(value);

    relaxed_add(state->buckets[bucket], 1);
    relaxed_add(state->count, 1);
    relaxed_add(state->sum, value);

    if (value < state->min.load(std::memory_order_relaxed)) {
        state->min.store(value, std::memory_order_relaxed);
    }

    if (value > state->max.load(std::memory_order_relaxed)) {
        state->max.store(value, std::memory_order_relaxed);
    }
}

Snapshot ThreadLocalCollector::snapshot() {
    std::vector<ThreadState*> states;

    {
        std::lock_guard<std::mutex> lock(list_mutex_);
        states.reserve(states_.size());

        for (const auto& state : states_) {
            states.push_back(state.get());
        }
    }

    Snapshot result{};
    result.min = UINT64_MAX;

    for (ThreadState* state : states) {
        for (size_t bucket = 0; bucket < BUCKET_COUNT; bucket++) {
            result.buckets[bucket] += state->buckets[bucket].load(std::memory_order_relaxed);
        }

        result.count += state->count.load(std::memory_order_relaxed);
        result.sum += state->sum.load(std::memory_order_relaxed);
        result.min = std::min(result.min, state->min.load(std::memory_order_relaxed));
        result.max = std::max(result.max, state->max.load(std::memory_order_relaxed));
    }

    result.p50 = find_percentile_value(result.buckets, result.count, 50);
    result.p99 = find_percentile_value(result.buckets, result.count, 99);

    return result;
}
