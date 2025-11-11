#include "EchoCanceller.h"
#include <android/log.h>
#include <algorithm>

#define LOG_TAG "EchoCanceller"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

EchoCanceller::EchoCanceller(int sampleRate)
        : mSampleRate(sampleRate),
          mDelayInSamples(0),
          mWriteIndex(0),
          mSuppressionAmount(0.7f),
          mAdaptiveCoeff(0.5f) {
    // Default 50ms delay buffer
    setEchoDelay(50.0f);
    LOGD("EchoCanceller initialized at %d Hz", sampleRate);
}

void EchoCanceller::setEchoDelay(float delayMs) {
    mDelayInSamples = static_cast<int>((delayMs / 1000.0f) * mSampleRate);
    mDelayBuffer.resize(mDelayInSamples, 0.0f);
    mWriteIndex = 0;
    LOGD("Echo Delay: %.1f ms (%d samples)", delayMs, mDelayInSamples);
}

void EchoCanceller::setSuppressionAmount(float amount) {
    mSuppressionAmount = std::max(0.0f, std::min(1.0f, amount));
    LOGD("Echo Suppression: %.2f", mSuppressionAmount);
}

float EchoCanceller::process(float input) {
    if (mDelayInSamples == 0) {
        return input;
    }

    // Get delayed sample (estimated echo)
    int readIndex = (mWriteIndex - mDelayInSamples + mDelayBuffer.size()) % mDelayBuffer.size();
    float delayedSample = mDelayBuffer[readIndex];

    // Adaptive echo estimation
    float estimatedEcho = delayedSample * mAdaptiveCoeff;

    // Subtract estimated echo from input
    float output = input - (estimatedEcho * mSuppressionAmount);

    // Update adaptive coefficient based on correlation
    // This is a simplified LMS (Least Mean Squares) approach
    float error = output;
    mAdaptiveCoeff += 0.001f * error * delayedSample;
    mAdaptiveCoeff = std::max(-1.0f, std::min(1.0f, mAdaptiveCoeff));

    // Store current input in delay buffer
    mDelayBuffer[mWriteIndex] = input;
    mWriteIndex = (mWriteIndex + 1) % mDelayBuffer.size();

    return output;
}

void EchoCanceller::reset() {
    std::fill(mDelayBuffer.begin(), mDelayBuffer.end(), 0.0f);
    mWriteIndex = 0;
    mAdaptiveCoeff = 0.5f;
}