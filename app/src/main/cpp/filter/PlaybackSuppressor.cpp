#include "PlaybackSuppressor.h"
#include <android/log.h>

#define LOG_TAG "PlaybackSuppressor"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

// Analyze 10 ms windows; smooth gain over 5 ms. This remains a heuristic.
PlaybackSuppressor::PlaybackSuppressor(int sampleRate)
        : mSampleRate(std::clamp(sampleRate, 8000, 192000)),
          mEnabled(false),
          mAggressiveness(0.0f),
          mHistoryIndex(0),
          mEnergySum(0.0f),
          mPrevInput(0.0f),
          mZeroCrossingCount(0),
          mVoiceThreshold(0.0001f) // A fixed low energy threshold for basic VAD
{
    // Initialize the energy buffer
    mEnergyHistory.resize(static_cast<size_t>(mSampleRate / 100), 0.0f);
    mGainCoeff = std::exp(-1.0f / (mSampleRate * 0.005f));
    LOGD("PlaybackSuppressor initialized at %d Hz. Window Size: %d", mSampleRate, static_cast<int>(mEnergyHistory.size()));
}

void PlaybackSuppressor::setEnabled(bool enabled) {
    if (mEnabled != enabled) reset();
    mEnabled = enabled;
    LOGD("PlaybackSuppressor enabled: %d", enabled);
}

void PlaybackSuppressor::setAggressiveness(float amount) {
    // Clamp aggressiveness between 0.0 (off) and 1.0 (max suppression)
    mAggressiveness = std::isfinite(amount) ? std::clamp(amount, 0.0f, 1.0f) : 0.0f;
    LOGD("PlaybackSuppressor aggressiveness set to %.2f", mAggressiveness);
}

void PlaybackSuppressor::reset() {
    std::fill(mEnergyHistory.begin(), mEnergyHistory.end(), 0.0f);
    mHistoryIndex = 0;
    mEnergySum = 0.0f;
    mPrevInput = 0.0f;
    mZeroCrossingCount = 0;
    mTargetGain = 1.0f;
    mGain = 1.0f;
}

float PlaybackSuppressor::process(float input) {
    if (!std::isfinite(input)) { reset(); return 0.0f; }
    if (!mEnabled || mAggressiveness == 0.0f) return input;

    mEnergySum -= mEnergyHistory[mHistoryIndex];
    const float energy = input * input;
    mEnergyHistory[mHistoryIndex] = energy;
    mEnergySum += energy;

    if ((input >= 0 && mPrevInput < 0) || (input < 0 && mPrevInput >= 0)) {
        ++mZeroCrossingCount;
    }
    mPrevInput = input;
    mHistoryIndex = (mHistoryIndex + 1) % mEnergyHistory.size();

    if (mHistoryIndex == 0) {
        const float averageEnergy = std::max(0.0f, mEnergySum) / mEnergyHistory.size();
        const float averageZCR = static_cast<float>(mZeroCrossingCount) / mEnergyHistory.size();
        mZeroCrossingCount = 0;
        // Heuristic only: sustained low-frequency sounds can also be speech.
        mTargetGain = averageEnergy > mVoiceThreshold && averageZCR < 0.05f
                ? 1.0f - mAggressiveness : 1.0f;
    }
    // Retain the completed window's decision until the next window completes.
    mGain = mGainCoeff * mGain + (1.0f - mGainCoeff) * mTargetGain;
    return input * mGain;
}
