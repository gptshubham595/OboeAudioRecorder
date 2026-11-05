package com.example.oboesample

import androidx.annotation.Keep

/** Each activity owns one engine and must close it after stopping audio. */
@Keep
class AudioEngine : AutoCloseable {
    companion object {
        init { System.loadLibrary("native-lib") }

        const val SOURCE_GENERIC = 0
        const val SOURCE_CAMCORDER = 1
        const val SOURCE_VOICE_RECOGNITION = 2
        const val SOURCE_VOICE_COMMUNICATION = 3
        const val SOURCE_UNPROCESSED = 4
        const val SOURCE_VOICE_PERFORMANCE = 5
        const val WAVEFORM_BUCKETS = 480
        const val WAVEFORM_FLOATS = 8 + WAVEFORM_BUCKETS * 4
    }

    // Synchronized JNI methods and close share this object's monitor, preventing
    // destruction while a native control call is in flight. Callbacks never take it.
    private var nativeHandle: Long = nativeCreate()

    @Synchronized private external fun nativeCreate(): Long
    @Synchronized private external fun nativeRelease()

    @Synchronized
    override fun close() { nativeRelease() }

    // Basic audio operations
    @Synchronized external fun setRecordingPath(path: String)
    @Synchronized external fun setAudioSource(sourceType: Int)

    // NEW: Enable/disable Android's built-in Acoustic Echo Canceler
    @Synchronized external fun setAndroidAECEnabled(enabled: Boolean)

    @Synchronized external fun startRecording(): Boolean
    @Synchronized external fun stopRecording()
    @Synchronized external fun getRecordingSessionId(): Int
    @Synchronized external fun isRecordingActive(): Boolean
    @Synchronized external fun playRecording(): Boolean
    @Synchronized external fun playInputRecording(): Boolean
    @Synchronized external fun readWaveform(destination: FloatArray): Long
    @Synchronized external fun isPlaybackActive(): Boolean
    @Synchronized external fun getLatencyDiagnostics(): String
    @Synchronized external fun stopPlayback()

    // Bandpass filter (voice isolation)
    @Synchronized external fun setBandpassFilterEnabled(enabled: Boolean)
    @Synchronized external fun configureBandpassFilter(centerFreq: Float, Q: Float)

    // High shelf filter (clarity enhancement)
    @Synchronized external fun setHighShelfFilterEnabled(enabled: Boolean)
    @Synchronized external fun configureHighShelfFilter(centerFreq: Float, Q: Float, gainDb: Float)

    // Peaking filter (presence boost)
    @Synchronized external fun setPeakingFilterEnabled(enabled: Boolean)
    @Synchronized external fun configurePeakingFilter(centerFreq: Float, Q: Float, gainDb: Float)

    // Noise gate (background noise removal)
    @Synchronized external fun setNoiseGateEnabled(enabled: Boolean)
    @Synchronized external fun configureNoiseGate(thresholdDb: Float, ratio: Float, attackMs: Float, releaseMs: Float)

    // Spectral noise attenuation with a configurable stationary noise floor
    @Synchronized external fun setNoiseReductionEnabled(enabled: Boolean)
    @Synchronized external fun configureNoiseReduction(amount: Float)
    @Synchronized external fun configureNoiseFloor(noiseFloorDb: Float)

    // Echo cancellation (feedback suppression)
    @Synchronized external fun setEchoCancellerEnabled(enabled: Boolean)
    @Synchronized external fun configureEchoCanceller(delayMs: Float, suppressionAmount: Float)

    // Playback suppressor (fallback if Android AEC doesn't work)
    @Synchronized external fun setPlaybackSuppressorEnabled(enabled: Boolean)
    @Synchronized external fun configurePlaybackSuppressor(aggressiveness: Float)
}
