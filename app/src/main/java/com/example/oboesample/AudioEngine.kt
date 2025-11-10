package com.example.oboesample

object AudioEngine {
    init {
        System.loadLibrary("native-lib")
    }

    external fun setRecordingPath(path: String) // New JNI method
    external fun startRecording()
    external fun stopRecording()
    external fun playRecording()
    external fun stopPlayback()

    external fun setFilterEnabled(enabled: Boolean)
    external fun configureBandpassFilter(centerFreq: Float, Q: Float)
}