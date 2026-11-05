#include <jni.h>
#include <oboe/Oboe.h>

extern "C" {
JNIEXPORT jint JNICALL Java_com_example_oboesample_AudioEngineLifecycleTest_liveStreams(JNIEnv*, jclass) {
    return oboe::liveStreams.load();
}
JNIEXPORT void JNICALL Java_com_example_oboesample_AudioEngineLifecycleTest_feed(JNIEnv*, jclass, jint samples) {
    if (auto input = oboe::lastInput.lock()) input->feed(samples);
}
JNIEXPORT void JNICALL Java_com_example_oboesample_AudioEngineLifecycleTest_failNextOpen(JNIEnv*, jclass) {
    oboe::failOpen.store(true);
}
JNIEXPORT void JNICALL Java_com_example_oboesample_AudioEngineLifecycleTest_failNextStart(JNIEnv*, jclass) {
    oboe::failStart.store(true);
}
}
