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
static std::string sCurrentRecordingPath; // Store the file path globally

extern "C" JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_setRecordingPath(JNIEnv *env, jobject, jstring path) {
    const char *pathPtr = env->GetStringUTFChars(path, nullptr);
    sCurrentRecordingPath = pathPtr;
    sRecorder.setStoragePath(sCurrentRecordingPath.c_str());
    env->ReleaseStringUTFChars(path, pathPtr);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_startRecording(JNIEnv *env, jobject) {
    sRecorder.startRecording();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_stopRecording(JNIEnv *env, jobject) {
    sRecorder.stopRecording();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_playRecording(JNIEnv *env, jobject) {
// Pass the file path to the player instead of recorded data
    if (!sCurrentRecordingPath.empty()) {
        sPlayer.startPlaybackFromFile(sCurrentRecordingPath.c_str());
    } else {
        LOGE("No recording path set for playback!");
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_stopPlayback(JNIEnv *env, jobject) {
    sPlayer.stopPlayback();
}