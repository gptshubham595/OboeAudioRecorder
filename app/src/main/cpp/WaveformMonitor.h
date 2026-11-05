#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <mutex>

// Used by the file/DSP worker and UI snapshots, never by the audio callback.
// 480 min/max buckets of 100 samples = one second at 48 kHz, with shared timing.
class WaveformMonitor {
public:
    static constexpr size_t buckets = 480;
    static constexpr size_t headerFloats = 8;
    static constexpr size_t snapshotFloats = headerFloats + buckets * 4;

    void reset(size_t delaySamples) {
        std::lock_guard<std::mutex> lock(mMutex);
        mHistory.fill({});
        mRawDelay.fill(0);
        mDelay = std::min(delaySamples, mRawDelay.size());
        mDelayPosition = mSeen = mWrite = mCount = 0;
        mSequence = 0;
        mPartial = {};
    }

    void append(const int16_t* input, const int16_t* output, size_t count) {
        std::lock_guard<std::mutex> lock(mMutex);
        for (size_t i = 0; i < count; ++i) {
            float raw = input[i] / 32768.0f;
            if (mDelay) {
                const float delayed = mRawDelay[mDelayPosition];
                mRawDelay[mDelayPosition] = raw;
                mDelayPosition = (mDelayPosition + 1) % mDelay;
                raw = delayed;
            }
            if (mSeen++ < mDelay) continue;
            const float processed = output[i] / 32768.0f;
            auto& bucket = mPartial;
            bucket.rawMin = std::min(bucket.rawMin, raw);
            bucket.rawMax = std::max(bucket.rawMax, raw);
            bucket.outMin = std::min(bucket.outMin, processed);
            bucket.outMax = std::max(bucket.outMax, processed);
            bucket.rawSquares += double(raw) * raw;
            bucket.outSquares += double(processed) * processed;
            bucket.rawClips += std::abs(raw) >= 0.9999f;
            bucket.outClips += std::abs(processed) >= 0.9999f;
            if (++bucket.samples == 100) {
                mHistory[mWrite] = bucket;
                mWrite = (mWrite + 1) % buckets;
                mCount = std::min(mCount + 1, buckets);
                mPartial = {};
                ++mSequence;
            }
        }
    }

    void finish() {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mPartial.samples) return;
        mHistory[mWrite] = mPartial;
        mWrite = (mWrite + 1) % buckets;
        mCount = std::min(mCount + 1, buckets);
        mPartial = {};
        ++mSequence;
    }

    uint64_t snapshot(float* destination) const {
        std::lock_guard<std::mutex> lock(mMutex);
        std::fill_n(destination, snapshotFloats, 0.0f);
        double rawSquares = 0, outSquares = 0;
        uint32_t rawClips = 0, outClips = 0, samples = 0;
        float rawPeak = 0, outPeak = 0;
        for (size_t i = 0; i < mCount; ++i) {
            const auto& bucket = mHistory[(mWrite + buckets - mCount + i) % buckets];
            const size_t offset = headerFloats + (buckets - mCount + i) * 4;
            destination[offset] = bucket.rawMin;
            destination[offset + 1] = bucket.rawMax;
            destination[offset + 2] = bucket.outMin;
            destination[offset + 3] = bucket.outMax;
            rawPeak = std::max(rawPeak, std::max(-bucket.rawMin, bucket.rawMax));
            outPeak = std::max(outPeak, std::max(-bucket.outMin, bucket.outMax));
            rawSquares += bucket.rawSquares; outSquares += bucket.outSquares;
            rawClips += bucket.rawClips; outClips += bucket.outClips;
            samples += bucket.samples;
        }
        destination[0] = rawPeak;
        destination[1] = samples ? std::sqrt(rawSquares / samples) : 0;
        destination[2] = outPeak;
        destination[3] = samples ? std::sqrt(outSquares / samples) : 0;
        destination[4] = static_cast<float>(rawClips);
        destination[5] = static_cast<float>(outClips);
        destination[6] = static_cast<float>(samples);
        destination[7] = static_cast<float>(mCount);
        return mSequence;
    }
private:
    struct Bucket {
        float rawMin = 1, rawMax = -1, outMin = 1, outMax = -1;
        double rawSquares = 0, outSquares = 0;
        uint32_t rawClips = 0, outClips = 0, samples = 0;
    };
    mutable std::mutex mMutex;
    std::array<Bucket, buckets> mHistory{};
    std::array<float, 128> mRawDelay{};
    Bucket mPartial;
    size_t mDelay = 0, mDelayPosition = 0, mSeen = 0, mWrite = 0, mCount = 0;
    uint64_t mSequence = 0;
};
