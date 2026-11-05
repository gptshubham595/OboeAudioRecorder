# Audio quality review

**Update:** The initial measurements below are historical. The subsequent filter-by-filter
changes, current behavior and validation are documented in [FILTER_REVIEW.md](FILTER_REVIEW.md).
`audio-quality-report.json` contains the original baseline and current synthetic results.

The audio-quality-controller and audio-engineer skills were consulted for this review.
The latter describes OBS workflows; only its level-diagnostics principles apply to this
Android project. No OBS settings were accessed or changed.

## Measured findings

These are synthetic DSP transfer measurements at 48 kHz, not measurements of speech,
microphone quality, LUFS, true peak, SNR, or broadcast compliance. No audio recordings
were found in the project. The gate probes use constant input after two seconds of
settling; frequency probes measure sine-wave RMS gain after a one-second warm-up.

| Test | Before | After | Interpretation |
| --- | ---: | ---: | --- |
| Gate gain at −60 dBFS | −9.7624 dB | −60.0004 dB | Correct 4:1 expansion below a −40 dBFS threshold |
| Gate gain at −50 dBFS | −6.2464 dB | −30.0002 dB | Correct 4:1 expansion |
| Gate gain at −20 dBFS | 0 dB | 0 dB | Above-threshold input is preserved |
| Voice bandpass at 100 Hz | −25.1083 dB | unchanged | Strong removal of low voice frequencies |
| Voice bandpass at 8 kHz | −16.7800 dB | unchanged | Strong removal of high-frequency detail |
| Five-sample smoothing at 8 kHz, amount 0.6 | −8.9825 dB | unchanged | Smoothing trades treble detail for attenuation |

The gate previously used linear amplitude subtraction, which limited attenuation to
approximately 12 dB at a 4:1 ratio even far below threshold. It now implements downward
expansion in decibels, with finite bounded parameters. Existing envelope attack and
release settings are retained. Their API comments were corrected: attack opens the gate,
release closes it. Also removed an unused high-shelf variable identified by strict compilation.

The corrected gate is stronger below threshold. Listen for loss of quiet syllables before
choosing the existing aggressive 6:1 or 8:1 presets. It is a downward expander with a
smooth envelope, not a hard gate with hold/hysteresis. No preset values were retuned.

## Remaining quality considerations

- Podcast and studio presets enable narrow bandpass filtering. Consider disabling it
  when natural full-band voice quality matters, after comparing actual recordings.
- `NoiseReduction` is moving-average smoothing, not an estimated-noise spectral denoiser.
  Measure intelligibility and high-frequency detail before increasing its amount.
- The software echo suppressor lacks a playback reference, and the playback suppressor
  can mistake sustained voice for unwanted sound. Neither provides assured separation.
- EQ boosts can cause clipping at PCM conversion. Final clipping bounds samples but does
  not establish true-peak headroom. Assess a measured final gain or limiter after EQ.
- Fixing DSP does not address the callback disk-I/O risk documented in `REVIEW.md`.

## Reproduce checks

```sh
rtk proxy sh app/src/test/cpp/run.sh
rtk proxy sh app/src/test/cpp/measure.sh
```

Both compile with strict warnings, AddressSanitizer and UndefinedBehaviorSanitizer.
The Android target explicitly requests C++17 for the bounded-parameter implementation.
Android compilation and device validation remain blocked by the dependency availability
described in `REVIEW.md`; these host checks do not verify JNI or Android lifecycle code.

## Measure an actual recording

Keep an untouched copy and compare identical speech passages, including quiet syllables,
pauses, plosives and sibilance. First measure, then choose processing. The following use
[FFmpeg's documented filters](https://ffmpeg.org/ffmpeg-filters.html):

```sh
# Integrated loudness, loudness range and true peak; no output file is changed.
rtk proxy ffmpeg -hide_banner -i recording.wav -af loudnorm=I=-16:TP=-1.5:LRA=11:print_format=json -f null -

# Sample peaks, RMS and other signal statistics.
rtk proxy ffmpeg -hide_banner -i recording.wav -af astats=metadata=0:reset=0 -f null -

# Frequency/time visualization for inspecting noise and spectral loss.
rtk proxy ffmpeg -i recording.wav -lavfi showspectrumpic=s=1600x900:legend=1 -frames:v 1 spectrum.png
```

A −16 LUFS / −1.5 dBTP target is an example delivery choice, not a universal broadcast
standard or a capture gain setting. Loudness range is content-dependent, particularly for
short clips. SNR requires a defined noise-only segment or a known clean reference; it
cannot be reliably inferred from peak/RMS alone. OBS fader gain is likewise distinct
from measured signal loudness.

For a separate offline experiment, candidate filters include `highpass=f=70` for rumble,
`afftdn=nr=6:nf=-50` for steady noise, `equalizer=f=3000:t=q:w=1:g=1.5` for mild presence,
and `acompressor=threshold=0.125:ratio=2:attack=10:release=150:makeup=1` for dynamics.
These are starting examples, not calibrated settings. Measure the noise floor, listen for
artifacts and only retain needed stages. Apply noise treatment before EQ, then dynamics,
and finally normalize the resulting signal.

For two-pass normalization, measure the exact signal entering the normalizer, then use
its reported `input_i`, `input_lra`, `input_tp`, `input_thresh` and `target_offset` as
`measured_I`, `measured_LRA`, `measured_TP`, `measured_thresh` and `offset` in a second
`loudnorm` invocation. Specify `-ar 48000` for the output. Re-measure the final encoded
file for true peak and loudness. Do not substitute synthetic DSP metrics for these values.

The JSON report in `audio-quality-report.json` leaves recording-level metrics and the
perceptual improvement score null until representative audio can be measured and heard.
