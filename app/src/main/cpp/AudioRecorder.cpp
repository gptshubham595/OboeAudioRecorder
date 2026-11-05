#include "AudioRecorder.h"
#include <android/log.h>
#include "WavFile.h"
#include <algorithm>
#include <sstream>
#include "filter/EqHeadroom.h"

#define LOG_TAG "AudioRecorder"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

AudioRecorder::AudioRecorder()
        : mEchoCanceller(48000),
          mPlaybackSuppressor(48000) {

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
    mPlaybackSuppressor.setEnabled(enabled);
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
    mHighShelfGainDb = gainDb;
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
    mPeakingGainDb = gainDb;
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

void AudioRecorder::configureNoiseFloor(float noiseFloorDb) {
    mNoiseReduction.setNoiseFloorDb(noiseFloorDb);
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
    if (mRecordingStream) return oboe::Result::ErrorInvalidState;

    if (mFilePath.empty()) {
        LOGE("Recording path not set!");
        return oboe::Result::ErrorInternal;
    }

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
            ->setSampleRateConversionQuality(oboe::SampleRateConversionQuality::Medium)
            ->setChannelConversionAllowed(true)
            ->setInputPreset(mInputPreset)
            ->setSessionId(mAndroidAECEnabled ? oboe::SessionId::Allocate : oboe::SessionId::None)
            ->setUsage(oboe::Usage::VoiceCommunication)  // NEW: Explicitly set usage
            ->setContentType(oboe::ContentType::Speech)   // NEW: Mark as speech content
            ->setDataCallback(this);

    oboe::Result result = builder.openStream(mRecordingStream);
    if (result != oboe::Result::OK) {
        LOGE("Failed to open recording stream: %s", oboe::convertToText(result));
        return result;
    }

    if (mRecordingStream->getSampleRate() != mSampleRate ||
        mRecordingStream->getChannelCount() != mChannelCount ||
        mRecordingStream->getFormat() != oboe::AudioFormat::I16) {
        stopRecording();
        return oboe::Result::ErrorInvalidFormat;
    }

    const bool processing = mBandpassEnabled || mHighShelfEnabled || mPeakingEnabled ||
            mNoiseGateEnabled || mNoiseReductionEnabled || mPlaybackSuppressorEnabled;
    const float eqGain = eqHeadroomGain(mPeakingEnabled, mPeakingGainDb,
                                        mHighShelfEnabled, mHighShelfGainDb);
    const size_t tail = mNoiseReductionEnabled ? mNoiseReduction.latencySamples() : 0;
    mWaveform.reset(tail);
    const auto inputPath = mFilePath + ".input.wav";
    if (!mWriter.start(mFilePath.c_str(), [this, processing, eqGain](int16_t* data, size_t count) {
        std::array<int16_t, 512> input{}; // Writer supplies at most 512 samples per block.
        std::copy_n(data, count, input.data());
        if (processing) {
            for (size_t i = 0; i < count; ++i) {
                data[i] = static_cast<int16_t>(processSample(data[i] / 32768.0f, eqGain) * 32767.0f);
            }
        }
        mWaveform.append(input.data(), data, count);
    }, tail, inputPath.c_str(), tail)) {
        stopRecording();
        return oboe::Result::ErrorInternal;
    }
    LOGD("Input stream: %s", latencyDiagnostics().c_str());
    result = mRecordingStream->requestStart();
    if (result != oboe::Result::OK) {
        LOGE("Failed to start recording stream: %s", oboe::convertToText(result));
        stopRecording();
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

void AudioRecorder::stopRecording(bool reportDiagnostics) {
    if (mRecordingStream) {
        if (reportDiagnostics) LOGD("Stream closing: %s", latencyDiagnostics().c_str());
        mRecordingStream->requestStop();
        mRecordingStream->close();
        mRecordingStream.reset();
        LOGD("Recording stream stopped.");
    }

    mWriter.stop(); // Close input first, then drain DSP/file work and join.
    mWaveform.finish();
}

std::string AudioRecorder::latencyDiagnostics() const {
    int32_t xrunCount = -1;
    if (mRecordingStream) {
        const auto xruns = mRecordingStream->getXRunCount();
        if (xruns) xrunCount = xruns.value();
    }
    std::ostringstream result;
    result << "{\"sampleRate\":" << mSampleRate
           << ",\"framesPerBurst\":" << (mRecordingStream ? mRecordingStream->getFramesPerBurst() : 0)
           << ",\"bufferFrames\":" << (mRecordingStream ? mRecordingStream->getBufferSizeInFrames() : 0)
           << ",\"performanceMode\":" << (mRecordingStream ? static_cast<int>(mRecordingStream->getPerformanceMode()) : -1)
           << ",\"sharingMode\":" << (mRecordingStream ? static_cast<int>(mRecordingStream->getSharingMode()) : -1)
           << ",\"xruns\":" << xrunCount
           << ",\"dspDelayFrames\":" << (mNoiseReductionEnabled ? mNoiseReduction.latencySamples() : 0)
           << ",\"queueCapacityFrames\":" << AudioFileWriter::capacity
           << ",\"queueOverruns\":" << mWriter.overruns()
           << ",\"workerFailed\":" << (mWriter.failed() ? "true" : "false") << "}";
    return result.str();
}

float AudioRecorder::processSample(float sample, float eqGain) {
    // Apply processing chain in order

    // Optional heuristic; not a substitute for reference-based AEC.
    if (mPlaybackSuppressorEnabled) {
        sample = mPlaybackSuppressor.process(sample);
    }

    // No playback reference is connected here, so software echo processing is transparent.
    // Hardware AEC is attached to the recording session by MainActivity.
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
    // Reserve EQ boost headroom after detection so gate thresholds remain unchanged.
    sample *= eqGain;
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
    return std::clamp(sample, -1.0f, 1.0f);
}

oboe::DataCallbackResult
AudioRecorder::onAudioReady(oboe::AudioStream*, void* audioData, int32_t numFrames) {
    // Mono PCM16 format was verified at open. No file I/O, DSP, logging or locks here.
    return mWriter.enqueue(static_cast<const int16_t*>(audioData), static_cast<size_t>(numFrames))
            ? oboe::DataCallbackResult::Continue : oboe::DataCallbackResult::Stop;
}
