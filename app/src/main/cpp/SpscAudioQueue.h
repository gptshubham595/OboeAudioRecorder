#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

// One producer and one consumer. reset() is only legal while both are stopped.
template<size_t Capacity>
class SpscAudioQueue {
    static_assert(Capacity > 0 && (Capacity & (Capacity - 1)) == 0);
    static_assert(Capacity < (1u << 30));
    static_assert(std::atomic<uint32_t>::is_always_lock_free);
public:
    size_t writeAvailable() const {
        return Capacity - (mWrite.load(std::memory_order_relaxed) -
                           mRead.load(std::memory_order_acquire));
    }
    bool push(const int16_t* input, size_t count) {
        const uint32_t write = mWrite.load(std::memory_order_relaxed);
        const uint32_t read = mRead.load(std::memory_order_acquire);
        if (count > Capacity - (write - read)) return false;
        const size_t offset = write & (Capacity - 1);
        const size_t first = std::min(count, Capacity - offset);
        if (first) std::memcpy(mData.data() + offset, input, first * sizeof(int16_t));
        if (count > first) std::memcpy(mData.data(), input + first, (count - first) * sizeof(int16_t));
        mWrite.store(write + static_cast<uint32_t>(count), std::memory_order_release);
        return true;
    }
    size_t pop(int16_t* output, size_t requested) {
        const uint32_t read = mRead.load(std::memory_order_relaxed);
        const uint32_t write = mWrite.load(std::memory_order_acquire);
        const size_t count = std::min<size_t>(requested, write - read);
        const size_t offset = read & (Capacity - 1);
        const size_t first = std::min(count, Capacity - offset);
        if (first) std::memcpy(output, mData.data() + offset, first * sizeof(int16_t));
        if (count > first) std::memcpy(output + first, mData.data(), (count - first) * sizeof(int16_t));
        mRead.store(read + static_cast<uint32_t>(count), std::memory_order_release);
        return count;
    }
    void reset() {
        // Quiescent reset: clear stale PCM samples as well as queue positions.
        mData.fill(0);
        mRead.store(0);
        mWrite.store(0);
    }
private:
    std::array<int16_t, Capacity> mData{};
    alignas(64) std::atomic<uint32_t> mRead{0};
    alignas(64) std::atomic<uint32_t> mWrite{0};
};
