package com.example.oboesample

import android.Manifest
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
import android.util.Log
import android.view.View
import android.widget.SeekBar
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import com.example.oboesample.databinding.ActivityMainBinding
import java.io.File

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private lateinit var audioManager: AudioManager
    private var audioFocusRequest: AudioFocusRequest? = null

    private val RECORDING_FILE_NAME = "recording.pcm"
    private val RECORD_AUDIO_PERMISSION_REQUEST_CODE = 101

    private val handler = Handler(Looper.getMainLooper())
    private var secondsElapsed = 0
    private var isRecording = false
    private var isPlaying = false

    // Processing parameters
    private var noiseGateThreshold = -40f
    private var noiseReductionAmount = 0.5f
    private var bandpassCenterFreq = 1500f

    private var advancedExpanded = false

    private val timerRunnable: Runnable = object : Runnable {
        override fun run() {
            if (isRecording || isPlaying) {
                secondsElapsed++
                updateTimerDisplay()
                handler.postDelayed(this, 1000)
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        // Initialize AudioManager
        audioManager = getSystemService(Context.AUDIO_SERVICE) as AudioManager
        val sessionId = audioManager.generateAudioSessionId()
        audioManager.mode = AudioManager.MODE_IN_COMMUNICATION

        val isAecAvailable = AcousticEchoCanceler.isAvailable()
        if(isAecAvailable) {
            val acousticEchoCanceler = AcousticEchoCanceler.create(sessionId)
            acousticEchoCanceler.enabled = true //THIS ENABLED THE AEC

            if (acousticEchoCanceler != null)
                Log.e("MainActivity", "AEC STATUS:  ${acousticEchoCanceler.enabled}")
            else
                Log.e("MainActivity", "AEC  IS NULL")
        }

        val recordingFilePath = filesDir.absolutePath + File.separator + RECORDING_FILE_NAME
        AudioEngine.setRecordingPath(recordingFilePath)

        // Set audio source to VOICE_COMMUNICATION to avoid recording system sounds
        AudioEngine.setAudioSource(AudioEngine.SOURCE_VOICE_COMMUNICATION)

        // Initialize with default configurations
        initializeAudioProcessing()

        checkAndRequestPermissions()
        setupListeners()
        setUiState(State.READY)
    }

    private fun initializeAudioProcessing() {

        //This is the line that enables AEC, when MODE is set
        audioManager.mode = AudioManager.MODE_IN_COMMUNICATION

        // CRITICAL: Enable Android's hardware Acoustic Echo Canceler
        AudioEngine.setAndroidAECEnabled(true)

        // Enable Playback Suppressor as fallback (in case Android AEC doesn't work)
        AudioEngine.setPlaybackSuppressorEnabled(true)
        AudioEngine.configurePlaybackSuppressor(0.8f) // 0.0 to 1.0, higher = more aggressive

        // Configure all processing modules with default values
        AudioEngine.configureBandpassFilter(1500f, 1.2f)
        AudioEngine.configureHighShelfFilter(8000f, 0.7f, 3.0f)
        AudioEngine.configurePeakingFilter(3000f, 1.0f, 6.0f)
        AudioEngine.configureNoiseGate(-40f, 4.0f, 5.0f, 50.0f)
        AudioEngine.configureNoiseReduction(0.5f)
        AudioEngine.configureEchoCanceller(50.0f, 0.7f)
    }

    private fun requestAudioFocus(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val audioAttributes = AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_VOICE_COMMUNICATION)
                .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
                .build()

            audioFocusRequest = AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_EXCLUSIVE)
                .setAudioAttributes(audioAttributes)
                .setAcceptsDelayedFocusGain(false)
                .setWillPauseWhenDucked(false)
                .setOnAudioFocusChangeListener { focusChange ->
                    when (focusChange) {
                        AudioManager.AUDIOFOCUS_LOSS,
                        AudioManager.AUDIOFOCUS_LOSS_TRANSIENT -> {
                            // Lost focus, stop recording
                            if (isRecording) {
                                stopRecording()
                                showToast("Recording stopped due to audio focus loss")
                            }
                        }
                    }
                }
                .build()

            val result = audioManager.requestAudioFocus(audioFocusRequest!!)
            result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED
        } else {
            @Suppress("DEPRECATION")
            val result = audioManager.requestAudioFocus(
                { focusChange ->
                    when (focusChange) {
                        AudioManager.AUDIOFOCUS_LOSS,
                        AudioManager.AUDIOFOCUS_LOSS_TRANSIENT -> {
                            if (isRecording) {
                                stopRecording()
                                showToast("Recording stopped due to audio focus loss")
                            }
                        }
                    }
                },
                AudioManager.STREAM_VOICE_CALL,
                AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_EXCLUSIVE
            )
            result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED
        }
    }

    private fun abandonAudioFocus() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            audioFocusRequest?.let {
                audioManager.abandonAudioFocusRequest(it)
            }
        } else {
            @Suppress("DEPRECATION")
            audioManager.abandonAudioFocus(null)
        }
    }

    private fun setupListeners() {
        // Recording controls
        binding.recordButton.setOnClickListener {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO)
                == PackageManager.PERMISSION_GRANTED) {
                startRecording()
            } else {
                Toast.makeText(this, "Permission required to record audio.", Toast.LENGTH_SHORT).show()
                checkAndRequestPermissions()
            }
        }

        binding.stopRecordButton.setOnClickListener { stopRecording() }
        binding.playButton.setOnClickListener { startPlayback() }
        binding.stopPlayButton.setOnClickListener { stopPlayback() }

        // Preset buttons
        binding.presetVoiceButton.setOnClickListener { applyVoiceClearPreset() }
        binding.presetPodcastButton.setOnClickListener { applyPodcastProPreset() }
        binding.presetStudioButton.setOnClickListener { applyStudioQualityPreset() }

        // Master switches
        binding.echoCancellerSwitch.setOnCheckedChangeListener { _, isChecked ->
            AudioEngine.setEchoCancellerEnabled(isChecked)
            showToast("Echo Cancellation: ${if (isChecked) "ON" else "OFF"}")
        }

        binding.noiseReductionSwitch.setOnCheckedChangeListener { _, isChecked ->
            AudioEngine.setNoiseReductionEnabled(isChecked)
            binding.noiseReductionSeekBar.isEnabled = isChecked
            showToast("Noise Reduction: ${if (isChecked) "ON" else "OFF"}")
        }

        binding.noiseGateSwitch.setOnCheckedChangeListener { _, isChecked ->
            AudioEngine.setNoiseGateEnabled(isChecked)
            binding.noiseGateSeekBar.isEnabled = isChecked
            showToast("Noise Gate: ${if (isChecked) "ON" else "OFF"}")
        }

        binding.bandpassSwitch.setOnCheckedChangeListener { _, isChecked ->
            AudioEngine.setBandpassFilterEnabled(isChecked)
            binding.bandpassFreqSeekBar.isEnabled = isChecked
            showToast("Voice Filter: ${if (isChecked) "ON" else "OFF"}")
        }

        binding.peakingSwitch.setOnCheckedChangeListener { _, isChecked ->
            AudioEngine.setPeakingFilterEnabled(isChecked)
            showToast("Presence Boost: ${if (isChecked) "ON" else "OFF"}")
        }

        binding.highShelfSwitch.setOnCheckedChangeListener { _, isChecked ->
            AudioEngine.setHighShelfFilterEnabled(isChecked)
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
                noiseGateThreshold = -60f + progress
                AudioEngine.configureNoiseGate(noiseGateThreshold, 4.0f, 5.0f, 50.0f)
                binding.noiseGateLabel.text = "Noise Gate Threshold: ${noiseGateThreshold.toInt()} dB"
            }
            override fun onStartTrackingTouch(seekBar: SeekBar?) {}
            override fun onStopTrackingTouch(seekBar: SeekBar?) {}
        })

        // Noise Reduction Amount (0-100%)
        binding.noiseReductionSeekBar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                noiseReductionAmount = progress / 100f
                AudioEngine.configureNoiseReduction(noiseReductionAmount)
                binding.noiseReductionLabel.text = "Noise Reduction: ${progress}%"
            }
            override fun onStartTrackingTouch(seekBar: SeekBar?) {}
            override fun onStopTrackingTouch(seekBar: SeekBar?) {}
        })

        // Bandpass Center Frequency (300-3400 Hz for voice)
        binding.bandpassFreqSeekBar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                bandpassCenterFreq = 300f + (progress * 31f) // Maps 0-100 to 300-3400 Hz
                AudioEngine.configureBandpassFilter(bandpassCenterFreq, 1.2f)
                binding.bandpassFreqLabel.text = "Voice Center: ${bandpassCenterFreq.toInt()} Hz"
            }
            override fun onStartTrackingTouch(seekBar: SeekBar?) {}
            override fun onStopTrackingTouch(seekBar: SeekBar?) {}
        })
    }

    // Preset configurations
    private fun applyVoiceClearPreset() {
        // Basic voice enhancement for calls
        setAllSwitches(echo = true, noiseReduction = true, noiseGate = true,
            bandpass = true, peaking = false, highShelf = false)

        AudioEngine.configureBandpassFilter(1500f, 1.2f)
        AudioEngine.configureNoiseGate(-35f, 4.0f, 5.0f, 50.0f)
        AudioEngine.configureNoiseReduction(0.4f)

        showToast("Preset: Voice Clear")
    }

    private fun applyPodcastProPreset() {
        // Professional podcast/broadcast quality
        setAllSwitches(echo = true, noiseReduction = true, noiseGate = true,
            bandpass = true, peaking = true, highShelf = true)

        AudioEngine.configureBandpassFilter(1800f, 1.0f)
        AudioEngine.configurePeakingFilter(3000f, 1.2f, 5.0f)
        AudioEngine.configureHighShelfFilter(8000f, 0.7f, 3.0f)
        AudioEngine.configureNoiseGate(-40f, 6.0f, 3.0f, 40.0f)
        AudioEngine.configureNoiseReduction(0.6f)

        showToast("Preset: Podcast Pro")
    }

    private fun applyStudioQualityPreset() {
        // Maximum quality with all processing
        setAllSwitches(echo = true, noiseReduction = true, noiseGate = true,
            bandpass = true, peaking = true, highShelf = true)

        AudioEngine.configureBandpassFilter(2000f, 0.9f)
        AudioEngine.configurePeakingFilter(3500f, 1.0f, 6.0f)
        AudioEngine.configureHighShelfFilter(10000f, 0.7f, 4.0f)
        AudioEngine.configureNoiseGate(-45f, 8.0f, 2.0f, 30.0f)
        AudioEngine.configureNoiseReduction(0.7f)
        AudioEngine.configureEchoCanceller(40.0f, 0.8f)

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

    private fun startRecording() {
        // Request audio focus before starting
        if (!requestAudioFocus()) {
            showToast("Failed to gain audio focus. Recording may capture system sounds.")
        }

        // Set audio mode to communication
        audioManager.mode = AudioManager.MODE_IN_COMMUNICATION

        AudioEngine.startRecording()
        setUiState(State.RECORDING)
    }

    private fun stopRecording() {
        AudioEngine.stopRecording()

        // Restore normal audio mode
        audioManager.mode = AudioManager.MODE_NORMAL

        // Abandon audio focus
        abandonAudioFocus()

        setUiState(State.RECORDED)
    }

    private fun startPlayback() {
        AudioEngine.playRecording()
        setUiState(State.PLAYING)
    }

    private fun stopPlayback() {
        AudioEngine.stopPlayback()
        setUiState(State.RECORDED)
    }

    private enum class State { READY, RECORDING, RECORDED, PLAYING }

    private fun setUiState(state: State) {
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
    }

    private fun startTimer() {
        secondsElapsed = 0
        isRecording = binding.stopRecordButton.isEnabled
        isPlaying = binding.stopPlayButton.isEnabled
        handler.post(timerRunnable)
    }

    private fun stopTimer() {
        isRecording = false
        isPlaying = false
        handler.removeCallbacks(timerRunnable)
    }

    private fun resetTimer() {
        secondsElapsed = 0
        updateTimerDisplay()
    }

    private fun updateTimerDisplay() {
        val minutes = secondsElapsed / 60
        val seconds = secondsElapsed % 60
        val time = String.format("%02d:%02d", minutes, seconds)

        val statusText = when {
            isRecording -> getString(R.string.status_recording, time)
            isPlaying -> getString(R.string.status_playing, time)
            else -> getString(R.string.status_ready)
        }
        binding.statusTextView.text = statusText
    }

    private fun showToast(message: String) {
        Toast.makeText(this, message, Toast.LENGTH_SHORT).show()
    }

    override fun onStop() {
        super.onStop()
        if (isRecording) stopRecording()
        if (isPlaying) stopPlayback()
    }

    override fun onDestroy() {
        super.onDestroy()
        abandonAudioFocus()
    }
}