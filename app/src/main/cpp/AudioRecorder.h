#ifndef OBOESAMPLE_AUDIORECORDER_H
#define OBOESAMPLE_AUDIORECORDER_H

#include <oboe/Oboe.h>
#include <vector>
#include <mutex>
#include <fstream>
#include "filter/BiquadFilter.h"
#include "filter/NoiseGate.h"
#include "filter/NoiseReduction.h"
#include "filter/EchoCanceller.h"
#include "filter/PlaybackSuppressor.h"  // NEW: Add this

class AudioRecorder : public oboe::AudioStreamDataCallback {
public:
    AudioRecorder();

    void setStoragePath(const char *path);

    oboe::Result startRecording();
    void stopRecording();

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

    // Echo cancellation controls
    void setEchoCancellerEnabled(bool enabled);
    void configureEchoCanceller(float delayMs, float suppressionAmount);

private:
    std::shared_ptr<oboe::AudioStream> mRecordingStream;
    std::mutex mBufferLock;
    int32_t mSampleRate = 48000;
    int32_t mChannelCount = 1;

    std::fstream mAudioFile;
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
};

#endif //OBOESAMPLE_AUDIORECORDER_H