#include <jni.h>
#include <string>
#include "AudioRecorder.h"
#include "AudioPlayer.h"
#include <android/log.h>

#define LOG_TAG "NativeLib"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

// Global instances for the recorder and player
static AudioRecorder sRecorder;
static AudioPlayer sPlayer;

extern "C" JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_startRecording(JNIEnv* env, jobject /* this */) {
    sRecorder.startRecording();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_stopRecording(JNIEnv* env, jobject /* this */) {
    sRecorder.stopRecording();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_playRecording(JNIEnv* env, jobject /* this */) {
    std::vector<int16_t> recordedData;
    sRecorder.getRecordingData(recordedData);
    sPlayer.startPlayback(recordedData);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_stopPlayback(JNIEnv* env, jobject /* this */) {
    sPlayer.stopPlayback();
}