#include "AsyncAudioFile.h"
#include "WavFile.h"
#include <chrono>
#include <limits>
#include <utility>

namespace {
constexpr size_t blockSize = 512;
void waitForWorker() { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
}

bool AudioFileWriter::start(const char* path, Processor processor, size_t tailSamples,
                            const char* inputPath, size_t trimOutputSamples) {
    stop();
    mQueue.reset();
    mFailed.store(false);
    mOverruns.store(0);
    mDataBytes = mInputBytes = 0;
    mTrimRemaining = trimOutputSamples;
    mProcessor = std::move(processor);
    mTailSamples = tailSamples;
    mFile.open(path, std::ios::binary | std::ios::trunc);
    if (!mFile) { mFailed.store(true); stop(); return false; }
    wav::writeHeader(mFile, 0);
    if (!mFile) { mFailed.store(true); stop(); return false; }
    if (inputPath) {
        mInputFile.open(inputPath, std::ios::binary | std::ios::trunc);
        wav::writeHeader(mInputFile, 0);
        if (!mInputFile) { mFailed.store(true); stop(); return false; }
    }
    mStop.store(false);
    try { mWorker = std::thread(&AudioFileWriter::run, this); }
    catch (...) { mFailed.store(true); stop(); return false; }
    return true;
}

bool AudioFileWriter::enqueue(const int16_t* samples, size_t count) {
    if (mStop.load(std::memory_order_relaxed) || mFailed.load(std::memory_order_relaxed)) return false;
    if (!mQueue.push(samples, count)) {
        mOverruns.fetch_add(1, std::memory_order_relaxed);
        mFailed.store(true);
        return false; // Stop capture instead of silently dropping a block.
    }
    return true;
}

bool AudioFileWriter::writeBlock(int16_t* samples, size_t count, bool capturedInput) {
    const size_t bytes = count * sizeof(int16_t);
    if (capturedInput) {
        if (bytes > std::numeric_limits<uint32_t>::max() - 36ULL - mInputBytes) return false;
        if (mInputFile.is_open()) {
            mInputFile.write(reinterpret_cast<const char*>(samples), bytes);
            if (!mInputFile) return false;
        }
        mInputBytes += static_cast<uint32_t>(bytes);
    }
    try { if (mProcessor) mProcessor(samples, count); }
    catch (...) { return false; }
    const size_t skip = std::min(count, mTrimRemaining);
    mTrimRemaining -= skip;
    const size_t outputBytes = (count - skip) * sizeof(int16_t);
    if (outputBytes > std::numeric_limits<uint32_t>::max() - 36ULL - mDataBytes) return false;
    mFile.write(reinterpret_cast<const char*>(samples + skip), outputBytes);
    if (!mFile) return false;
    mDataBytes += static_cast<uint32_t>(outputBytes);
    return true;
}

void AudioFileWriter::run() {
    std::array<int16_t, blockSize> block{};
    for (;;) {
        // Snapshot the producer's completion before popping. Seeing stop=true means
        // every enqueue has completed; an empty pop after that snapshot is final.
        const bool stopping = mStop.load(std::memory_order_acquire) || mFailed.load();
        const size_t count = mQueue.pop(block.data(), block.size());
        if (count) {
            if (!writeBlock(block.data(), count, true)) { mFailed.store(true); break; }
        } else if (stopping) {
            break;
        } else {
            waitForWorker();
        }
    }
    if (mInputBytes && mFile) {
        for (size_t left = mTailSamples; left > 0;) {
            const size_t count = std::min(left, block.size());
            block.fill(0);
            if (!writeBlock(block.data(), count, false)) { mFailed.store(true); break; }
            left -= count;
        }
    }
    auto finalize = [this](std::ofstream& file, uint32_t bytes) {
        if (!file.is_open()) return;
        file.clear();
        file.seekp(0);
        wav::writeHeader(file, bytes);
        file.flush();
        if (!file) mFailed.store(true);
        file.close();
        if (!file) mFailed.store(true);
    };
    finalize(mFile, mDataBytes);
    finalize(mInputFile, mInputBytes);
}

void AudioFileWriter::stop() {
    // The owner must stop/close its producer stream before calling stop().
    mStop.store(true, std::memory_order_release);
    if (mWorker.joinable()) mWorker.join();
    if (mFile.is_open()) mFile.close();
    mFile.clear();
    if (mInputFile.is_open()) mInputFile.close();
    mInputFile.clear();
    mProcessor = {}; // Release captured objects immediately after the worker is joined.
    mTailSamples = 0;
    mDataBytes = mInputBytes = 0;
    mTrimRemaining = 0;
    mQueue.reset();
}

bool AudioFileReader::fill(size_t count) {
    std::array<int16_t, blockSize> block{};
    count = std::min({count, block.size(), static_cast<size_t>(mFileSamplesRemaining)});
    mFile.read(reinterpret_cast<char*>(block.data()), count * sizeof(int16_t));
    if (static_cast<size_t>(mFile.gcount()) != count * sizeof(int16_t)) {
        mFailed.store(true);
        return false;
    }
    if (!mQueue.push(block.data(), count)) { mFailed.store(true); return false; }
    mFileSamplesRemaining -= static_cast<uint32_t>(count);
    return true;
}

bool AudioFileReader::start(const char* path) {
    stop();
    mQueue.reset();
    mFailed.store(false);
    mUnderruns.store(0);
    mFile.open(path, std::ios::binary);
    uint32_t bytes = 0;
    if (!mFile || !wav::readHeader(mFile, bytes)) {
        mFailed.store(true);
        stop();
        return false;
    }
    mPlaybackSamplesRemaining = mFileSamplesRemaining = bytes / sizeof(int16_t);
    // Prime a small prefix before starting the stream, without a worker-read race.
    size_t prefill = std::min<uint32_t>(1024, mFileSamplesRemaining);
    while (prefill) {
        const size_t count = std::min(prefill, blockSize);
        if (!fill(count)) { stop(); return false; }
        prefill -= count;
    }
    mStop.store(false);
    try { mWorker = std::thread(&AudioFileReader::run, this); }
    catch (...) { mFailed.store(true); stop(); return false; }
    return true;
}

void AudioFileReader::run() {
    while (!mStop.load(std::memory_order_relaxed) && mFileSamplesRemaining) {
        const size_t available = mQueue.writeAvailable();
        if (available) {
            if (!fill(available)) break;
        } else {
            waitForWorker();
        }
    }
}

AudioFileReader::ReadResult AudioFileReader::read(int16_t* output, size_t count) {
    if (mStop.load(std::memory_order_relaxed)) {
        if (count) std::fill_n(output, count, 0);
        return {0, true, mFailed.load(std::memory_order_relaxed)};
    }
    if (!count) return {0, mPlaybackSamplesRemaining == 0, mFailed.load(std::memory_order_relaxed)};
    const size_t read = mQueue.pop(output, count);
    std::fill(output + read, output + count, 0);
    mPlaybackSamplesRemaining -= static_cast<uint32_t>(read);
    if (read < count && mPlaybackSamplesRemaining) mUnderruns.fetch_add(1, std::memory_order_relaxed);
    return {read, mPlaybackSamplesRemaining == 0, mFailed.load(std::memory_order_relaxed)};
}

void AudioFileReader::stop() {
    // The owner must stop/close its consumer stream before calling stop().
    mStop.store(true);
    if (mWorker.joinable()) mWorker.join();
    if (mFile.is_open()) mFile.close();
    mFile.clear();
    mFileSamplesRemaining = mPlaybackSamplesRemaining = 0;
    mQueue.reset();
}
