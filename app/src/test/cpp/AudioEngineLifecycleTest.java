package com.example.oboesample;

import java.io.File;
import java.nio.file.Files;
import java.util.concurrent.atomic.AtomicReference;

// Runs the production Kotlin class and JNI against a host Oboe lifecycle double.
// It does not replace Android lifecycle or real Oboe/device tests.
public final class AudioEngineLifecycleTest {
    private static native int liveStreams();
    private static native void feed(int samples);
    private static native void failNextOpen();
    private static native void failNextStart();

    private static void expectClosed(AudioEngine engine) {
        try { engine.isRecordingActive(); throw new AssertionError("closed handle was accepted"); }
        catch (IllegalStateException expected) { }
    }

    public static void main(String[] args) throws Exception {
        for (var method : AudioEngine.class.getDeclaredMethods()) {
            if (java.lang.reflect.Modifier.isNative(method.getModifiers())) {
                assert java.lang.reflect.Modifier.isSynchronized(method.getModifiers());
            }
        }
        File directory = new File(args[0]);
        File firstPath = new File(directory, "first.wav");
        File secondPath = new File(directory, "second.wav");
        AudioEngine first = new AudioEngine();
        AudioEngine second = new AudioEngine();
        first.setRecordingPath(firstPath.toString());
        second.setRecordingPath(secondPath.toString());
        assert first.startRecording();
        try { first.configureNoiseReduction(0.2f); throw new AssertionError("active DSP mutation accepted"); }
        catch (IllegalStateException expected) { }
        assert second.startRecording();
        assert liveStreams() == 2;
        first.close();
        first.close();
        expectClosed(first);
        assert second.isRecordingActive();
        assert liveStreams() == 1;
        feed(1000);
        second.close();
        assert liveStreams() == 0;
        assert Files.size(secondPath.toPath()) == 44 + 2000; // Destroy drained the worker.

        try (AudioEngine engine = new AudioEngine()) {
            try { engine.setRecordingPath(null); throw new AssertionError("null JNI string accepted"); }
            catch (IllegalArgumentException expected) { }
            engine.setRecordingPath(secondPath.toString());
            failNextOpen();
            assert !engine.startRecording();
            assert liveStreams() == 0;
            failNextStart();
            assert !engine.startRecording();
            assert liveStreams() == 0;
            assert engine.startRecording();
            feed(256);
            engine.stopRecording();
            float[] waveform = new float[AudioEngine.WAVEFORM_FLOATS];
            assert engine.readWaveform(waveform) > 0;
            assert waveform[6] == 256;
            assert waveform[0] == waveform[2];
            try { engine.readWaveform(new float[2]); throw new AssertionError("small snapshot buffer accepted"); }
            catch (IllegalArgumentException expected) { }
            engine.configureNoiseReduction(0.2f);
            assert engine.playInputRecording();
            engine.stopPlayback();
            assert engine.playRecording();
            assert liveStreams() == 1;
        }
        assert liveStreams() == 0; // Destroy an active playback stream/read-ahead worker.

        // A concurrent close cannot race native handle lookup or diagnostics allocation.
        AudioEngine racing = new AudioEngine();
        racing.setRecordingPath(firstPath.toString());
        assert racing.startRecording();
        AtomicReference<Throwable> error = new AtomicReference<>();
        Thread observer = new Thread(() -> {
            try {
                for (int i = 0; i < 10000; i++) {
                    try { racing.getLatencyDiagnostics(); }
                    catch (IllegalStateException closed) { return; }
                }
            } catch (Throwable failure) { error.set(failure); }
        });
        observer.start();
        racing.close();
        observer.join();
        if (error.get() != null) throw new AssertionError(error.get());
        expectClosed(racing);
        assert liveStreams() == 0;
        System.out.println("Production JNI lifecycle checks passed with host stream double");
    }
}
