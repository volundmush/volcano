#pragma once

#include <functional>
#include <memory>

#include <boost/asio/awaitable.hpp>

#include "net/Connection.hpp"

namespace vol::net {

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
}