#pragma once

#include "metrics_collector.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

class DoubleBufferCollector : public MetricsCollector {
public:
    explicit DoubleBufferCollector(bool check_buffer_index = true);

    void record(uint64_t value) override;
    Snapshot snapshot() override;

private:
    static constexpr int NOT_WRITING = -1;

    struct Buffer {
        std::array<uint64_t, BUCKET_COUNT> buckets{};
        uint64_t count = 0;
        uint64_t sum = 0;
        uint64_t min = UINT64_MAX;
        uint64_t max = 0;

        void clear();
    };

    struct alignas(64) ThreadBuffers {
        std::atomic<int> writing_buffer_index{NOT_WRITING};
        std::array<Buffer, 2> buffers;
    };

    ThreadBuffers* get_my_buffers();

    const uint64_t id_;
    const bool check_buffer_index_;
    std::atomic<int> active_buffer_index_{0};
    std::mutex snapshot_mutex_;
    std::vector<std::unique_ptr<ThreadBuffers>> thread_buffers_;
    Buffer total_;
};
