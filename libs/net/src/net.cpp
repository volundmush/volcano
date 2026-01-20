#include "net/net.hpp"
#include "logging/Log.hpp"

#include <boost/beast/websocket.hpp>

#include <boost/algorithm/string.hpp>

namespace vol::net {

    TlsConfig tls_config;
    Config tcp_config;

    boost::asio::io_context& context() {
        static boost::asio::io_context ioc;
        return ioc;
    }

    boost::asio::awaitable<std::expected<std::string, boost::system::error_code>> reverse_lookup(boost::asio::ip::tcp::socket& socket) {
        boost::system::error_code ec;
        auto remote_endpoint = socket.remote_endpoint(ec);
        if(ec) {
            co_return std::unexpected(ec);
        }
        auto remote_address = remote_endpoint.address();
        boost::asio::ip::tcp::resolver resolver(co_await boost::asio::this_coro::executor);
        try {
            auto results = co_await resolver.async_resolve(
            remote_address.to_string(),
            "",
            boost::asio::ip::tcp::resolver::flags::numeric_service,
            boost::asio::use_awaitable
            );
            if(results.empty()) {
                co_return std::unexpected(boost::asio::error::host_not_found);
            }
            co_return results.begin()->host_name();
        } catch(const boost::system::system_error& e) {
            co_return std::unexpected(e.code());
        }
    }

    std::expected<boost::asio::ip::address, boost::system::error_code> parse_address(std::string_view addr_str) {
        if(boost::iequals(addr_str, "any") || boost::iequals(addr_str, "*")) {
            return boost::asio::ip::address_v6::any();
        }
        try {
            auto address = boost::asio::ip::make_address(std::string(addr_str));
            return address;
        } catch(const std::exception& e) {
            boost::system::error_code ec = boost::asio::error::invalid_argument;
            return std::unexpected(ec);
        }
    }

    boost::asio::awaitable<void> Server::accept_client(TcpStream socket) {
        auto client_address = socket.remote_endpoint().address().to_string();
        auto client_hostname = client_address;
        LINFO("Incoming connection from {}", client_address);

        if(performReverseLookup) {
            if(auto rev_res = co_await reverse_lookup(socket); rev_res) {
                client_hostname = rev_res.value();
                LINFO("Resolved hostname {} for {}", client_hostname, client_address);
            } else {
                LINFO("Could not resolve hostname for {}: {}", client_address, rev_res.error().message());
            }
        }

        if(tls_context) {
            auto ssl_socket = TlsStream(std::move(socket), *tls_context);
            boost::system::error_code ec;
            co_await ssl_socket.async_handshake(boost::asio::ssl::stream_base::server, boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            if(ec) {
                LERROR("TLS handshake failed with {}: {}", client_hostname, ec.message());
                co_return;
            }
            LINFO("Completed TLS handshake with {}", client_hostname);
            auto con = AnyConnection(std::move(ssl_socket), true, client_hostname, client_address);
            co_await handle_client(std::move(con));
        } else {
            auto con = AnyConnection(std::move(socket), false, client_hostname, client_address);
            co_await handle_client(std::move(con));
        }

    }

    boost::asio::awaitable<void> Server::accept_loop() {
        for(;;) {
            boost::system::error_code ec;
            auto socket = co_await acceptor.async_accept(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            if(ec) {
                LERROR("Accept error: {}", ec.message());
                continue;
            }
            auto strand = boost::asio::make_strand(context());
            boost::asio::co_spawn(strand,
                accept_client(std::move(socket)),
                boost::asio::detached
            );
        }
        co_return;
    }

    void Server::run() {
        if(!handle_client) {
            LERROR("Server has no client handler defined; cannot run.");
            return;
        }
        auto exec = acceptor.get_executor();
        LINFO("{} Server listening on {}:{}", tls_context ? "TLS" : "TCP", acceptor.local_endpoint().address().to_string(), acceptor.local_endpoint().port());
        boost::asio::co_spawn(exec,
            accept_loop(),
            boost::asio::detached
        );
    }

    std::expected<std::shared_ptr<boost::asio::ssl::context>, std::string> create_ssl_context(std::filesystem::path cert_path, std::filesystem::path key_path) {
        try {
            if(cert_path.empty() || key_path.empty()) {
                return std::unexpected("Certificate path or key path is empty.");
            }
            if(!std::filesystem::exists(cert_path)) {
                return std::unexpected("Certificate file does not exist: " + cert_path.string());
            }
            if(!std::filesystem::exists(key_path)) {
                return std::unexpected("Key file does not exist: " + key_path.string());
            }
            auto ssl_context = std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tls_server);
            ssl_context->set_options(
                boost::asio::ssl::context::default_workarounds |
                boost::asio::ssl::context::no_sslv2 |
                boost::asio::ssl::context::no_sslv3 |
                boost::asio::ssl::context::single_dh_use
            );
            ssl_context->use_certificate_chain_file(cert_path.string());
            ssl_context->use_private_key_file(key_path.string(), boost::asio::ssl::context::pem);
            return ssl_context;
        } catch (const std::exception& e) {
            return std::unexpected(std::string("Failed to initialize TLS context: ") + e.what());
        }
    }

    std::vector<std::shared_ptr<Server>> servers;

    void bind_server(boost::asio::ip::address address, uint16_t port, std::shared_ptr<boost::asio::ssl::context> tls_context, std::function<boost::asio::awaitable<void>(AnyConnection&&)> handle_client) {
        auto &src = servers.emplace_back(std::make_shared<Server>(address, port, tls_context));
        src->handle_client = handle_client;
        src->run();
        return;
    }

    boost::asio::awaitable<std::expected<size_t, boost::system::error_code>> AnyConnection::async_read_some(boost::beast::flat_buffer& buffer, std::size_t ceiling) {
        boost::system::error_code ec;
        std::size_t n = co_await connection.async_read_some(
            buffer.prepare(ceiling),
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if(ec) {
            co_return std::unexpected(ec);
        }
        buffer.commit(n);
        co_return n;
    }

    boost::asio::awaitable<std::expected<size_t, boost::system::error_code>> AnyConnection::async_write(boost::beast::flat_buffer& buffer) {
        boost::system::error_code ec;
        std::size_t n = co_await boost::asio::async_write(
            connection,
            buffer.data(),
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if(ec) {
            co_return std::unexpected(ec);
        }
        buffer.consume(n);
        co_return n;
    }

    void websocket_any_stream_compile_check() {
        TcpStream socket(context());
        AnyStream stream(std::move(socket));
        boost::beast::websocket::stream<AnyStream> ws(std::move(stream));
        (void)ws;
    }
}
