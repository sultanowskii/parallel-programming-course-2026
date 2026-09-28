#include "double_buffer_collector.h"

#include <algorithm>
#include <thread>
#include <utility>

uint64_t next_double_buffer_collector_id() {
    static std::atomic<uint64_t> next_id{1};
    return next_id.fetch_add(1);
}

DoubleBufferCollector::DoubleBufferCollector(bool check_buffer_index)
    : id_(next_double_buffer_collector_id()), check_buffer_index_(check_buffer_index) {
}

void DoubleBufferCollector::Buffer::clear() {
    buckets.fill(0);
    count = 0;
    sum = 0;
    min = UINT64_MAX;
    max = 0;
}

DoubleBufferCollector::ThreadBuffers* DoubleBufferCollector::get_my_buffers() {
    struct Slot {
        uint64_t collector_id = 0;
        ThreadBuffers* buffers = nullptr;
    };

    static thread_local Slot slot;

    if (slot.collector_id != id_) {
        auto buffers = std::make_unique<ThreadBuffers>();
        ThreadBuffers* pointer = buffers.get();

        {
            std::lock_guard<std::mutex> lock(snapshot_mutex_);
            thread_buffers_.push_back(std::move(buffers));
        }

        slot.collector_id = id_;
        slot.buffers = pointer;
    }

    return slot.buffers;
}

void DoubleBufferCollector::record(uint64_t value) {
    ThreadBuffers* state = get_my_buffers();
    int buffer_index;

    while (true) {
        buffer_index = active_buffer_index_.load();
        state->writing_buffer_index.store(buffer_index);

        // читатель мог переключить буфер, перепроверка
        if (!check_buffer_index_ || active_buffer_index_.load() == buffer_index) {
            break;
        }

        // если не вышло - явно об этом говорим
        state->writing_buffer_index.store(NOT_WRITING, std::memory_order_release);
    }

    Buffer& buffer = state->buffers[buffer_index];
    buffer.buckets[get_bucket_index(value)]++;
    buffer.count++;
    buffer.sum += value;
    buffer.min = std::min(buffer.min, value);
    buffer.max = std::max(buffer.max, value);

    // не забываем отпустить
    state->writing_buffer_index.store(NOT_WRITING, std::memory_order_release);
}

Snapshot DoubleBufferCollector::snapshot() {
    std::lock_guard<std::mutex> lock(snapshot_mutex_);

    int old_buffer_index = active_buffer_index_.load();
    active_buffer_index_.store(1 - old_buffer_index);

    // идем по всем состояниям тредов и у каждого ждем, пока он не перестанет писать в текущий занятый буфер
    // иначе говоря: ждем, пока его writing_buffer_index не станет -1 или новым active, тогда мы уверенно сможем прочитать inactive буфер 
    for (const auto& state : thread_buffers_) {
        while (state->writing_buffer_index.load() == old_buffer_index) {
            std::this_thread::yield();
        }

        Buffer& buffer = state->buffers[old_buffer_index];

        for (size_t bucket = 0; bucket < BUCKET_COUNT; bucket++) {
            total_.buckets[bucket] += buffer.buckets[bucket];
        }

        total_.count += buffer.count;
        total_.sum += buffer.sum;
        total_.min = std::min(total_.min, buffer.min);
        total_.max = std::max(total_.max, buffer.max);

        buffer.clear();
    }

    Snapshot result{};
    std::copy(total_.buckets.begin(), total_.buckets.end(), result.buckets.begin());
    result.count = total_.count;
    result.sum = total_.sum;
    result.min = total_.min;
    result.max = total_.max;
    result.p50 = find_percentile_value(result.buckets, result.count, 50);
    result.p99 = find_percentile_value(result.buckets, result.count, 99);

    return result;
}
