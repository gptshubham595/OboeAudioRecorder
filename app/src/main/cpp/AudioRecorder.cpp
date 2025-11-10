#include "AudioRecorder.h"
#include <android/log.h>

#define LOG_TAG "AudioRecorder"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

AudioRecorder::AudioRecorder() {
    // Determine the optimal sample rate from the device's default input stream
    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Input);
    std::shared_ptr<oboe::AudioStream> testStream;
    oboe::Result result = builder.openStream(testStream);
    if (result == oboe::Result::OK) {
        mSampleRate = testStream->getSampleRate();
        mChannelCount = testStream->getChannelCount();
        testStream->close();
    }
    LOGD("Recorder initialized. Sample Rate: %d, Channels: %d", mSampleRate, mChannelCount);
}

oboe::Result AudioRecorder::startRecording() {
    std::lock_guard<std::mutex> lock(mBufferLock);
    mRecordingBuffer.clear(); // Clear previous recording

    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Input)
            ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
            ->setSharingMode(oboe::SharingMode::Exclusive)
            ->setFormat(oboe::AudioFormat::I16) // 16-bit PCM for simple storage
            ->setChannelCount(mChannelCount)
            ->setSampleRate(mSampleRate)
            ->setDataCallback(this);

    oboe::Result result = builder.openStream(mRecordingStream);
    if (result != oboe::Result::OK) {
        LOGE("Failed to open recording stream: %s", oboe::convertToText(result));
        return result;
    }

    // Set sample rate to what the stream actually opened with
    mSampleRate = mRecordingStream->getSampleRate();

    result = mRecordingStream->requestStart();
    if (result != oboe::Result::OK) {
        LOGE("Failed to start recording stream: %s", oboe::convertToText(result));
    } else {
        LOGD("Recording started successfully.");
    }
    return result;
}

void AudioRecorder::stopRecording() {
    if (mRecordingStream && mRecordingStream->getState() != oboe::StreamState::Stopped) {
        mRecordingStream->requestStop();
        mRecordingStream->close();
        mRecordingStream.reset();
        LOGD("Recording stopped.");
    }
}

oboe::DataCallbackResult AudioRecorder::onAudioReady(oboe::AudioStream *oboeStream, void *audioData, int32_t numFrames) {
    auto *inputData = static_cast<const int16_t *>(audioData);
    size_t numSamples = numFrames * oboeStream->getChannelCount();

    std::lock_guard<std::mutex> lock(mBufferLock);
    // Append the new audio data to the buffer
    mRecordingBuffer.insert(
            mRecordingBuffer.end(),
            inputData,
            inputData + numSamples
    );

    return oboe::DataCallbackResult::Continue;
}

void AudioRecorder::getRecordingData(std::vector<int16_t>& buffer) {
    std::lock_guard<std::mutex> lock(mBufferLock);
    buffer = mRecordingBuffer; // Copy the data
}