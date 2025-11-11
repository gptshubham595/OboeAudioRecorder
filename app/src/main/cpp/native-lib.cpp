#include <jni.h>
#include <string>
#include "AudioRecorder.h"
#include "AudioPlayer.h"
#include <android/log.h>

#define LOG_TAG "NativeLib"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

static AudioRecorder sRecorder;
static AudioPlayer sPlayer;
static std::string sCurrentRecordingPath;

// Basic audio operations
extern "C" {
JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_setRecordingPath(JNIEnv *env, jobject, jstring path) {
    const char *pathPtr = env->GetStringUTFChars(path, nullptr);
    sCurrentRecordingPath = pathPtr;
    sRecorder.setStoragePath(sCurrentRecordingPath.c_str());
    env->ReleaseStringUTFChars(path, pathPtr);
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_setAudioSource(JNIEnv *env, jobject, jint sourceType) {
    oboe::InputPreset preset;
    switch (sourceType) {
        case 0:
            preset = oboe::InputPreset::Generic;
            break;
        case 1:
            preset = oboe::InputPreset::Camcorder;
            break;
        case 2:
            preset = oboe::InputPreset::VoiceRecognition;
            break;
        case 3:
            preset = oboe::InputPreset::VoiceCommunication;
            break;
        case 4:
            preset = oboe::InputPreset::Unprocessed;
            break;
        case 5:
            preset = oboe::InputPreset::VoicePerformance;
            break;
        default:
            preset = oboe::InputPreset::VoiceCommunication;
            break;
    }
    sRecorder.setAudioSource(preset);
    LOGD("Audio source set to: %d", sourceType);
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_setAndroidAECEnabled(JNIEnv *env, jobject, jboolean enabled) {
    sRecorder.setAndroidAECEnabled(enabled);
    LOGD("Android AEC set to: %s", enabled ? "ENABLED" : "DISABLED");
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_startRecording(JNIEnv *env, jobject) {
    sRecorder.startRecording();
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_stopRecording(JNIEnv *env, jobject) {
    sRecorder.stopRecording();
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_playRecording(JNIEnv *env, jobject) {
    if (!sCurrentRecordingPath.empty()) {
        sPlayer.startPlaybackFromFile(sCurrentRecordingPath.c_str());
    } else {
        LOGE("No recording path set for playback!");
    }
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_stopPlayback(JNIEnv *env, jobject) {
    sPlayer.stopPlayback();
}

// Bandpass filter
JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_setBandpassFilterEnabled(JNIEnv *env, jobject,
                                                                 jboolean enabled) {
    sRecorder.setBandpassFilterEnabled(enabled);
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_configureBandpassFilter(JNIEnv *env, jobject,
                                                                jfloat centerFreq, jfloat Q) {
    sRecorder.configureBandpassFilter(centerFreq, Q);
}

// High shelf filter
JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_setHighShelfFilterEnabled(JNIEnv *env, jobject,
                                                                  jboolean enabled) {
    sRecorder.setHighShelfFilterEnabled(enabled);
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_configureHighShelfFilter(JNIEnv *env, jobject,
                                                                 jfloat centerFreq, jfloat Q,
                                                                 jfloat gainDb) {
    sRecorder.configureHighShelfFilter(centerFreq, Q, gainDb);
}

// Peaking filter
JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_setPeakingFilterEnabled(JNIEnv *env, jobject,
                                                                jboolean enabled) {
    sRecorder.setPeakingFilterEnabled(enabled);
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_configurePeakingFilter(JNIEnv *env, jobject,
                                                               jfloat centerFreq, jfloat Q,
                                                               jfloat gainDb) {
    sRecorder.configurePeakingFilter(centerFreq, Q, gainDb);
}

// Noise gate
JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_setNoiseGateEnabled(JNIEnv *env, jobject,
                                                            jboolean enabled) {
    sRecorder.setNoiseGateEnabled(enabled);
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_configureNoiseGate(JNIEnv *env, jobject, jfloat thresholdDb,
                                                           jfloat ratio, jfloat attackMs,
                                                           jfloat releaseMs) {
    sRecorder.configureNoiseGate(thresholdDb, ratio, attackMs, releaseMs);
}

// Noise reduction
JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_setNoiseReductionEnabled(JNIEnv *env, jobject,
                                                                 jboolean enabled) {
    sRecorder.setNoiseReductionEnabled(enabled);
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_configureNoiseReduction(JNIEnv *env, jobject,
                                                                jfloat amount) {
    sRecorder.configureNoiseReduction(amount);
}

// Echo canceller
JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_setEchoCancellerEnabled(JNIEnv *env, jobject,
                                                                jboolean enabled) {
    sRecorder.setEchoCancellerEnabled(enabled);
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_configureEchoCanceller(JNIEnv *env, jobject, jfloat delayMs,
                                                               jfloat suppressionAmount) {
    sRecorder.configureEchoCanceller(delayMs, suppressionAmount);
}

// Playback suppressor (NEW)
JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_setPlaybackSuppressorEnabled(JNIEnv *env, jobject,
                                                                     jboolean enabled) {
    sRecorder.setPlaybackSuppressorEnabled(enabled);
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_configurePlaybackSuppressor(JNIEnv *env, jobject,
                                                                    jfloat aggressiveness) {
    sRecorder.configurePlaybackSuppressor(aggressiveness);
}
}