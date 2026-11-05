#ifndef OBOESAMPLE_AUDIORECORDER_H
#define OBOESAMPLE_AUDIORECORDER_H

#include <oboe/Oboe.h>
#include "AsyncAudioFile.h"
#include "WaveformMonitor.h"
#include <string>
#include "filter/BiquadFilter.h"
#include "filter/NoiseGate.h"
#include "filter/NoiseReduction.h"
#include "filter/EchoCanceller.h"
#include "filter/PlaybackSuppressor.h"  // NEW: Add this

class AudioRecorder : public oboe::AudioStreamDataCallback {
public:
    AudioRecorder();
    ~AudioRecorder() { stopRecording(false); }
    AudioRecorder(const AudioRecorder&) = delete;
    AudioRecorder& operator=(const AudioRecorder&) = delete;
    AudioRecorder(AudioRecorder&&) = delete;
    AudioRecorder& operator=(AudioRecorder&&) = delete;
    bool hasOpenStream() const { return bool(mRecordingStream); }
    std::string latencyDiagnostics() const;
    uint64_t waveformSnapshot(float* destination) const { return mWaveform.snapshot(destination); }

    void setStoragePath(const char *path);

    oboe::Result startRecording();
    void stopRecording(bool reportDiagnostics = true);
    bool isRecording() const {
        if (!mRecordingStream || mWriter.failed()) return false;
        const auto state = mRecordingStream->getState();
        return state == oboe::StreamState::Starting || state == oboe::StreamState::Started;
    }
    int32_t getSessionId() const {
        return mRecordingStream ? static_cast<int32_t>(mRecordingStream->getSessionId()) : -1;
    }

    // Set audio source (call this before recording)
    void setAudioSource(oboe::InputPreset preset);

    // Enable/disable Android's built-in echo cancellation
    void setAndroidAECEnabled(bool enabled);

    // NEW: Enable/disable playback suppressor (fallback if Android AEC doesn't work)
    void setPlaybackSuppressorEnabled(bool enabled);
    void configurePlaybackSuppressor(float aggressiveness);

    oboe::DataCallbackResult
    onAudioReady(oboe::AudioStream *oboeStream, void *audioData, int32_t numFrames) override;

    // Filter controls
    void setBandpassFilterEnabled(bool enabled);
    void configureBandpassFilter(float centerFreq, float Q);

    void setHighShelfFilterEnabled(bool enabled);
    void configureHighShelfFilter(float centerFreq, float Q, float gainDb);

    void setPeakingFilterEnabled(bool enabled);
    void configurePeakingFilter(float centerFreq, float Q, float gainDb);

    // Noise gate controls
    void setNoiseGateEnabled(bool enabled);
    void configureNoiseGate(float thresholdDb, float ratio, float attackMs, float releaseMs);

    // Noise reduction controls
    void setNoiseReductionEnabled(bool enabled);
    void configureNoiseReduction(float amount);
    void configureNoiseFloor(float noiseFloorDb);

    // Echo cancellation controls
    void setEchoCancellerEnabled(bool enabled);
    void configureEchoCanceller(float delayMs, float suppressionAmount);

private:
    float processSample(float sample, float eqGain);
    std::shared_ptr<oboe::AudioStream> mRecordingStream;
    int32_t mSampleRate = 48000;
    int32_t mChannelCount = 1;

    AudioFileWriter mWriter;
    WaveformMonitor mWaveform;
    std::string mFilePath;

    // Audio source preset
    oboe::InputPreset mInputPreset = oboe::InputPreset::VoiceCommunication;

    // Flag for Android AEC
    bool mAndroidAECEnabled = true;

    // Processing modules
    BiquadFilter mBandpassFilter;
    BiquadFilter mHighShelfFilter;
    BiquadFilter mPeakingFilter;
    NoiseGate mNoiseGate;
    NoiseReduction mNoiseReduction;
    EchoCanceller mEchoCanceller;
    PlaybackSuppressor mPlaybackSuppressor;  // NEW: Add this

    // Enable flags
    bool mBandpassEnabled = false;
    bool mHighShelfEnabled = false;
    bool mPeakingEnabled = false;
    bool mNoiseGateEnabled = false;
    bool mNoiseReductionEnabled = false;
    bool mEchoCancellerEnabled = false;
    bool mPlaybackSuppressorEnabled = false;  // NEW: Add this
    float mPeakingGainDb = 6.0f;
    float mHighShelfGainDb = 3.0f;
};

#endif //OBOESAMPLE_AUDIORECORDER_H
