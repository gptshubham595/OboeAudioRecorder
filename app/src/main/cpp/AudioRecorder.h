#ifndef OBOESAMPLE_AUDIORECORDER_H
#define OBOESAMPLE_AUDIORECORDER_H

#include <oboe/Oboe.h>
#include <vector>
#include <mutex>
#include <fstream>
#include "filter/BiquadFilter.h" // Add this include

class AudioRecorder : public oboe::AudioStreamDataCallback {
public:
    AudioRecorder();

    void setStoragePath(const char *path);

    oboe::Result startRecording();

    void stopRecording();

    oboe::DataCallbackResult
    onAudioReady(oboe::AudioStream *oboeStream, void *audioData, int32_t numFrames) override;

    // New: Enable/disable filtering
    void setFilterEnabled(bool enabled);

    // New: Configure the bandpass filter
    void configureBandpassFilter(float centerFreq, float Q);

private:
    std::shared_ptr<oboe::AudioStream> mRecordingStream;
    std::mutex mBufferLock;
    int32_t mSampleRate = 48000;
    int32_t mChannelCount = 1;

    // File stream and path for storage
    std::fstream mAudioFile;
    std::string mFilePath;

    // New: Filter members
    BiquadFilter mFilter;
    bool mFilterEnabled = false;
};

#endif //OBOESAMPLE_AUDIORECORDER_H