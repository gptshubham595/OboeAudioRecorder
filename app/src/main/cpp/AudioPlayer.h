#ifndef OBOESAMPLE_AUDIOPLAYER_H
#define OBOESAMPLE_AUDIOPLAYER_H

#include <oboe/Oboe.h>
#include <vector>
#include <atomic>

class AudioPlayer : public oboe::AudioStreamDataCallback {
public:
    AudioPlayer();
    oboe::Result startPlayback(const std::vector<int16_t>& data);
    void stopPlayback();

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream *oboeStream, void *audioData, int32_t numFrames) override;

private:
    std::shared_ptr<oboe::AudioStream> mPlaybackStream;
    std::vector<int16_t> mPlaybackBuffer;
    std::atomic<int64_t> mReadIndex;
    int32_t mSampleRate = 48000;
    int32_t mChannelCount = 1;
};

#endif //OBOESAMPLE_AUDIOPLAYER_H