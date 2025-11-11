#include "NoiseReduction.h"
#include <android/log.h>
#include <algorithm>

#define LOG_TAG "NoiseReduction"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

NoiseReduction::NoiseReduction(int smoothingWindow)
        : mWindowSize(smoothingWindow),
          mIndex(0),
          mReductionAmount(0.5f),
          mSum(0.0f) {
    mBuffer.resize(mWindowSize, 0.0f);
    LOGD("NoiseReduction initialized with window size: %d", mWindowSize);
}

void NoiseReduction::setReductionAmount(float amount) {
    mReductionAmount = std::max(0.0f, std::min(1.0f, amount));
    LOGD("Noise Reduction Amount: %.2f", mReductionAmount);
}

float NoiseReduction::process(float input) {
    // Remove oldest sample from sum
    mSum -= mBuffer[mIndex];

    // Add new sample
    mBuffer[mIndex] = input;
    mSum += input;

    // Move to next position
    mIndex = (mIndex + 1) % mWindowSize;

    // Calculate moving average
    float average = mSum / mWindowSize;

    // Blend between original and smoothed based on reduction amount
    // This reduces high-frequency noise while preserving signal
    return input * (1.0f - mReductionAmount) + average * mReductionAmount;
}

void NoiseReduction::reset() {
    std::fill(mBuffer.begin(), mBuffer.end(), 0.0f);
    mIndex = 0;
    mSum = 0.0f;
}