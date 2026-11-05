#include "AudioPlayer.h"
#include <android/log.h>
#include <algorithm>
#include <cstring>
#include <sstream>

#define LOG_TAG "AudioPlayer"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

AudioPlayer::AudioPlayer() = default;

oboe::Result AudioPlayer::startPlaybackFromFile(const char* path) {

    stopPlayback();

    if (!mReader.start(path)) return oboe::Result::ErrorInvalidFormat;

    // 2. Configure and open the Oboe stream
    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Output)
            ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
            ->setSharingMode(oboe::SharingMode::Exclusive)
            ->setFormat(oboe::AudioFormat::I16)
            ->setChannelCount(mChannelCount)
            ->setSampleRate(mSampleRate)
            ->setSampleRateConversionQuality(oboe::SampleRateConversionQuality::Medium)
            ->setChannelConversionAllowed(true)
            ->setDataCallback(this);

    oboe::Result result = builder.openStream(mPlaybackStream);
    if (result != oboe::Result::OK) {
        LOGE("Failed to open playback stream: %s", oboe::convertToText(result));
        mReader.stop();
        return result;
    }

    if (mPlaybackStream->getSampleRate() != mSampleRate ||
        mPlaybackStream->getChannelCount() != mChannelCount ||
        mPlaybackStream->getFormat() != oboe::AudioFormat::I16) {
        stopPlayback();
        return oboe::Result::ErrorInvalidFormat;
    }
    const int32_t burst = mPlaybackStream->getFramesPerBurst();
    if (burst > 0 && burst <= INT32_MAX / 2) {
        // Requested size may be clamped or unsupported. Diagnostics report the actual size.
        mPlaybackStream->setBufferSizeInFrames(2 * burst);
    }
    LOGD("Output stream: %s", latencyDiagnostics().c_str());
    mPlaying.store(true);

    result = mPlaybackStream->requestStart();
    if (result != oboe::Result::OK) {
        LOGE("Failed to start playback stream: %s", oboe::convertToText(result));
        stopPlayback();
    } else {
        LOGD("Playback started successfully. Reading from file.");
    }
    return result;
}

void AudioPlayer::stopPlayback(bool reportDiagnostics) {
    if (mPlaybackStream) {
        if (reportDiagnostics) LOGD("Stream closing: %s", latencyDiagnostics().c_str());
        mPlaybackStream->requestStop();
        mPlaybackStream->close();
        mPlaybackStream.reset();
        LOGD("Playback stopped.");
    }

    mPlaying.store(false);

    mReader.stop(); // Consumer is closed before cancelling/joining its producer.
}

std::string AudioPlayer::latencyDiagnostics() const {
    int32_t xrunCount = -1;
    if (mPlaybackStream) {
        const auto xruns = mPlaybackStream->getXRunCount();
        if (xruns) xrunCount = xruns.value();
    }
    std::ostringstream result;
    result << "{\"sampleRate\":" << mSampleRate
           << ",\"framesPerBurst\":" << (mPlaybackStream ? mPlaybackStream->getFramesPerBurst() : 0)
           << ",\"bufferFrames\":" << (mPlaybackStream ? mPlaybackStream->getBufferSizeInFrames() : 0)
           << ",\"performanceMode\":" << (mPlaybackStream ? static_cast<int>(mPlaybackStream->getPerformanceMode()) : -1)
           << ",\"sharingMode\":" << (mPlaybackStream ? static_cast<int>(mPlaybackStream->getSharingMode()) : -1)
           << ",\"xruns\":" << xrunCount
           << ",\"queueCapacityFrames\":" << AudioFileReader::capacity
           << ",\"queueUnderruns\":" << mReader.underruns()
           << ",\"workerFailed\":" << (mReader.failed() ? "true" : "false") << "}";
    return result.str();
}

oboe::DataCallbackResult AudioPlayer::onAudioReady(oboe::AudioStream*, void* audioData, int32_t numFrames) {
    const auto read = mReader.read(static_cast<int16_t*>(audioData), static_cast<size_t>(numFrames));
    if (read.finished || read.failed) {
        mPlaying.store(false);
        return oboe::DataCallbackResult::Stop;
    }
    return oboe::DataCallbackResult::Continue;
}
