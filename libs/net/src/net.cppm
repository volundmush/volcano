module;

#include <atomic>
#include <cctype>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <fmt/format.h>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/algorithm/string.hpp>

export module volcano.net;

import volcano.log;

export namespace boost::asio::ip {
    inline auto format_as(const address& address) {
        return address.to_string();
    }

    inline auto format_as(const tcp::endpoint& endpoint) {
        return fmt::format("{}:{}", endpoint.address().to_string(), endpoint.port());
    }

    inline auto format_as(const tcp::socket& socket) {
        boost::system::error_code ec;
        auto endpoint = socket.remote_endpoint(ec);
        std::string address = ec ? "<unknown>" : format_as(endpoint.address());
        return fmt::format("TcpStream({})", address);
    }
}

export namespace boost::asio::ssl {
    inline auto format_as(const stream<boost::asio::ip::tcp::socket>& ssl_stream) {
        boost::system::error_code ec;
        auto endpoint = ssl_stream.next_layer().remote_endpoint(ec);
        std::string address = ec ? "<unknown>" : boost::asio::ip::format_as(endpoint.address());
        return fmt::format("TlsStream({})", address);
    }
}

export namespace volcano::net {
    using TcpStream = boost::asio::ip::tcp::socket;
    using TlsStream = boost::asio::ssl::stream<TcpStream>;

    class AnyStream {
    public:
        using executor_type = TcpStream::executor_type;
        using lowest_layer_type = TcpStream::lowest_layer_type;

        AnyStream() = delete;
        AnyStream(int64_t id, TcpStream stream, boost::asio::ip::tcp::endpoint endpoint, std::string hostname);
        AnyStream(int64_t id, TlsStream stream, boost::asio::ip::tcp::endpoint endpoint, std::string hostname);

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

        const std::string& hostname() const {
            return hostname_;
        }

        const boost::asio::ip::tcp::endpoint& endpoint() const {
            return endpoint_;
        }

    private:
        std::variant<TcpStream, TlsStream> stream_;
        int64_t id_{0};
        std::string hostname_;
        boost::asio::ip::tcp::endpoint endpoint_;
    };

    inline auto format_as(const AnyStream& any_stream) {
        auto& lowest = any_stream.lowest_layer();
        boost::system::error_code ec;
        auto endpoint = lowest.remote_endpoint(ec);
        std::string address = ec ? "<unknown>" : boost::asio::ip::format_as(endpoint.address());
        if (any_stream.is_tls()) {
            return fmt::format("AnyTlsStream#{}({})", any_stream.id(), address);
        }
        return fmt::format("AnyTcpStream#{}({})", any_stream.id(), address);
    }

    inline void beast_close_socket(AnyStream& stream) {
        boost::system::error_code ec;
        stream.lowest_layer().close(ec);
    }

    using ClientHandler = std::function<boost::asio::awaitable<void>(AnyStream&&)>;

    class Server {
    public:
        Server(boost::asio::ip::tcp::acceptor acc, std::shared_ptr<boost::asio::ssl::context> tls_ctx, ClientHandler handler);

        Server(boost::asio::ip::address address, uint16_t port, std::shared_ptr<boost::asio::ssl::context> tls_ctx, ClientHandler handler);

        void run();

    private:
        boost::asio::ip::tcp::acceptor acceptor;
        std::shared_ptr<boost::asio::ssl::context> tls_context;
        bool performReverseLookup{true};
        ClientHandler handle_client;
        boost::asio::awaitable<void> accept_loop();
        boost::asio::awaitable<void> accept_client(TcpStream socket, int64_t connection_id);
    };

    boost::asio::io_context& context();

    boost::asio::awaitable<std::expected<std::string, boost::system::error_code>> reverse_lookup(const boost::asio::ip::tcp::endpoint& endpoint);
    boost::asio::awaitable<std::expected<std::string, boost::system::error_code>> reverse_lookup(boost::asio::ip::tcp::socket& socket);

    std::expected<boost::asio::ip::address, boost::system::error_code> parse_address(std::string_view addr_str);

    std::expected<std::shared_ptr<boost::asio::ssl::context>, std::string> create_ssl_context(std::filesystem::path cert_path, std::filesystem::path key_path);

    void bind_server(boost::asio::ip::address address, uint16_t port, std::shared_ptr<boost::asio::ssl::context> tls_context, ClientHandler handle_client);
}

namespace volcano::net {
    AnyStream::AnyStream(int64_t id, TcpStream stream, boost::asio::ip::tcp::endpoint endpoint, std::string hostname)
        : stream_(std::move(stream)), id_(id), hostname_(std::move(hostname)), endpoint_(std::move(endpoint)) {}

    AnyStream::AnyStream(int64_t id, TlsStream stream, boost::asio::ip::tcp::endpoint endpoint, std::string hostname)
        : stream_(std::move(stream)), id_(id), hostname_(std::move(hostname)), endpoint_(std::move(endpoint)) {}

    bool AnyStream::is_tls() const {
        return std::holds_alternative<TlsStream>(stream_);
    }

    int64_t AnyStream::id() const {
        return id_;
    }

    AnyStream::executor_type AnyStream::get_executor() {
        if (auto* tcp = std::get_if<TcpStream>(&stream_)) {
            return tcp->get_executor();
        }
        return std::get<TlsStream>(stream_).get_executor();
    }

    AnyStream::executor_type AnyStream::get_executor() const {
        return const_cast<AnyStream*>(this)->get_executor();
    }

    AnyStream::lowest_layer_type& AnyStream::lowest_layer() {
        if (auto* tcp = std::get_if<TcpStream>(&stream_)) {
            return tcp->lowest_layer();
        }
        return std::get<TlsStream>(stream_).lowest_layer();
    }

    AnyStream::lowest_layer_type& AnyStream::lowest_layer() const {
        return const_cast<AnyStream*>(this)->lowest_layer();
    }

    boost::asio::io_context& context() {
        static boost::asio::io_context ioc;
        return ioc;
    }

    boost::asio::awaitable<std::expected<std::string, boost::system::error_code>> reverse_lookup(const boost::asio::ip::tcp::endpoint& endpoint) {
        boost::system::error_code ec;
        auto remote_address = endpoint.address();
        boost::asio::ip::tcp::resolver resolver(co_await boost::asio::this_coro::executor);
        try {
            auto results = co_await resolver.async_resolve(
                remote_address.to_string(),
                "",
                boost::asio::ip::tcp::resolver::flags::numeric_service,
                boost::asio::use_awaitable);
            if (results.empty()) {
                co_return std::unexpected(boost::asio::error::host_not_found);
            }
            co_return results.begin()->host_name();
        } catch (const boost::system::system_error& e) {
            co_return std::unexpected(e.code());
        }
    }

    boost::asio::awaitable<std::expected<std::string, boost::system::error_code>> reverse_lookup(boost::asio::ip::tcp::socket& socket) {
        boost::system::error_code ec;
        auto endpoint = socket.remote_endpoint(ec);
        if (ec) {
            co_return std::unexpected(ec);
        }
        co_return co_await reverse_lookup(endpoint);
    }

    std::expected<boost::asio::ip::address, boost::system::error_code> parse_address(std::string_view addr_str) {
        if (boost::iequals(addr_str, "any") || boost::iequals(addr_str, "*")) {
            return boost::asio::ip::address_v6::any();
        }
        try {
            auto address = boost::asio::ip::make_address(std::string(addr_str));
            return address;
        } catch (const std::exception&) {
            boost::system::error_code ec = boost::asio::error::invalid_argument;
            return std::unexpected(ec);
        }
    }

    std::expected<std::shared_ptr<boost::asio::ssl::context>, std::string> create_ssl_context(std::filesystem::path cert_path, std::filesystem::path key_path) {
        try {
            if (cert_path.empty() || key_path.empty()) {
                return std::unexpected("Certificate path or key path is empty.");
            }
            if (!std::filesystem::exists(cert_path)) {
                return std::unexpected("Certificate file does not exist: " + cert_path.string());
            }
            if (!std::filesystem::exists(key_path)) {
                return std::unexpected("Key file does not exist: " + key_path.string());
            }
            auto ssl_context = std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tls_server);
            ssl_context->set_options(
                boost::asio::ssl::context::default_workarounds |
                boost::asio::ssl::context::no_sslv2 |
                boost::asio::ssl::context::no_sslv3 |
                boost::asio::ssl::context::single_dh_use);
            ssl_context->use_certificate_chain_file(cert_path.string());
            ssl_context->use_private_key_file(key_path.string(), boost::asio::ssl::context::pem);
            return ssl_context;
        } catch (const std::exception& e) {
            return std::unexpected(std::string("Failed to initialize TLS context: ") + e.what());
        }
    }

    static std::atomic<int64_t> connection_id_seed{1};

    Server::Server(boost::asio::ip::tcp::acceptor acc, std::shared_ptr<boost::asio::ssl::context> tls_ctx, ClientHandler handler)
        : acceptor(std::move(acc)), tls_context(std::move(tls_ctx)), handle_client(std::move(handler)) {}

    Server::Server(boost::asio::ip::address address, uint16_t port, std::shared_ptr<boost::asio::ssl::context> tls_ctx, ClientHandler handler)
        : acceptor(boost::asio::make_strand(context()), boost::asio::ip::tcp::endpoint(address, port)),
          tls_context(std::move(tls_ctx)), handle_client(std::move(handler)) {}

    boost::asio::awaitable<void> Server::accept_client(TcpStream socket, int64_t connection_id) {
        auto endpoint = socket.remote_endpoint();
        auto client_address = endpoint.address().to_string();
        auto client_hostname = client_address;
        LINFO("Incoming connection from {}", client_address);

        if (performReverseLookup) {
            if (auto rev_res = co_await reverse_lookup(endpoint); rev_res) {
                client_hostname = rev_res.value();
                LINFO("Resolved hostname {} for {}", client_hostname, client_address);
            } else {
                LINFO("Could not resolve hostname for {}: {}", client_address, rev_res.error().message());
            }
        }

        if (tls_context) {
            auto ssl_socket = TlsStream(std::move(socket), *tls_context);
            boost::system::error_code ec;
            co_await ssl_socket.async_handshake(boost::asio::ssl::stream_base::server, boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            if (ec) {
                LERROR("TLS handshake failed with {}: {}", client_hostname, ec.message());
                co_return;
            }
            LINFO("Completed TLS handshake with {}", client_hostname);
            AnyStream stream(connection_id, std::move(ssl_socket), endpoint, client_hostname);
            co_await handle_client(std::move(stream));
        } else {
            AnyStream stream(connection_id, std::move(socket), endpoint, client_hostname);
            co_await handle_client(std::move(stream));
        }
    }

    boost::asio::awaitable<void> Server::accept_loop() {
        for (;;) {
            boost::system::error_code ec;
            auto socket = co_await acceptor.async_accept(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            if (ec) {
                LERROR("Accept error: {}", ec.message());
                continue;
            }
            const int64_t connection_id = connection_id_seed.fetch_add(1, std::memory_order_relaxed);
            auto strand = boost::asio::make_strand(context());
            boost::asio::co_spawn(strand,
                                  accept_client(std::move(socket), connection_id),
                                  boost::asio::detached);
        }
        co_return;
    }

    void Server::run() {
        if (!handle_client) {
            LERROR("Server has no client handler defined; cannot run.");
            return;
        }
        auto exec = acceptor.get_executor();
        LINFO("{} Server listening on {}:{}", tls_context ? "TLS" : "TCP", acceptor.local_endpoint().address().to_string(), acceptor.local_endpoint().port());
        boost::asio::co_spawn(exec,
                              accept_loop(),
                              boost::asio::detached);
    }

    std::vector<std::shared_ptr<Server>> servers;

    void bind_server(boost::asio::ip::address address, uint16_t port, std::shared_ptr<boost::asio::ssl::context> tls_context, ClientHandler handle_client) {
        auto& src = servers.emplace_back(std::make_shared<Server>(address, port, tls_context, std::move(handle_client)));
        src->run();
    }
}
