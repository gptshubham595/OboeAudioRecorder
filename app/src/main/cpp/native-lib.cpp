#include <jni.h>
#include <memory>
#include <new>
#include <string>
#include <type_traits>
#include <utility>
#include "AudioRecorder.h"
#include "AudioPlayer.h"

namespace {
struct NativeEngine {
    AudioRecorder recorder;
    AudioPlayer player;
    std::string recordingPath;
};

void throwJava(JNIEnv* env, const char* type, const char* message) {
    if (env->ExceptionCheck()) return;
    const jclass exception = env->FindClass(type);
    if (exception) {
        env->ThrowNew(exception, message);
        env->DeleteLocalRef(exception);
    }
}

jfieldID handleField(JNIEnv* env, jobject owner) {
    const jclass type = env->GetObjectClass(owner);
    if (!type) return nullptr;
    const auto field = env->GetFieldID(type, "nativeHandle", "J");
    env->DeleteLocalRef(type);
    return field;
}

// Every caller is a synchronized instance native method. No Java/global references
// or JNIEnv pointers escape this boundary or enter audio/file worker threads.
template<typename Function>
auto withEngine(JNIEnv* env, jobject owner, Function&& function)
        -> decltype(function(std::declval<NativeEngine&>())) {
    using Return = decltype(function(std::declval<NativeEngine&>()));
    try {
        const auto field = handleField(env, owner);
        if (field) {
            auto* engine = reinterpret_cast<NativeEngine*>(env->GetLongField(owner, field));
            if (engine) return function(*engine);
            throwJava(env, "java/lang/IllegalStateException", "Audio engine is closed");
        }
    } catch (const std::bad_alloc&) {
        throwJava(env, "java/lang/OutOfMemoryError", "Unable to allocate audio resources");
    } catch (...) {
        throwJava(env, "java/lang/IllegalStateException", "Native audio operation failed");
    }
    if constexpr (!std::is_void_v<Return>) return Return{};
}

template<typename Function>
void withStoppedRecorder(JNIEnv* env, jobject owner, Function&& function) {
    withEngine(env, owner, [&](NativeEngine& engine) {
        if (engine.recorder.hasOpenStream()) {
            throwJava(env, "java/lang/IllegalStateException", "Stop recording before changing its configuration");
            return;
        }
        function(engine);
    });
}

class UtfChars {
public:
    UtfChars(JNIEnv* env, jstring value) : mEnv(env), mValue(value),
            mChars(value ? env->GetStringUTFChars(value, nullptr) : nullptr) {}
    ~UtfChars() { if (mChars) mEnv->ReleaseStringUTFChars(mValue, mChars); }
    const char* get() const { return mChars; }
private:
    JNIEnv* mEnv;
    jstring mValue;
    const char* mChars;
};
}

extern "C" {
JNIEXPORT jlong JNICALL
Java_com_example_oboesample_AudioEngine_nativeCreate(JNIEnv* env, jobject) {
    try { return reinterpret_cast<jlong>(new NativeEngine); }
    catch (const std::bad_alloc&) {
        throwJava(env, "java/lang/OutOfMemoryError", "Unable to create audio engine");
    } catch (...) {
        throwJava(env, "java/lang/IllegalStateException", "Unable to create audio engine");
    }
    return 0;
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_nativeRelease(JNIEnv* env, jobject owner) {
    const auto field = handleField(env, owner);
    if (!field) return;
    auto* engine = reinterpret_cast<NativeEngine*>(env->GetLongField(owner, field));
    env->SetLongField(owner, field, 0); // Invalidate before destroying; repeated close is safe.
    delete engine; // Stream owners close callbacks and join workers before buffers die.
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_setRecordingPath(JNIEnv* env, jobject owner, jstring path) {
    withEngine(env, owner, [&](NativeEngine& engine) {
        if (!path) {
            throwJava(env, "java/lang/IllegalArgumentException", "Recording path is null");
            return;
        }
        const UtfChars chars(env, path);
        if (!chars.get()) return; // Preserve the pending JNI allocation exception.
        if (engine.recorder.hasOpenStream() || engine.player.hasOpenStream()) {
            throwJava(env, "java/lang/IllegalStateException", "Stop audio before changing its path");
            return;
        }
        engine.recordingPath = chars.get();
        engine.recorder.setStoragePath(engine.recordingPath.c_str());
    });
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_setAudioSource(JNIEnv* env, jobject owner, jint source) {
    withStoppedRecorder(env, owner, [&](NativeEngine& engine) {
        oboe::InputPreset preset = oboe::InputPreset::VoiceCommunication;
        switch (source) {
            case 0: preset = oboe::InputPreset::Generic; break;
            case 1: preset = oboe::InputPreset::Camcorder; break;
            case 2: preset = oboe::InputPreset::VoiceRecognition; break;
            case 3: preset = oboe::InputPreset::VoiceCommunication; break;
            case 4: preset = oboe::InputPreset::Unprocessed; break;
            case 5: preset = oboe::InputPreset::VoicePerformance; break;
        }
        engine.recorder.setAudioSource(preset);
    });
}

JNIEXPORT jboolean JNICALL
Java_com_example_oboesample_AudioEngine_startRecording(JNIEnv* env, jobject owner) {
    return withEngine(env, owner, [&](NativeEngine& engine) {
        engine.player.stopPlayback(); return engine.recorder.startRecording() == oboe::Result::OK;
    });
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_stopRecording(JNIEnv* env, jobject owner) {
    withEngine(env, owner, [&](NativeEngine& engine) {
        engine.recorder.stopRecording();
    });
}

JNIEXPORT jint JNICALL
Java_com_example_oboesample_AudioEngine_getRecordingSessionId(JNIEnv* env, jobject owner) {
    return withEngine(env, owner, [&](NativeEngine& engine) {
        return engine.recorder.getSessionId();
    });
}

JNIEXPORT jboolean JNICALL
Java_com_example_oboesample_AudioEngine_isRecordingActive(JNIEnv* env, jobject owner) {
    return withEngine(env, owner, [&](NativeEngine& engine) {
        return engine.recorder.isRecording();
    });
}

JNIEXPORT jboolean JNICALL
Java_com_example_oboesample_AudioEngine_playRecording(JNIEnv* env, jobject owner) {
    return withEngine(env, owner, [&](NativeEngine& engine) {
        engine.recorder.stopRecording(); return !engine.recordingPath.empty() && engine.player.startPlaybackFromFile(engine.recordingPath.c_str()) == oboe::Result::OK;
    });
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_stopPlayback(JNIEnv* env, jobject owner) {
    withEngine(env, owner, [&](NativeEngine& engine) {
        engine.player.stopPlayback();
    });
}

JNIEXPORT jboolean JNICALL
Java_com_example_oboesample_AudioEngine_playInputRecording(JNIEnv* env, jobject owner) {
    return withEngine(env, owner, [&](NativeEngine& engine) {
        engine.recorder.stopRecording();
        const auto path = engine.recordingPath + ".input.wav";
        return !engine.recordingPath.empty() &&
                engine.player.startPlaybackFromFile(path.c_str()) == oboe::Result::OK;
    });
}

JNIEXPORT jlong JNICALL
Java_com_example_oboesample_AudioEngine_readWaveform(JNIEnv* env, jobject owner, jfloatArray destination) {
    return withEngine(env, owner, [&](NativeEngine& engine) -> jlong {
        if (!destination || env->GetArrayLength(destination) < static_cast<jsize>(WaveformMonitor::snapshotFloats)) {
            throwJava(env, "java/lang/IllegalArgumentException", "Waveform destination is too small");
            return 0;
        }
        std::array<float, WaveformMonitor::snapshotFloats> snapshot{};
        const auto sequence = engine.recorder.waveformSnapshot(snapshot.data());
        env->SetFloatArrayRegion(destination, 0, static_cast<jsize>(snapshot.size()), snapshot.data());
        return static_cast<jlong>(sequence);
    });
}

JNIEXPORT jboolean JNICALL
Java_com_example_oboesample_AudioEngine_isPlaybackActive(JNIEnv* env, jobject owner) {
    return withEngine(env, owner, [&](NativeEngine& engine) {
        return engine.player.isPlaying();
    });
}

JNIEXPORT jstring JNICALL
Java_com_example_oboesample_AudioEngine_getLatencyDiagnostics(JNIEnv* env, jobject owner) {
    return withEngine(env, owner, [&](NativeEngine& engine) {
        const auto json = std::string("{\"recording\":") + engine.recorder.latencyDiagnostics() + ",\"playback\":" + engine.player.latencyDiagnostics() + "}"; return env->NewStringUTF(json.c_str());
    });
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_setAndroidAECEnabled(JNIEnv* env, jobject owner, jboolean enabled) {
    withStoppedRecorder(env, owner, [&](NativeEngine& engine) {
        engine.recorder.setAndroidAECEnabled(enabled);
    });
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_setBandpassFilterEnabled(JNIEnv* env, jobject owner, jboolean enabled) {
    withStoppedRecorder(env, owner, [&](NativeEngine& engine) {
        engine.recorder.setBandpassFilterEnabled(enabled);
    });
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_setHighShelfFilterEnabled(JNIEnv* env, jobject owner, jboolean enabled) {
    withStoppedRecorder(env, owner, [&](NativeEngine& engine) {
        engine.recorder.setHighShelfFilterEnabled(enabled);
    });
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_setPeakingFilterEnabled(JNIEnv* env, jobject owner, jboolean enabled) {
    withStoppedRecorder(env, owner, [&](NativeEngine& engine) {
        engine.recorder.setPeakingFilterEnabled(enabled);
    });
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_setNoiseGateEnabled(JNIEnv* env, jobject owner, jboolean enabled) {
    withStoppedRecorder(env, owner, [&](NativeEngine& engine) {
        engine.recorder.setNoiseGateEnabled(enabled);
    });
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_setNoiseReductionEnabled(JNIEnv* env, jobject owner, jboolean enabled) {
    withStoppedRecorder(env, owner, [&](NativeEngine& engine) {
        engine.recorder.setNoiseReductionEnabled(enabled);
    });
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_setEchoCancellerEnabled(JNIEnv* env, jobject owner, jboolean enabled) {
    withStoppedRecorder(env, owner, [&](NativeEngine& engine) {
        engine.recorder.setEchoCancellerEnabled(enabled);
    });
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_setPlaybackSuppressorEnabled(JNIEnv* env, jobject owner, jboolean enabled) {
    withStoppedRecorder(env, owner, [&](NativeEngine& engine) {
        engine.recorder.setPlaybackSuppressorEnabled(enabled);
    });
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_configureBandpassFilter(JNIEnv* env, jobject owner, jfloat frequency, jfloat q) {
    withStoppedRecorder(env, owner, [&](NativeEngine& engine) {
        engine.recorder.configureBandpassFilter(frequency, q);
    });
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_configureHighShelfFilter(JNIEnv* env, jobject owner, jfloat frequency, jfloat q, jfloat gain) {
    withStoppedRecorder(env, owner, [&](NativeEngine& engine) {
        engine.recorder.configureHighShelfFilter(frequency, q, gain);
    });
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_configurePeakingFilter(JNIEnv* env, jobject owner, jfloat frequency, jfloat q, jfloat gain) {
    withStoppedRecorder(env, owner, [&](NativeEngine& engine) {
        engine.recorder.configurePeakingFilter(frequency, q, gain);
    });
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_configureNoiseGate(JNIEnv* env, jobject owner, jfloat threshold, jfloat ratio, jfloat attack, jfloat release) {
    withStoppedRecorder(env, owner, [&](NativeEngine& engine) {
        engine.recorder.configureNoiseGate(threshold, ratio, attack, release);
    });
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_configureNoiseReduction(JNIEnv* env, jobject owner, jfloat amount) {
    withStoppedRecorder(env, owner, [&](NativeEngine& engine) {
        engine.recorder.configureNoiseReduction(amount);
    });
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_configureNoiseFloor(JNIEnv* env, jobject owner, jfloat db) {
    withStoppedRecorder(env, owner, [&](NativeEngine& engine) {
        engine.recorder.configureNoiseFloor(db);
    });
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_configureEchoCanceller(JNIEnv* env, jobject owner, jfloat delay, jfloat amount) {
    withStoppedRecorder(env, owner, [&](NativeEngine& engine) {
        engine.recorder.configureEchoCanceller(delay, amount);
    });
}

JNIEXPORT void JNICALL
Java_com_example_oboesample_AudioEngine_configurePlaybackSuppressor(JNIEnv* env, jobject owner, jfloat amount) {
    withStoppedRecorder(env, owner, [&](NativeEngine& engine) {
        engine.recorder.configurePlaybackSuppressor(amount);
    });
}
}
