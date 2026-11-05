#include "EchoCanceller.h"
#include <algorithm>
#include <cmath>

EchoCanceller::EchoCanceller(int sampleRate)
        : mSampleRate(std::clamp(sampleRate, 8000, 192000)) {
    setEchoDelay(50.0f);
}

void EchoCanceller::setEchoDelay(float delayMs) {
    delayMs = std::isfinite(delayMs) ? std::clamp(delayMs, 0.0f, 500.0f) : 0.0f;
    mDelayInSamples = static_cast<int>(delayMs * 0.001f * mSampleRate);
    mDelayBuffer.assign(static_cast<size_t>(mDelayInSamples) + 1, 0.0f);
    reset();
}

void EchoCanceller::setSuppressionAmount(float amount) {
    mSuppressionAmount = std::isfinite(amount) ? std::clamp(amount, 0.0f, 1.0f) : 0.0f;
}

float EchoCanceller::process(float input) const {
    return std::isfinite(input) ? input : 0.0f;
}

float EchoCanceller::process(float input, float playbackReference) {
    if (!std::isfinite(input) || !std::isfinite(playbackReference)) {
        reset();
        return std::isfinite(input) ? input : 0.0f;
    }
    mDelayBuffer[mWriteIndex] = playbackReference;
    const auto readIndex = (mWriteIndex + mDelayBuffer.size() - mDelayInSamples)
            % mDelayBuffer.size();
    const float reference = mDelayBuffer[readIndex];
    mWriteIndex = (mWriteIndex + 1) % mDelayBuffer.size();
    const float error = input - mAdaptiveCoeff * reference;
    // Normalized one-tap adaptation; do not adapt on a silent reference.
    if (std::abs(reference) > 1e-5f) {
        mAdaptiveCoeff = std::clamp(mAdaptiveCoeff +
                0.05f * error * reference / (reference * reference + 1e-6f), -2.0f, 2.0f);
    }
    return input - mSuppressionAmount * mAdaptiveCoeff * reference;
}

void EchoCanceller::reset() {
    std::fill(mDelayBuffer.begin(), mDelayBuffer.end(), 0.0f);
    mWriteIndex = 0;
    mAdaptiveCoeff = 0.0f;
}
