#pragma once
// Host lifecycle test double only. This is never on the Android target's include path.
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace oboe {
enum class Result { OK, ErrorInternal, ErrorInvalidState, ErrorInvalidFormat };
enum class Direction { Input, Output };
enum class PerformanceMode { LowLatency };
enum class SharingMode { Exclusive };
enum class AudioFormat { I16 };
enum class SampleRateConversionQuality { Medium };
enum class InputPreset { Generic, Camcorder, VoiceRecognition, VoiceCommunication, Unprocessed, VoicePerformance };
enum class SessionId { None = -1, Allocate = 0 };
enum class Usage { VoiceCommunication };
enum class ContentType { Speech };
enum class StreamState { Stopped, Starting, Started };
enum class DataCallbackResult { Continue, Stop };
inline const char* convertToText(Result) { return "test stream result"; }

template<typename T> class ResultWithValue {
public:
    explicit ResultWithValue(T value) : mValue(value) {}
    explicit operator bool() const { return true; }
    T value() const { return mValue; }
private:
    T mValue;
};

class AudioStream;
class AudioStreamDataCallback {
public:
    virtual ~AudioStreamDataCallback() = default;
    virtual DataCallbackResult onAudioReady(AudioStream*, void*, int32_t) = 0;
};

inline std::atomic<int> liveStreams{0};
inline std::atomic<bool> failOpen{false};
inline std::atomic<bool> failStart{false};
inline std::weak_ptr<AudioStream> lastInput;

class AudioStream {
public:
    explicit AudioStream(AudioStreamDataCallback* callback) : mCallback(callback) { ++liveStreams; }
    ~AudioStream() { --liveStreams; }
    Result requestStart() {
        if (failStart.exchange(false)) return Result::ErrorInternal;
        mState = StreamState::Started;
        return Result::OK;
    }
    Result requestStop() { mState = StreamState::Stopped; return Result::OK; }
    Result close() { mState = StreamState::Stopped; return Result::OK; }
    StreamState getState() const { return mState; }
    int32_t getSessionId() const { return 1; }
    int32_t getSampleRate() const { return 48000; }
    int32_t getChannelCount() const { return 1; }
    AudioFormat getFormat() const { return AudioFormat::I16; }
    int32_t getFramesPerBurst() const { return 192; }
    int32_t getBufferSizeInFrames() const { return 384; }
    PerformanceMode getPerformanceMode() const { return PerformanceMode::LowLatency; }
    SharingMode getSharingMode() const { return SharingMode::Exclusive; }
    ResultWithValue<int32_t> getXRunCount() const { return ResultWithValue<int32_t>(0); }
    ResultWithValue<int32_t> setBufferSizeInFrames(int32_t frames) { return ResultWithValue<int32_t>(frames); }
    void feed(size_t count) {
        std::vector<int16_t> samples(count, 123);
        mCallback->onAudioReady(this, samples.data(), static_cast<int32_t>(count));
    }
private:
    AudioStreamDataCallback* mCallback;
    StreamState mState = StreamState::Stopped;
};

class AudioStreamBuilder {
public:
    AudioStreamBuilder* setDirection(Direction value) { mDirection = value; return this; }
    AudioStreamBuilder* setPerformanceMode(PerformanceMode) { return this; }
    AudioStreamBuilder* setSharingMode(SharingMode) { return this; }
    AudioStreamBuilder* setFormat(AudioFormat) { return this; }
    AudioStreamBuilder* setChannelCount(int32_t) { return this; }
    AudioStreamBuilder* setSampleRate(int32_t) { return this; }
    AudioStreamBuilder* setSampleRateConversionQuality(SampleRateConversionQuality) { return this; }
    AudioStreamBuilder* setChannelConversionAllowed(bool) { return this; }
    AudioStreamBuilder* setInputPreset(InputPreset) { return this; }
    AudioStreamBuilder* setSessionId(SessionId) { return this; }
    AudioStreamBuilder* setUsage(Usage) { return this; }
    AudioStreamBuilder* setContentType(ContentType) { return this; }
    AudioStreamBuilder* setDataCallback(AudioStreamDataCallback* callback) { mCallback = callback; return this; }
    Result openStream(std::shared_ptr<AudioStream>& stream) {
        if (failOpen.exchange(false)) return Result::ErrorInternal;
        stream = std::make_shared<AudioStream>(mCallback);
        if (mDirection == Direction::Input) lastInput = stream;
        return Result::OK;
    }
private:
    Direction mDirection = Direction::Input;
    AudioStreamDataCallback* mCallback = nullptr;
};
}
