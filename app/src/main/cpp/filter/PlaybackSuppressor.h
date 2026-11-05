#ifndef OBOESAMPLE_PLAYBACKSUPPRESSOR_H
#define OBOESAMPLE_PLAYBACKSUPPRESSOR_H

#include <vector>
#include <cmath>
#include <algorithm>

/**
 * Aggressively suppresses music/playback audio by detecting
 * non-voice characteristics and attenuating them.
 * * Uses simple time-domain heuristics (Energy and Zero Crossing Rate)
 * to differentiate voice from sustained/stable playback.
 */
class PlaybackSuppressor {
public:
    PlaybackSuppressor(int sampleRate);

    // Enable/disable the suppression filter
    void setEnabled(bool enabled);

    // Set the strength of the suppression (0.0 = off, 1.0 = max)
    void setAggressiveness(float amount);

    // Process a single sample
    float process(float input);

    // Reset all internal state buffers
    void reset();

private:
    int mSampleRate;
    bool mEnabled;
    float mAggressiveness;

    // --- State for Energy and Zero Crossing Rate (ZCR) Analysis ---

    // Buffer to hold squared amplitude (energy) of recent samples
    std::vector<float> mEnergyHistory;
    int mHistoryIndex;
    float mEnergySum;

    // State for ZCR calculation
    float mPrevInput;
    int mZeroCrossingCount;

    // Threshold for energy detection
    float mVoiceThreshold;
    float mTargetGain = 1.0f;
    float mGain = 1.0f;
    float mGainCoeff = 0.0f;
};

#endif //OBOESAMPLE_PLAYBACKSUPPRESSOR_H