package com.example.oboesample

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.util.AttributeSet
import android.util.TypedValue
import android.view.View

/** Shared fixed amplitude scale; renders min/max so short peaks survive decimation. */
class OscilloscopeView @JvmOverloads constructor(
    context: Context, attrs: AttributeSet? = null
) : View(context, attrs) {
    private val density = resources.displayMetrics.density
    private val backgroundPaint = Paint().apply { color = Color.rgb(15, 23, 42) }
    private val gridPaint = Paint().apply { color = Color.rgb(51, 65, 85); strokeWidth = density }
    private val tracePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.rgb(103, 232, 249)
        strokeWidth = density
        strokeCap = Paint.Cap.ROUND
    }
    private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.rgb(148, 163, 184)
        textSize = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, 10f, resources.displayMetrics)
    }
    private val segments = FloatArray(AudioEngine.WAVEFORM_BUCKETS * 4)
    private val nowLabel = context.getString(R.string.scope_now)
    private var samples: FloatArray? = null
    private var channelOffset = 0
    private var zoom = 4f
    private var topLabel = "+0.25"
    private var bottomLabel = "−0.25"

    init { importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_NO }

    fun showWaveform(snapshot: FloatArray, output: Boolean) {
        samples = snapshot
        channelOffset = if (output) 2 else 0
        tracePaint.color = if (output) Color.rgb(196, 181, 253) else Color.rgb(103, 232, 249)
        invalidate()
    }

    fun setDisplayZoom(value: Float) {
        zoom = value
        topLabel = "+${1f / value}"
        bottomLabel = "−${1f / value}"
        invalidate()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val left = maxOf(40f * density, textPaint.measureText(topLabel) + 8f * density,
            textPaint.measureText(bottomLabel) + 8f * density)
        val right = width - 8f * density
        val top = 12f * density
        val bottom = height - 24f * density
        if (right <= left || bottom <= top) return
        val mid = (top + bottom) / 2f
        val half = (bottom - top) / 2f
        canvas.drawRoundRect(0f, 0f, width.toFloat(), height.toFloat(), 8f * density, 8f * density, backgroundPaint)
        for (line in 0..4) {
            val y = top + (bottom - top) * line / 4f
            canvas.drawLine(left, y, right, y, gridPaint)
        }
        for (line in 0..10) {
            val x = left + (right - left) * line / 10f
            canvas.drawLine(x, top, x, bottom, gridPaint)
        }
        canvas.drawText(topLabel, 4f * density, top + 8f * density, textPaint)
        canvas.drawText("0", 4f * density, mid + 3f * density, textPaint)
        canvas.drawText(bottomLabel, 4f * density, bottom, textPaint)
        canvas.drawText("−1.0 s", left, height - 6f * density, textPaint)
        canvas.drawText(nowLabel, right - textPaint.measureText(nowLabel), height - 6f * density, textPaint)
        val data = samples ?: return
        for (i in 0 until AudioEngine.WAVEFORM_BUCKETS) {
            val index = 8 + i * 4 + channelOffset
            val x = left + (right - left) * i / (AudioEngine.WAVEFORM_BUCKETS - 1f)
            val offset = i * 4
            segments[offset] = x
            segments[offset + 1] = mid - (data[index + 1] * zoom).coerceIn(-1f, 1f) * half
            segments[offset + 2] = x
            segments[offset + 3] = mid - (data[index] * zoom).coerceIn(-1f, 1f) * half
        }
        canvas.drawLines(segments, tracePaint)
    }
}
