#include "volcano/net/Server.hpp"
#include "volcano/net/net.hpp"
#include "volcano/log/Log.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ssl.hpp>

#include <atomic>

namespace volcano::net
{
    boost::asio::awaitable<std::expected<std::string, boost::system::error_code>> reverse_lookup(const boost::asio::ip::tcp::endpoint &endpoint)
    {
        boost::system::error_code ec;
        auto remote_address = endpoint.address();
        boost::asio::ip::tcp::resolver resolver(co_await boost::asio::this_coro::executor);
        try
        {
            auto results = co_await resolver.async_resolve(
                remote_address.to_string(),
                "",
                boost::asio::ip::tcp::resolver::flags::numeric_service,
                boost::asio::use_awaitable);
            if (results.empty())
            {
                co_return std::unexpected(boost::asio::error::host_not_found);
            }
            co_return results.begin()->host_name();
        }
        catch (const boost::system::system_error &e)
        {
            co_return std::unexpected(e.code());
        }
    }

    static std::atomic<int64_t> connection_id_seed{1};

    Server::Server(boost::asio::ip::tcp::acceptor acc, std::shared_ptr<boost::asio::ssl::context> tls_ctx, ClientHandler handler)
        : acceptor(std::move(acc)), tls_context(std::move(tls_ctx)), handle_client(std::move(handler)) {}

    Server::Server(boost::asio::ip::address address, uint16_t port, std::shared_ptr<boost::asio::ssl::context> tls_ctx, ClientHandler handler)
        : acceptor(boost::asio::make_strand(context()), boost::asio::ip::tcp::endpoint(address, port)),
          tls_context(std::move(tls_ctx)), handle_client(std::move(handler)) {}

    boost::asio::awaitable<void> Server::accept_client(TcpStream socket, int64_t connection_id)
    {
        auto endpoint = socket.remote_endpoint();
        auto client_address = endpoint.address().to_string();
        auto client_hostname = client_address;
        LINFO("Incoming connection from {}", client_address);

        if (performReverseLookup)
        {
            if (auto rev_res = co_await reverse_lookup(endpoint); rev_res)
            {
                client_hostname = rev_res.value();
                LINFO("Resolved hostname {} for {}", client_hostname, client_address);
            }
            else
            {
                LINFO("Could not resolve hostname for {}: {}", client_address, rev_res.error().message());
            }
        }

        if (tls_context)
        {
            auto ssl_socket = TlsStream(std::move(socket), *tls_context);
            boost::system::error_code ec;
            co_await ssl_socket.async_handshake(boost::asio::ssl::stream_base::server, boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            if (ec)
            {
                LERROR("TLS handshake failed with {}: {}", client_hostname, ec.message());
                co_return;
            }
            LINFO("Completed TLS handshake with {}", client_hostname);
            AnyStream stream(connection_id, std::move(ssl_socket), endpoint, client_hostname);
            co_await handle_client(std::move(stream));
        }
        else
        {
            AnyStream stream(connection_id, std::move(socket), endpoint, client_hostname);
            co_await handle_client(std::move(stream));
        }
    }

    boost::asio::awaitable<void> Server::accept_loop()
    {
        for (;;)
        {
            boost::system::error_code ec;
            auto socket = co_await acceptor.async_accept(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            if (ec)
            {
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

    void Server::run()
    {
        if (!handle_client)
        {
            LERROR("Server has no client handler defined; cannot run.");
            return;
        }
        auto exec = acceptor.get_executor();
        LINFO("{} Server listening on {}:{}", tls_context ? "TLS" : "TCP", acceptor.local_endpoint().address().to_string(), acceptor.local_endpoint().port());
        boost::asio::co_spawn(exec,
                              accept_loop(),
                              boost::asio::detached);
    }

} // namespace vol::net
