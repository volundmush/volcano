#pragma once


#include <string>
#include <string_view>
#include <expected>
#include <filesystem>
#include <memory>
#include <variant>

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core/flat_buffer.hpp>

#include <net/Connection.hpp>


namespace vol::net {
    extern boost::asio::io_context& context();

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

    template<typename T>
    struct Connection {
        T connection;
        bool isSecure{false};
        std::string hostname, address;
    };

    

    struct AnyConnection {
        AnyStream connection;
        bool isSecure{false};
        std::string hostname, address;

        boost::asio::awaitable<std::expected<size_t, boost::system::error_code>> async_read_some(boost::beast::flat_buffer& buffer, std::size_t ceiling);
        boost::asio::awaitable<std::expected<size_t, boost::system::error_code>> async_write(boost::beast::flat_buffer& buffer);
        
    };

    struct Server {
        boost::asio::ip::tcp::acceptor acceptor;
        std::shared_ptr<boost::asio::ssl::context> tls_context;
        bool performReverseLookup{true};
        std::function<boost::asio::awaitable<void>(AnyConnection&&)> handle_client;

        Server(boost::asio::ip::tcp::acceptor acc, std::shared_ptr<boost::asio::ssl::context> tls_ctx)
            : acceptor(std::move(acc)), tls_context(std::move(tls_ctx)) {}
        
        Server(boost::asio::ip::address address, uint16_t port, std::shared_ptr<boost::asio::ssl::context> tls_ctx)
            : acceptor(boost::asio::make_strand(context()), boost::asio::ip::tcp::endpoint(address, port)), tls_context(std::move(tls_ctx)) {}

        void run();
        boost::asio::awaitable<void> accept_loop();
        boost::asio::awaitable<void> accept_client(TcpStream socket);
    };

    boost::asio::awaitable<std::expected<std::string, boost::system::error_code>> reverse_lookup(boost::asio::ip::tcp::socket& socket);

    std::expected<boost::asio::ip::address, boost::system::error_code> parse_address(std::string_view addr_str);

    std::expected<std::shared_ptr<boost::asio::ssl::context>, std::string> create_ssl_context(std::filesystem::path cert_path, std::filesystem::path key_path);

    void bind_server(boost::asio::ip::address address, uint16_t port, std::shared_ptr<boost::asio::ssl::context> tls_context, std::function<boost::asio::awaitable<void>(AnyConnection&&)> handle_client);

}
