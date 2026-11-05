# Latency improvements

**Waveform/file update:** See [OSCILLOSCOPE_RECORDING.md](OSCILLOSCOPE_RECORDING.md) for
live aligned input/output plots, paired equal-length WAVs and document-picker saves.

**Lifecycle update:** See [MEMORY_LIFECYCLE.md](MEMORY_LIFECYCLE.md) for per-activity
engine ownership, explicit close, worker capture cleanup and lifecycle tests.

## Implemented changes

| Path | Before | Now |
| --- | --- | --- |
| Recording callback | Per-sample DSP and synchronous WAV writes | Bounded PCM copy to a lock-free single-producer/single-consumer queue |
| Playback callback | Synchronous file reads | Copy prefetched PCM; pad with silence and count starvation |
| Recording storage | Work on the audio callback deadline | DSP, file writes, spectral-tail draining and WAV finalization on a worker |
| Spectral filter | 256 samples / 5.33 ms at 48 kHz | 128 samples / 2.67 ms; verified by impulse position |
| FFT work | Trigonometry and repeated bit-reversal calculation per transform | Precomputed twiddle factors and bit-reversal indices; power-of-two circular addressing |
| Playback buffer | Relied on Oboe defaults | Explicit two-burst request, with actual negotiated size reported |
| Completion detection | Poll every 1000 ms | Poll every 100 ms; timer text only changes when displayed time changes |

The 50% reduction is the noise reducer's fixed algorithmic delay. It does not establish
an equivalent reduction in hardware or end-to-end latency. The shorter FFT also reduces
frequency resolution from 187.5 Hz to 375 Hz at 48 kHz; representative speech/noise
recordings still need listening evaluation.

The queue is bounded at 8192 mono PCM16 frames (16 KiB, 170.7 ms of audio at 48 kHz).
That is storage-stall tolerance, not a mandatory buffering delay. Workers consume
available data immediately, using 512-frame maximum work blocks without waiting to
fill a block. Idle worker polling sleeps up to 1 ms; callbacks never wait or notify a
condition variable. Playback primes 1024 samples before stream start; this is read-ahead
data, not an intentional output delay. No buffer grows with recording duration.

Capture stops and exposes an error if the queue fills; it does not silently discard a
block. Playback starvation preserves queued sample ordering, emits silence and counts
the event. A successful stop closes the stream first, drains/joins its worker and then
allows parameter changes. Stop remains synchronous and can wait for outstanding storage
work; this change protects callback deadlines, not UI behavior during a stalled disk.

## Stream configuration and diagnostics

Both streams retain low-latency/exclusive requests and the app's mono 48 kHz PCM16 file
contract. Oboe's medium-quality sample-rate conversion remains available for devices
whose natural rate differs. Playback remains media playback; it is not relabeled as a
game merely to request routing optimizations. Session effects and voice-communication
capture can affect the available low-latency path and require device comparison.

The explicit two-burst request is a policy, not a measured improvement over the existing
Oboe default. An implementation may clamp or ignore it, especially on older backends.
Input buffer size is not forced: Oboe documents output-only buffer sizing. See
[Android low-latency guidance](https://developer.android.com/games/sdk/oboe/low-latency-audio)
and [Oboe's buffer API](https://google.github.io/oboe/classoboe_1_1_audio_stream.html).

`engine.getLatencyDiagnostics()` returns recording/playback JSON with:

- App sample rate, frames per burst, current buffer frames, and negotiated performance
  and sharing mode enum values.
- Device xrun count, or −1 when unavailable/closed.
- Recording DSP delay, queue capacity, queue overflow count and worker failure status.
- Playback queue starvation count and worker failure status.

Open/close snapshots are logged outside callbacks under `AudioRecorder`/`AudioPlayer`.
The activity logs final counters under `MainActivity` and reports worker errors to the
user. Closed stream snapshots use zero for frame/buffer fields and −1 for modes/xruns;
these mean unavailable, not zero hardware latency. Inspect active or closing snapshots
for negotiated settings. `bufferFrames / sampleRate` estimates only that buffer's duration.

## Review of the supplied gist

The [DSP gist](https://gist.github.com/FOSSforlife/d0975cb7442099f0776cbed1f1a873ac)
supports precomputation and circular addressing, which are applied here. Its examples
are sketches rather than validated production code: the biquad computes an unused
numerator coefficient, delay wrapping assumes restricted ranges, and the SSE loop
assumes aligned buffers and a multiple-of-four length. It also cannot run on ARM as
written. Those implementations were not imported. Zero-delay filter topology likewise
does not remove Android buffering, device-effect or disk-I/O delays.

## Validation and limits

```sh
rtk proxy sh app/src/test/cpp/async.sh
rtk proxy sh app/src/test/cpp/async.sh tsan
rtk proxy sh app/src/test/cpp/filters.sh
rtk proxy sh app/src/test/cpp/run.sh
rtk proxy sh app/src/test/cpp/measure.sh
```

- AddressSanitizer/UndefinedBehaviorSanitizer pass for queue, worker, WAV and filter tests.
- ThreadSanitizer passes for queue/file-worker tests, including DSP on the worker.
- Tests exercise one million ordered concurrent samples, wraparound, full/empty queues,
  immediate-stop draining over 100 restarts, saved PCM ordering, processor failure,
  overflow reporting, EOF/short-output padding, starvation, cancellation and restart.
- A real spectral processor is tested through the writer, including its last-sample tail.
  Filter checks retain strong 8 kHz signal preservation and low-level noise attenuation.
- Layout/binding checks and all 30 Kotlin/JNI method names match. These checks do not
  establish compilation against Android/Oboe.

The Android build, JVM tests and lint were attempted again offline with JDK 21. They
failed before compilation because Android Gradle Plugin dependencies are not cached.
The previous online attempt could not reach the dependency repository. No Android
binary or device latency measurement was produced.

For device verification, compare negotiated modes, buffer sizes, device xruns and queue
counters with AEC on/off, speaker/headset/Bluetooth routes and sustained storage load.
Use loopback measurement for physical end-to-end latency. This app records and later
plays clips; it does not implement a live monitoring/round-trip path.
