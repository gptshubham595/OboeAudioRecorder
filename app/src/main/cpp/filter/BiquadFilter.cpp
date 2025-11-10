#include "BiquadFilter.h"
#include <android/log.h>

#define LOG_TAG "BiquadFilter"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

// Clears the filter's history (important before start/stop)
void BiquadFilter::reset() {
    x1 = x2 = y1 = y2 = 0.0f;
}

// Implement Bandpass filter calculation (peak/skirt gain = 0dB)
void BiquadFilter::setBandpass(float sampleRate, float centerFreq, float Q) {
    reset();

    // Clamp values to prevent instability or division by zero
    centerFreq = std::min(centerFreq, sampleRate / 2.0f - 1.0f);
    Q = std::max(Q, 0.1f);

    // Convert to radians/sec
    float w0 = 2.0f * M_PI * centerFreq / sampleRate;
    float alpha = std::sin(w0) / (2.0f * Q);
    float cos_w0 = std::cos(w0);

    float a0 = 1.0f + alpha;

    // Numerator coefficients
    b0 = alpha / a0;
    b1 = 0.0f;
    b2 = -alpha / a0;

    // Denominator coefficients (a0 is normalized to 1)
    a1 = (-2.0f * cos_w0) / a0;
    a2 = (1.0f - alpha) / a0;

    LOGD("Bandpass Filter set: F=%.1f, Q=%.2f", centerFreq, Q);
}

// Biquad Direct Form I implementation
float BiquadFilter::process(float in) {
    float out = b0 * in + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;

    // Update state variables (shift history)
    x2 = x1;
    x1 = in;
    y2 = y1;
    y1 = out;

    return out;
}

// --- Placeholders for future steps ---

void BiquadFilter::setHighShelf(float sampleRate, float centerFreq, float Q, float gainDb) {
    // Implementation for Step 2 will go here
    reset();
    LOGD("High Shelf Filter initialized (coefficients pending implementation).");
}

void BiquadFilter::setPeaking(float sampleRate, float centerFreq, float Q, float gainDb) {
    // Implementation for Step 3 will go here
    reset();
    LOGD("Peaking Filter initialized (coefficients pending implementation).");
}