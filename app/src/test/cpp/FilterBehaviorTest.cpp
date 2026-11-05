#include "filter/EchoCanceller.h"
#include "filter/NoiseReduction.h"
#include "filter/NoiseGate.h"
#include "filter/BiquadFilter.h"
#include "filter/PlaybackSuppressor.h"
#include "filter/EqHeadroom.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

static void testEcho() {
    EchoCanceller echo(48000);
    for (int i = 0; i < 10000; ++i) {
        const float speech = 0.2f * std::sin(i * 0.1f);
        assert(echo.process(speech) == speech);
    }
    echo.setEchoDelay(2); // 96 samples at 48 kHz.
    echo.setSuppressionAmount(1);
    std::mt19937 generator(42);
    std::uniform_real_distribution<float> noise(-0.5f, 0.5f);
    std::vector<float> reference(12000);
    double before = 0, after = 0;
    for (size_t i = 0; i < reference.size(); ++i) {
        reference[i] = noise(generator);
        const float microphone = i >= 96 ? 0.4f * reference[i - 96] : 0;
        const float output = echo.process(microphone, reference[i]);
        if (i > 2000) {
            before += microphone * microphone;
            after += output * output;
        }
    }
    assert(after < before * 0.0001); // >40 dB reduction for the modeled single echo path.
    echo.reset();
    assert(echo.process(0.2f, 0) == 0.2f);
    echo.setEchoDelay(-100);
    assert(echo.process(0.2f, 0) == 0.2f);
    echo.setEchoDelay(std::numeric_limits<float>::infinity());
    echo.setSuppressionAmount(std::numeric_limits<float>::quiet_NaN());
    assert(echo.process(0.2f, 0.2f) == 0.2f);
    assert(std::isfinite(echo.process(0.2f, std::numeric_limits<float>::quiet_NaN())));
}

static void testNoiseReduction() {
    NoiseReduction reduction;
    reduction.setReductionAmount(0);
    assert(reduction.process(0.25f) == 0.25f);
    reduction.setReductionAmount(1);
    reduction.setNoiseFloorDb(-50);
    std::mt19937 generator(73);
    std::normal_distribution<float> noise(0, 0.0003f);
    double before = 0, after = 0;
    for (int i = 0; i < 48000; ++i) {
        const float input = noise(generator);
        const float output = reduction.process(input);
        assert(std::isfinite(output));
        if (i > 1000) { before += input * input; after += output * output; }
    }
    assert(after < before * 0.1); // Noise below the configured floor is attenuated >10 dB.
    reduction.reset();
    before = after = 0;
    for (int i = 0; i < 48000; ++i) {
        const float input = 0.1f * std::sin(2 * 3.141592653589793 * 8000 * i / 48000);
        const float output = reduction.process(input);
        if (i > 1000) { before += input * input; after += output * output; }
    }
    assert(std::abs(10 * std::log10(after / before)) < 0.1); // Preserve strong treble.
    reduction.reset();
    for (int i = 0; i < 1000; ++i) assert(reduction.process(0) == 0);
    reduction.setReductionAmount(std::numeric_limits<float>::quiet_NaN());
    assert(reduction.process(0.2f) == 0.2f);
    reduction.setReductionAmount(1);
    reduction.setNoiseFloorDb(-100);
    reduction.reset();
    std::vector<float> impulseOutput;
    for (int i = 0; i < 1024; ++i) impulseOutput.push_back(reduction.process(i == 0 ? 0.5f : 0));
    assert(std::abs(impulseOutput[128] - 0.5f) < 0.001f);
    assert(reduction.latencySamples() == 128);
    // Feed the documented tail length after a short capture; the last input must survive.
    reduction.reset();
    for (int i = 0; i < 100; ++i) reduction.process(i == 99 ? 0.5f : 0);
    float tailPeak = 0;
    for (int i = 0; i < reduction.latencySamples(); ++i) {
        tailPeak = std::max(tailPeak, std::abs(reduction.process(0)));
    }
    assert(tailPeak > 0.49f);
}

static double toneGain(BiquadFilter& filter, double hz) {
    double inputEnergy = 0, outputEnergy = 0;
    for (int i = 0; i < 48000; ++i) {
        const float input = 0.1f * std::sin(2 * 3.141592653589793 * hz * i / 48000);
        const float output = filter.process(input);
        assert(std::isfinite(output));
        if (i > 10000) { inputEnergy += input * input; outputEnergy += output * output; }
    }
    return 10 * std::log10(outputEnergy / inputEnergy);
}

static void testGateEnvelope() {
    NoiseGate gate;
    gate.setThreshold(-40);
    gate.setRatio(4);
    gate.setAttack(5, 48000);
    gate.setRelease(50, 48000);
    for (int i = 0; i < 48000; ++i) gate.process(0.1f);
    float last = 1;
    for (int i = 0; i < 48000; ++i) {
        const float gain = gate.process(0.001f) / 0.001f;
        assert(std::abs(gain - last) < 0.002f);
        last = gain;
    }
    assert(last < 0.0011f);
    assert(gate.process(std::numeric_limits<float>::quiet_NaN()) == 0);
    assert(std::isfinite(gate.process(0.1f)));
}

static void testEqualizers() {
    BiquadFilter filter;
    filter.setBandpass(48000, 1500, 1.2f);
    assert(std::abs(toneGain(filter, 1500)) < 0.01);
    assert(toneGain(filter, 100) < -20);
    assert(toneGain(filter, 10000) < -15);
    filter.setPeaking(48000, 3000, 1, 6);
    assert(std::abs(toneGain(filter, 3000) - 6) < 0.01);
    filter.setPeaking(48000, 3000, 1, -6);
    assert(std::abs(toneGain(filter, 3000) + 6) < 0.01);
    filter.setPeaking(48000, 3000, 1, 0);
    assert(filter.process(0.123f) == 0.123f);
    filter.setHighShelf(48000, 8000, 0.7f, 6);
    assert(std::abs(toneGain(filter, 100)) < 0.01);
    assert(std::abs(toneGain(filter, 8000) - 3) < 0.02);
    assert(toneGain(filter, 22000) > 5.9);
    filter.setHighShelf(48000, 8000, 0.7f, 0);
    assert(filter.process(0.123f) == 0.123f);
    for (int type = 0; type < 3; ++type) {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        if (type == 0) filter.setBandpass(0, nan, -1);
        if (type == 1) filter.setPeaking(48000, 3000, 1, nan);
        if (type == 2) filter.setHighShelf(48000, nan, 1, 6);
        assert(filter.process(0.2f) == 0.2f); // Invalid configuration safely bypasses.
    }
    filter.setBandpass(48000, -100, -1);
    for (int i = 0; i < 200000; ++i) assert(std::isfinite(filter.process(i == 0 ? 1 : 0)));
    filter.reset();
    assert(filter.process(0) == 0);
    assert(filter.process(std::numeric_limits<float>::infinity()) == 0);
    assert(std::isfinite(filter.process(0.1f)));
}

static void testHeadroomAndSuppressor() {
    assert(eqHeadroomGain(false, 6, false, 3) == 1);
    assert(std::abs(eqHeadroomGain(true, 6, true, 3) - std::pow(10.0f, -9.0f / 20)) < 1e-6f);
    assert(eqHeadroomGain(true, -6, true, -3) == 1);
    BiquadFilter peak, shelf;
    peak.setPeaking(48000, 3000, 1, 6);
    shelf.setHighShelf(48000, 8000, 0.7f, 3);
    for (int i = 0; i < 48000; ++i) {
        const float input = 0.99f * std::sin(i * 0.2f);
        const float output = shelf.process(peak.process(input * eqHeadroomGain(true, 6, true, 3)));
        assert(std::abs(output) <= 1);
    }
    PlaybackSuppressor suppressor(48000);
    assert(suppressor.process(0.5f) == 0.5f);
    suppressor.setEnabled(true);
    suppressor.setAggressiveness(0.8f);
    float previous = 0.5f;
    for (int i = 0; i < 20000; ++i) {
        const float output = suppressor.process(0.5f);
        assert(std::abs(output - previous) < 0.002f);
        previous = output;
    }
    assert(std::abs(previous - 0.1f) < 0.0001f);
    suppressor.setEnabled(false);
    assert(suppressor.process(0.5f) == 0.5f);
    suppressor.setAggressiveness(std::numeric_limits<float>::quiet_NaN());
    assert(suppressor.process(0.5f) == 0.5f);
}

int main() {
    testEcho();
    testNoiseReduction();
    testGateEnvelope();
    testEqualizers();
    testHeadroomAndSuppressor();
    std::cout << "Filter behavior checks passed\n";
}
