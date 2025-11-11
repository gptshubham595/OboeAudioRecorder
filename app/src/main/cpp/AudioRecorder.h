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

class AudioRecorder : public oboe::AudioStreamDataCallback {
public:
    AudioRecorder();

    void setStoragePath(const char *path);

    oboe::Result startRecording();
    void stopRecording();

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

    // Processing modules
    BiquadFilter mBandpassFilter;
    BiquadFilter mHighShelfFilter;
    BiquadFilter mPeakingFilter;
    NoiseGate mNoiseGate;
    NoiseReduction mNoiseReduction;
    EchoCanceller mEchoCanceller;

    // Enable flags
    bool mBandpassEnabled = false;
    bool mHighShelfEnabled = false;
    bool mPeakingEnabled = false;
    bool mNoiseGateEnabled = false;
    bool mNoiseReductionEnabled = false;
    bool mEchoCancellerEnabled = false;
};

#endif //OBOESAMPLE_AUDIORECORDER_H