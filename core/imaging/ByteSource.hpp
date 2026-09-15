#ifndef GLCM_BYTE_SOURCE_HPP_
#define GLCM_BYTE_SOURCE_HPP_

// Internal to imaging/: random access to the bytes of an encoded image, from a file or from memory

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <opencv2/core.hpp>
#include <stdexcept>
#include <string>
#include <vector>

namespace glcm::imaging_detail {

// Read returns the number of bytes available at the offset (fewer at the end)
class ByteSource {
public:
    virtual ~ByteSource() = default;
    virtual size_t Read(uint64_t offset, uint8_t* out, size_t count) = 0;
};

class FileSource : public ByteSource {
public:
    explicit FileSource(const std::string& path) : _stream(path, std::ios::binary) {
        if (!_stream) {
            throw std::runtime_error("Cannot read the image: " + path);
        }
    }

    size_t Read(uint64_t offset, uint8_t* out, size_t count) override {
        _stream.clear();
        _stream.seekg(static_cast<std::streamoff>(offset));
        if (!_stream) {
            return 0;
        }
        _stream.read(reinterpret_cast<char*>(out), static_cast<std::streamsize>(count));
        return static_cast<size_t>(_stream.gcount());
    }

private:
    std::ifstream _stream;
};

class MemorySource : public ByteSource {
public:
    explicit MemorySource(const std::vector<uchar>& bytes) : _bytes(bytes) {}

    size_t Read(uint64_t offset, uint8_t* out, size_t count) override {
        if (offset >= _bytes.size()) {
            return 0;
        }
        const size_t available = std::min(count, static_cast<size_t>(_bytes.size() - offset));
        std::memcpy(out, _bytes.data() + offset, available);
        return available;
    }

private:
    const std::vector<uchar>& _bytes;
};

} // namespace glcm::imaging_detail

#endif // GLCM_BYTE_SOURCE_HPP_
