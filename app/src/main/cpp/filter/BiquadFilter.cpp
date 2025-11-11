#include "BiquadFilter.h"
#include <android/log.h>

#define LOG_TAG "BiquadFilter"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

void BiquadFilter::reset() {
    x1 = x2 = y1 = y2 = 0.0f;
}

// Bandpass filter (already implemented)
void BiquadFilter::setBandpass(float sampleRate, float centerFreq, float Q) {
    reset();
    centerFreq = std::min(centerFreq, sampleRate / 2.0f - 1.0f);
    Q = std::max(Q, 0.1f);

    float w0 = 2.0f * M_PI * centerFreq / sampleRate;
    float alpha = std::sin(w0) / (2.0f * Q);
    float cos_w0 = std::cos(w0);

    float a0 = 1.0f + alpha;
    b0 = alpha / a0;
    b1 = 0.0f;
    b2 = -alpha / a0;
    a1 = (-2.0f * cos_w0) / a0;
    a2 = (1.0f - alpha) / a0;

    LOGD("Bandpass Filter: F=%.1f Hz, Q=%.2f", centerFreq, Q);
}

// High Shelf filter - Boosts/cuts high frequencies
void BiquadFilter::setHighShelf(float sampleRate, float centerFreq, float Q, float gainDb) {
    reset();
    centerFreq = std::min(centerFreq, sampleRate / 2.0f - 1.0f);
    Q = std::max(Q, 0.1f);

    float A = std::pow(10.0f, gainDb / 40.0f); // Amplitude
    float w0 = 2.0f * M_PI * centerFreq / sampleRate;
    float alpha = std::sin(w0) / (2.0f * Q);
    float cos_w0 = std::cos(w0);
    float beta = std::sqrt(A) / Q;

    float a0 = (A + 1.0f) - (A - 1.0f) * cos_w0 + beta * std::sin(w0);

    b0 = (A * ((A + 1.0f) + (A - 1.0f) * cos_w0 + beta * std::sin(w0))) / a0;
    b1 = (-2.0f * A * ((A - 1.0f) + (A + 1.0f) * cos_w0)) / a0;
    b2 = (A * ((A + 1.0f) + (A - 1.0f) * cos_w0 - beta * std::sin(w0))) / a0;
    a1 = (2.0f * ((A - 1.0f) - (A + 1.0f) * cos_w0)) / a0;
    a2 = ((A + 1.0f) - (A - 1.0f) * cos_w0 - beta * std::sin(w0)) / a0;

    LOGD("High Shelf Filter: F=%.1f Hz, Q=%.2f, Gain=%.1f dB", centerFreq, Q, gainDb);
}

// Peaking EQ filter - Boosts/cuts at specific frequency
void BiquadFilter::setPeaking(float sampleRate, float centerFreq, float Q, float gainDb) {
    reset();
    centerFreq = std::min(centerFreq, sampleRate / 2.0f - 1.0f);
    Q = std::max(Q, 0.1f);

    float A = std::pow(10.0f, gainDb / 40.0f); // Amplitude
    float w0 = 2.0f * M_PI * centerFreq / sampleRate;
    float alpha = std::sin(w0) / (2.0f * Q);
    float cos_w0 = std::cos(w0);

    float a0 = 1.0f + alpha / A;

    b0 = (1.0f + alpha * A) / a0;
    b1 = (-2.0f * cos_w0) / a0;
    b2 = (1.0f - alpha * A) / a0;
    a1 = (-2.0f * cos_w0) / a0;
    a2 = (1.0f - alpha / A) / a0;

    LOGD("Peaking Filter: F=%.1f Hz, Q=%.2f, Gain=%.1f dB", centerFreq, Q, gainDb);
}

float BiquadFilter::process(float in) {
    float out = b0 * in + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;

    x2 = x1;
    x1 = in;
    y2 = y1;
    y1 = out;

    return out;
}