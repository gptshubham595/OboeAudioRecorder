# Oboe Audio Recorder

Android microphone recorder with native Oboe audio, configurable voice filters, and paired input/output monitoring.

- Two stacked oscilloscopes compare input before app software processing with the processed output on the same scale. Freeze and zoom help inspect a take; peak, RMS and clipping indicators show signal levels.
- Record paired mono 48 kHz PCM16 WAV files. Audition input or output, then save either with Android's document picker.
- Configure noise reduction, noise gate, voice bandpass, presence EQ and clarity shelf. Hardware echo cancellation reports its actual session availability; software cancellation requires a playback reference.
- Bounded queues move DSP and file access off audio callbacks. Per-Activity native ownership and explicit stop/close paths release audio resources.

Grant microphone permission, choose the filters or a preset, and tap **Record**. Tap **Stop** before comparing playback or saving input/output WAVs. Export finishes before another take can start.

Build with JDK 21, Android SDK 36, NDK `29.0.14206865` and CMake `3.22.1`:

```sh
./gradlew :app:assembleDebug
```

See [validation results and test commands](VALIDATION.md), [filter review](FILTER_REVIEW.md), [latency review](LATENCY_REVIEW.md), [memory/lifecycle review](MEMORY_LIFECYCLE.md), and [waveform/recording details](OSCILLOSCOPE_RECORDING.md).

Input may already include hardware/Android processing. Waveform appearance and level indicators cannot determine whether speech sounds good; listening and device-specific testing remain necessary.

Original interface reference:

![Original recorder interface](https://github.com/user-attachments/assets/c8c9344b-e87b-4c62-9268-93b74deba25c)
