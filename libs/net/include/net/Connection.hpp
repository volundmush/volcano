#pragma once

#include <utility>
#include <variant>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

namespace vol::net {
    using TcpStream = boost::asio::ip::tcp::socket;
    using TlsStream = boost::asio::ssl::stream<TcpStream>;

    class AnyStream {
    public:
        using executor_type = TcpStream::executor_type;
        using lowest_layer_type = TcpStream;

        AnyStream() = default;
        AnyStream(TcpStream stream) : stream_(std::move(stream)) {}
        AnyStream(TlsStream stream) : stream_(std::move(stream)) {}

        AnyStream(const AnyStream&) = delete;
        AnyStream& operator=(const AnyStream&) = delete;
        AnyStream(AnyStream&&) noexcept = default;
        AnyStream& operator=(AnyStream&&) noexcept = default;

        [[nodiscard]] bool is_tls() const {
            return std::holds_alternative<TlsStream>(stream_);
        }

        executor_type get_executor() {
            return std::visit([](auto& stream) { return stream.get_executor(); }, stream_);
        }

        executor_type get_executor() const {
            return std::visit([](auto const& stream) { return stream.get_executor(); }, stream_);
        }

        lowest_layer_type& lowest_layer() {
            return std::visit(
                [](auto& stream) -> lowest_layer_type& { return boost::asio::get_lowest_layer(stream); },
                stream_);
        }

        const lowest_layer_type& lowest_layer() const {
            return std::visit(
                [](auto const& stream) -> const lowest_layer_type& { return boost::asio::get_lowest_layer(stream); },
                stream_);
        }

        template <typename MutableBufferSequence>
        std::size_t read_some(const MutableBufferSequence& buffers, boost::system::error_code& ec) {
            return std::visit([&](auto& stream) { return stream.read_some(buffers, ec); }, stream_);
        }

        template <typename ConstBufferSequence>
        std::size_t write_some(const ConstBufferSequence& buffers, boost::system::error_code& ec) {
            return std::visit([&](auto& stream) { return stream.write_some(buffers, ec); }, stream_);
        }

        template <typename MutableBufferSequence, typename CompletionToken>
        auto async_read_some(const MutableBufferSequence& buffers, CompletionToken&& token) {
            return std::visit(
                [&](auto& stream) -> decltype(auto) {
                    return stream.async_read_some(buffers, std::forward<CompletionToken>(token));
                },
                stream_);
        }

        template <typename ConstBufferSequence, typename CompletionToken>
        auto async_write_some(const ConstBufferSequence& buffers, CompletionToken&& token) {
            return std::visit(
                [&](auto& stream) -> decltype(auto) {
                    return stream.async_write_some(buffers, std::forward<CompletionToken>(token));
                },
                stream_);
        }

        template <typename Visitor>
        decltype(auto) visit(Visitor&& visitor) {
            return std::visit(std::forward<Visitor>(visitor), stream_);
        }

        template <typename Visitor>
        decltype(auto) visit(Visitor&& visitor) const {
            return std::visit(std::forward<Visitor>(visitor), stream_);
        }

    private:
        std::variant<TcpStream, TlsStream> stream_;
    };

}