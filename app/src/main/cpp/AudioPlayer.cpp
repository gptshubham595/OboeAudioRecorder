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

void AudioPlayer::stopPlayback() {
    if (mPlaybackStream && mPlaybackStream->getState() != oboe::StreamState::Stopped) {
        mPlaybackStream->requestStop();
        mPlaybackStream->close();
        mPlaybackStream.reset();
        LOGD("Playback stopped.");
    }
}

oboe::DataCallbackResult AudioPlayer::onAudioReady(oboe::AudioStream *oboeStream, void *audioData, int32_t numFrames) {
    auto *outputData = static_cast<int16_t *>(audioData);
    size_t numSamples = numFrames * oboeStream->getChannelCount();

    // Calculate how many samples we can actually write
    int64_t samplesRemaining = mPlaybackBuffer.size() - mReadIndex;
    int32_t samplesToWrite = std::min((int32_t)numSamples, (int32_t)samplesRemaining);

    if (samplesToWrite > 0) {
        // Copy data from our buffer to the output buffer
        memcpy(outputData, mPlaybackBuffer.data() + mReadIndex, samplesToWrite * sizeof(int16_t));
        mReadIndex += samplesToWrite;
    }

    // Fill the rest of the buffer with silence if we ran out of data
    if (samplesToWrite < numSamples) {
        int32_t silenceSamples = numSamples - samplesToWrite;
        memset(outputData + samplesToWrite, 0, silenceSamples * sizeof(int16_t));
    }

    // If we've reached the end of the buffer, stop playback
    if (mReadIndex >= mPlaybackBuffer.size()) {
        LOGD("Playback complete.");
        return oboe::DataCallbackResult::Stop;
    }

    return oboe::DataCallbackResult::Continue;
}