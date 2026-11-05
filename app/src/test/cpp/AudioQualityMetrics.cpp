#include "filter/NoiseGate.h"
#include "filter/BiquadFilter.h"
#include "filter/NoiseReduction.h"
#include <cmath>
#include <iomanip>
#include <iostream>

static double gateGain(float inputDb) {
    NoiseGate gate;
    gate.setThreshold(-40.0f);
    gate.setRatio(4.0f);
    gate.setAttack(5.0f, 48000);
    gate.setRelease(50.0f, 48000);
    const float input = std::pow(10.0f, inputDb / 20.0f);
    float output = 0;
    for (int i = 0; i < 96000; ++i) output = gate.process(input);
    return 20 * std::log10(output / input);
}

template<typename Filter>
static double toneGain(Filter& filter, double frequency) {
    double inputEnergy = 0, outputEnergy = 0;
    for (int i = 0; i < 96000; ++i) {
        const float input = 0.1 * std::sin(2 * 3.141592653589793 * frequency * i / 48000);
        const float output = filter.process(input);
        if (i >= 48000) {
            inputEnergy += input * input;
            outputEnergy += output * output;
        }
    }
    return 10 * std::log10(outputEnergy / inputEnergy);
}

int main() {
    BiquadFilter bandpass;
    bandpass.setBandpass(48000, 1500, 1.2f);
    NoiseReduction reduction;
    reduction.setReductionAmount(0.6f);
    std::cout << std::fixed << std::setprecision(4)
        << "{\"gate_gain_db_at_minus60_dbfs\":" << gateGain(-60)
        << ",\"gate_gain_db_at_minus50_dbfs\":" << gateGain(-50)
        << ",\"gate_gain_db_at_minus20_dbfs\":" << gateGain(-20)
        << ",\"voice_bandpass_gain_db_at_100hz\":" << toneGain(bandpass, 100)
        << ",\"voice_bandpass_gain_db_at_8000hz\":" << toneGain(bandpass, 8000)
        << ",\"spectral_reduction_gain_db_at_8000hz\":" << toneGain(reduction, 8000)
        << "}\n";
}
