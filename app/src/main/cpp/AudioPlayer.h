#ifndef OBOESAMPLE_AUDIOPLAYER_H
#define OBOESAMPLE_AUDIOPLAYER_H

#include <oboe/Oboe.h>
#include "AsyncAudioFile.h"
#include <atomic>
#include <fstream> // New: Include fstream for file operations
#include <string>

class AudioPlayer : public oboe::AudioStreamDataCallback {
public:
    AudioPlayer();
    ~AudioPlayer() { stopPlayback(false); }
    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;
    AudioPlayer(AudioPlayer&&) = delete;
    AudioPlayer& operator=(AudioPlayer&&) = delete;
    bool hasOpenStream() const { return bool(mPlaybackStream); }
    std::string latencyDiagnostics() const;

    bool isPlaying() const {
        if (!mPlaying.load() || !mPlaybackStream || mReader.failed()) return false;
        const auto state = mPlaybackStream->getState();
        return state == oboe::StreamState::Starting || state == oboe::StreamState::Started;
    }

    //Takes a file path instead of a data buffer
    oboe::Result startPlaybackFromFile(const char* path);
    void stopPlayback(bool reportDiagnostics = true);

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream *oboeStream, void *audioData, int32_t numFrames) override;

private:
    std::shared_ptr<oboe::AudioStream> mPlaybackStream;
    std::atomic<bool> mPlaying{false};


    // New: File stream and playback buffer
    AudioFileReader mReader;

    // Stream properties (should match recorder)
    int32_t mSampleRate = 48000;
    int32_t mChannelCount = 1;
};

#endif //OBOESAMPLE_AUDIOPLAYER_H
