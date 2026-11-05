package com.example.oboesample

import android.app.Application
import android.net.Uri
import android.provider.DocumentsContract
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import java.io.File
import java.io.IOException
import java.io.InterruptedIOException
import java.util.concurrent.Executors
import java.util.concurrent.Future

/** Holds application context only; a save can survive activity recreation. */
class WavExportViewModel(application: Application) : AndroidViewModel(application) {
    data class State(val busy: Boolean = false, val message: String = "")
    private val mutableState = MutableLiveData(State())
    val state: LiveData<State> = mutableState
    private val executor = Executors.newSingleThreadExecutor()
    private var task: Future<*>? = null

    fun save(source: File, destination: Uri, input: Boolean) {
        if (mutableState.value?.busy == true) return
        val application = getApplication<Application>()
        val resolver = application.contentResolver
        mutableState.value = State(busy = true)
        task = executor.submit {
            var completed = false
            try {
                if (source.length() <= 44) throw IOException("No completed recording")
                source.inputStream().use { sourceStream ->
                    val target = resolver.openOutputStream(destination, "w") ?: throw IOException("No output stream")
                    target.use { output ->
                        val buffer = ByteArray(16 * 1024)
                        while (true) {
                            if (Thread.currentThread().isInterrupted) throw InterruptedIOException()
                            val count = sourceStream.read(buffer)
                            if (count < 0) break
                            output.write(buffer, 0, count)
                        }
                        output.flush()
                    }
                }
                completed = true
                mutableState.postValue(State(message = application.getString(
                    if (input) R.string.export_input_success else R.string.export_output_success
                )))
            } catch (_: Exception) {
                mutableState.postValue(State(message = application.getString(R.string.export_failed)))
            } finally {
                // CreateDocument returns a newly created document; remove a partial failed copy when supported.
                if (!completed) runCatching { DocumentsContract.deleteDocument(resolver, destination) }
            }
        }
    }

    override fun onCleared() {
        task?.cancel(true)
        executor.shutdownNow()
        super.onCleared()
    }
}
