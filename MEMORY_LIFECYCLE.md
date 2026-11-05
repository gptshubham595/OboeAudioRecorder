> Validation update (2026-11-05): the full Android debug build, JVM test, lint, native sanitizer suites, and all four Android emulator instrumentation tests now pass. Earlier build-blocker notes below are historical. See [VALIDATION.md](VALIDATION.md) for current evidence and remaining limits.

# Memory ownership and lifecycle cleanup

**Waveform/file update:** See [OSCILLOSCOPE_RECORDING.md](OSCILLOSCOPE_RECORDING.md) for
live aligned input/output plots, paired equal-length WAVs and document-picker saves.

## Implemented ownership

Each `MainActivity` now owns an `AudioEngine` instance. The Kotlin wrapper implements
`AutoCloseable` and owns a private native handle. The native engine owns its recorder,
player, DSP state, fixed queues, worker threads and recording-path strings. The former
process-global recorder/player/path objects are removed.

| Resource | Cleanup point and behavior |
| --- | --- |
| Native engine | `close()` invalidates the handle before deletion. Repeated close is safe; operations after close throw `IllegalStateException`. Different engine instances own independent resources |
| Native control calls | Synchronized on the same Kotlin instance monitor as `close()`, preventing handle lookup/use from racing destruction. Audio callbacks never take this monitor |
| Streams | Stop and close before cancelling/draining their associated workers. Stream/DSP owners cannot be copied or moved while callbacks hold their addresses |
| Recording worker | Joins before DSP members are destroyed; finalizes the WAV and then releases its processor function and captured objects |
| Reader/writer stop | Clear queue contents and obsolete counters after producer/consumer shutdown. Reader calls after stop produce silence and report completion |
| Failed starts | Run the same worker/file/callback cleanup, including releasing a processor whose output file could not be opened |
| DSP configuration | JNI rejects configuration changes while a recording stream is owned, including stopped/faulted streams whose worker may still need draining |
| JNI references and strings | Local class references are deleted within each call. UTF chars use a scoped release guard, including C++ exceptions and invalid-path handling. No Activity, global JNI reference or `JNIEnv` is retained in the engine/workers |
| Backgrounding | Unconditionally attempts to stop both owned native paths, removes handler work, cancels toast state and restores audio settings even if UI flags became stale |
| Activity destruction | Closes and clears the engine reference, removes queued work, clears view binding, and runs superclass destruction through `finally` blocks |
| Audio focus | Per-request listener uses a weak Activity reference plus generation checks, so stale focus callbacks cannot operate a newer session. Request/listener references clear on abandonment |
| Android audio effects/settings | Effect references detach before release. Mode, focus and volume cleanup run independently; a runtime failure in one step does not skip later steps |
| Transient UI work | Toasts use application context and cancel on stop. Timer text formats once per elapsed second while native completion remains polled every 100 ms |

`AudioManager` is obtained from application context. Destructors avoid constructing
diagnostic JSON during fallback stream teardown. JNI translates allocation/operation
exceptions from native control operations instead of letting C++ exceptions cross the
managed boundary. The class is kept for JNI field/method-name stability if minification
is enabled later.

Native buffers are reused between recordings while the owning activity exists. In
particular the two fixed PCM queues retain a bounded 32 KiB in total until engine
destruction. Stop clears their samples; close releases the owning allocation. Shared
library code and normal JVM class metadata remain process-owned. No heap/PSS reduction
number or claim of device-wide leak freedom is inferred from these changes.

## Verification

```sh
rtk proxy sh app/src/test/cpp/async.sh
rtk proxy sh app/src/test/cpp/async.sh tsan
rtk proxy sh app/src/test/cpp/owners.sh
rtk proxy sh app/src/test/cpp/owners.sh tsan
# JAVA_HOME must point to JDK 21; helper uses existing cached Kotlin 2.0.21 dependencies.
rtk proxy sh app/src/test/cpp/jni-lifecycle.sh
```

Passed checks:

- Worker tests under AddressSanitizer/UndefinedBehaviorSanitizer and ThreadSanitizer,
  including release of shared processor captures after stop, open failure and active
  worker destruction; stopped-reader silence and repeated stop/restart.
- Production recorder/player ownership tests under those sanitizers with a host Oboe
  stream double: 100 active destruction cycles, full DSP-tail/WAV finalization, failed
  open/start cleanup, zero live test streams after destruction and safe restart.
- Actual production `AudioEngine.kt` compiled with Kotlin 2.0.21, linked to the production
  JNI/stream-owner/worker code using the host stream double. JVM `-Xcheck:jni` verifies
  native calls; reflection confirms synchronized native methods. Tests cover independent
  owners, double close, use after close, closing active recording/playback, failed starts,
  null JNI strings, active-DSP mutation rejection and concurrent close/diagnostics calls.
- XML/view-binding checks and all 30 production Kotlin/JNI method names match.

The host stream double lives only under `app/src/test/cpp/oboe`; it is not used by the
Android target. These tests exercise application ownership, not actual Oboe callbacks or
Android effect/Activity behavior. The JVM suite runs without native sanitizers because
the host JVM's platform policy rejects loading the sanitizer runtime. Native owner and
worker suites provide the separate sanitizer coverage.

`AudioLifecycleTest.kt` adds Android recreation/destruction coverage: the old owner must
close, its binding and engine fields must clear, and the replacement owner must remain
usable. It is added but not run here. Android build/device validation remains blocked
by the uncached Android Gradle dependencies described in the earlier reports. On-device
heap/native allocation profiling across repeated rotation, background/resume, permission
changes and audio-focus loss is still required to verify platform resource retention.

## Usage contract

Use `AudioEngine` as a scoped owner (`use { ... }` for short-lived clients, or explicit
`close()` in an Android lifecycle owner). Stop capture before changing DSP configuration.
All file-worker `stop()`/destructor calls require their audio producer/consumer to be
closed first; the stream owners enforce that order. Close waits for worker shutdown and
outstanding storage operations. Activity recreation restores settings into a new engine
instead of sharing the old activity's native state.
