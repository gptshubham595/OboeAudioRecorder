#pragma once

#include <cstdint>
#include <cstring>
#include <istream>
#include <ostream>

// The app writes canonical little-endian, mono 48 kHz PCM16 WAV files.
namespace wav {
constexpr uint32_t sampleRate = 48000;
constexpr uint16_t channels = 1;
constexpr uint32_t headerSize = 44;

inline void writeHeader(std::ostream& stream, uint32_t dataBytes) {
    auto write = [&](uint32_t value, int bytes) {
        for (int i = 0; i < bytes; ++i) stream.put(static_cast<char>(value >> (8 * i)));
    };
    stream.write("RIFF", 4); write(36 + dataBytes, 4);
    stream.write("WAVEfmt ", 8); write(16, 4); write(1, 2); write(channels, 2);
    write(sampleRate, 4); write(sampleRate * channels * 2, 4);
    write(channels * 2, 2); write(16, 2);
    stream.write("data", 4); write(dataBytes, 4);
}

// Accept only the format emitted by this app; reject legacy raw PCM and truncated files.
inline bool readHeader(std::istream& stream, uint32_t& dataBytes) {
    unsigned char header[headerSize]{};
    if (!stream.read(reinterpret_cast<char*>(header), headerSize)) return false;
    auto read = [&](int offset, int bytes) {
        uint32_t value = 0;
        for (int i = 0; i < bytes; ++i) value |= uint32_t(header[offset + i]) << (8 * i);
        return value;
    };
    dataBytes = read(40, 4);
    if (std::memcmp(header, "RIFF", 4) || std::memcmp(header + 8, "WAVEfmt ", 8) ||
        std::memcmp(header + 36, "data", 4) || read(16, 4) != 16 ||
        read(20, 2) != 1 || read(22, 2) != channels || read(24, 4) != sampleRate ||
        read(28, 4) != sampleRate * channels * 2 || read(32, 2) != channels * 2 ||
        read(34, 2) != 16 || dataBytes == 0 || dataBytes % 2 != 0 ||
        uint64_t(read(4, 4)) != uint64_t(dataBytes) + 36) return false;
    stream.seekg(0, std::ios::end);
    if (stream.tellg() < std::streamoff(headerSize) + dataBytes) return false;
    stream.seekg(headerSize);
    return bool(stream);
}
}
