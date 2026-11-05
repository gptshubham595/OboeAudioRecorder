#include "NoiseReduction.h"
#include <algorithm>
#include <cmath>

namespace { constexpr float pi = 3.14159265358979323846f; }

NoiseReduction::NoiseReduction() {
    for (size_t i = 0; i < kSize; ++i) {
        // Sine windows at 50% overlap have unity summed squared gain.
        mWindow[i] = std::sin(pi * (static_cast<float>(i) + 0.5f) / kSize);
        size_t reversed = 0;
        for (size_t value = i, bit = kSize >> 1; bit; bit >>= 1, value >>= 1) {
            if (value & 1) reversed |= bit;
        }
        mBitReversed[i] = reversed;
    }
    for (size_t i = 0; i < mTwiddles.size(); ++i) {
        const float angle = -2.0f * pi * i / kSize;
        mTwiddles[i] = {std::cos(angle), std::sin(angle)};
    }
    setNoiseFloorDb(-60.0f);
}

void NoiseReduction::setReductionAmount(float amount) {
    const float next = std::isfinite(amount) ? std::clamp(amount, 0.0f, 1.0f) : 0.0f;
    if ((next == 0.0f) != (mReductionAmount == 0.0f)) reset();
    mReductionAmount = next;
}

void NoiseReduction::setNoiseFloorDb(float db) {
    db = std::isfinite(db) ? std::clamp(db, -100.0f, -20.0f) : -60.0f;
    // Expected FFT-bin power for white noise through the analysis window.
    mNoisePower = std::pow(10.0f, db / 10.0f) * (kSize / 2);
}

void NoiseReduction::fft(bool inverse) {
    for (size_t i = 0; i < kSize; ++i) {
        const size_t j = mBitReversed[i];
        if (i < j) std::swap(mSpectrum[i], mSpectrum[j]);
    }
    for (size_t length = 2; length <= kSize; length <<= 1) {
        const size_t stride = kSize / length;
        for (size_t offset = 0; offset < kSize; offset += length) {
            for (size_t j = 0; j < length / 2; ++j) {
                const auto phase = inverse ? std::conj(mTwiddles[j * stride]) : mTwiddles[j * stride];
                const auto even = mSpectrum[offset + j];
                const auto odd = mSpectrum[offset + j + length / 2] * phase;
                mSpectrum[offset + j] = even + odd;
                mSpectrum[offset + j + length / 2] = even - odd;
            }
        }
    }
    if (inverse) for (auto& value : mSpectrum) value /= kSize;
}

void NoiseReduction::processFrame() {
    for (size_t i = 0; i < kSize; ++i) {
        mSpectrum[i] = mInput[(mPosition + i) & (kSize - 1)] * mWindow[i];
    }
    fft(false);
    // Share real-valued gains between conjugate bins. Retain a 10% wet gain floor
    // to limit isolated spectral holes; amount=0 is a true bypass in process().
    for (size_t i = 0; i <= kSize / 2; ++i) {
        const float power = std::norm(mSpectrum[i]);
        const float wetGain = std::max(0.1f,
                std::sqrt(std::max(0.0f, 1.0f - mNoisePower / (power + 1e-20f))));
        const float gain = 1.0f - mReductionAmount + mReductionAmount * wetGain;
        mSpectrum[i] *= gain;
        if (i > 0 && i < kSize / 2) mSpectrum[kSize - i] *= gain;
    }
    fft(true);
    for (size_t i = 0; i < kSize; ++i) {
        mOutput[(mPosition + i) & (kSize - 1)] += mSpectrum[i].real() * mWindow[i];
    }
}

float NoiseReduction::process(float input) {
    if (!std::isfinite(input)) input = 0.0f;
    if (mReductionAmount == 0.0f) return input;
    const float output = mOutput[mPosition];
    mOutput[mPosition] = 0.0f;
    mInput[mPosition] = input;
    mPosition = (mPosition + 1) & (kSize - 1);
    if (++mSinceHop == kHop) {
        mSinceHop = 0;
        processFrame();
    }
    return output;
}

void NoiseReduction::reset() {
    mInput.fill(0.0f);
    mOutput.fill(0.0f);
    mSpectrum.fill({0.0f, 0.0f});
    mPosition = mSinceHop = 0;
}
