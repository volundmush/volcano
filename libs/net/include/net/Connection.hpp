#pragma once

#include <cstdint>
#include <utility>
#include <variant>
#include <fmt/format.h>

#include "net/Base.hpp"
#include <boost/asio/ssl.hpp>

namespace boost::asio::ip {
    auto inline to_string(const address& address) {
        return address.to_string();
    }
}

namespace boost::asio::ip {

    auto inline to_string(const tcp::endpoint& endpoint) {
        return fmt::format("{}:{}", endpoint.address().to_string(), endpoint.port());
    }

    auto inline to_string(const tcp::socket& socket) {
        boost::system::error_code ec;
        auto endpoint = socket.remote_endpoint(ec);
        std::string address = ec ? "<unknown>" : to_string(endpoint.address());
        return fmt::format("TcpStream({})", address);
    }
}

namespace boost::asio::ssl {
    auto inline to_string(const stream<boost::asio::ip::tcp::socket>& ssl_stream) {
        boost::system::error_code ec;
        auto endpoint = ssl_stream.next_layer().remote_endpoint(ec);
        std::string address = ec ? "<unknown>" : boost::asio::ip::to_string(endpoint.address());
        return fmt::format("TlsStream({})", address);
    }
}


namespace vol::net {
    using TcpStream = boost::asio::ip::tcp::socket;
    using TlsStream = boost::asio::ssl::stream<TcpStream>;

    class AnyStream {
    public:
        using executor_type = TcpStream::executor_type;
        using lowest_layer_type = TcpStream::lowest_layer_type;

        AnyStream() = delete;
        AnyStream(int64_t id, TcpStream stream);
        AnyStream(int64_t id, TlsStream stream);

        AnyStream(const AnyStream&) = delete;
        AnyStream& operator=(const AnyStream&) = delete;
        AnyStream(AnyStream&&) noexcept = default;
        AnyStream& operator=(AnyStream&&) noexcept = default;

        [[nodiscard]] bool is_tls() const;
        [[nodiscard]] int64_t id() const;

        executor_type get_executor();
        executor_type get_executor() const;

        lowest_layer_type& lowest_layer();
        lowest_layer_type& lowest_layer() const;

        template <typename MutableBufferSequence>
        std::size_t read_some(const MutableBufferSequence& buffers, boost::system::error_code& ec) {
            if (auto* tcp = std::get_if<TcpStream>(&stream_)) {
                return tcp->read_some(buffers, ec);
            }
            return std::get<TlsStream>(stream_).read_some(buffers, ec);
        }

        template <typename ConstBufferSequence>
        std::size_t write_some(const ConstBufferSequence& buffers, boost::system::error_code& ec) {
            if (auto* tcp = std::get_if<TcpStream>(&stream_)) {
                return tcp->write_some(buffers, ec);
            }
            return std::get<TlsStream>(stream_).write_some(buffers, ec);
        }

        template <typename MutableBufferSequence, typename CompletionToken>
        auto async_read_some(const MutableBufferSequence& buffers, CompletionToken&& token) {
            if (auto* tcp = std::get_if<TcpStream>(&stream_)) {
                return tcp->async_read_some(buffers, std::forward<CompletionToken>(token));
            }
            return std::get<TlsStream>(stream_).async_read_some(buffers, std::forward<CompletionToken>(token));
        }

        template <typename ConstBufferSequence, typename CompletionToken>
        auto async_write_some(const ConstBufferSequence& buffers, CompletionToken&& token) {
            if (auto* tcp = std::get_if<TcpStream>(&stream_)) {
                return tcp->async_write_some(buffers, std::forward<CompletionToken>(token));
            }
            return std::get<TlsStream>(stream_).async_write_some(buffers, std::forward<CompletionToken>(token));
        }

    private:
        std::variant<TcpStream, TlsStream> stream_;
        int64_t id_{0};
    };

    auto inline to_string(const AnyStream& any_stream) {
        auto &lowest = any_stream.lowest_layer();
        boost::system::error_code ec;
        auto endpoint = lowest.remote_endpoint(ec);
        std::string address = ec ? "<unknown>" : boost::asio::ip::to_string(endpoint.address());
        if (any_stream.is_tls()) {
            return fmt::format("TlsStream#{}({})", any_stream.id(), address);
        }
        return fmt::format("TcpStream#{}({})", any_stream.id(), address);
    }
}

namespace boost::beast {
inline void beast_close_socket(vol::net::AnyStream& stream) {
    boost::system::error_code ec;
    stream.lowest_layer().close(ec);
}
}