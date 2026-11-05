package com.example.oboesample

import android.app.Application
import android.net.Uri
import androidx.lifecycle.Observer
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.ViewModelStore
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.io.File
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class WavExportTest {
    @Test
    fun copiesCompletedWavAndReportsSuccess() {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        val app = instrumentation.targetContext.applicationContext as Application
        val source = File.createTempFile("export-source", ".wav", app.cacheDir)
        val destination = File.createTempFile("export-target", ".wav", app.cacheDir)
        val buffer = ByteBuffer.allocate(64).order(ByteOrder.LITTLE_ENDIAN)
        buffer.put("RIFF".toByteArray()).putInt(56).put("WAVEfmt ".toByteArray())
            .putInt(16).putShort(1).putShort(1).putInt(48000).putInt(96000)
            .putShort(2).putShort(16).put("data".toByteArray()).putInt(20)
        repeat(10) { buffer.putShort((it * 100).toShort()) }
        source.writeBytes(buffer.array())
        val store = ViewModelStore()
        val completed = CountDownLatch(1)
        var model: WavExportViewModel? = null
        var message = ""
        val observer = Observer<WavExportViewModel.State> { state ->
            if (!state.busy && state.message.isNotEmpty()) {
                message = state.message
                completed.countDown()
            }
        }
        try {
            instrumentation.runOnMainSync {
                model = ViewModelProvider(store, ViewModelProvider.AndroidViewModelFactory(app))[WavExportViewModel::class.java]
                checkNotNull(model).state.observeForever(observer)
                checkNotNull(model).save(source, Uri.fromFile(destination), input = false)
            }
            assertTrue(completed.await(10, TimeUnit.SECONDS))
            assertEquals(app.getString(R.string.export_output_success), message)
            assertArrayEquals(source.readBytes(), destination.readBytes())
        } finally {
            instrumentation.runOnMainSync {
                model?.state?.removeObserver(observer)
                store.clear()
            }
            source.delete()
            destination.delete()
        }
    }
}
