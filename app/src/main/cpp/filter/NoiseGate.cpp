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
    thresholdDb = std::isfinite(thresholdDb) ? std::clamp(thresholdDb, -120.0f, 0.0f) : -40.0f;
    // Bound the threshold so division in the gain calculation remains well-defined.
    mThreshold = std::pow(10.0f, thresholdDb / 20.0f);
    LOGD("Noise Gate Threshold: %.1f dB (%.6f linear)", thresholdDb, mThreshold);
}

void NoiseGate::setRatio(float ratio) {
    mRatio = std::isfinite(ratio) ? std::clamp(ratio, 1.0f, 20.0f) : 1.0f;
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
    if (!std::isfinite(timeMs) || !std::isfinite(sampleRate) ||
        timeMs <= 0.0f || sampleRate <= 0.0f) return 0.0f;
    return std::exp(-1.0f / (timeMs * 0.001f * sampleRate));
}

float NoiseGate::process(float input) {
    if (!std::isfinite(input)) { reset(); return 0.0f; }

    // Get input level (absolute value)
    float inputLevel = std::abs(input);

    // Smooth envelope follower
    if (inputLevel > mEnvelope) {
        mEnvelope = mAttackCoeff * mEnvelope + (1.0f - mAttackCoeff) * inputLevel;
    } else {
        mEnvelope = mReleaseCoeff * mEnvelope + (1.0f - mReleaseCoeff) * inputLevel;
    }

    if (mEnvelope < 1e-20f) mEnvelope = 0.0f;

    // Calculate gain reduction
    float gain = 1.0f;
    if (mEnvelope < mThreshold) {
        // R:1 downward expansion in dB: gainDb = (R - 1) * (levelDb - thresholdDb).
        // The equivalent linear expression reaches silence instead of a fixed gain floor.
        gain = std::pow(std::max(0.0f, mEnvelope) / mThreshold, mRatio - 1.0f);
    }

    // Smooth gain separately from detection so threshold crossings do not step the waveform.
    const float coefficient = gain > mGain ? mAttackCoeff : mReleaseCoeff;
    mGain = coefficient * mGain + (1.0f - coefficient) * gain;
    if (mGain < 1e-20f) mGain = 0.0f;
    return input * mGain;
}

void NoiseGate::reset() {
    mEnvelope = 0.0f;
    mGain = 1.0f;
}
