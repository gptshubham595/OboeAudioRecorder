#include "AudioPlayer.h"
#include <android/log.h>
#include <algorithm>

#define LOG_TAG "AudioPlayer"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

AudioPlayer::AudioPlayer() : mReadIndex(0) {
    // Determine the optimal sample rate from the device's default output stream
    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Output);
    std::shared_ptr<oboe::AudioStream> testStream;
    oboe::Result result = builder.openStream(testStream);
    if (result == oboe::Result::OK) {
        mSampleRate = testStream->getSampleRate();
        mChannelCount = testStream->getChannelCount();
        testStream->close();
    }
    LOGD("Player initialized. Sample Rate: %d, Channels: %d", mSampleRate, mChannelCount);
}

oboe::Result AudioPlayer::startPlayback(const std::vector<int16_t>& data) {
    if (data.empty()) {
        LOGE("Cannot start playback, audio data is empty.");
        return oboe::Result::ErrorInvalidState;
    }
    mPlaybackBuffer = data;
    mReadIndex = 0;

    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Output)
            ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
            ->setSharingMode(oboe::SharingMode::Exclusive)
            ->setFormat(oboe::AudioFormat::I16)
            ->setChannelCount(mChannelCount)
            ->setSampleRate(mSampleRate)
            ->setDataCallback(this);

    oboe::Result result = builder.openStream(mPlaybackStream);
    if (result != oboe::Result::OK) {
        LOGE("Failed to open playback stream: %s", oboe::convertToText(result));
        return result;
    }

    // Set sample rate to what the stream actually opened with
    mSampleRate = mPlaybackStream->getSampleRate();

    result = mPlaybackStream->requestStart();
    if (result != oboe::Result::OK) {
        LOGE("Failed to start playback stream: %s", oboe::convertToText(result));
    } else {
        LOGD("Playback started successfully. Data size: %zu", mPlaybackBuffer.size());
    }
    return result;
}

oboe::Result AudioPlayer::startPlaybackFromFile(const char* path) {

    // 1. Open the file stream for reading
    mAudioFile.open(path, std::ios::in | std::ios::binary);
    if (!mAudioFile.is_open()) {
        LOGE("Failed to open file for playback: %s", path);
        return oboe::Result::ErrorInternal;
    }
    LOGD("Playback file opened successfully: %s", path);

    // 2. Configure and open the Oboe stream
    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Output)
            ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
            ->setSharingMode(oboe::SharingMode::Exclusive)
            ->setFormat(oboe::AudioFormat::I16)
            ->setChannelCount(mChannelCount)
            ->setSampleRate(mSampleRate)
            ->setDataCallback(this);

    oboe::Result result = builder.openStream(mPlaybackStream);
    if (result != oboe::Result::OK) {
        LOGE("Failed to open playback stream: %s", oboe::convertToText(result));
        mAudioFile.close();
        return result;
    }

    mSampleRate = mPlaybackStream->getSampleRate();

    result = mPlaybackStream->requestStart();
    if (result != oboe::Result::OK) {
        LOGE("Failed to start playback stream: %s", oboe::convertToText(result));
        mAudioFile.close();
    } else {
        LOGD("Playback started successfully. Reading from file.");
    }
    return result;
}

void AudioPlayer::stopPlayback() {
    if (mPlaybackStream && mPlaybackStream->getState() != oboe::StreamState::Stopped) {
        mPlaybackStream->requestStop();
        mPlaybackStream->close();
        mPlaybackStream.reset();
        LOGD("Playback stopped.");
    }

    // Close the file stream
    if (mAudioFile.is_open()) {
        mAudioFile.close();
        LOGD("Audio playback file closed.");
    }
}

oboe::DataCallbackResult AudioPlayer::onAudioReady(oboe::AudioStream *oboeStream, void *audioData, int32_t numFrames) {
    auto *outputData = static_cast<int16_t *>(audioData);
    size_t numSamples = numFrames * oboeStream->getChannelCount();
    size_t numBytes = numSamples * sizeof(int16_t);

    if (mAudioFile.is_open()) {
        // Read the requested number of bytes from the file directly into the output buffer
        mAudioFile.read(static_cast<char *>(audioData), numBytes);

        // Get the number of bytes actually read (in case we hit EOF)
        size_t bytesRead = mAudioFile.gcount();
        size_t samplesRead = bytesRead / sizeof(int16_t);

        // Check if we hit the end of the file
        if (mAudioFile.eof()) {
            LOGD("Reached end of file during playback.");

            // Fill any remaining space in the buffer with silence (0s)
            size_t silenceSamples = numSamples - samplesRead;
            if (silenceSamples > 0) {
                memset(outputData + samplesRead, 0, silenceSamples * sizeof(int16_t));
            }

            // Stop the playback stream
            return oboe::DataCallbackResult::Stop;
        }

        // If we read the exact amount, continue
        return oboe::DataCallbackResult::Continue;

    } else {
        LOGE("Audio file is not open for playback, outputting silence.");
        memset(outputData, 0, numBytes);
        return oboe::DataCallbackResult::Stop;
    }
}