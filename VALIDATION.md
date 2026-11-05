# Validation — 2026-11-05

The debug build and automated checks pass. This supersedes the historical dependency/cache blockers in the earlier review documents.

| Check | Result |
| --- | --- |
| `:app:assembleDebug` | Passed; real NDK/CMake builds for arm64-v8a, armeabi-v7a, x86 and x86_64 |
| `:app:testDebugUnitTest` | Passed; 1 existing template test |
| `:app:lintDebug` | Passed; 0 errors, 87 warnings |
| Native WAV/regression and filter suites | Passed with AddressSanitizer and UndefinedBehaviorSanitizer |
| Waveform, asynchronous file/queue and native-owner suites | Passed with ASan/UBSan and separately with ThreadSanitizer |
| Production Kotlin/JNI lifecycle harness | Passed with JVM `-Xcheck:jni`; host Oboe test double |
| Android instrumentation | All 4 tests passed on Android 10 / API 29 arm64 emulator using the actual APK and Oboe streams |

The Android tests exercise Activity recreation and owner cleanup; temporary microphone capture through the software filters; live waveform snapshots; finite/range checks; equal-length input/output WAVs with correct headers; input/output playback and stop; completed-WAV export copying and success reporting; and app context. Test capture uses temporary cache files and deletes them afterward. Emulator audio does not establish real microphone quality.

The physical-phone Gradle instrumentation run stalled without returning test results. It was cancelled; no physical-phone pass is claimed. No private recordings are included in this repository.

## Reproduce

Use JDK 21 and the Android SDK configured in `local.properties`. The project pins NDK `29.0.14206865` and CMake `3.22.1`.

```sh
./gradlew :app:assembleDebug :app:testDebugUnitTest :app:lintDebug
sh app/src/test/cpp/run.sh
sh app/src/test/cpp/filters.sh
sh app/src/test/cpp/waveforms.sh
sh app/src/test/cpp/waveforms.sh tsan
sh app/src/test/cpp/async.sh
sh app/src/test/cpp/async.sh tsan
sh app/src/test/cpp/owners.sh
sh app/src/test/cpp/owners.sh tsan
sh app/src/test/cpp/jni-lifecycle.sh
```

Build the test APK with `./gradlew :app:assembleDebugAndroidTest`. On a dedicated test emulator, install both APKs and run:

```sh
adb -s <test-emulator> install -r app/build/outputs/apk/debug/app-debug.apk
adb -s <test-emulator> install -r app/build/outputs/apk/androidTest/debug/app-debug-androidTest.apk
adb -s <test-emulator> shell am instrument -w -r com.example.oboesample.test/androidx.test.runner.AndroidJUnitRunner
```

## Remaining limits

- Lint warnings remain for UI text/style/layout, unused resources and dependency versions. They are not build failures.
- Real speech listening, hardware AEC effectiveness, routing/device changes, permission revocation, focus interruption and cloud document-provider interaction need physical-device coverage.
- No measured end-to-end device latency, sustained-load xrun rate or device-wide memory profile is claimed. The measured spectral algorithm delay is 128 samples (2.67 ms at 48 kHz); queue scheduling and hardware add latency.
- Scope levels/clipping are objective signal indicators, not a perceptual quality score. “Input” precedes this app's software filters but can already include Android or hardware processing.
