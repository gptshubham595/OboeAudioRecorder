#ifndef OBOESAMPLE_AUDIOPLAYER_H
#define OBOESAMPLE_AUDIOPLAYER_H

#include <oboe/Oboe.h>
#include <vector>
#include <atomic>
#include <fstream> // New: Include fstream for file operations
#include <string>

class AudioPlayer : public oboe::AudioStreamDataCallback {
public:
    AudioPlayer();

    oboe::Result startPlayback(const std::vector<int16_t>& data);

    //Takes a file path instead of a data buffer
    oboe::Result startPlaybackFromFile(const char* path);
    void stopPlayback();

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream *oboeStream, void *audioData, int32_t numFrames) override;

private:
    std::shared_ptr<oboe::AudioStream> mPlaybackStream;
    std::vector<int16_t> mPlaybackBuffer;
    std::atomic<int64_t> mReadIndex;

    // New: File stream and playback buffer
    std::ifstream mAudioFile;

    // Buffer for reading chunks from the file before playback
    std::vector<int16_t> mReadBuffer;

    // Stream properties (should match recorder)
    int32_t mSampleRate = 48000;
    int32_t mChannelCount = 1;
};

#endif //OBOESAMPLE_AUDIOPLAYER_H