#include "AsyncAudioFile.h"
#include "WavFile.h"
#include "filter/NoiseReduction.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>
#include <memory>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<AudioFileWriter>);
static_assert(!std::is_move_constructible_v<AudioFileWriter>);
static_assert(!std::is_copy_constructible_v<AudioFileReader>);

static void testQueue() {
    SpscAudioQueue<8> queue;
    const int16_t input[] = {1,2,3,4,5,6,7,8,9};
    int16_t output[12]{};
    assert(!queue.push(input, 9));
    assert(queue.push(input, 8));
    assert(!queue.push(input, 1));
    assert(queue.pop(output, 5) == 5);
    for (int i = 0; i < 5; ++i) assert(output[i] == i + 1);
    assert(queue.push(input, 5));
    assert(queue.pop(output, 12) == 8);
    for (int i = 0; i < 8; ++i) assert(output[i] == (i < 3 ? i + 6 : i - 2));
    assert(queue.pop(output, 1) == 0);
    queue.reset();
    assert(queue.writeAvailable() == 8);

    SpscAudioQueue<1024> concurrent;
    constexpr size_t total = 1000000;
    std::thread producer([&] {
        size_t sent = 0;
        std::array<int16_t, 73> block{};
        while (sent < total) {
            const size_t count = std::min(block.size(), total - sent);
            for (size_t i = 0; i < count; ++i) block[i] = static_cast<int16_t>((sent + i) % 30000);
            if (concurrent.push(block.data(), count)) sent += count;
            else std::this_thread::yield();
        }
    });
    size_t received = 0;
    std::array<int16_t, 117> block{};
    while (received < total) {
        const size_t count = concurrent.pop(block.data(), block.size());
        for (size_t i = 0; i < count; ++i) assert(block[i] == (received + i) % 30000);
        received += count;
        if (!count) std::this_thread::yield();
    }
    producer.join();
}

static std::vector<int16_t> readWav(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    uint32_t bytes = 0;
    assert(wav::readHeader(file, bytes));
    std::vector<int16_t> data(bytes / 2);
    assert(file.read(reinterpret_cast<char*>(data.data()), bytes));
    return data;
}

static void testWriter(const std::string& directory) {
    AudioFileWriter writer;
    const std::string path = directory + "/roundtrip.wav";
    std::vector<int16_t> samples(4800);
    for (size_t i = 0; i < samples.size(); ++i) samples[i] = i % 4096 - 2048;
    assert(writer.start(path.c_str(), [](int16_t* data, size_t count) {
        // Simulate slower DSP/I/O without blocking the producer.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        for (size_t i = 0; i < count; ++i) data[i] *= 2;
    }, 5));
    assert(writer.enqueue(samples.data(), samples.size()));
    writer.stop(); // Must drain the queue before writing the tail/header.
    assert(!writer.failed());
    assert(writer.overruns() == 0);
    auto data = readWav(path);
    assert(data.size() == samples.size() + 5);
    for (size_t i = 0; i < samples.size(); ++i) assert(data[i] == samples[i] * 2);
    for (size_t i = samples.size(); i < data.size(); ++i) assert(data[i] == 0);
    writer.stop();
    assert(readWav(path) == data);

    // Repeated immediate shutdown exercises the stop/enqueue publication race.
    for (int i = 0; i < 100; ++i) {
        assert(writer.start(path.c_str(), {}, 0));
        assert(writer.enqueue(samples.data(), 17));
        writer.stop();
        data = readWav(path);
        assert(data == std::vector<int16_t>(samples.begin(), samples.begin() + 17));
    }
    assert(writer.start(path.c_str(), {}, 0));
    std::vector<int16_t> tooLarge(AudioFileWriter::capacity + 1);
    assert(!writer.enqueue(tooLarge.data(), tooLarge.size()));
    writer.stop();
    assert(writer.failed() && writer.overruns() == 1);
    assert(!writer.start(directory.c_str(), {}, 0));
    assert(writer.failed());

    int processedBlocks = 0;
    assert(writer.start(path.c_str(), [&](int16_t*, size_t) {
        if (++processedBlocks == 2) throw std::runtime_error("simulated DSP failure");
    }, 0));
    assert(writer.enqueue(samples.data(), 1024));
    writer.stop();
    assert(writer.failed());
    data = readWav(path);
    assert(data.size() == 512); // The successful prefix still has a valid finalized header.

    NoiseReduction reduction;
    reduction.setReductionAmount(1);
    reduction.setNoiseFloorDb(-100);
    assert(writer.start(path.c_str(), [&](int16_t* block, size_t count) {
        for (size_t i = 0; i < count; ++i) block[i] = reduction.process(block[i] / 32768.0f) * 32767;
    }, reduction.latencySamples()));
    std::array<int16_t, 100> impulse{};
    impulse.back() = 16384;
    assert(writer.enqueue(impulse.data(), impulse.size()));
    writer.stop();
    data = readWav(path);
    assert(data.size() == impulse.size() + reduction.latencySamples());
    assert(data.back() > 16000); // Last captured sample survives worker drain.
}

static void testReader(const std::string& directory) {
    const std::string path = directory + "/playback.wav";
    std::vector<int16_t> expected(20003);
    for (size_t i = 0; i < expected.size(); ++i) expected[i] = i % 10000;
    {
        std::ofstream file(path, std::ios::binary);
        wav::writeHeader(file, expected.size() * 2);
        file.write(reinterpret_cast<const char*>(expected.data()), expected.size() * 2);
    }
    AudioFileReader reader;
    assert(!reader.start((directory + "/missing.wav").c_str()));
    assert(reader.start(path.c_str()));
    std::vector<int16_t> output(AudioFileReader::capacity + 1);
    auto result = reader.read(output.data(), output.size());
    assert(!result.finished && !result.failed);
    assert(result.samples <= AudioFileReader::capacity);
    assert(reader.underruns() == 1); // Oversized callback deterministically starves the queue.
    size_t received = result.samples;
    for (size_t i = 0; i < result.samples; ++i) assert(output[i] == expected[i]);
    for (size_t i = result.samples; i < output.size(); ++i) assert(output[i] == 0);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!result.finished) {
        result = reader.read(output.data(), 137);
        assert(!result.failed);
        for (size_t i = 0; i < result.samples; ++i) assert(output[i] == expected[received + i]);
        for (size_t i = result.samples; i < 137; ++i) assert(output[i] == 0);
        received += result.samples;
        if (!result.samples) std::this_thread::yield();
        assert(std::chrono::steady_clock::now() < deadline);
    }
    assert(received == expected.size());
    reader.stop();
    reader.stop();
    // Restart after EOF/failure must reset stream flags, queue contents and counters.
    assert(reader.start(path.c_str()));
    result = reader.read(output.data(), 10);
    assert(result.samples == 10 && !result.failed && !result.finished);
    for (size_t i = 0; i < 10; ++i) assert(output[i] == expected[i]);
    reader.stop();
    std::fill(output.begin(), output.end(), 123);
    result = reader.read(output.data(), output.size());
    assert(result.samples == 0 && result.finished);
    for (auto value : output) assert(value == 0);
}

static void testCapturedLifetime(const std::string& directory) {
    AudioFileWriter writer;
    const auto path = directory + "/lifetime.wav";
    auto payload = std::make_shared<std::array<int16_t, 8192>>();
    std::weak_ptr<std::array<int16_t, 8192>> weak = payload;
    assert(writer.start(path.c_str(), [payload](int16_t* data, size_t count) {
        if (count) data[0] = (*payload)[0];
    }, 0));
    payload.reset();
    assert(!weak.expired()); // The active writer deliberately owns its processor capture.
    writer.stop();
    assert(weak.expired()); // Stop must release it, not wait for the writer's destructor.

    payload = std::make_shared<std::array<int16_t, 8192>>();
    weak = payload;
    assert(!writer.start(directory.c_str(), [payload](int16_t*, size_t) {}, 0));
    payload.reset();
    assert(weak.expired()); // File-open failure must also release the capture.

    payload = std::make_shared<std::array<int16_t, 8192>>();
    weak = payload;
    {
        AudioFileWriter active;
        assert(active.start(path.c_str(), [payload](int16_t*, size_t) {}, 0));
        payload.reset();
    }
    assert(weak.expired()); // Destruction with a running worker must join and release it.
    writer.stop();
}

static void testPairedFiles(const std::string& directory) {
    const auto path = directory + "/paired.wav";
    const auto rawPath = directory + "/paired-input.wav";
    for (size_t length : {3, 37, 1000}) {
        AudioFileWriter writer;
        std::vector<int16_t> samples(length);
        for (size_t i = 0; i < length; ++i) samples[i] = static_cast<int16_t>(int(i) - 100);
        std::array<int16_t, 7> delay{};
        size_t position = 0;
        assert(writer.start(path.c_str(), [&](int16_t* data, size_t count) {
            for (size_t i = 0; i < count; ++i) {
                const auto delayed = delay[position];
                delay[position] = data[i];
                data[i] = delayed;
                position = (position + 1) % delay.size();
            }
        }, 7, rawPath.c_str(), 7));
        assert(writer.enqueue(samples.data(), samples.size()));
        writer.stop();
        assert(!writer.failed());
        assert(readWav(path) == samples);
        assert(readWav(rawPath) == samples); // No synthetic tail samples in input WAV.
    }
    AudioFileWriter failed;
    assert(!failed.start(path.c_str(), {}, 0, directory.c_str()));
    assert(failed.failed());
}

int main(int argc, char** argv) {
    assert(argc == 2);
    testQueue();
    testWriter(argv[1]);
    testReader(argv[1]);
    testCapturedLifetime(argv[1]);
    testPairedFiles(argv[1]);
    std::cout << "Async audio and queue checks passed\n";
}
