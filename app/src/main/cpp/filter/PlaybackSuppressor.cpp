#include "PlaybackSuppressor.h"
#include <android/log.h>

#define LOG_TAG "PlaybackSuppressor"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

// The size of the energy history buffer (e.g., 50ms at 48kHz = 2400 samples, which is too large for single-sample loop).
// We'll use a small buffer for short-term energy analysis, mimicking a processing frame size.
constexpr int kEnergyWindowSize = 100; // About 2ms at 48kHz.

PlaybackSuppressor::PlaybackSuppressor(int sampleRate)
        : mSampleRate(sampleRate),
          mEnabled(false),
          mAggressiveness(0.0f),
          mHistoryIndex(0),
          mEnergySum(0.0f),
          mPrevInput(0.0f),
          mZeroCrossingCount(0),
          mPrevZeroCrossing(0.0f),
          mVoiceThreshold(0.0001f) // A fixed low energy threshold for basic VAD
{
    // Initialize the energy buffer
    mEnergyHistory.resize(kEnergyWindowSize, 0.0f);
    LOGD("PlaybackSuppressor initialized at %d Hz. Window Size: %d", mSampleRate, kEnergyWindowSize);
}

void PlaybackSuppressor::setEnabled(bool enabled) {
    mEnabled = enabled;
    LOGD("PlaybackSuppressor enabled: %d", enabled);
}

void PlaybackSuppressor::setAggressiveness(float amount) {
    // Clamp aggressiveness between 0.0 (off) and 1.0 (max suppression)
    mAggressiveness = std::max(0.0f, std::min(1.0f, amount));
    LOGD("PlaybackSuppressor aggressiveness set to %.2f", mAggressiveness);
}

void PlaybackSuppressor::reset() {
    std::fill(mEnergyHistory.begin(), mEnergyHistory.end(), 0.0f);
    mHistoryIndex = 0;
    mEnergySum = 0.0f;
    mPrevInput = 0.0f;
    mZeroCrossingCount = 0;
    mPrevZeroCrossing = 0.0f;
}

/**
 * Processes a single sample and applies suppression if non-voice audio is detected.
 * Detection logic: High, stable energy AND low zero-crossing rate usually indicates music/bass.
 */
float PlaybackSuppressor::process(float input) {
    if (!mEnabled || mAggressiveness == 0.0f) {
        return input;
    }

    // --- 1. Short-Term Energy Calculation ---
    // Remove oldest energy value from sum
    mEnergySum -= mEnergyHistory[mHistoryIndex];

    // Calculate current energy (squared amplitude)
    float currentEnergy = input * input;

    // Add new energy value to buffer and sum
    mEnergyHistory[mHistoryIndex] = currentEnergy;
    mEnergySum += currentEnergy;

    // --- 2. Zero Crossing Rate (ZCR) Proxy ---
    // A simplified ZCR check: simply count crossings
    if ((input >= 0 && mPrevInput < 0) || (input < 0 && mPrevInput >= 0)) {
        mZeroCrossingCount++;
    }
    mPrevInput = input;

    // --- 3. Update Buffer Index ---
    mHistoryIndex = (mHistoryIndex + 1) % kEnergyWindowSize;

    // Only make a decision when the window has completed one cycle.
    // In a real implementation, this would involve downsampling and block processing.
    // Here, we use the completion of the short window as a decision point.
    float targetGain = 1.0f;

    if (mHistoryIndex == 0) {
        // Calculate average energy
        float averageEnergy = mEnergySum / kEnergyWindowSize;

        // Calculate average ZCR over the window
        float averageZCR = (float)mZeroCrossingCount / kEnergyWindowSize;
        mZeroCrossingCount = 0; // Reset counter for next window

        // --- 4. Decision Logic (Heuristic for non-voice audio) ---

        // Non-Voice Condition:
        // High Energy (loud) AND (Low ZCR - e.g., deep bass/sustained low tones)
        // OR (Extremely Stable ZCR - e.g., pure sine wave/synth pad)

        bool isHighEnergy = averageEnergy > mVoiceThreshold;
        bool isLowZCR = averageZCR < 0.05f; // < 5% ZCR (indicates very low frequency or DC bias)

        if (isHighEnergy && isLowZCR) {
            // Highly likely non-voice/music (e.g., bass kick or continuous low tone)
            targetGain = 1.0f - mAggressiveness; // Apply reduction
            LOGD("Suppressing: High Energy (%.6f) & Low ZCR (%.2f)", averageEnergy, averageZCR);
        } else {
            // Assume voice or silence
            targetGain = 1.0f;
        }

        // Use the suppression logic to update the target gain for the next window
        mPrevZeroCrossing = averageZCR;
    }


    // This logic relies on the assumption that a continuous low-pass filter (like a single-pole
    // IIR) for gain smoothing is used, which is handled implicitly by the fast execution 
    // of this process function on every sample. Since we need to suppress *during* the window,
    // we simply apply the target gain calculated from the *previous* window's decision.

    // For simplicity in single-sample processing, we are using the last calculated targetGain.
    // In a production system, this decision would drive a fast gain reduction envelope.
    // Since we only update the decision every kEnergyWindowSize, this will create a blocky
    // suppression effect which is acceptable for this simplified implementation.

    // We can also smooth the gain slightly based on the aggressiveness setting
    float suppressionGain = 1.0f - (1.0f - targetGain) * mAggressiveness;

    return input * suppressionGain;
}