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

[[maybe_unused]] JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_configureEchoCanceller(JNIEnv *env, jobject, jfloat delayMs,
                                                               jfloat suppressionAmount) {
    sRecorder.configureEchoCanceller(delayMs, suppressionAmount);
}
}