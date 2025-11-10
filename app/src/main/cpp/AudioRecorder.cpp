#include "AudioRecorder.h"
#include <android/log.h>
#include <unistd.h>

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

    // Initialize filter with default bandpass settings
    // These values can be adjusted via configureBandpassFilter()
    mFilter.setBandpass(mSampleRate, 1000.0f, 1.0f);
}

void AudioRecorder::setStoragePath(const char *path) {
    mFilePath = path;
    LOGD("Set recording path to: %s", mFilePath.c_str());
}

void AudioRecorder::setFilterEnabled(bool enabled) {
    mFilterEnabled = enabled;
    if (enabled) {
        mFilter.reset(); // Reset filter state when enabling
        LOGD("Filter enabled");
    } else {
        LOGD("Filter disabled");
    }
}

void AudioRecorder::configureBandpassFilter(float centerFreq, float Q) {
    mFilter.setBandpass(mSampleRate, centerFreq, Q);
    LOGD("Bandpass filter configured: Center=%.1f Hz, Q=%.2f", centerFreq, Q);
}

oboe::Result AudioRecorder::startRecording() {
    std::lock_guard<std::mutex> lock(mBufferLock);

    // Open the file stream for writing (binary mode)
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

    // Reset filter state before starting
    if (mFilterEnabled) {
        mFilter.reset();
        LOGD("Filter reset for new recording");
    }

    // Configure and open the Oboe stream
    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Input)
            ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
            ->setSharingMode(oboe::SharingMode::Exclusive)
            ->setFormat(oboe::AudioFormat::I16)
            ->setChannelCount(mChannelCount)
            ->setSampleRate(mSampleRate)
            ->setDataCallback(this);

    oboe::Result result = builder.openStream(mRecordingStream);
    if (result != oboe::Result::OK) {
        LOGE("Failed to open recording stream: %s", oboe::convertToText(result));
        mAudioFile.close();
        return result;
    }

    // Update sample rate to what the stream actually opened with
    int32_t actualSampleRate = mRecordingStream->getSampleRate();
    if (actualSampleRate != mSampleRate) {
        mSampleRate = actualSampleRate;
        // Reconfigure filter with actual sample rate
        if (mFilterEnabled) {
            configureBandpassFilter(1000.0f, 1.0f); // Use default values
        }
        LOGD("Updated sample rate to: %d", mSampleRate);
    }

    result = mRecordingStream->requestStart();
    if (result != oboe::Result::OK) {
        LOGE("Failed to start recording stream: %s", oboe::convertToText(result));
        mAudioFile.close();
    } else {
        LOGD("Recording started successfully. Writing to file. Filter: %s",
             mFilterEnabled ? "ON" : "OFF");
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
    }
}

oboe::DataCallbackResult
AudioRecorder::onAudioReady(oboe::AudioStream *oboeStream, void *audioData, int32_t numFrames) {
    if (!mAudioFile.is_open()) {
        LOGE("Audio file is not open, cannot write data!");
        return oboe::DataCallbackResult::Continue;
    }

    auto *inputData = static_cast<int16_t *>(audioData);
    size_t numSamples = numFrames * oboeStream->getChannelCount();

    if (mFilterEnabled) {
        // Process audio through the filter
        // Create a temporary buffer for filtered output
        std::vector<int16_t> filteredData(numSamples);

        for (size_t i = 0; i < numSamples; i++) {
            // Convert int16_t to float for processing
            float sample = static_cast<float>(inputData[i]) / 32768.0f;

            // Apply filter
            float filtered = mFilter.process(sample);

            // Convert back to int16_t with clipping
            filtered = std::max(-1.0f, std::min(1.0f, filtered));
            filteredData[i] = static_cast<int16_t>(filtered * 32767.0f);
        }

        // Write filtered data to file
        size_t numBytes = numSamples * sizeof(int16_t);
        mAudioFile.write(reinterpret_cast<const char *>(filteredData.data()), numBytes);
    } else {
        // Write raw data directly to file (original behavior)
        size_t numBytes = numSamples * sizeof(int16_t);
        mAudioFile.write(static_cast<const char *>(audioData), numBytes);
    }

    return oboe::DataCallbackResult::Continue;
}