module;

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include <zlib.h>

export module volcano.zlib;

export namespace volcano::zlib {

enum class FlushMode : int {
    none = Z_NO_FLUSH,
    sync = Z_SYNC_FLUSH,
    full = Z_FULL_FLUSH,
    finish = Z_FINISH
};

class DeflateStream {
public:
    explicit DeflateStream(int level = Z_DEFAULT_COMPRESSION);
    ~DeflateStream();

    DeflateStream(const DeflateStream&) = delete;
    DeflateStream& operator=(const DeflateStream&) = delete;
    DeflateStream(DeflateStream&& other) noexcept;
    DeflateStream& operator=(DeflateStream&& other) noexcept;

    void reset(int level = Z_DEFAULT_COMPRESSION);

    template <typename Sink>
        requires requires(Sink&& sink, std::span<const std::byte> chunk) { sink(chunk); }
    std::size_t write(std::span<const std::byte> input, Sink&& sink, FlushMode flush = FlushMode::none) {
        return process(input, static_cast<int>(flush), std::forward<Sink>(sink));
    }

    template <typename Sink>
        requires requires(Sink&& sink, std::span<const std::byte> chunk) { sink(chunk); }
    std::size_t finish(Sink&& sink) {
        return process({}, Z_FINISH, std::forward<Sink>(sink));
    }

    std::size_t write(std::span<const std::byte> input, std::vector<std::byte>& out, FlushMode flush = FlushMode::none);
    std::size_t finish(std::vector<std::byte>& out);

private:
    template <typename Sink>
    std::size_t process(std::span<const std::byte> input, int flush, Sink&& sink) {
        if (ended_) {
            throw std::runtime_error("DeflateStream used after finish().");
        }

        zstream_.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(input.data()));
        zstream_.avail_in = static_cast<uInt>(input.size());

        std::size_t total_out = 0;
        while (zstream_.avail_in > 0 || flush != Z_NO_FLUSH) {
            zstream_.next_out = reinterpret_cast<Bytef*>(buffer_.data());
            zstream_.avail_out = static_cast<uInt>(buffer_.size());

            const int ret = deflate(&zstream_, flush);
            if (ret != Z_OK && ret != Z_STREAM_END) {
                throw std::runtime_error("zlib deflate failed.");
            }

            const std::size_t produced = buffer_.size() - zstream_.avail_out;
            if (produced > 0) {
                sink(std::span<const std::byte>(buffer_.data(), produced));
                total_out += produced;
            }

            if (ret == Z_STREAM_END) {
                ended_ = true;
                break;
            }

            if (zstream_.avail_in == 0 && zstream_.avail_out != 0) {
                break;
            }
        }

        return total_out;
    }

    z_stream zstream_{};
    std::vector<std::byte> buffer_;
    bool ended_{false};
};

class InflateStream {
public:
    InflateStream();
    ~InflateStream();

    InflateStream(const InflateStream&) = delete;
    InflateStream& operator=(const InflateStream&) = delete;
    InflateStream(InflateStream&& other) noexcept;
    InflateStream& operator=(InflateStream&& other) noexcept;

    void reset();

    template <typename Sink>
        requires requires(Sink&& sink, std::span<const std::byte> chunk) { sink(chunk); }
    std::size_t write(std::span<const std::byte> input, Sink&& sink) {
        return process(input, std::forward<Sink>(sink));
    }

    template <typename Sink>
        requires requires(Sink&& sink, std::span<const std::byte> chunk) { sink(chunk); }
    std::size_t finish(Sink&& sink) {
        return process({}, std::forward<Sink>(sink));
    }

    std::size_t write(std::span<const std::byte> input, std::vector<std::byte>& out);

private:
    template <typename Sink>
    std::size_t process(std::span<const std::byte> input, Sink&& sink) {
        if (ended_) {
            throw std::runtime_error("InflateStream used after finish().");
        }

        zstream_.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(input.data()));
        zstream_.avail_in = static_cast<uInt>(input.size());

        std::size_t total_out = 0;
        while (zstream_.avail_in > 0) {
            zstream_.next_out = reinterpret_cast<Bytef*>(buffer_.data());
            zstream_.avail_out = static_cast<uInt>(buffer_.size());

            const int ret = inflate(&zstream_, Z_NO_FLUSH);
            if (ret == Z_STREAM_END) {
                ended_ = true;
            } else if (ret != Z_OK) {
                throw std::runtime_error("zlib inflate failed.");
            }

            const std::size_t produced = buffer_.size() - zstream_.avail_out;
            if (produced > 0) {
                sink(std::span<const std::byte>(buffer_.data(), produced));
                total_out += produced;
            }

            if (ret == Z_STREAM_END) {
                break;
            }

            if (zstream_.avail_in == 0 && zstream_.avail_out != 0) {
                break;
            }
        }

        return total_out;
    }

    z_stream zstream_{};
    std::vector<std::byte> buffer_;
    bool ended_{false};
};

} // namespace volcano::zlib

namespace volcano::zlib {

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

} // namespace volcano::zlib
