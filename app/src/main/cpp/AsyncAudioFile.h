#pragma once
#include "SpscAudioQueue.h"
#include <fstream>
#include <functional>
#include <thread>
static_assert(std::atomic<bool>::is_always_lock_free);

// File and DSP work run on workers. Callback methods only copy bounded PCM blocks
// and access lock-free atomics; they never wait, allocate, log, or touch a file.
class AudioFileWriter {
public:
    using Processor = std::function<void(int16_t*, size_t)>;
    AudioFileWriter() = default;
    ~AudioFileWriter() { stop(); }
    AudioFileWriter(const AudioFileWriter&) = delete;
    AudioFileWriter& operator=(const AudioFileWriter&) = delete;
    AudioFileWriter(AudioFileWriter&&) = delete;
    AudioFileWriter& operator=(AudioFileWriter&&) = delete;
    bool start(const char* path, Processor processor, size_t tailSamples,
               const char* inputPath = nullptr, size_t trimOutputSamples = 0);
    bool enqueue(const int16_t* samples, size_t count);
    void stop();
    bool failed() const { return mFailed.load(); }
    uint32_t overruns() const { return mOverruns.load(); }
    static constexpr size_t capacity = 8192;
private:
    void run();
    bool writeBlock(int16_t* samples, size_t count, bool capturedInput);
    SpscAudioQueue<capacity> mQueue;
    std::ofstream mFile;
    std::ofstream mInputFile;
    std::thread mWorker;
    Processor mProcessor;
    size_t mTailSamples = 0;
    uint32_t mDataBytes = 0; // Worker-owned until join.
    uint32_t mInputBytes = 0;
    size_t mTrimRemaining = 0;
    std::atomic<bool> mStop{true};
    std::atomic<bool> mFailed{false};
    std::atomic<uint32_t> mOverruns{0};
};

class AudioFileReader {
public:
    struct ReadResult { size_t samples; bool finished; bool failed; };
    AudioFileReader() = default;
    ~AudioFileReader() { stop(); }
    AudioFileReader(const AudioFileReader&) = delete;
    AudioFileReader& operator=(const AudioFileReader&) = delete;
    AudioFileReader(AudioFileReader&&) = delete;
    AudioFileReader& operator=(AudioFileReader&&) = delete;
    bool start(const char* path);
    ReadResult read(int16_t* output, size_t count);
    void stop();
    bool failed() const { return mFailed.load(); }
    uint32_t underruns() const { return mUnderruns.load(); }
    static constexpr size_t capacity = 8192;
private:
    void run();
    bool fill(size_t count);
    SpscAudioQueue<capacity> mQueue;
    std::ifstream mFile;
    std::thread mWorker;
    uint32_t mFileSamplesRemaining = 0; // File worker (prefill before launch).
    uint32_t mPlaybackSamplesRemaining = 0; // Callback-owned until stream close.
    std::atomic<bool> mStop{true};
    std::atomic<bool> mFailed{false};
    std::atomic<uint32_t> mUnderruns{0};
};
