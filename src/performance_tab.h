#pragma once

#include <array>
#include <vector>

#include "system_info.h"

// Fixed-size circular history buffer used for the scrolling graphs.
class RingBuffer {
public:
    explicit RingBuffer(int capacity) : data_(capacity, 0.0f), capacity_(capacity) {}

    void Push(float v) {
        data_[write_] = v;
        write_ = (write_ + 1) % capacity_;
        if (count_ < capacity_) count_++;
    }

    // Returns a contiguous copy starting from the oldest sample, suitable
    // for ImGui::PlotLines.
    std::vector<float> Linearized() const {
        std::vector<float> out(count_);
        int start = (write_ - count_ + capacity_) % capacity_;
        for (int i = 0; i < count_; ++i)
            out[i] = data_[(start + i) % capacity_];
        return out;
    }

    float Last() const {
        if (count_ == 0) return 0.0f;
        int idx = (write_ - 1 + capacity_) % capacity_;
        return data_[idx];
    }

private:
    std::vector<float> data_;
    int capacity_;
    int write_ = 0;
    int count_ = 0;
};

// "Performance" tab: compact scrollable dashboard with CPU, memory, disk,
// network graphs plus uptime.
class PerformanceTab {
public:
    PerformanceTab();

    // Pulls fresh samples from sysinfo and pushes them into the history
    // ring buffers. Called on the app's 1s tick.
    void Refresh();

    void Draw();

private:
    static constexpr int kHistoryLen = 120; // 2 minutes at 1s resolution

    sysinfo::CpuSample cpu_;
    sysinfo::MemSample mem_;
    sysinfo::DiskSample disk_;
    sysinfo::NetSample net_;
    double uptime_seconds_ = 0.0;

    RingBuffer cpu_history_{kHistoryLen};
    RingBuffer mem_history_{kHistoryLen};
    RingBuffer disk_read_history_{kHistoryLen};
    RingBuffer disk_write_history_{kHistoryLen};
    RingBuffer net_recv_history_{kHistoryLen};
    RingBuffer net_send_history_{kHistoryLen};

    void DrawCpuCard();
    void DrawMemoryCard();
    void DrawDiskCard();
    void DrawNetworkCard();
    void DrawUptimeCard();
};
