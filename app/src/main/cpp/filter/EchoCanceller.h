#pragma once

#include <vector>
#include <cstddef>

// Reference-driven single-path adaptive suppressor, not a full multipath acoustic AEC.
// Without a synchronized playback reference, microphone audio must pass through unchanged.
class EchoCanceller {
public:
    explicit EchoCanceller(int sampleRate);
    void setEchoDelay(float delayMs);
    void setSuppressionAmount(float amount);
    float process(float input) const;
    float process(float input, float playbackReference);
    void reset();
private:
    int mSampleRate;
    int mDelayInSamples = 0;
    std::vector<float> mDelayBuffer;
    size_t mWriteIndex = 0;
    float mSuppressionAmount = 0.7f;
    float mAdaptiveCoeff = 0.0f;
};
