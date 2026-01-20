#include <zlib/zlib.hpp>

namespace vol::zlib {

DeflateStream::DeflateStream(int level) : buffer_(16 * 1024) {
    zstream_.zalloc = Z_NULL;
    zstream_.zfree = Z_NULL;
    zstream_.opaque = Z_NULL;

    const int ret = deflateInit(&zstream_, level);
    if (ret != Z_OK) {
        throw std::runtime_error("zlib deflateInit failed.");
    }
}

DeflateStream::~DeflateStream() {
    deflateEnd(&zstream_);
}

DeflateStream::DeflateStream(DeflateStream&& other) noexcept
    : zstream_(other.zstream_), buffer_(std::move(other.buffer_)), ended_(other.ended_) {
    other.zstream_.zalloc = Z_NULL;
    other.zstream_.zfree = Z_NULL;
    other.zstream_.opaque = Z_NULL;
    other.zstream_.next_in = Z_NULL;
    other.zstream_.avail_in = 0;
    other.zstream_.next_out = Z_NULL;
    other.zstream_.avail_out = 0;
    other.ended_ = true;
}

DeflateStream& DeflateStream::operator=(DeflateStream&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    deflateEnd(&zstream_);
    zstream_ = other.zstream_;
    buffer_ = std::move(other.buffer_);
    ended_ = other.ended_;

    other.zstream_.zalloc = Z_NULL;
    other.zstream_.zfree = Z_NULL;
    other.zstream_.opaque = Z_NULL;
    other.zstream_.next_in = Z_NULL;
    other.zstream_.avail_in = 0;
    other.zstream_.next_out = Z_NULL;
    other.zstream_.avail_out = 0;
    other.ended_ = true;
    return *this;
}

void DeflateStream::reset(int level) {
    deflateEnd(&zstream_);
    zstream_ = {};
    ended_ = false;

    const int ret = deflateInit(&zstream_, level);
    if (ret != Z_OK) {
        throw std::runtime_error("zlib deflateInit failed.");
    }
}

std::size_t DeflateStream::write(std::span<const std::byte> input, std::vector<std::byte>& out, FlushMode flush) {
    auto sink = [&out](std::span<const std::byte> chunk) {
        out.insert(out.end(), chunk.begin(), chunk.end());
    };
    return write(input, sink, flush);
}

std::size_t DeflateStream::finish(std::vector<std::byte>& out) {
    auto sink = [&out](std::span<const std::byte> chunk) {
        out.insert(out.end(), chunk.begin(), chunk.end());
    };
    return finish(sink);
}

InflateStream::InflateStream() : buffer_(16 * 1024) {
    zstream_.zalloc = Z_NULL;
    zstream_.zfree = Z_NULL;
    zstream_.opaque = Z_NULL;

    const int ret = inflateInit(&zstream_);
    if (ret != Z_OK) {
        throw std::runtime_error("zlib inflateInit failed.");
    }
}

InflateStream::~InflateStream() {
    inflateEnd(&zstream_);
}

InflateStream::InflateStream(InflateStream&& other) noexcept
    : zstream_(other.zstream_), buffer_(std::move(other.buffer_)), ended_(other.ended_) {
    other.zstream_.zalloc = Z_NULL;
    other.zstream_.zfree = Z_NULL;
    other.zstream_.opaque = Z_NULL;
    other.zstream_.next_in = Z_NULL;
    other.zstream_.avail_in = 0;
    other.zstream_.next_out = Z_NULL;
    other.zstream_.avail_out = 0;
    other.ended_ = true;
}

InflateStream& InflateStream::operator=(InflateStream&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    inflateEnd(&zstream_);
    zstream_ = other.zstream_;
    buffer_ = std::move(other.buffer_);
    ended_ = other.ended_;

    other.zstream_.zalloc = Z_NULL;
    other.zstream_.zfree = Z_NULL;
    other.zstream_.opaque = Z_NULL;
    other.zstream_.next_in = Z_NULL;
    other.zstream_.avail_in = 0;
    other.zstream_.next_out = Z_NULL;
    other.zstream_.avail_out = 0;
    other.ended_ = true;
    return *this;
}

void InflateStream::reset() {
    inflateEnd(&zstream_);
    zstream_ = {};
    ended_ = false;

    const int ret = inflateInit(&zstream_);
    if (ret != Z_OK) {
        throw std::runtime_error("zlib inflateInit failed.");
    }
}

std::size_t InflateStream::write(std::span<const std::byte> input, std::vector<std::byte>& out) {
    auto sink = [&out](std::span<const std::byte> chunk) {
        out.insert(out.end(), chunk.begin(), chunk.end());
    };
    return write(input, sink);
}

} // namespace vol::zlib
