package com.example.oboesample

object AudioEngine {
    init {
        System.loadLibrary("native-lib")
    }

    external fun startRecording()
    external fun stopRecording()
    external fun playRecording()
    external fun stopPlayback()
}
