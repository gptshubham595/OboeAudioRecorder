#ifndef OBOESAMPLE_AUDIORECORDER_H
#define OBOESAMPLE_AUDIORECORDER_H

#include <oboe/Oboe.h>
#include <vector>
#include <mutex>
#include <fstream> // New: Include fstream for file operations

class AudioRecorder : public oboe::AudioStreamDataCallback {
public:
    AudioRecorder();

    // Modified: New method to set the file path
    void setStoragePath(const char *path);

    oboe::Result startRecording();

    void stopRecording();

    oboe::DataCallbackResult
    onAudioReady(oboe::AudioStream *oboeStream, void *audioData, int32_t numFrames) override;

    // Remove getRecordingData as we are writing to file now.
    // void getRecordingData(std::vector<int16_t>& buffer);

private:
    std::shared_ptr <oboe::AudioStream> mRecordingStream;
    // std::vector<int16_t> mRecordingBuffer; // Removed: No longer storing in memory
    std::mutex mBufferLock;
    int32_t mSampleRate = 48000;
    int32_t mChannelCount = 1;

    // New: File stream and path for chunk storage
    std::fstream mAudioFile;
    std::string mFilePath;

    // Audio metadata for WAV (optional, but good practice)
    // For simplicity here, we'll focus on raw PCM write.
    // Storing full WAV header is usually done on stop.
};

#endif //OBOESAMPLE_AUDIORECORDER_H