#include "AudioRecorder.h"
#include <android/log.h>
#include <unistd.h> // For file checks (optional)

#define LOG_TAG "AudioRecorder"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

AudioRecorder::AudioRecorder() {
    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Input);
    std::shared_ptr <oboe::AudioStream> testStream;
    if (builder.openStream(testStream) == oboe::Result::OK) {
        mSampleRate = testStream->getSampleRate();
        mChannelCount = testStream->getChannelCount();
        testStream->close();
    }
    LOGD("Recorder initialized. Sample Rate: %d, Channels: %d", mSampleRate, mChannelCount);
}

// New method to set the path
void AudioRecorder::setStoragePath(const char *path) {
    mFilePath = path;
    LOGD("Set recording path to: %s", mFilePath.c_str());
}

oboe::Result AudioRecorder::startRecording() {
    std::lock_guard <std::mutex> lock(mBufferLock);

    // 1. Open the file stream for writing (binary mode)
    if (mFilePath.empty()) {
        LOGE("Recording path not set!");
        return oboe::Result::ErrorInternal;
    }
    mAudioFile.open(mFilePath, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!mAudioFile.is_open()) {
        LOGE("Failed to open file for recording: %s", mFilePath.c_str());
        return oboe::Result::ErrorInternal;
    }
    LOGD("File opened successfully.");

    // 2. Configure and open the Oboe stream
    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Input)
            ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
            ->setSharingMode(oboe::SharingMode::Exclusive)
            ->setFormat(oboe::AudioFormat::I16) // 16-bit PCM (2 bytes per sample)
            ->setChannelCount(mChannelCount)
            ->setSampleRate(mSampleRate)
            ->setDataCallback(this);

    oboe::Result result = builder.openStream(mRecordingStream);
    if (result != oboe::Result::OK) {
        LOGE("Failed to open recording stream: %s", oboe::convertToText(result));
        mAudioFile.close();
        return result;
    }

    // Set sample rate to what the stream actually opened with
    mSampleRate = mRecordingStream->getSampleRate();

    result = mRecordingStream->requestStart();
    if (result != oboe::Result::OK) {
        LOGE("Failed to start recording stream: %s", oboe::convertToText(result));
        mAudioFile.close();
    } else {
        LOGD("Recording started successfully. Writing to file.");
    }
    return result;
}

void AudioRecorder::stopRecording() {
    if (mRecordingStream && mRecordingStream->getState() != oboe::StreamState::Stopped) {
        mRecordingStream->requestStop();
        mRecordingStream->close();
        mRecordingStream.reset();
        LOGD("Recording stream stopped.");
    }

    // Close the file stream
    if (mAudioFile.is_open()) {
        mAudioFile.close();
        LOGD("Audio file closed.");
        // Note: If you were writing a WAV file, the WAV header would be written/updated here.
    }
}

oboe::DataCallbackResult
AudioRecorder::onAudioReady(oboe::AudioStream *oboeStream, void *audioData, int32_t numFrames) {
    // We are writing raw 16-bit PCM (I16) data.
    // 1 frame = mChannelCount samples. 1 sample = 2 bytes (I16).

    size_t numBytes = numFrames * oboeStream->getChannelCount() * sizeof(int16_t);
    // 2KB chunk size is 2048 bytes. Oboe writes data in bursts (numBytes),
    // and this burst often is your "chunk" size or smaller.

    if (mAudioFile.is_open()) {
        mAudioFile.write(static_cast<const char *>(audioData), numBytes);
        // The data is written to the file system (or a buffer) here.
        // Oboe handles the chunks being available in `audioData`.
    } else {
        LOGE("Audio file is not open, cannot write data!");
    }

    return oboe::DataCallbackResult::Continue;
}