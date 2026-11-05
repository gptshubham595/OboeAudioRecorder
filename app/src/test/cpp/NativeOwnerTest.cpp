#include "AudioRecorder.h"
#include "AudioPlayer.h"
#include "WavFile.h"
#include <cassert>
#include <iostream>
#include <memory>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<AudioRecorder>);
static_assert(!std::is_move_constructible_v<AudioRecorder>);
static_assert(!std::is_move_constructible_v<AudioPlayer>);

int main(int argc, char** argv) {
    assert(argc == 2);
    const auto path = std::string(argv[1]) + "/owner.wav";
    std::array<int16_t, 1000> samples{};
    samples.back() = 16384;
    for (int iteration = 0; iteration < 100; ++iteration) {
        {
            AudioRecorder recorder;
            recorder.setStoragePath(path.c_str());
            recorder.setNoiseReductionEnabled(true);
            assert(recorder.startRecording() == oboe::Result::OK);
            assert(recorder.hasOpenStream());
            assert(recorder.onAudioReady(nullptr, samples.data(), samples.size()) == oboe::DataCallbackResult::Continue);
            // Destructor must join the worker before destroying any captured DSP members.
        }
        assert(oboe::liveStreams == 0);
        {
            std::ifstream file(path, std::ios::binary);
            uint32_t bytes = 0;
            assert(wav::readHeader(file, bytes));
            assert(bytes == samples.size() * sizeof(int16_t));
        }
        {
            std::ifstream raw(path + ".input.wav", std::ios::binary);
            uint32_t bytes = 0;
            assert(wav::readHeader(raw, bytes));
            assert(bytes == samples.size() * sizeof(int16_t));
            std::array<int16_t, 1000> saved{};
            assert(raw.read(reinterpret_cast<char*>(saved.data()), bytes));
            assert(saved == samples);
        }
        {
            AudioPlayer player;
            assert(player.startPlaybackFromFile(path.c_str()) == oboe::Result::OK);
            assert(player.hasOpenStream());
            // Destroy while the reader is still prefetching: stream closes before join.
        }
        assert(oboe::liveStreams == 0);
    }
    AudioRecorder recorder;
    recorder.setStoragePath(path.c_str());
    oboe::failOpen = true;
    assert(recorder.startRecording() != oboe::Result::OK);
    assert(!recorder.hasOpenStream());
    oboe::failStart = true;
    assert(recorder.startRecording() != oboe::Result::OK);
    assert(!recorder.hasOpenStream());
    recorder.stopRecording();
    recorder.stopRecording();
    assert(oboe::liveStreams == 0);
    assert(recorder.startRecording() == oboe::Result::OK);
    recorder.stopRecording();
    assert(oboe::liveStreams == 0);
    std::cout << "Native stream/worker ownership checks passed\n";
}
