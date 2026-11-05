package com.example.oboesample

import android.Manifest
import android.app.NotificationManager
import android.content.Context
import android.content.pm.PackageManager
import android.media.AudioAttributes
import android.media.AudioFocusRequest
import android.media.AudioManager
import android.media.audiofx.AcousticEchoCanceler
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.util.Log
import android.view.View
import android.widget.SeekBar
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.activity.result.contract.ActivityResultContracts
import androidx.lifecycle.ViewModelProvider
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import com.example.oboesample.databinding.ActivityMainBinding
import java.io.File
import java.lang.ref.WeakReference
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import kotlin.math.log10
import org.json.JSONObject

class MainActivity : AppCompatActivity() {

    private var activityBinding: ActivityMainBinding? = null
    private val binding: ActivityMainBinding get() = checkNotNull(activityBinding)
    private var ownedAudioEngine: AudioEngine? = null
    private val audioEngine: AudioEngine get() = checkNotNull(ownedAudioEngine)
    @Volatile private var isActivityStarted = false
    @Volatile private var focusGeneration = 0
    private var focusChangeListener: AudioManager.OnAudioFocusChangeListener? = null
    private lateinit var audioManager: AudioManager

    private val TAG = MainActivity::class.java.simpleName

    private var originalMusicVolume = -1
    private var originalCallVolume = -1
    private var originalRingVolume = -1
    private var originalNotificationVolume = -1
    private var originalSystemVolume = -1
    private var originalRingerMode = -1
    private var audioFocusRequest: AudioFocusRequest? = null
    private var originalAudioMode: Int? = null
    private var acousticEchoCanceler: AcousticEchoCanceler? = null
    private var startedAt = 0L
    private class FocusListener(activity: MainActivity, private val generation: Int) :
        AudioManager.OnAudioFocusChangeListener {
        private val activityReference = WeakReference(activity)

        override fun onAudioFocusChange(change: Int) {
            if (change != AudioManager.AUDIOFOCUS_LOSS &&
                change != AudioManager.AUDIOFOCUS_LOSS_TRANSIENT) return
            val reference = activityReference
            val activity = reference.get() ?: return
            if (!activity.isActivityStarted || generation != activity.focusGeneration) return
            activity.handler.post {
                val current = reference.get() ?: return@post
                if (current.isActivityStarted && generation == current.focusGeneration && current.isRecording) {
                    current.stopRecording()
                    current.showToast("Recording stopped due to audio focus loss")
                }
            }
        }
    }

    private enum class RecordingMode {
        IDLE,
        MICROPHONE // Only when in this mode do we want to silence the system volume
    }
    private var currentMode= RecordingMode.IDLE

    private val RECORDING_FILE_NAME = "recording.wav"
    private val RECORD_AUDIO_PERMISSION_REQUEST_CODE = 101

    private val handler = Handler(Looper.getMainLooper())
    private var secondsElapsed = 0
    private var isRecording = false
    private var isPlaying = false
    private var activeToast: Toast? = null

    // Processing parameters
    private var noiseGateThreshold = -40f
    private var noiseReductionAmount = 0.5f
    private var bandpassCenterFreq = 1500f
    private var bandpassQ = 1.2f
    private var noiseFloorDb = -60f
    private var gateRatio = 4f
    private var gateAttack = 5f
    private var gateRelease = 50f
    private var peakingFrequency = 3000f
    private var peakingQ = 1f
    private var peakingGain = 6f
    private var shelfFrequency = 8000f
    private var shelfGain = 3f

    private var advancedExpanded = false

    private val waveformBuffer = FloatArray(AudioEngine.WAVEFORM_FLOATS)
    private var waveformSequence = -1L
    private var hasCapturedInThisEngine = false
    private var waveformFrozen = false
    private var zoomIndex = 1
    private val zoomValues = floatArrayOf(1f, 4f, 16f)
    private var playbackIsInput = false
    private var pendingExportInput = false
    private lateinit var exports: WavExportViewModel
    private val exportBusy: Boolean get() = ::exports.isInitialized && exports.state.value?.busy == true
    private val saveDocument = registerForActivityResult(ActivityResultContracts.CreateDocument("audio/wav")) { uri ->
        val input = pendingExportInput
        pendingExportInput = false
        if (uri != null) exports.save(recordingFile(input), uri, input)
    }
    private val scopeRunnable: Runnable = object : Runnable {
        override fun run() {
            if (!isActivityStarted || !isRecording) return
            refreshWaveform()
            handler.postDelayed(this, 33)
        }
    }

    private val timerRunnable: Runnable = object : Runnable {
        override fun run() {
            if (isActivityStarted && (isRecording || isPlaying)) {
                if (isRecording && !audioEngine.isRecordingActive()) {
                    stopRecording()
                    return
                }
                if (isPlaying && !audioEngine.isPlaybackActive()) {
                    stopPlayback()
                    return
                }
                val elapsed = ((SystemClock.elapsedRealtime() - startedAt) / 1000).toInt()
                if (secondsElapsed != elapsed) {
                    secondsElapsed = elapsed
                    updateTimerDisplay()
                }
                handler.postDelayed(this, 100)
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        activityBinding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)
        if (savedInstanceState != null) {
            noiseGateThreshold = savedInstanceState.getFloat("noiseGateThreshold", noiseGateThreshold)
            noiseReductionAmount = savedInstanceState.getFloat("noiseReductionAmount", noiseReductionAmount)
            bandpassCenterFreq = savedInstanceState.getFloat("bandpassCenterFreq", bandpassCenterFreq)
            bandpassQ = savedInstanceState.getFloat("bandpassQ", bandpassQ)
            noiseFloorDb = savedInstanceState.getFloat("noiseFloorDb", noiseFloorDb)
            gateRatio = savedInstanceState.getFloat("gateRatio", gateRatio)
            gateAttack = savedInstanceState.getFloat("gateAttack", gateAttack)
            gateRelease = savedInstanceState.getFloat("gateRelease", gateRelease)
            peakingFrequency = savedInstanceState.getFloat("peakingFrequency", peakingFrequency)
            peakingQ = savedInstanceState.getFloat("peakingQ", peakingQ)
            peakingGain = savedInstanceState.getFloat("peakingGain", peakingGain)
            shelfFrequency = savedInstanceState.getFloat("shelfFrequency", shelfFrequency)
            shelfGain = savedInstanceState.getFloat("shelfGain", shelfGain)
            binding.echoCancellerSwitch.isChecked = savedInstanceState.getBoolean("echoCancellerEnabled")
            binding.noiseReductionSwitch.isChecked = savedInstanceState.getBoolean("noiseReductionEnabled")
            binding.noiseGateSwitch.isChecked = savedInstanceState.getBoolean("noiseGateEnabled")
            binding.bandpassSwitch.isChecked = savedInstanceState.getBoolean("bandpassEnabled")
            binding.peakingSwitch.isChecked = savedInstanceState.getBoolean("peakingEnabled")
            binding.highShelfSwitch.isChecked = savedInstanceState.getBoolean("highShelfEnabled")
        }
        pendingExportInput = savedInstanceState?.getBoolean("pendingExportInput") ?: false
        waveformFrozen = savedInstanceState?.getBoolean("waveformFrozen") ?: false
        zoomIndex = (savedInstanceState?.getInt("waveformZoom", 1) ?: 1).coerceIn(0, 2)
        savedInstanceState?.getFloatArray("waveformSnapshot")?.takeIf { it.size == waveformBuffer.size }
            ?.copyInto(waveformBuffer)
        exports = ViewModelProvider(this)[WavExportViewModel::class.java]
        exports.state.observe(this) { state ->
            binding.exportStatusTextView.text = if (state.busy) getString(R.string.export_busy) else state.message
            updateFileActions()
        }
        audioManager = applicationContext.getSystemService(Context.AUDIO_SERVICE) as AudioManager
        ownedAudioEngine = AudioEngine()

        val recordingFilePath = filesDir.absolutePath + File.separator + RECORDING_FILE_NAME
        audioEngine.setRecordingPath(recordingFilePath)

        // Set audio source to VOICE_COMMUNICATION to avoid recording system sounds
        audioEngine.setAudioSource(AudioEngine.SOURCE_VOICE_COMMUNICATION)

        // Initialize with default configurations
        initializeAudioProcessing()

        checkAndRequestPermissions()
        setupListeners()
        audioEngine.setEchoCancellerEnabled(binding.echoCancellerSwitch.isChecked)
        audioEngine.setNoiseReductionEnabled(binding.noiseReductionSwitch.isChecked)
        audioEngine.setNoiseGateEnabled(binding.noiseGateSwitch.isChecked)
        audioEngine.setBandpassFilterEnabled(binding.bandpassSwitch.isChecked)
        audioEngine.setPeakingFilterEnabled(binding.peakingSwitch.isChecked)
        audioEngine.setHighShelfFilterEnabled(binding.highShelfSwitch.isChecked)
        setUiState(restingState())
        displayWaveforms()
        updateWaveformControls()
    }

    private fun initializeAudioProcessing() {

        // CRITICAL: Enable Android's hardware Acoustic Echo Canceler
        audioEngine.setAndroidAECEnabled(true)

        // The heuristic can attenuate sustained speech; keep it opt-in through the native API.
        audioEngine.setPlaybackSuppressorEnabled(false)
        audioEngine.configurePlaybackSuppressor(0.8f) // 0.0 to 1.0, higher = more aggressive

        configureCurrentProcessing()
    }

    private fun configureCurrentProcessing() {
        audioEngine.configureBandpassFilter(bandpassCenterFreq, bandpassQ)
        audioEngine.configureHighShelfFilter(shelfFrequency, 0.7f, shelfGain)
        audioEngine.configurePeakingFilter(peakingFrequency, peakingQ, peakingGain)
        audioEngine.configureNoiseGate(noiseGateThreshold, gateRatio, gateAttack, gateRelease)
        audioEngine.configureNoiseReduction(noiseReductionAmount)
        audioEngine.configureNoiseFloor(noiseFloorDb)
        binding.noiseGateSeekBar.progress = (noiseGateThreshold + 60).toInt()
        binding.noiseReductionSeekBar.progress = (noiseReductionAmount * 100).toInt()
        binding.bandpassFreqSeekBar.progress = (bandpassCenterFreq - 300).toInt()
        binding.noiseFloorSeekBar.progress = (noiseFloorDb + 90).toInt()
        binding.peakingGainSeekBar.progress = (peakingGain + 12).toInt()
        binding.highShelfGainSeekBar.progress = (shelfGain + 12).toInt()
        binding.noiseGateLabel.text = "Noise Gate Threshold: ${noiseGateThreshold.toInt()} dB"
        binding.noiseReductionLabel.text = "Noise Reduction: ${(noiseReductionAmount * 100).toInt()}%"
        binding.bandpassFreqLabel.text = "Voice Center: ${bandpassCenterFreq.toInt()} Hz"
        binding.noiseFloorLabel.text = "Noise Floor: ${noiseFloorDb.toInt()} dBFS"
        binding.peakingGainLabel.text = "Presence Gain: ${peakingGain.toInt()} dB"
        binding.highShelfGainLabel.text = "Clarity Gain: ${shelfGain.toInt()} dB"
    }

    // --- NEW: Volume Control Functions ---
    private fun setSystemVolumeSilent() {
        if (currentMode != RecordingMode.MICROPHONE) return

        val notificationManager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager

        // Save current volumes
        originalMusicVolume = audioManager.getStreamVolume(AudioManager.STREAM_MUSIC)
        originalCallVolume = audioManager.getStreamVolume(AudioManager.STREAM_VOICE_CALL)
        originalRingVolume = audioManager.getStreamVolume(AudioManager.STREAM_RING)
        originalNotificationVolume = audioManager.getStreamVolume(AudioManager.STREAM_NOTIFICATION)
        originalSystemVolume = audioManager.getStreamVolume(AudioManager.STREAM_SYSTEM)
        originalRingerMode = audioManager.ringerMode

        // Silence media & call
        audioManager.setStreamVolume(AudioManager.STREAM_MUSIC, 0, AudioManager.FLAG_REMOVE_SOUND_AND_VIBRATE)
        audioManager.setStreamVolume(AudioManager.STREAM_VOICE_CALL, 0, AudioManager.FLAG_REMOVE_SOUND_AND_VIBRATE)

        // Try to silence ring & notifications if allowed
        try {
            if (notificationManager.isNotificationPolicyAccessGranted) {
                audioManager.ringerMode = AudioManager.RINGER_MODE_SILENT
                Log.d(TAG, "Phone set to silent mode.")
            } else {
                // fallback: just mute ring and notifications manually
                audioManager.setStreamVolume(AudioManager.STREAM_RING, 0, AudioManager.FLAG_REMOVE_SOUND_AND_VIBRATE)
                audioManager.setStreamVolume(AudioManager.STREAM_NOTIFICATION, 0, AudioManager.FLAG_REMOVE_SOUND_AND_VIBRATE)
                audioManager.setStreamVolume(AudioManager.STREAM_SYSTEM, 0, AudioManager.FLAG_REMOVE_SOUND_AND_VIBRATE)
                Log.w(TAG, "No DND access — manually silenced ring/notification/system streams.")
            }
        } catch (e: SecurityException) {
            Log.e(TAG, "Not allowed to change DND state: ${e.message}")
        }

        Log.d(TAG, "All key audio streams silenced before recording.")
        Log.d(
            TAG,
            "All audio streams silenced before recording. $originalMusicVolume " +
                    "$originalCallVolume " +
                    "$originalRingVolume " +
                    "$originalNotificationVolume " +
                    "$originalSystemVolume " +
                    "$originalRingerMode"
        )
    }


    private fun restoreSystemVolume() {
        val savedVolumes = listOf(
            AudioManager.STREAM_MUSIC to originalMusicVolume,
            AudioManager.STREAM_VOICE_CALL to originalCallVolume,
            AudioManager.STREAM_RING to originalRingVolume,
            AudioManager.STREAM_NOTIFICATION to originalNotificationVolume,
            AudioManager.STREAM_SYSTEM to originalSystemVolume
        )
        val savedRingerMode = originalRingerMode
        originalMusicVolume = -1
        originalCallVolume = -1
        originalRingVolume = -1
        originalNotificationVolume = -1
        originalSystemVolume = -1
        originalRingerMode = -1

        // A denied stream must not prevent the other streams from being restored.
        if (savedRingerMode != -1) {
            try {
                audioManager.ringerMode = savedRingerMode
            } catch (e: RuntimeException) {
                Log.w(TAG, "Unable to restore ringer mode", e)
            }
        }
        savedVolumes.forEach { (stream, volume) ->
            if (volume != -1) {
                try {
                    audioManager.setStreamVolume(stream, volume, 0)
                } catch (e: RuntimeException) {
                    Log.w(TAG, "Unable to restore audio stream $stream", e)
                }
            }
        }
    }

    private fun requestAudioFocus(): Boolean {
        abandonAudioFocus()
        val listener = FocusListener(this, focusGeneration)
        focusChangeListener = listener
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val audioAttributes = AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_VOICE_COMMUNICATION)
                .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
                .build()

            audioFocusRequest = AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_EXCLUSIVE)
                .setAudioAttributes(audioAttributes)
                .setAcceptsDelayedFocusGain(false)
                .setWillPauseWhenDucked(false)
                .setOnAudioFocusChangeListener(listener, handler)
                .build()

            val result = audioManager.requestAudioFocus(audioFocusRequest!!)
            result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED
        } else {
            @Suppress("DEPRECATION")
            val result = audioManager.requestAudioFocus(
                listener,
                AudioManager.STREAM_VOICE_CALL,
                AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_EXCLUSIVE
            )
            result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED
        }
    }

    private fun abandonAudioFocus() {
        ++focusGeneration // Invalidate queued callbacks before dropping either request reference.
        val request = audioFocusRequest
        val listener = focusChangeListener
        audioFocusRequest = null
        focusChangeListener = null
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            request?.let { audioManager.abandonAudioFocusRequest(it) }
        } else if (listener != null) {
            @Suppress("DEPRECATION")
            audioManager.abandonAudioFocus(listener)
        }
    }

    private fun setupListeners() {
        // Recording controls
        binding.recordButton.setOnClickListener {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO)
                == PackageManager.PERMISSION_GRANTED) {
                startRecording()
            } else {
                showToast("Permission required to record audio.")
                checkAndRequestPermissions()
            }
        }

        binding.stopRecordButton.setOnClickListener { stopRecording() }
        binding.playButton.setOnClickListener { startPlayback() }
        binding.playInputButton.setOnClickListener { startPlayback(input = true) }
        binding.saveInputButton.setOnClickListener { chooseSaveLocation(input = true) }
        binding.saveOutputButton.setOnClickListener { chooseSaveLocation(input = false) }
        binding.freezeWaveformButton.setOnClickListener {
            waveformFrozen = !waveformFrozen
            updateWaveformControls()
            if (!waveformFrozen) refreshWaveform(force = true)
        }
        binding.waveformZoomButton.setOnClickListener {
            zoomIndex = (zoomIndex + 1) % zoomValues.size
            updateWaveformControls()
        }
        binding.stopPlayButton.setOnClickListener { stopPlayback() }

        // Preset buttons
        binding.presetVoiceButton.setOnClickListener { applyVoiceClearPreset() }
        binding.presetPodcastButton.setOnClickListener { applyPodcastProPreset() }
        binding.presetStudioButton.setOnClickListener { applyStudioQualityPreset() }

        // Master switches
        binding.echoCancellerSwitch.setOnCheckedChangeListener { _, isChecked ->
            if (isChecked && !AcousticEchoCanceler.isAvailable()) {
                binding.echoCancellerSwitch.isChecked = false
                showToast("Hardware echo cancellation is unavailable on this device.")
            } else {
                audioEngine.setEchoCancellerEnabled(isChecked)
                showToast("Echo Cancellation: ${if (isChecked) "ON when supported" else "OFF"}")
            }
        }

        binding.noiseReductionSwitch.setOnCheckedChangeListener { _, isChecked ->
            audioEngine.setNoiseReductionEnabled(isChecked)
            binding.noiseReductionSeekBar.isEnabled = isChecked
            binding.noiseFloorSeekBar.isEnabled = isChecked
            showToast("Noise Reduction: ${if (isChecked) "ON" else "OFF"}")
        }

        binding.noiseGateSwitch.setOnCheckedChangeListener { _, isChecked ->
            audioEngine.setNoiseGateEnabled(isChecked)
            binding.noiseGateSeekBar.isEnabled = isChecked
            showToast("Noise Gate: ${if (isChecked) "ON" else "OFF"}")
        }

        binding.bandpassSwitch.setOnCheckedChangeListener { _, isChecked ->
            audioEngine.setBandpassFilterEnabled(isChecked)
            binding.bandpassFreqSeekBar.isEnabled = isChecked
            showToast("Voice Filter: ${if (isChecked) "ON" else "OFF"}")
        }

        binding.peakingSwitch.setOnCheckedChangeListener { _, isChecked ->
            audioEngine.setPeakingFilterEnabled(isChecked)
            binding.peakingGainSeekBar.isEnabled = isChecked
            showToast("Presence Boost: ${if (isChecked) "ON" else "OFF"}")
        }

        binding.highShelfSwitch.setOnCheckedChangeListener { _, isChecked ->
            audioEngine.setHighShelfFilterEnabled(isChecked)
            binding.highShelfGainSeekBar.isEnabled = isChecked
            showToast("Clarity Boost: ${if (isChecked) "ON" else "OFF"}")
        }

        // Advanced settings expand/collapse
        binding.advancedHeader.setOnClickListener {
            advancedExpanded = !advancedExpanded
            binding.advancedContent.visibility = if (advancedExpanded) View.VISIBLE else View.GONE
            binding.advancedExpandIcon.text = if (advancedExpanded) "▲" else "▼"
        }

        // Advanced parameter controls
        setupAdvancedControls()
    }

    private fun setupAdvancedControls() {
        // Noise Gate Threshold (-60 to 0 dB)
        binding.noiseGateSeekBar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                if (!fromUser) return
                noiseGateThreshold = -60f + progress
                audioEngine.configureNoiseGate(noiseGateThreshold, gateRatio, gateAttack, gateRelease)
                binding.noiseGateLabel.text = "Noise Gate Threshold: ${noiseGateThreshold.toInt()} dB"
            }
            override fun onStartTrackingTouch(seekBar: SeekBar?) {}
            override fun onStopTrackingTouch(seekBar: SeekBar?) {}
        })

        // Noise Reduction Amount (0-100%)
        binding.noiseReductionSeekBar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                if (!fromUser) return
                noiseReductionAmount = progress / 100f
                audioEngine.configureNoiseReduction(noiseReductionAmount)
                binding.noiseReductionLabel.text = "Noise Reduction: ${progress}%"
            }
            override fun onStartTrackingTouch(seekBar: SeekBar?) {}
            override fun onStopTrackingTouch(seekBar: SeekBar?) {}
        })

        // Bandpass Center Frequency (300-3400 Hz for voice)
        binding.bandpassFreqSeekBar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                if (!fromUser) return
                bandpassCenterFreq = 300f + progress
                audioEngine.configureBandpassFilter(bandpassCenterFreq, bandpassQ)
                binding.bandpassFreqLabel.text = "Voice Center: ${bandpassCenterFreq.toInt()} Hz"
            }
            override fun onStartTrackingTouch(seekBar: SeekBar?) {}
            override fun onStopTrackingTouch(seekBar: SeekBar?) {}
        })
        binding.noiseFloorSeekBar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                if (!fromUser) return
                noiseFloorDb = -90f + progress
                audioEngine.configureNoiseFloor(noiseFloorDb)
                binding.noiseFloorLabel.text = "Noise Floor: ${noiseFloorDb.toInt()} dBFS"
            }
            override fun onStartTrackingTouch(seekBar: SeekBar?) {}
            override fun onStopTrackingTouch(seekBar: SeekBar?) {}
        })
        binding.peakingGainSeekBar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                if (!fromUser) return
                peakingGain = progress - 12f
                audioEngine.configurePeakingFilter(peakingFrequency, peakingQ, peakingGain)
                binding.peakingGainLabel.text = "Presence Gain: ${peakingGain.toInt()} dB"
            }
            override fun onStartTrackingTouch(seekBar: SeekBar?) {}
            override fun onStopTrackingTouch(seekBar: SeekBar?) {}
        })
        binding.highShelfGainSeekBar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                if (!fromUser) return
                shelfGain = progress - 12f
                audioEngine.configureHighShelfFilter(shelfFrequency, 0.7f, shelfGain)
                binding.highShelfGainLabel.text = "Clarity Gain: ${shelfGain.toInt()} dB"
            }
            override fun onStartTrackingTouch(seekBar: SeekBar?) {}
            override fun onStopTrackingTouch(seekBar: SeekBar?) {}
        })
    }

    // Full-band presets reserve narrow bandpass filtering for the voice-isolation mode.
    private fun applyVoiceClearPreset() {
        noiseGateThreshold = -35f; gateRatio = 4f; gateAttack = 5f; gateRelease = 80f
        noiseReductionAmount = 0.4f; bandpassCenterFreq = 1500f; bandpassQ = 1.2f
        peakingFrequency = 3000f; peakingQ = 1f; peakingGain = 3f
        shelfFrequency = 8000f; shelfGain = 2f
        setAllSwitches(echo = true, noiseReduction = true, noiseGate = true,
            bandpass = true, peaking = false, highShelf = false)
        configureCurrentProcessing()
        showToast("Preset: Voice Clear")
    }

    private fun applyPodcastProPreset() {
        noiseGateThreshold = -45f; gateRatio = 2f; gateAttack = 5f; gateRelease = 120f
        noiseReductionAmount = 0.5f; bandpassCenterFreq = 1800f; bandpassQ = 1f
        peakingFrequency = 3000f; peakingQ = 1f; peakingGain = 3f
        shelfFrequency = 8000f; shelfGain = 2f
        setAllSwitches(echo = true, noiseReduction = true, noiseGate = true,
            bandpass = false, peaking = true, highShelf = true)
        configureCurrentProcessing()
        showToast("Preset: Podcast Pro")
    }

    private fun applyStudioQualityPreset() {
        noiseGateThreshold = -50f; gateRatio = 2f; gateAttack = 5f; gateRelease = 150f
        noiseReductionAmount = 0.3f; bandpassCenterFreq = 2000f; bandpassQ = 0.9f
        peakingFrequency = 3500f; peakingQ = 1f; peakingGain = 2f
        shelfFrequency = 10000f; shelfGain = 2f
        setAllSwitches(echo = true, noiseReduction = true, noiseGate = true,
            bandpass = false, peaking = true, highShelf = true)
        configureCurrentProcessing()
        showToast("Preset: Studio Quality")
    }

    private fun setAllSwitches(echo: Boolean, noiseReduction: Boolean, noiseGate: Boolean,
                               bandpass: Boolean, peaking: Boolean, highShelf: Boolean) {
        binding.echoCancellerSwitch.isChecked = echo
        binding.noiseReductionSwitch.isChecked = noiseReduction
        binding.noiseGateSwitch.isChecked = noiseGate
        binding.bandpassSwitch.isChecked = bandpass
        binding.peakingSwitch.isChecked = peaking
        binding.highShelfSwitch.isChecked = highShelf
    }

    private fun checkAndRequestPermissions() {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO)
            != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(
                this,
                arrayOf(Manifest.permission.RECORD_AUDIO),
                RECORD_AUDIO_PERMISSION_REQUEST_CODE
            )
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == RECORD_AUDIO_PERMISSION_REQUEST_CODE) {
            if (grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                showToast("Record permission granted!")
            } else {
                showToast("Record permission denied. Cannot record audio.")
            }
        }
    }

    private fun restingState(): State =
        if (File(filesDir, RECORDING_FILE_NAME).length() > 44) State.RECORDED else State.READY

    private fun startRecording() {
        if (!isActivityStarted || isRecording || isPlaying || exportBusy) return
        val hasFocus = try { requestAudioFocus() } catch (e: RuntimeException) {
            Log.w(TAG, "Unable to request audio focus", e)
            false
        }
        if (!hasFocus) {
            releaseRecordingAudio()
            showToast("Unable to gain audio focus.")
            return
        }
        val started = try {
            originalAudioMode = audioManager.mode
            audioManager.mode = AudioManager.MODE_IN_COMMUNICATION
            currentMode = RecordingMode.MICROPHONE
            setSystemVolumeSilent()
            waveformBuffer.fill(0f)
            waveformSequence = -1L
            waveformFrozen = false
            displayWaveforms()
            updateWaveformControls()
            audioEngine.startRecording()
        } catch (e: RuntimeException) {
            Log.e(TAG, "Unable to start recording", e)
            cleanupResource("failed recording start") { audioEngine.stopRecording() }
            false
        }
        if (!started) {
            releaseRecordingAudio()
            setUiState(restingState())
            showToast("Unable to start recording.")
            return
        }
        hasCapturedInThisEngine = true
        val sessionId = audioEngine.getRecordingSessionId()
        if (sessionId > 0 && AcousticEchoCanceler.isAvailable()) {
            try {
                acousticEchoCanceler = AcousticEchoCanceler.create(sessionId)
                val enabled = acousticEchoCanceler?.setEnabled(binding.echoCancellerSwitch.isChecked)
                if (binding.echoCancellerSwitch.isChecked && enabled != android.media.audiofx.AudioEffect.SUCCESS) {
                    showToast("Hardware echo cancellation could not be enabled.")
                }
            } catch (e: RuntimeException) {
                val failedEffect = acousticEchoCanceler
                acousticEchoCanceler = null
                cleanupResource("failed echo effect") { failedEffect?.release() }
                Log.w(TAG, "Hardware echo cancellation unavailable", e)
                if (binding.echoCancellerSwitch.isChecked) {
                    showToast("Recording started without hardware echo cancellation.")
                }
            }
        } else if (binding.echoCancellerSwitch.isChecked) {
            showToast("This recording stream does not support hardware echo cancellation.")
        }
        setUiState(State.RECORDING)
    }

    private inline fun cleanupResource(name: String, action: () -> Unit) {
        try { action() } catch (e: RuntimeException) {
            Log.w(TAG, "Unable to clean up $name", e)
        }
    }

    private fun releaseRecordingAudio() {
        val effect = acousticEchoCanceler
        acousticEchoCanceler = null
        cleanupResource("echo effect") { effect?.release() }
        val previousMode = originalAudioMode
        originalAudioMode = null
        if (::audioManager.isInitialized) {
            cleanupResource("audio mode") { previousMode?.let { audioManager.mode = it } }
            cleanupResource("audio focus") { abandonAudioFocus() }
            cleanupResource("system volumes") { restoreSystemVolume() }
        }
        currentMode = RecordingMode.IDLE
    }

    private fun stopRecording() {
        if (!isRecording) return
        stopTimer() // Prevent reentrant focus/timer callbacks from stopping a new session.
        try {
            audioEngine.stopRecording()
            refreshWaveform(force = true)
            val diagnostics = audioEngine.getLatencyDiagnostics()
            Log.d(TAG, "Audio diagnostics: $diagnostics")
            if (JSONObject(diagnostics).getJSONObject("recording").getBoolean("workerFailed")) {
                showToast("Recording stopped after a storage or audio queue error. The clip may be incomplete.")
            }
        } finally {
            releaseRecordingAudio()
            setUiState(restingState())
        }
    }

    private fun startPlayback(input: Boolean = false) {
        if (!isActivityStarted || isRecording || isPlaying || exportBusy) return
        if (!(if (input) audioEngine.playInputRecording() else audioEngine.playRecording())) {
            showToast("Unable to play recording. Record a new clip if the file is invalid.")
            setUiState(restingState())
            return
        }
        playbackIsInput = input
        setUiState(State.PLAYING)
    }

    private fun stopPlayback() {
        if (!isPlaying) return
        stopTimer()
        try {
            audioEngine.stopPlayback()
            val diagnostics = audioEngine.getLatencyDiagnostics()
            Log.d(TAG, "Audio diagnostics: $diagnostics")
            if (JSONObject(diagnostics).getJSONObject("playback").getBoolean("workerFailed")) {
                showToast("Playback stopped because the recording could not be read.")
            }
        } finally {
            setUiState(restingState())
        }
    }

    private fun recordingFile(input: Boolean = false): File =
        File(filesDir, RECORDING_FILE_NAME + if (input) ".input.wav" else "")

    private fun chooseSaveLocation(input: Boolean) {
        if (isRecording || isPlaying || exportBusy || recordingFile(input).length() <= 44) return
        pendingExportInput = input
        val timestamp = SimpleDateFormat("yyyyMMdd-HHmmss", Locale.US).format(Date())
        saveDocument.launch("voice-${if (input) "input" else "output"}-$timestamp.wav")
    }

    private fun updateFileActions() {
        val idle = !isRecording && !isPlaying && !exportBusy
        binding.recordButton.isEnabled = idle
        binding.playButton.isEnabled = idle && recordingFile().length() > 44
        binding.playInputButton.isEnabled = idle && recordingFile(input = true).length() > 44
        binding.saveOutputButton.isEnabled = idle && recordingFile().length() > 44
        binding.saveInputButton.isEnabled = idle && recordingFile(input = true).length() > 44
    }

    private fun updateWaveformControls() {
        binding.freezeWaveformButton.setText(if (waveformFrozen) R.string.scope_resume else R.string.scope_freeze)
        binding.waveformZoomButton.text = getString(R.string.scope_zoom, zoomValues[zoomIndex].toInt())
        binding.inputOscilloscope.setDisplayZoom(zoomValues[zoomIndex])
        binding.outputOscilloscope.setDisplayZoom(zoomValues[zoomIndex])
        updateSignalLevels()
    }

    private fun refreshWaveform(force: Boolean = false) {
        if (waveformFrozen || !hasCapturedInThisEngine) return
        val sequence = audioEngine.readWaveform(waveformBuffer)
        if (sequence != waveformSequence || force) {
            waveformSequence = sequence
            displayWaveforms()
        }
    }

    private fun displayWaveforms() {
        binding.inputOscilloscope.showWaveform(waveformBuffer, output = false)
        binding.outputOscilloscope.showWaveform(waveformBuffer, output = true)
        updateSignalLevels()
    }

    private fun dbfs(level: Float): Float = 20f * log10(level.coerceAtLeast(0.000001f))
    private fun dbLabel(level: Float): String =
        if (level <= 0.000001f) "−∞" else String.format(Locale.getDefault(), "%.1f", dbfs(level))

    private fun updateSignalLevels() {
        binding.inputLevelsTextView.text = getString(R.string.scope_levels, dbLabel(waveformBuffer[0]), dbLabel(waveformBuffer[1]))
        binding.outputLevelsTextView.text = getString(R.string.scope_levels, dbLabel(waveformBuffer[2]), dbLabel(waveformBuffer[3]))
        val message = when {
            waveformFrozen -> if (isRecording) R.string.scope_frozen else R.string.scope_frozen_idle
            waveformBuffer[6] == 0f -> R.string.scope_waiting
            waveformBuffer[4] > 0 || waveformBuffer[5] > 0 -> R.string.scope_clipping
            dbfs(waveformBuffer[1]) < -55f -> R.string.scope_quiet
            dbfs(waveformBuffer[3]) < dbfs(waveformBuffer[1]) - 25f -> R.string.scope_suppressed
            else -> R.string.scope_usable
        }
        if (binding.signalQualityTextView.text.toString() != getString(message)) {
            binding.signalQualityTextView.setText(message)
        }
    }

    private enum class State { READY, RECORDING, RECORDED, PLAYING }

    private fun setUiState(state: State) {
        stopTimer()
        isRecording = state == State.RECORDING
        isPlaying = state == State.PLAYING
        // DSP state belongs to the recording worker until it has been joined.
        val canConfigure = !isRecording
        listOf(binding.presetVoiceButton, binding.presetPodcastButton,
            binding.presetStudioButton, binding.echoCancellerSwitch,
            binding.noiseReductionSwitch, binding.noiseGateSwitch, binding.bandpassSwitch,
            binding.peakingSwitch, binding.highShelfSwitch).forEach { it.isEnabled = canConfigure }
        binding.noiseGateSeekBar.isEnabled = canConfigure && binding.noiseGateSwitch.isChecked
        binding.noiseReductionSeekBar.isEnabled = canConfigure && binding.noiseReductionSwitch.isChecked
        binding.bandpassFreqSeekBar.isEnabled = canConfigure && binding.bandpassSwitch.isChecked
        binding.noiseFloorSeekBar.isEnabled = canConfigure && binding.noiseReductionSwitch.isChecked
        binding.peakingGainSeekBar.isEnabled = canConfigure && binding.peakingSwitch.isChecked
        binding.highShelfGainSeekBar.isEnabled = canConfigure && binding.highShelfSwitch.isChecked
        when (state) {
            State.READY -> {
                resetTimer()
                binding.statusTextView.setText(R.string.status_ready)
                binding.recordButton.isEnabled = true
                binding.stopRecordButton.isEnabled = false
                binding.playButton.isEnabled = false
                binding.stopPlayButton.isEnabled = false
            }
            State.RECORDING -> {
                startTimer()
                binding.recordButton.isEnabled = false
                binding.stopRecordButton.isEnabled = true
                binding.playButton.isEnabled = false
                binding.stopPlayButton.isEnabled = false
            }
            State.RECORDED -> {
                stopTimer()
                binding.statusTextView.text = getString(R.string.status_ready)
                binding.recordButton.isEnabled = true
                binding.stopRecordButton.isEnabled = false
                binding.playButton.isEnabled = true
                binding.stopPlayButton.isEnabled = false
            }
            State.PLAYING -> {
                startTimer()
                binding.recordButton.isEnabled = false
                binding.stopRecordButton.isEnabled = false
                binding.playButton.isEnabled = false
                binding.stopPlayButton.isEnabled = true
            }
        }
        updateFileActions()
        updateWaveformControls()
    }

    private fun startTimer() {
        secondsElapsed = 0
        startedAt = SystemClock.elapsedRealtime()
        updateTimerDisplay()
        handler.postDelayed(timerRunnable, 100)
        if (isRecording) handler.post(scopeRunnable)
    }

    private fun stopTimer() {
        isRecording = false
        isPlaying = false
        handler.removeCallbacks(timerRunnable)
        handler.removeCallbacks(scopeRunnable)
    }

    private fun resetTimer() {
        secondsElapsed = 0
        updateTimerDisplay()
    }

    private fun updateTimerDisplay() {
        val minutes = secondsElapsed / 60
        val seconds = secondsElapsed % 60
        val time = String.format(Locale.getDefault(), "%02d:%02d", minutes, seconds)

        val statusText = when {
            isRecording -> getString(R.string.status_recording, time)
            isPlaying -> getString(if (playbackIsInput) R.string.status_playing_input else R.string.status_playing_output, time)
            else -> getString(R.string.status_ready)
        }
        if (binding.statusTextView.text.toString() != statusText) {
            binding.statusTextView.text = statusText
        }
    }

    private fun showToast(message: String) {
        if (!isActivityStarted) return
        activeToast?.cancel()
        activeToast = Toast.makeText(applicationContext, message, Toast.LENGTH_SHORT).also { it.show() }
    }

    override fun onSaveInstanceState(outState: Bundle) {
        outState.putBoolean("pendingExportInput", pendingExportInput)
        outState.putBoolean("waveformFrozen", waveformFrozen)
        outState.putInt("waveformZoom", zoomIndex)
        outState.putFloatArray("waveformSnapshot", waveformBuffer)
        outState.putFloat("noiseGateThreshold", noiseGateThreshold)
        outState.putFloat("noiseReductionAmount", noiseReductionAmount)
        outState.putFloat("bandpassCenterFreq", bandpassCenterFreq)
        outState.putFloat("bandpassQ", bandpassQ)
        outState.putFloat("noiseFloorDb", noiseFloorDb)
        outState.putFloat("gateRatio", gateRatio)
        outState.putFloat("gateAttack", gateAttack)
        outState.putFloat("gateRelease", gateRelease)
        outState.putFloat("peakingFrequency", peakingFrequency)
        outState.putFloat("peakingQ", peakingQ)
        outState.putFloat("peakingGain", peakingGain)
        outState.putFloat("shelfFrequency", shelfFrequency)
        outState.putFloat("shelfGain", shelfGain)
        outState.putBoolean("echoCancellerEnabled", binding.echoCancellerSwitch.isChecked)
        outState.putBoolean("noiseReductionEnabled", binding.noiseReductionSwitch.isChecked)
        outState.putBoolean("noiseGateEnabled", binding.noiseGateSwitch.isChecked)
        outState.putBoolean("bandpassEnabled", binding.bandpassSwitch.isChecked)
        outState.putBoolean("peakingEnabled", binding.peakingSwitch.isChecked)
        outState.putBoolean("highShelfEnabled", binding.highShelfSwitch.isChecked)
        super.onSaveInstanceState(outState)
    }

    override fun onRestoreInstanceState(savedInstanceState: Bundle) {
        super.onRestoreInstanceState(savedInstanceState)
        // Reapply exact parameter values after Android restores integer slider positions.
        configureCurrentProcessing()
    }

    override fun onStart() {
        super.onStart()
        isActivityStarted = true
    }

    private fun stopOwnedAudio() {
        stopTimer()
        handler.removeCallbacksAndMessages(null)
        val toast = activeToast
        activeToast = null
        cleanupResource("toast") { toast?.cancel() }
        val engine = ownedAudioEngine
        if (engine != null) {
            cleanupResource("recording stream") { engine.stopRecording() }
            cleanupResource("playback stream") { engine.stopPlayback() }
            if (activityBinding != null && !waveformFrozen) {
                cleanupResource("waveform snapshot") { refreshWaveform(force = true) }
            }
        }
        releaseRecordingAudio()
    }

    override fun onStop() {
        isActivityStarted = false
        try {
            stopOwnedAudio()
            if (activityBinding != null) setUiState(restingState())
        } finally {
            super.onStop()
        }
    }

    override fun onDestroy() {
        isActivityStarted = false
        try {
            stopOwnedAudio()
        } finally {
            val engine = ownedAudioEngine
            ownedAudioEngine = null
            try {
                cleanupResource("native engine") { engine?.close() }
            } finally {
                handler.removeCallbacksAndMessages(null)
                activityBinding = null
                super.onDestroy()
            }
        }
    }
}
