#include "WavFile.h"
#include "filter/PlaybackSuppressor.h"
#include "filter/NoiseGate.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <sstream>
#include <limits>

static bool accepts(const std::string& bytes) {
    std::istringstream stream(bytes, std::ios::binary);
    uint32_t size = 0;
    return wav::readHeader(stream, size);
}

int main() {
    std::ostringstream output(std::ios::binary);
    wav::writeHeader(output, 4);
    output.write("\x00\x00\xff\x7f", 4);
    const auto valid = output.str();
    assert(valid.size() == 48);
    assert(accepts(valid));
    assert(!accepts(valid.substr(0, 43)));
    assert(!accepts(valid.substr(0, 47)));
    auto stereo = valid;
    stereo[22] = 2;
    assert(!accepts(stereo));
    auto wrongRate = valid;
    wrongRate[24] = 0;
    assert(!accepts(wrongRate));
    auto oddSamples = valid;
    oddSamples[40] = 3;
    assert(!accepts(oddSamples));
    assert(!accepts(std::string(100, '\0')));
    std::ostringstream empty(std::ios::binary);
    wav::writeHeader(empty, 0);
    assert(!accepts(empty.str()));

    // The header is finalized after capture without overwriting the sample payload.
    std::stringstream recording(std::ios::in | std::ios::out | std::ios::binary);
    wav::writeHeader(recording, 0);
    recording.write("\x00\x00\xff\x7f", 4);
    recording.seekp(0);
    wav::writeHeader(recording, 4);
    assert(recording.str() == valid);

    PlaybackSuppressor suppressor(48000);
    suppressor.setAggressiveness(0.8f);
    assert(suppressor.process(0.5f) == 0.5f);
    suppressor.setEnabled(true);
    for (int i = 0; i < 10000; ++i) suppressor.process(0.5f);
    // Suppression must persist throughout the following window, not just its last sample.
    for (int i = 0; i < 99; ++i) {
        assert(std::abs(suppressor.process(0.5f) - 0.1f) < 0.0001f);
    }
    suppressor.reset();
    assert(suppressor.process(0.5f) == 0.5f);
    suppressor.setEnabled(false);
    assert(suppressor.process(0.5f) == 0.5f);
    NoiseGate gate;
    gate.setThreshold(-40);
    gate.setRatio(4);
    gate.setAttack(0, 48000);
    gate.setRelease(0, 48000);
    // 20 dB below threshold at 4:1 requires 60 dB of gain reduction.
    assert(std::abs(gate.process(0.001f) - 0.000001f) < 1e-10f);
    // 10 dB below threshold requires 30 dB of gain reduction.
    assert(std::abs(gate.process(std::pow(10.0f, -50.0f / 20)) - 0.0001f) < 1e-8f);
    assert(gate.process(0) == 0);
    assert(gate.process(0.1f) == 0.1f);
    assert(gate.process(-0.1f) == -0.1f);
    gate.setRatio(1);
    assert(gate.process(0.001f) == 0.001f);
    gate.setThreshold(-10000);
    gate.setRatio(std::numeric_limits<float>::infinity());
    gate.setAttack(std::numeric_limits<float>::quiet_NaN(), 48000);
    gate.setRelease(5, 0);
    assert(std::isfinite(gate.process(0)));
    assert(std::isfinite(gate.process(0.001f)));
    gate.reset();
    assert(gate.process(0) == 0);
    std::cout << "Audio regression checks passed\n";
}
