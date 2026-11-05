#include "WaveformMonitor.h"
#include <atomic>
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>

int main() {
    WaveformMonitor monitor;
    std::array<float, WaveformMonitor::snapshotFloats> snapshot{};
    monitor.reset(0);
    assert(monitor.snapshot(snapshot.data()) == 0);
    std::array<int16_t, 100> input{}, output{};
    input.fill(16384); output.fill(8192);
    monitor.append(input.data(), output.data(), input.size());
    assert(monitor.snapshot(snapshot.data()) == 1);
    assert(snapshot[0] == 0.5f && snapshot[1] == 0.5f);
    assert(snapshot[2] == 0.25f && snapshot[3] == 0.25f);
    assert(snapshot[6] == 100 && snapshot[7] == 1);
    const size_t last = 8 + 479 * 4;
    assert(snapshot[last] == 0.5f && snapshot[last + 2] == 0.25f);

    monitor.reset(0);
    input.fill(0); output.fill(0); input[31] = 32767; output[31] = -32768;
    monitor.append(input.data(), output.data(), input.size());
    monitor.snapshot(snapshot.data());
    assert(snapshot[4] == 1 && snapshot[5] == 1);
    assert(snapshot[last + 1] > 0.9999f && snapshot[last + 2] == -1);
    assert(std::abs(snapshot[1] - 0.1f) < 0.0001f); // Peak retained; RMS is independently computed.

    monitor.reset(128);
    std::vector<int16_t> raw(428), processed(428);
    for (size_t i = 0; i < 300; ++i) raw[i] = static_cast<int16_t>((int(i) - 150) * 100);
    for (size_t i = 128; i < processed.size(); ++i) processed[i] = raw[i - 128];
    for (size_t i = 0; i < raw.size(); i += 37) {
        monitor.append(raw.data() + i, processed.data() + i, std::min<size_t>(37, raw.size() - i));
    }
    monitor.finish();
    monitor.snapshot(snapshot.data());
    assert(snapshot[6] == 300 && snapshot[7] == 3);
    assert(snapshot[0] == snapshot[2] && snapshot[1] == snapshot[3]);
    for (size_t i = 8; i < snapshot.size(); i += 4) {
        assert(snapshot[i] == snapshot[i + 2] && snapshot[i + 1] == snapshot[i + 3]);
    }

    monitor.reset(0);
    std::atomic<bool> done{false};
    std::thread producer([&] {
        input.fill(16384); output.fill(8192);
        for (int i = 0; i < 2000; ++i) monitor.append(input.data(), output.data(), input.size());
        monitor.finish();
        done.store(true);
    });
    uint64_t sequence = 0;
    do {
        const auto next = monitor.snapshot(snapshot.data());
        assert(next >= sequence);
        sequence = next;
        assert(snapshot[6] <= 48000 && snapshot[7] <= 480);
        for (float value : snapshot) assert(std::isfinite(value));
    } while (!done.load());
    producer.join();
    monitor.snapshot(snapshot.data());
    assert(snapshot[6] == 48000 && snapshot[7] == 480);
    assert(snapshot[1] == 0.5f && snapshot[3] == 0.25f);
    monitor.reset(0);
    assert(monitor.snapshot(snapshot.data()) == 0);
    for (float value : snapshot) assert(value == 0);
    std::cout << "Paired waveform alignment/metrics checks passed\n";
}
