package com.example.oboesample

import android.os.ParcelFileDescriptor
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.io.File
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.concurrent.TimeUnit
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith

/** Real microphone/Oboe smoke test, using temporary files rather than the user's take. */
@RunWith(AndroidJUnit4::class)
class AudioPipelineTest {
    @Test
    fun captureProducesAlignedWavsLiveWaveformsAndPlayableFiles() {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        val context = instrumentation.targetContext
        ParcelFileDescriptor.AutoCloseInputStream(instrumentation.uiAutomation.executeShellCommand(
            "pm grant ${context.packageName} android.permission.RECORD_AUDIO"
        )).use { it.readBytes() }
        val output = File.createTempFile("pipeline-test", ".wav", context.cacheDir)
        val input = File(output.path + ".input.wav")
        try {
            ActivityScenario.launch(MainActivity::class.java).use {
                AudioEngine().use { engine ->
                    engine.setRecordingPath(output.path)
                    engine.setAudioSource(AudioEngine.SOURCE_GENERIC)
                    engine.setAndroidAECEnabled(false)
                    engine.configureNoiseReduction(0.3f)
                    engine.setNoiseReductionEnabled(true)
                    engine.configureNoiseGate(-50f, 2f, 5f, 100f)
                    engine.setNoiseGateEnabled(true)
                    engine.configureBandpassFilter(1500f, 1.2f)
                    engine.setBandpassFilterEnabled(true)
                    engine.configurePeakingFilter(3000f, 1f, 2f)
                    engine.setPeakingFilterEnabled(true)
                    engine.configureHighShelfFilter(8000f, 0.7f, 2f)
                    engine.setHighShelfFilterEnabled(true)
                    assertTrue("Oboe input stream did not start", engine.startRecording())
                    val snapshot = FloatArray(AudioEngine.WAVEFORM_FLOATS)
                    val deadline = System.nanoTime() + TimeUnit.SECONDS.toNanos(5)
                    while (engine.readWaveform(snapshot) == 0L && System.nanoTime() < deadline) {
                        Thread.sleep(20)
                    }
                    assertTrue("No microphone callbacks reached the display", snapshot[6] > 0)
                    engine.stopRecording()
                    engine.readWaveform(snapshot)
                    assertTrue(snapshot.all { value -> value.isFinite() })
                    assertTrue(snapshot[0] in 0f..1f && snapshot[2] in 0f..1f)
                    assertEquals(input.length(), output.length())
                    assertTrue(output.length() > 44)
                    for (file in listOf(input, output)) {
                        val header = ByteArray(44)
                        file.inputStream().use { stream -> assertEquals(44, stream.read(header)) }
                        assertEquals("RIFF", String(header, 0, 4, Charsets.US_ASCII))
                        assertEquals("WAVE", String(header, 8, 4, Charsets.US_ASCII))
                        val dataBytes = ByteBuffer.wrap(header).order(ByteOrder.LITTLE_ENDIAN).getInt(40)
                        assertEquals(file.length() - 44, dataBytes.toLong())
                    }
                    assertTrue(engine.playInputRecording())
                    Thread.sleep(50)
                    engine.stopPlayback()
                    assertTrue(engine.playRecording())
                    Thread.sleep(50)
                    engine.stopPlayback()
                    assertFalse(engine.isRecordingActive())
                    assertFalse(engine.isPlaybackActive())
                }
            }
        } finally {
            input.delete()
            output.delete()
        }
    }
}
