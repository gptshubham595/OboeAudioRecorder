#ifndef OBOESAMPLE_AUDIORECORDER_H
#define OBOESAMPLE_AUDIORECORDER_H

#include <oboe/Oboe.h>
#include <vector>
#include <mutex>

class AudioRecorder : public oboe::AudioStreamDataCallback {
public:
    AudioRecorder();
    oboe::Result startRecording();
    void stopRecording();
    void getRecordingData(std::vector<int16_t>& buffer);

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream *oboeStream, void *audioData, int32_t numFrames) override;

private:
    std::shared_ptr<oboe::AudioStream> mRecordingStream;
    std::vector<int16_t> mRecordingBuffer;
    std::mutex mBufferLock;
    int32_t mSampleRate = 48000;
    int32_t mChannelCount = 1;
};

#endif //OBOESAMPLE_AUDIORECORDER_H