# Code review and improvements

**Update:** Subsequent DSP and control changes supersede the filter limitations below; see
[FILTER_REVIEW.md](FILTER_REVIEW.md) and [LATENCY_REVIEW.md](LATENCY_REVIEW.md) for current behavior, validation and remaining limits.

## Fixed

- **High — recording lifecycle:** the timer previously inferred state from buttons before
  they were updated. This prevented reliable timers, focus-loss handling and background
  cleanup. State is now explicit and elapsed time uses a monotonic clock.
- **High — file format:** raw PCM was named `recording.wav`, and playback independently
  probed output channel count. Capture and playback now use mono 48 kHz PCM16, emit and
  validate a canonical WAV header, and reject unsupported stream formats.
- **High — native failures and stream leaks:** JNI now reports start failures. Streams
  close after failed starts and after reaching EOF, including already-stopped streams.
  The activity polls completion and disconnection, restores controls and closes resources.
- **High — echo-cancellation session:** the Java effect used an unrelated session ID,
  could dereference a null effect and was never released. It now uses the Oboe input
  stream's allocated session, checks availability and releases the effect after recording.
  Session allocation follows [Oboe's documented API](https://google.github.io/oboe/classoboe_1_1_audio_stream_builder.html).
- **High — concurrent filter changes:** presets, switches and sliders are disabled during
  capture so this activity cannot mutate DSP coefficients or resize delay buffers while
  the callback processes them. Native control calls remain confined to the main thread.
- **Medium — suppressor:** the recorder never enabled the suppressor's internal flag.
  Its gain decision also lasted only one sample per window. Both are corrected, and
  per-window callback logging is removed.
- **Medium — callback allocation and clipping:** processing now reuses the input buffer
  and removes the unconditional 2x gain applied whenever processing was enabled.
- **Medium — audio settings:** audio mode changes only during recording and restores its
  previous value. Volume restoration attempts each stream independently. The legacy
  focus listener is retained for abandonment. Opening the app or recording no longer
  launches DND settings automatically.
- **Low — CMake:** corrected the malformed filter include-directory expression.

## Validation

Run native regression checks on a host with a C++ compiler:

```sh
rtk proxy sh app/src/test/cpp/run.sh
```

These checks pass with AddressSanitizer and UndefinedBehaviorSanitizer. They cover
WAV finalization, truncated/invalid/empty files, channel and sample-rate rejection,
and suppression persisting across processing windows and resetting correctly.

Android build, JVM tests and lint were attempted with JDK 21:

```sh
rtk proxy ./gradlew :app:assembleDebug :app:testDebugUnitTest :app:lintDebug
```

They could not run: required Android Gradle Plugin dependencies are missing from the
local cache, and a direct connection to `dl.google.com` timed out. The online build
was cancelled while resolving dependencies. The changed Android/JNI code still needs
build and device validation.

## Remaining gaps

1. **High — real-time disk I/O:** recorder and player still access files directly from
   Oboe callbacks. Move storage to worker threads using bounded single-producer,
   single-consumer buffers, with explicit overrun and underrun handling.
2. **Medium — DSP limits:** the software `EchoCanceller` processes delayed microphone
   samples without a speaker reference. The suppressor is an energy/zero-crossing
   heuristic that can attenuate speech. Neither guarantees removal of other apps' audio;
   evaluate presets using measured recordings and a proper reference-based AEC if needed.
3. **Medium — live controls:** controls are deliberately unavailable while recording.
   Supporting live adjustment requires publishing parameter snapshots or queued commands
   consumed by the callback, rather than directly mutating DSP objects through JNI.
4. **Medium — device coverage:** verify permission denial/revocation, focus loss,
   backgrounding, rotation, repeated record/play/EOF cycles, Bluetooth routing, storage
   errors, and devices with and without hardware AEC. Effects and audio routing depend
   on the device; session allocation is not available on every supported Android version.
5. **Medium — compatibility and durability:** old files containing raw PCM under the
   `.wav` name are rejected; record a new clip. WAV sizes finalize on orderly stop;
   process death during capture can leave an unfinished header. Capturing currently
   replaces the previous recording, so a temporary-file/atomic-replace workflow would
   improve failure recovery. Playback accepts only the canonical WAV format this app emits.
6. **Low — UI and automated coverage:** most UI text is hardcoded and preset state is
   not persisted. Existing Android/JVM tests are template tests; meaningful lifecycle
   coverage remains to be added once a device/build environment is available.
