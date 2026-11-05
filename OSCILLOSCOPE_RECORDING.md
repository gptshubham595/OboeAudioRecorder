> Validation update (2026-11-05): the full Android debug build, JVM test, lint, native sanitizer suites, and all four Android emulator instrumentation tests now pass. Earlier build-blocker notes below are historical. See [VALIDATION.md](VALIDATION.md) for current evidence and remaining limits.

# Live input/output comparison and WAV saving

## Using the screen

1. Tap **Record** and speak. The input waveform appears above the processed output.
2. Both use the same one-second window, time buckets and amplitude scale. **Zoom ×1 / ×4 /
   ×16** changes both displays together; it never changes recorded audio gain.
3. **Freeze plots** holds the current comparison while recording continues. Resume to
   return to the latest window. Stop preserves the last displayed window.
4. Tap **Stop**, then **Play input** or **Play output** to compare the actual saved signals.
5. Choose **Save input WAV** or **Save output WAV**, then select a filename/location in
   Android's document picker. Saving runs off the UI thread and blocks replacing the
   local take until its copy completes.

The record/stop controls remain above the two stacked scopes. Playback/export controls
are below the comparison, followed by the existing presets and filters. The local take
is replaced by the next recording; exported copies remain at the chosen location.

## What is measured

Each plot shows sample min/max values so short peaks remain visible when many samples
share a screen column. Peak and RMS are measured independently, in dBFS, over the displayed
window. The level message distinguishes quiet/silent input, near-full-scale samples,
and output more than 25 dB below input. Otherwise it says levels look usable and asks
the user to listen. These are level heuristics, not a perceptual quality score, SNR,
LUFS or a true-peak measurement.

Input means PCM delivered by Android/Oboe **before this app's software filters**.
Device processing, hardware AEC, AGC and sample-rate/channel conversion may already
affect it. Hardware AEC therefore affects the common input to both displayed branches;
the graphs do not expose an unavailable pre-hardware-AEC signal. Software EQ can also
change phase and waveform shape without making audio worse.

## Native data and files

- The audio callback still only enqueues PCM. The file/DSP worker copies the incoming
  block, processes its output and submits both to `WaveformMonitor`.
- The monitor retains 480 buckets of 100 samples (one second at 48 kHz) plus a bounded
  raw delay line. Fixed FFT delay is compensated so paired buckets correspond in time.
- The UI requests a snapshot into one reusable float array approximately every 33 ms.
  Monitor locking occurs only between the worker and UI, never on the audio callback.
  Rendering reuses paints and line-coordinate arrays. Updates stop with the activity.
- Recording writes the unmodified app-input PCM to `recording.wav.input.wav` and the
  processed PCM to `recording.wav`, both app-private mono 48 kHz PCM16 WAV files.
- On successful stop, the processed startup delay is trimmed and the filter tail is
  drained. Both WAV files have the same sample count, including captures shorter than
  the processing delay. Raw files never include synthetic tail samples. Failure can
  leave valid but incomplete prefixes; the existing worker-error message reports this.
- `playInputRecording()` auditions the input file; existing playback auditions output.
  `readWaveform(FloatArray)` returns a consistent native snapshot and rejects undersized
  destinations. Lifecycle close and configuration guards remain in effect.

Saving uses [Android's document creation flow](https://developer.android.com/training/data-storage/shared/documents-files),
with no broad storage permission. An application-context
[ViewModel](https://developer.android.com/topic/libraries/architecture/viewmodel) owns
the bounded-copy worker so rotation can retain the operation. Streams close on completion
or failure. A cleared ViewModel interrupts the task; partial created documents are
removed when the provider supports deletion. Process death during a copy can still leave
an unfinished export. Waveform snapshot/zoom/freeze state is kept in saved instance state.

## Verification

```sh
rtk proxy sh app/src/test/cpp/waveforms.sh
rtk proxy sh app/src/test/cpp/waveforms.sh tsan
rtk proxy sh app/src/test/cpp/async.sh
rtk proxy sh app/src/test/cpp/async.sh tsan
rtk proxy sh app/src/test/cpp/owners.sh
rtk proxy sh app/src/test/cpp/owners.sh tsan
# JDK 21 and the cached Kotlin compiler dependencies are required:
rtk proxy sh app/src/test/cpp/jni-lifecycle.sh
```

Host checks pass for:

- Real-valued snapshot peak/RMS, impulse-peak retention, shared bucket timing, fixed-delay
  alignment, bounded history, reset and concurrent reads under ASan/UBSan and TSan.
- Bit-exact raw files and equal-length aligned processed files, including 3-sample,
  37-sample and 1000-sample captures, through the asynchronous writer.
- Recorder/player destruction and worker cleanup with actual spectral processing;
  raw-file contents remain unchanged and processed duration matches the input.
- Actual Kotlin `AudioEngine` plus production JNI with a host stream double: waveform
  reads, small-buffer rejection, raw audition and existing lifecycle invariants.
- XML/string parsing, view-binding ID checks and all 30 Kotlin/JNI method names.

All production Kotlin UI/export sources also compiled against SDK 36 and cached AndroidX
libraries using R/view-binding stand-ins. This verifies source/API usage, not Android
resource generation or a full configured APK build.

Android recreation coverage now checks retained export ownership. `WavExportTest` adds
a byte-for-byte export/success-state check. These device tests are added but not run here.
The full build/JVM/lint attempt failed before compilation because the configured Android
Gradle Plugin artifacts are not cached. Consequently actual microphone capture, rendering,
document-picker/provider interaction and device performance remain unverified.
