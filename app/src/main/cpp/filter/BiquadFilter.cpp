#include "BiquadFilter.h"
#include <android/log.h>

#define LOG_TAG "BiquadFilter"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

namespace {
bool valid(float sampleRate, float frequency, float q) {
    return std::isfinite(sampleRate) && sampleRate >= 8000 && sampleRate <= 384000 &&
            std::isfinite(frequency) && std::isfinite(q);
}
}

void BiquadFilter::setIdentity() {
    b0 = 1.0; b1 = b2 = a1 = a2 = 0.0;
    reset();
}

void BiquadFilter::reset() {
    x1 = x2 = y1 = y2 = 0.0f;
}

// Bandpass filter (already implemented)
void BiquadFilter::setBandpass(float sampleRate, float centerFreq, float Q) {
    if (!valid(sampleRate, centerFreq, Q)) { setIdentity(); return; }
    reset();
    centerFreq = std::clamp(centerFreq, 1.0f, sampleRate * 0.49f);
    Q = std::clamp(Q, 0.1f, 20.0f);

    double w0 = 2.0f * M_PI * centerFreq / sampleRate;
    double alpha = std::sin(w0) / (2.0f * Q);
    double cos_w0 = std::cos(w0);

    double a0 = 1.0f + alpha;
    b0 = alpha / a0;
    b1 = 0.0f;
    b2 = -alpha / a0;
    a1 = (-2.0f * cos_w0) / a0;
    a2 = (1.0f - alpha) / a0;

    LOGD("Bandpass Filter: F=%.1f Hz, Q=%.2f", centerFreq, Q);
}

// High Shelf filter - Boosts/cuts high frequencies
void BiquadFilter::setHighShelf(float sampleRate, float centerFreq, float Q, float gainDb) {
    if (!valid(sampleRate, centerFreq, Q)) { setIdentity(); return; }
    reset();
    centerFreq = std::clamp(centerFreq, 1.0f, sampleRate * 0.49f);
    Q = std::clamp(Q, 0.1f, 0.70710678f);

    if (!std::isfinite(gainDb)) { setIdentity(); return; }
    gainDb = std::clamp(gainDb, -24.0f, 24.0f);
    if (gainDb == 0.0f) { setIdentity(); return; }
    double A = std::pow(10.0, static_cast<double>(gainDb) / 40.0); // Amplitude
    double w0 = 2.0f * M_PI * centerFreq / sampleRate;
    double cos_w0 = std::cos(w0);
    double beta = std::sqrt(A) / Q;

    double a0 = (A + 1.0f) - (A - 1.0f) * cos_w0 + beta * std::sin(w0);

    b0 = (A * ((A + 1.0f) + (A - 1.0f) * cos_w0 + beta * std::sin(w0))) / a0;
    b1 = (-2.0f * A * ((A - 1.0f) + (A + 1.0f) * cos_w0)) / a0;
    b2 = (A * ((A + 1.0f) + (A - 1.0f) * cos_w0 - beta * std::sin(w0))) / a0;
    a1 = (2.0f * ((A - 1.0f) - (A + 1.0f) * cos_w0)) / a0;
    a2 = ((A + 1.0f) - (A - 1.0f) * cos_w0 - beta * std::sin(w0)) / a0;

    LOGD("High Shelf Filter: F=%.1f Hz, Q=%.2f, Gain=%.1f dB", centerFreq, Q, gainDb);
}

// Peaking EQ filter - Boosts/cuts at specific frequency
void BiquadFilter::setPeaking(float sampleRate, float centerFreq, float Q, float gainDb) {
    if (!valid(sampleRate, centerFreq, Q)) { setIdentity(); return; }
    reset();
    centerFreq = std::clamp(centerFreq, 1.0f, sampleRate * 0.49f);
    Q = std::clamp(Q, 0.1f, 20.0f);

    if (!std::isfinite(gainDb)) { setIdentity(); return; }
    gainDb = std::clamp(gainDb, -24.0f, 24.0f);
    if (gainDb == 0.0f) { setIdentity(); return; }
    double A = std::pow(10.0, static_cast<double>(gainDb) / 40.0); // Amplitude
    double w0 = 2.0f * M_PI * centerFreq / sampleRate;
    double alpha = std::sin(w0) / (2.0f * Q);
    double cos_w0 = std::cos(w0);

    double a0 = 1.0f + alpha / A;

    b0 = (1.0f + alpha * A) / a0;
    b1 = (-2.0f * cos_w0) / a0;
    b2 = (1.0f - alpha * A) / a0;
    a1 = (-2.0f * cos_w0) / a0;
    a2 = (1.0f - alpha / A) / a0;

    LOGD("Peaking Filter: F=%.1f Hz, Q=%.2f, Gain=%.1f dB", centerFreq, Q, gainDb);
}

float BiquadFilter::process(float in) {
    if (!std::isfinite(in)) { reset(); return 0.0f; }
    const double out = b0 * in + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    if (!std::isfinite(out)) { reset(); return 0.0f; }
    x2 = x1;
    x1 = std::abs(in) < 1e-20f ? 0.0 : in;
    y2 = y1;
    y1 = std::abs(out) < 1e-20 ? 0.0 : out;
    return static_cast<float>(y1);
}
