# Filter-by-filter improvements

**Waveform/file update:** See [OSCILLOSCOPE_RECORDING.md](OSCILLOSCOPE_RECORDING.md) for
live aligned input/output plots, paired equal-length WAVs and document-picker saves.

**Lifecycle update:** See [MEMORY_LIFECYCLE.md](MEMORY_LIFECYCLE.md) for per-activity
engine ownership, explicit close, worker capture cleanup and lifecycle tests.

**Latency update:** See [LATENCY_REVIEW.md](LATENCY_REVIEW.md) for the subsequent worker
queues, 128-sample spectral delay, callback changes and concurrency validation.

The supplied `dsp-audio-filter-effects` skill was installed and validated at
`/Users/shukugup/.codex/skills/dsp-audio-filter-effects/SKILL.md`. Compatibility metadata
was nested under `metadata` for this Codex validator. Example wording clarifies causal
filtering, Nyquist limits and denormal handling; the compressor remains an example,
not an added app feature.

| Filter | Functionality gap | Implemented improvement | Focused verification |
| --- | --- | --- | --- |
| Echo cancellation | Delayed microphone subtraction altered speech without a playback reference; switch did not control the Java effect | Microphone-only software processing is transparent. Added an explicit reference-driven single-path adaptation API. The switch requests hardware AEC on the actual recording session; off requests disabling the effect. Unsupported sessions and enable failures are reported | Exact microphone-only pass-through; >40 dB reduction on a modeled delayed, scaled reference; reset, zero reference and invalid settings |
| Noise reduction | Moving average removed treble regardless of whether it was noise | Replaced with a fixed 128-point FFT spectral attenuator, 50% overlap, sine windows and a bounded gain floor. Added a configurable stationary noise floor. No per-frame allocation. Flushed buffered output at orderly stop | >10 dB reduction on noise well below the configured floor; strong 8 kHz tone within 0.1 dB; zero-strength bypass; 128-sample impulse delay and tail recovery; reset and invalid strength |
| Noise gate | Corrected expansion still changed gain without a separate gain envelope; reset/invalid-input behavior needed coverage | Added attack/release gain smoothing and flushed tiny recursive states. Nonfinite input resets safely. Gate ratio/timing survive threshold slider changes | Correct 4:1 transfer; above-threshold and ratio-1 behavior; bounded per-sample gain changes; silence/reset/nonfinite recovery |
| Voice bandpass | Lower bounds, invalid values and near-edge precision were not protected; full-band presets unnecessarily enabled it | Validate sample rate/frequency/Q, bound cutoffs below Nyquist and use double-precision coefficients/state. Preserve its intended center/Q behavior. Podcast and studio presets now leave narrow bandpass off | Unity center gain, low/high-frequency rejection, finite long impulse decay at a cutoff boundary, reset and invalid-configuration bypass |
| Presence / peaking EQ | Invalid or extreme gain/Q could poison the signal; no explicit boost headroom; no direct gain control | Bound gain/Q and validate finite values. Zero gain bypasses exactly. Added a gain slider and conservative pre-EQ attenuation for positive boosts | Requested +6 and −6 dB at the center, exact zero-gain bypass, invalid settings and combined-EQ signal bounds |
| Clarity / high shelf | Shelf Q could create resonant overshoot; no direct gain control or headroom | Restrict shelf Q to its monotonic range, use double precision, validate inputs and bound gain. Added a gain slider and shared EQ headroom | Near 0 dB at low frequency, half requested gain at the shelf center, expected high-frequency shelf, zero-gain bypass and invalid settings |
| Optional playback suppressor | Fixed 100-sample decisions depended on sample rate and stepped gain; heuristic could suppress voiced speech automatically | Use 10 ms analysis windows and 5 ms gain smoothing, reset on enable transitions, sanitize strength/input. Disabled by default; remains available through the existing API | Smooth transitions, settled attenuation, disabled/invalid-strength bypass |

The EQ implementation follows the coefficient definitions in the
[W3C Audio EQ Cookbook](https://www.w3.org/TR/audio-eq-cookbook/). Reference-based echo
processing requires a synchronized speaker signal; see the
[Speex processing API](https://www.speex.org/docs/manual/speex-manual/node7.html).

## Controls and presets

- Added noise-floor, presence-gain and clarity-gain sliders. They are disabled while
  recording and when their associated filter is off.
- Fixed the initial gate slider position and made the bandpass slider represent exact Hz.
- Presets update labels, sliders and native settings together. Changing one slider retains
  the preset's other parameters. Numeric settings and switches survive activity recreation.
- Podcast/studio presets use gentler expansion and EQ and preserve full-band frequency
  content. These are starting settings, not listening-calibrated delivery presets.
- Positive EQ boosts reserve the sum of requested boost gains before EQ. This lowers
  the broadband level to provide headroom; it is not a true-peak limiter. Final sample
  clipping remains a last guard.

## Validation

```sh
rtk proxy sh app/src/test/cpp/filters.sh
rtk proxy sh app/src/test/cpp/run.sh
rtk proxy sh app/src/test/cpp/measure.sh
```

Host behavior and regression checks pass with strict warnings, AddressSanitizer and
UndefinedBehaviorSanitizer. Layout XML parses, all referenced view-binding IDs exist,
and all 30 Kotlin native method names match JNI symbols. These static integration checks
do not replace an Android build. Build/JVM/lint and device testing remain unverified:
the earlier build lacked cached dependencies and the dependency repository timed out.

Current synthetic transfer metrics are in `audio-quality-report.json`. The former
moving-average loss of roughly 8.98 dB at 8 kHz is now roughly 0.0001 dB for a strong
8 kHz tone at the default noise floor and 60% strength. This is frequency-preservation
evidence, not a perceptual quality or SNR score.

## Practical limits

- Spectral attenuation assumes a stationary, approximately white noise floor supplied
  through the slider; it does not learn a noise profile or separate competing speech.
  Incorrect floor settings can remove quiet detail or create spectral artifacts.
- Active noise reduction adds 128 samples of latency (2.67 ms at 48 kHz). Stop drains
  that many samples and trims startup delay, preserving the original capture duration.
- The reference-driven software echo model handles a single aligned path. It is not a
  production multipath/double-talk AEC and is not fed a playback reference by this app.
  Actual device AEC effectiveness and OEM processing when off require device testing.
- Filter controls remain unavailable during recording because setters mutate DSP state.
  Supporting live changes requires worker-owned parameter updates.
- File I/O and capture DSP now run on workers behind bounded queues. Hardware routing, CPU deadline behavior,
  listening quality, LUFS, true peak and real-world SNR still need device recordings.
