#include "NoiseGate.h"
#include <android/log.h>

#define LOG_TAG "NoiseGate"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

NoiseGate::NoiseGate()
        : mThreshold(0.01f), // Default -40 dB
          mRatio(4.0f),
          mAttackCoeff(0.0f),
          mReleaseCoeff(0.0f),
          mEnvelope(0.0f) {
}

void NoiseGate::setThreshold(float thresholdDb) {
    // Convert dB to linear
    mThreshold = std::pow(10.0f, thresholdDb / 20.0f);
    LOGD("Noise Gate Threshold: %.1f dB (%.6f linear)", thresholdDb, mThreshold);
}

void NoiseGate::setRatio(float ratio) {
    mRatio = std::max(1.0f, ratio);
    LOGD("Noise Gate Ratio: %.1f:1", mRatio);
}

void NoiseGate::setAttack(float attackMs, float sampleRate) {
    mAttackCoeff = calculateCoeff(attackMs, sampleRate);
    LOGD("Noise Gate Attack: %.1f ms", attackMs);
}

void NoiseGate::setRelease(float releaseMs, float sampleRate) {
    mReleaseCoeff = calculateCoeff(releaseMs, sampleRate);
    LOGD("Noise Gate Release: %.1f ms", releaseMs);
}

float NoiseGate::calculateCoeff(float timeMs, float sampleRate) {
    // Convert milliseconds to samples and calculate exponential coefficient
    if (timeMs <= 0.0f) return 0.0f;
    return std::exp(-1.0f / (timeMs * 0.001f * sampleRate));
}

float NoiseGate::process(float input) {
    // Get input level (absolute value)
    float inputLevel = std::abs(input);

    // Smooth envelope follower
    if (inputLevel > mEnvelope) {
        mEnvelope = mAttackCoeff * mEnvelope + (1.0f - mAttackCoeff) * inputLevel;
    } else {
        mEnvelope = mReleaseCoeff * mEnvelope + (1.0f - mReleaseCoeff) * inputLevel;
    }

    // Calculate gain reduction
    float gain = 1.0f;
    if (mEnvelope < mThreshold) {
        // Below threshold: apply expansion
        float diff = mThreshold - mEnvelope;
        float reduction = diff * (mRatio - 1.0f) / mRatio;
        gain = std::max(0.0f, (mThreshold - reduction) / mThreshold);
    }

    return input * gain;
}

void NoiseGate::reset() {
    mEnvelope = 0.0f;
}