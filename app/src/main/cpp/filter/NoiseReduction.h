#pragma once
#include <array>
#include <complex>
#include <cstddef>

// Fixed-size, overlapping spectral attenuation. Latency: 128 samples when active.
// A configured stationary noise floor is required; this is not source separation.
class NoiseReduction {
public:
    NoiseReduction();
    void setReductionAmount(float amount);
    void setNoiseFloorDb(float noiseFloorDb);
    float process(float input);
    void reset();
    int latencySamples() const { return mReductionAmount > 0.0f ? kSize : 0; }
private:
    static constexpr size_t kSize = 128;
    static constexpr size_t kHop = kSize / 2;
    void processFrame();
    void fft(bool inverse);
    std::array<float, kSize> mInput{};
    std::array<float, kSize> mOutput{};
    std::array<float, kSize> mWindow{};
    std::array<std::complex<float>, kSize> mSpectrum{};
    std::array<std::complex<float>, kSize / 2> mTwiddles{};
    std::array<size_t, kSize> mBitReversed{};
    size_t mPosition = 0;
    size_t mSinceHop = 0;
    float mReductionAmount = 0.5f;
    float mNoisePower = 0.0f;
};
