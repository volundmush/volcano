#pragma once


#include <string>
#include <string_view>
#include <expected>
#include <filesystem>
#include <memory>
#include <variant>
#include <chrono>

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core/flat_buffer.hpp>

#include "Server.hpp"


namespace volcano::net {
    

    struct TlsConfig {
        boost::asio::ip::address address = boost::asio::ip::address_v6::any();
        uint16_t port{8080};
        std::filesystem::path cert_path;
        std::filesystem::path key_path;
        std::shared_ptr<boost::asio::ssl::context> ssl_context{nullptr};
    };

    struct Config {
        boost::asio::ip::address tcp_address = boost::asio::ip::address_v6::any();
        uint16_t tcp_port{8000};
    };

    extern Config tcp_config;
    extern TlsConfig tls_config;

    enum class TransportMode {
        tcp,
        tls
    };

    struct ConnectOptions {
        TransportMode transport{TransportMode::tcp};
        std::shared_ptr<boost::asio::ssl::context> tls_context{nullptr};
        bool verify_peer{true};
        bool tcp_no_delay{false};
        bool keep_alive{false};
        std::chrono::steady_clock::duration timeout{std::chrono::seconds(10)};
    };

    boost::asio::awaitable<std::expected<std::string, boost::system::error_code>> reverse_lookup(boost::asio::ip::tcp::socket& socket);

    std::expected<boost::asio::ip::address, boost::system::error_code> parse_address(std::string_view addr_str);

    boost::asio::awaitable<std::expected<boost::asio::ip::address, boost::system::error_code>> resolve_address(std::string_view host, uint16_t port);
    boost::asio::awaitable<std::expected<boost::asio::ip::address, boost::system::error_code>> resolve_address(
        std::string_view host,
        uint16_t port,
        std::chrono::steady_clock::duration timeout);
    std::expected<boost::asio::ip::address, boost::system::error_code> resolve_address(boost::asio::ip::address address);

    std::expected<std::shared_ptr<boost::asio::ssl::context>, std::string> create_ssl_context(std::filesystem::path cert_path, std::filesystem::path key_path);

    boost::asio::awaitable<std::expected<AnyStream, boost::system::error_code>> connect_any(std::string_view host, uint16_t port, ConnectOptions options = {});
    boost::asio::awaitable<std::expected<AnyStream, boost::system::error_code>> connect_any(boost::asio::ip::address address, uint16_t port, ConnectOptions options = {});

    void bind_server(boost::asio::ip::address address, uint16_t port, std::shared_ptr<boost::asio::ssl::context> tls_context, std::function<boost::asio::awaitable<void>(AnyStream&&)> handle_client);

    void run(int numThreads = std::thread::hardware_concurrency());

    boost::asio::awaitable<void> waitForever(boost::asio::cancellation_signal& signal);

}
