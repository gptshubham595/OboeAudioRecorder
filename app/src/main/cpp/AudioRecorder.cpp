#include "AudioRecorder.h"
#include <android/log.h>
#include <unistd.h>

#define LOG_TAG "AudioRecorder"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

AudioRecorder::AudioRecorder()
        : mEchoCanceller(48000),
          mNoiseReduction(5),    // 5-sample smoothing window
          mPlaybackSuppressor(48000) {

    oboe::AudioStreamBuilder recordingBuilder;
    recordingBuilder.setDirection(oboe::Direction::Input);
    std::shared_ptr<oboe::AudioStream> recordingStream;
    if (recordingBuilder.openStream(recordingStream) == oboe::Result::OK) {
        mSampleRate = recordingStream->getSampleRate();
        mChannelCount = recordingStream->getChannelCount();
        recordingStream->close();
    }
    LOGD("Recorder initialized. Sample Rate: %d, Channels: %d", mSampleRate, mChannelCount);

    // Initialize filters with default values
    mBandpassFilter.setBandpass(mSampleRate, 1000.0f, 1.0f);
    mHighShelfFilter.setHighShelf(mSampleRate, 8000.0f, 0.7f, 3.0f);
    mPeakingFilter.setPeaking(mSampleRate, 3000.0f, 1.0f, 6.0f);

    // Initialize noise gate
    mNoiseGate.setThreshold(-40.0f);  // -40 dB
    mNoiseGate.setRatio(4.0f);        // 4:1 ratio
    mNoiseGate.setAttack(5.0f, mSampleRate);   // 5ms attack
    mNoiseGate.setRelease(50.0f, mSampleRate); // 50ms release

    // Initialize noise reduction
    mNoiseReduction.setReductionAmount(0.5f);

    // Initialize echo canceller
    mEchoCanceller.setEchoDelay(50.0f);
    mEchoCanceller.setSuppressionAmount(0.7f);
    mPlaybackSuppressor.setAggressiveness(0.8f);
}

void AudioRecorder::setPlaybackSuppressorEnabled(bool enabled) {
    mPlaybackSuppressorEnabled = enabled;
    if (enabled) {
        mPlaybackSuppressor.reset();
        LOGD("Playback suppressor enabled");
    } else {
        LOGD("Playback suppressor disabled");
    }
}

void AudioRecorder::configurePlaybackSuppressor(float aggressiveness) {
    mPlaybackSuppressor.setAggressiveness(aggressiveness);
}

void AudioRecorder::setStoragePath(const char *path) {
    mFilePath = path;
    LOGD("Set recording path to: %s", mFilePath.c_str());
}

void AudioRecorder::setAudioSource(oboe::InputPreset preset) {
    mInputPreset = preset;
    LOGD("Audio source set to: %d", static_cast<int>(preset));
}

void AudioRecorder::setAndroidAECEnabled(bool enabled) {
    mAndroidAECEnabled = enabled;
    LOGD("Android AEC set to: %s", enabled ? "ENABLED" : "DISABLED");
}

// Bandpass filter controls
void AudioRecorder::setBandpassFilterEnabled(bool enabled) {
    mBandpassEnabled = enabled;
    if (enabled) {
        mBandpassFilter.reset();
        LOGD("Bandpass filter enabled");
    } else {
        LOGD("Bandpass filter disabled");
    }
}

void AudioRecorder::configureBandpassFilter(float centerFreq, float Q) {
    mBandpassFilter.setBandpass(mSampleRate, centerFreq, Q);
}

// High shelf filter controls
void AudioRecorder::setHighShelfFilterEnabled(bool enabled) {
    mHighShelfEnabled = enabled;
    if (enabled) {
        mHighShelfFilter.reset();
        LOGD("High shelf filter enabled");
    } else {
        LOGD("High shelf filter disabled");
    }
}

void AudioRecorder::configureHighShelfFilter(float centerFreq, float Q, float gainDb) {
    mHighShelfFilter.setHighShelf(mSampleRate, centerFreq, Q, gainDb);
}

// Peaking filter controls
void AudioRecorder::setPeakingFilterEnabled(bool enabled) {
    mPeakingEnabled = enabled;
    if (enabled) {
        mPeakingFilter.reset();
        LOGD("Peaking filter enabled");
    } else {
        LOGD("Peaking filter disabled");
    }
}

void AudioRecorder::configurePeakingFilter(float centerFreq, float Q, float gainDb) {
    mPeakingFilter.setPeaking(mSampleRate, centerFreq, Q, gainDb);
}

// Noise gate controls
void AudioRecorder::setNoiseGateEnabled(bool enabled) {
    mNoiseGateEnabled = enabled;
    if (enabled) {
        mNoiseGate.reset();
        LOGD("Noise gate enabled");
    } else {
        LOGD("Noise gate disabled");
    }
}

void AudioRecorder::configureNoiseGate(float thresholdDb, float ratio, float attackMs, float releaseMs) {
    mNoiseGate.setThreshold(thresholdDb);
    mNoiseGate.setRatio(ratio);
    mNoiseGate.setAttack(attackMs, mSampleRate);
    mNoiseGate.setRelease(releaseMs, mSampleRate);
}

// Noise reduction controls
void AudioRecorder::setNoiseReductionEnabled(bool enabled) {
    mNoiseReductionEnabled = enabled;
    if (enabled) {
        mNoiseReduction.reset();
        LOGD("Noise reduction enabled");
    } else {
        LOGD("Noise reduction disabled");
    }
}

void AudioRecorder::configureNoiseReduction(float amount) {
    mNoiseReduction.setReductionAmount(amount);
}

// Echo canceller controls
void AudioRecorder::setEchoCancellerEnabled(bool enabled) {
    mEchoCancellerEnabled = enabled;
    if (enabled) {
        mEchoCanceller.reset();
        LOGD("Echo canceller enabled");
    } else {
        LOGD("Echo canceller disabled");
    }
}

void AudioRecorder::configureEchoCanceller(float delayMs, float suppressionAmount) {
    mEchoCanceller.setEchoDelay(delayMs);
    mEchoCanceller.setSuppressionAmount(suppressionAmount);
}

oboe::Result AudioRecorder::startRecording() {
    std::lock_guard<std::mutex> lock(mBufferLock);

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

    // Reset all processing modules
    if (mBandpassEnabled) mBandpassFilter.reset();
    if (mHighShelfEnabled) mHighShelfFilter.reset();
    if (mPeakingEnabled) mPeakingFilter.reset();
    if (mNoiseGateEnabled) mNoiseGate.reset();
    if (mNoiseReductionEnabled) mNoiseReduction.reset();
    if (mEchoCancellerEnabled) mEchoCanceller.reset();
    if (mPlaybackSuppressorEnabled) mPlaybackSuppressor.reset();  // NEW: Add this line

    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Input)
            ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
            ->setSharingMode(oboe::SharingMode::Exclusive)
            ->setFormat(oboe::AudioFormat::I16)
            ->setChannelCount(mChannelCount)
            ->setSampleRate(mSampleRate)
            ->setInputPreset(mInputPreset)
            ->setUsage(oboe::Usage::VoiceCommunication)  // NEW: Explicitly set usage
            ->setContentType(oboe::ContentType::Speech)   // NEW: Mark as speech content
            ->setDataCallback(this);

    oboe::Result result = builder.openStream(mRecordingStream);
    if (result != oboe::Result::OK) {
        LOGE("Failed to open recording stream: %s", oboe::convertToText(result));
        mAudioFile.close();
        return result;
    }

    int32_t actualSampleRate = mRecordingStream->getSampleRate();
    if (actualSampleRate != mSampleRate) {
        mSampleRate = actualSampleRate;
        // Reconfigure all filters with actual sample rate
        if (mBandpassEnabled) configureBandpassFilter(1000.0f, 1.0f);
        if (mHighShelfEnabled) configureHighShelfFilter(8000.0f, 0.7f, 3.0f);
        if (mPeakingEnabled) configurePeakingFilter(3000.0f, 1.0f, 6.0f);
        if (mNoiseGateEnabled) configureNoiseGate(-40.0f, 4.0f, 5.0f, 50.0f);
        LOGD("Updated sample rate to: %d", mSampleRate);
    }

    result = mRecordingStream->requestStart();
    if (result != oboe::Result::OK) {
        LOGE("Failed to start recording stream: %s", oboe::convertToText(result));
        mAudioFile.close();
    } else {
        LOGD("Recording started with processing chain:");
        LOGD("  InputPreset: %d, Usage: VoiceCommunication, Content: Speech",
             static_cast<int>(mInputPreset));
        LOGD("  Bandpass: %s, HighShelf: %s, Peaking: %s",
             mBandpassEnabled ? "ON" : "OFF",
             mHighShelfEnabled ? "ON" : "OFF",
             mPeakingEnabled ? "ON" : "OFF");
        LOGD("  NoiseGate: %s, NoiseReduction: %s, Echo: %s",
             mNoiseGateEnabled ? "ON" : "OFF",
             mNoiseReductionEnabled ? "ON" : "OFF",
             mEchoCancellerEnabled ? "ON" : "OFF");
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

    // Check if any processing is enabled
    bool anyProcessingEnabled = mBandpassEnabled || mHighShelfEnabled || mPeakingEnabled ||
                                mNoiseGateEnabled || mNoiseReductionEnabled ||
                                mEchoCancellerEnabled || mPlaybackSuppressorEnabled;  // NEW: Added


    if (anyProcessingEnabled) {
        std::vector<int16_t> processedData(numSamples);

        for (size_t i = 0; i < numSamples; i++) {
            // Convert int16_t to float (-1.0 to 1.0)
            float sample = static_cast<float>(inputData[i]) / 32768.0f;

            // Apply processing chain in order

            // 1. Playback suppressor (NEW: Add as first software processing step)
            //    This acts as a fallback if Android AEC doesn't work
            if (mPlaybackSuppressorEnabled) {
                sample = mPlaybackSuppressor.process(sample);
            }

            // 1. Echo cancellation (first, to remove feedback)
            if (mEchoCancellerEnabled) {
                sample = mEchoCanceller.process(sample);
            }

            // 2. Noise reduction (remove background noise)
            if (mNoiseReductionEnabled) {
                sample = mNoiseReduction.process(sample);
            }

            // 3. Noise gate (cut very low signals)
            if (mNoiseGateEnabled) {
                sample = mNoiseGate.process(sample);
            }

            // 4. Bandpass filter (isolate voice frequencies)
            if (mBandpassEnabled) {
                sample = mBandpassFilter.process(sample);
            }

            // 5. Peaking EQ (boost presence)
            if (mPeakingEnabled) {
                sample = mPeakingFilter.process(sample);
            }

            // 6. High shelf (enhance clarity)
            if (mHighShelfEnabled) {
                sample = mHighShelfFilter.process(sample);
            }

            // Convert back to int16_t with clipping
            sample = std::max(-1.0f, std::min(1.0f, sample));
            processedData[i] = static_cast<int16_t>(sample * 32767.0f);
        }

        // Write processed data to file
        size_t numBytes = numSamples * sizeof(int16_t);
        mAudioFile.write(reinterpret_cast<const char *>(processedData.data()), numBytes);
    } else {
        // No processing: write raw data directly
        size_t numBytes = numSamples * sizeof(int16_t);
        mAudioFile.write(static_cast<const char *>(audioData), numBytes);
    }

    return oboe::DataCallbackResult::Continue;
}