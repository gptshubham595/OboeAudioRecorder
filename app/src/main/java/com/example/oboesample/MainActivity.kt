package com.example.oboesample

import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.widget.SeekBar
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import com.example.oboesample.databinding.ActivityMainBinding
import java.io.File

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    private val RECORDING_FILE_NAME = "recording.pcm"
    private val RECORD_AUDIO_PERMISSION_REQUEST_CODE = 101

    // Filter parameters
    private var currentFrequency = 1000f
    private var currentQ = 1.0f

    // Timer logic
    private val handler = Handler(Looper.getMainLooper())
    private var secondsElapsed = 0
    private var isRecording = false
    private var isPlaying = false

    private val timerRunnable: Runnable = object : Runnable {
        override fun run() {
            if (isRecording || isPlaying) {
                secondsElapsed++
                updateTimerDisplay()
                handler.postDelayed(this, 1000) // Run again after 1 second
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        val recordingFilePath = filesDir.absolutePath + File.separator + RECORDING_FILE_NAME
        AudioEngine.setRecordingPath(recordingFilePath)
        // Configure filter once at startup
        AudioEngine.configureBandpassFilter(1000f, 1.0f)
        AudioEngine.setFilterEnabled(true) // Enable it permanently


        // Request permissions if not granted
        checkAndRequestPermissions()

        // Set up button listeners
        setupListeners()

        // Initial state
        setUiState(State.READY)
    }

    private fun setupListeners() {
        binding.recordButton.setOnClickListener {
            if (ContextCompat.checkSelfPermission(
                    this,
                    Manifest.permission.RECORD_AUDIO
                ) == PackageManager.PERMISSION_GRANTED
            ) {
                startRecording()
            } else {
                Toast.makeText(this, "Permission required to record audio.", Toast.LENGTH_SHORT)
                    .show()
                checkAndRequestPermissions()
            }
        }

        binding.stopRecordButton.setOnClickListener {
            stopRecording()
        }

        binding.playButton.setOnClickListener {
            startPlayback()
        }

        binding.stopPlayButton.setOnClickListener {
            stopPlayback()
        }

        binding.filterSwitch.setOnCheckedChangeListener { _, isChecked ->
            AudioEngine.setFilterEnabled(isChecked)
            binding.frequencySeekBar.isEnabled = isChecked
            binding.qSeekBar.isEnabled = isChecked

            val message = if (isChecked) {
                "Filter enabled: ${currentFrequency.toInt()} Hz, Q=${String.format("%.1f", currentQ)}"
            } else {
                "Filter disabled"
            }
            Toast.makeText(this, message, Toast.LENGTH_SHORT).show()
        }

        // Frequency SeekBar (300 Hz to 3000 Hz)
        binding.frequencySeekBar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                // Map 0-100 to 300-3000 Hz
                currentFrequency = 300f + (progress * 27f)
                updateFilterConfiguration()
                binding.frequencyLabel.text = "Center Frequency: ${currentFrequency.toInt()} Hz"
            }

            override fun onStartTrackingTouch(seekBar: SeekBar?) {}
            override fun onStopTrackingTouch(seekBar: SeekBar?) {}
        })

        // Q Factor SeekBar (0.5 to 5.0)
        binding.qSeekBar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                // Map 0-45 to 0.5-5.0
                currentQ = (progress / 10f) + 0.5f
                updateFilterConfiguration()
                binding.qLabel.text = "Q Factor: ${String.format("%.1f", currentQ)}"
            }

            override fun onStartTrackingTouch(seekBar: SeekBar?) {}
            override fun onStopTrackingTouch(seekBar: SeekBar?) {}
        })
    }

    private fun updateFilterConfiguration() {
        if (binding.filterSwitch.isChecked) {
            AudioEngine.configureBandpassFilter(currentFrequency, currentQ)
        }
    }

    private fun checkAndRequestPermissions() {
        if (ContextCompat.checkSelfPermission(
                this,
                Manifest.permission.RECORD_AUDIO
            ) != PackageManager.PERMISSION_GRANTED
        ) {
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
                Toast.makeText(this, "Record permission granted!", Toast.LENGTH_SHORT).show()
            } else {
                Toast.makeText(
                    this,
                    "Record permission denied. Cannot record audio.",
                    Toast.LENGTH_LONG
                ).show()
            }
        }
    }

    private fun startRecording() {
        AudioEngine.setFilterEnabled(true)
        AudioEngine.configureBandpassFilter(1000f, 1.0f) // 1000 Hz center, Q=1.0

        AudioEngine.startRecording()
        setUiState(State.RECORDING)
    }

    private fun stopRecording() {
        AudioEngine.stopRecording()
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

    // --- UI and Timer Management ---

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
                // Update status to show the length of the recording
                binding.statusTextView.text = getString(R.string.status_ready)
                binding.recordButton.isEnabled = true
                binding.stopRecordButton.isEnabled = false
                binding.playButton.isEnabled = true // Playback is possible now
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
        isRecording = if (binding.stopRecordButton.isEnabled) true else false
        isPlaying = if (binding.stopPlayButton.isEnabled) true else false
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

        val statusText = if (isRecording) {
            getString(R.string.status_recording, time)
        } else if (isPlaying) {
            getString(R.string.status_playing, time)
        } else {
            getString(R.string.status_ready)
        }
        binding.statusTextView.text = statusText
    }

    override fun onStop() {
        super.onStop()
        // Stop all native audio operations when the app goes into the background
        if (isRecording) stopRecording()
        if (isPlaying) stopPlayback()
    }
}