package com.example.oboesample

import android.os.ParcelFileDescriptor
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.assertNotSame
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.fail
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class AudioLifecycleTest {
    @Before
    fun grantMicrophonePermission() {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        val permission = "pm grant ${instrumentation.targetContext.packageName} android.permission.RECORD_AUDIO"
        ParcelFileDescriptor.AutoCloseInputStream(
            instrumentation.uiAutomation.executeShellCommand(permission)
        ).use { it.readBytes() }
    }

    private fun field(activity: MainActivity, name: String): Any? =
        MainActivity::class.java.getDeclaredField(name).apply { isAccessible = true }.get(activity)

    private fun assertClosed(engine: AudioEngine) {
        try {
            engine.isRecordingActive()
            fail("Destroyed activity retained an open native engine")
        } catch (_: IllegalStateException) {
            // A closed native handle must reject further operations.
        }
    }

    @Test
    fun recreationReleasesOldOwnerAndDestroyClearsViewReferences() {
        var originalActivity: MainActivity? = null
        var replacementActivity: MainActivity? = null
        var originalEngine: AudioEngine? = null
        var replacementEngine: AudioEngine? = null
        var retainedExports: Any? = null
        ActivityScenario.launch(MainActivity::class.java).use { scenario ->
            scenario.onActivity {
                originalActivity = it
                originalEngine = field(it, "ownedAudioEngine") as AudioEngine
                retainedExports = field(it, "exports")
            }
            scenario.recreate()
            scenario.onActivity {
                replacementActivity = it
                replacementEngine = field(it, "ownedAudioEngine") as AudioEngine
                assertNotSame(originalEngine, replacementEngine)
                assertSame(retainedExports, field(it, "exports"))
                assertClosed(checkNotNull(originalEngine))
                assertNull(field(checkNotNull(originalActivity), "activityBinding"))
                assertNull(field(checkNotNull(originalActivity), "ownedAudioEngine"))
                checkNotNull(replacementEngine).getLatencyDiagnostics() // Replacement is still usable.
            }
        }
        assertClosed(checkNotNull(replacementEngine))
        assertNull(field(checkNotNull(replacementActivity), "activityBinding"))
        assertNull(field(checkNotNull(replacementActivity), "ownedAudioEngine"))
    }
}
