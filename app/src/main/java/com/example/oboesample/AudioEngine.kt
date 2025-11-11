package com.example.oboesample

object AudioEngine {
    init {
        System.loadLibrary("native-lib")
    }

    // Basic audio operations
    external fun setRecordingPath(path: String)
    external fun startRecording()
    external fun stopRecording()
    external fun playRecording()
    external fun stopPlayback()

    // Bandpass filter (voice isolation)
    external fun setBandpassFilterEnabled(enabled: Boolean)
    external fun configureBandpassFilter(centerFreq: Float, Q: Float)

    // High shelf filter (clarity enhancement)
    external fun setHighShelfFilterEnabled(enabled: Boolean)
    external fun configureHighShelfFilter(centerFreq: Float, Q: Float, gainDb: Float)

    // Peaking filter (presence boost)
    external fun setPeakingFilterEnabled(enabled: Boolean)
    external fun configurePeakingFilter(centerFreq: Float, Q: Float, gainDb: Float)

    // Noise gate (background noise removal)
    external fun setNoiseGateEnabled(enabled: Boolean)
    external fun configureNoiseGate(thresholdDb: Float, ratio: Float, attackMs: Float, releaseMs: Float)

    // Noise reduction (smoothing)
    external fun setNoiseReductionEnabled(enabled: Boolean)
    external fun configureNoiseReduction(amount: Float)

    // Echo cancellation (feedback suppression)
    external fun setEchoCancellerEnabled(enabled: Boolean)
    external fun configureEchoCanceller(delayMs: Float, suppressionAmount: Float)
}