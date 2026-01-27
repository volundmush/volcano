#include "volcano/net/net.hpp"
#include "volcano/net/Server.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <openssl/ssl.h>

#include <atomic>

namespace volcano::net {

    namespace {
        std::atomic<int64_t> stream_id_seed{1};

        int64_t next_stream_id() {
            return stream_id_seed.fetch_add(1, std::memory_order_relaxed);
        }

        std::shared_ptr<boost::asio::ssl::context> default_client_tls_context(bool verify_peer) {
            auto ctx = std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tls_client);
            ctx->set_default_verify_paths();
            ctx->set_verify_mode(verify_peer ? boost::asio::ssl::verify_peer : boost::asio::ssl::verify_none);
            return ctx;
        }

        template <typename Operation>
        boost::asio::awaitable<boost::system::error_code> run_with_timeout(Operation operation,
            std::chrono::steady_clock::duration timeout) {
            boost::system::error_code op_ec;

            if (timeout <= std::chrono::steady_clock::duration::zero()) {
                co_await operation(boost::asio::cancellation_slot{}, op_ec);
                co_return op_ec;
            }

            boost::asio::cancellation_signal cancel;
            boost::asio::steady_timer timer(context());
            timer.expires_after(timeout);

            boost::asio::co_spawn(
                context(),
                [&]() -> boost::asio::awaitable<void> {
                    boost::system::error_code timer_ec;
                    co_await timer.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, timer_ec));
                    if (!timer_ec) {
                        cancel.emit(boost::asio::cancellation_type::all);
                    }
                },
                boost::asio::detached);

            co_await operation(cancel.slot(), op_ec);
            timer.cancel();

            if (op_ec == boost::asio::error::operation_aborted) {
                co_return boost::asio::error::timed_out;
            }
            co_return op_ec;
        }
    }

    TlsConfig tls_config;
    Config tcp_config;

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

    boost::asio::awaitable<std::expected<boost::asio::ip::address, boost::system::error_code>> resolve_address(std::string_view host, uint16_t port) {
        co_return co_await resolve_address(host, port, std::chrono::seconds(10));
    }

    boost::asio::awaitable<std::expected<boost::asio::ip::address, boost::system::error_code>> resolve_address(
        std::string_view host,
        uint16_t port,
        std::chrono::steady_clock::duration timeout) {
        std::string host_string(host);
        boost::asio::ip::tcp::resolver resolver(context());
        boost::system::error_code ec;
        boost::asio::ip::tcp::resolver::results_type results;
        ec = co_await run_with_timeout(
            [&](boost::asio::cancellation_slot slot, boost::system::error_code& op_ec) -> boost::asio::awaitable<void> {
                auto [resolve_ec, resolved] = co_await resolver.async_resolve(
                    host_string,
                    std::to_string(port),
                    boost::asio::as_tuple(
                        boost::asio::bind_cancellation_slot(slot, boost::asio::use_awaitable)));
                op_ec = resolve_ec;
                if (!op_ec) {
                    results = std::move(resolved);
                }
            },
            timeout);
        if (ec) {
            co_return std::unexpected(ec);
        }
        if (results.begin() == results.end()) {
            co_return std::unexpected(boost::asio::error::host_not_found);
        }
        co_return results.begin()->endpoint().address();
    }

    std::expected<boost::asio::ip::address, boost::system::error_code> resolve_address(boost::asio::ip::address address) {
        return address;
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

    boost::asio::awaitable<std::expected<AnyStream, boost::system::error_code>> connect_any(std::string_view host, uint16_t port, ConnectOptions options) {
        std::string host_string(host);
        boost::asio::ip::tcp::resolver resolver(context());
        boost::system::error_code ec;
        boost::asio::ip::tcp::resolver::results_type results;
        ec = co_await run_with_timeout(
            [&](boost::asio::cancellation_slot slot, boost::system::error_code& op_ec) -> boost::asio::awaitable<void> {
                auto [resolve_ec, resolved] = co_await resolver.async_resolve(
                    host_string,
                    std::to_string(port),
                    boost::asio::as_tuple(
                        boost::asio::bind_cancellation_slot(slot, boost::asio::use_awaitable)));
                op_ec = resolve_ec;
                if (!op_ec) {
                    results = std::move(resolved);
                }
            },
            options.timeout);
        if (ec) {
            co_return std::unexpected(ec);
        }
        if (results.begin() == results.end()) {
            co_return std::unexpected(boost::asio::error::host_not_found);
        }
        auto endpoint = results.begin()->endpoint();

        if (options.transport == TransportMode::tls) {
            auto ctx = options.tls_context ? options.tls_context : default_client_tls_context(options.verify_peer);
            TlsStream tls_stream(boost::asio::make_strand(context()), *ctx);
            auto connect_ec = co_await run_with_timeout(
                [&](boost::asio::cancellation_slot slot, boost::system::error_code& ec) -> boost::asio::awaitable<void> {
                    co_await tls_stream.next_layer().async_connect(
                        endpoint,
                        boost::asio::bind_cancellation_slot(
                            slot,
                            boost::asio::redirect_error(boost::asio::use_awaitable, ec)));
                },
                options.timeout);
            if (connect_ec) {
                co_return std::unexpected(connect_ec);
            }

            if (!host_string.empty()) {
                SSL_set_tlsext_host_name(tls_stream.native_handle(), host_string.c_str());
            }

            auto hs_ec = co_await run_with_timeout(
                [&](boost::asio::cancellation_slot slot, boost::system::error_code& ec) -> boost::asio::awaitable<void> {
                    co_await tls_stream.async_handshake(
                        boost::asio::ssl::stream_base::client,
                        boost::asio::bind_cancellation_slot(
                            slot,
                            boost::asio::redirect_error(boost::asio::use_awaitable, ec)));
                },
                options.timeout);
            if (hs_ec) {
                co_return std::unexpected(hs_ec);
            }

            boost::system::error_code remote_ec;
            auto remote = tls_stream.next_layer().remote_endpoint(remote_ec);
            if (remote_ec) {
                remote = endpoint;
            }
            co_return AnyStream(next_stream_id(), std::move(tls_stream), remote, host_string);
        }

        TcpStream socket(boost::asio::make_strand(context()));
        auto connect_ec = co_await run_with_timeout(
            [&](boost::asio::cancellation_slot slot, boost::system::error_code& ec) -> boost::asio::awaitable<void> {
                co_await socket.async_connect(
                    endpoint,
                    boost::asio::bind_cancellation_slot(
                        slot,
                        boost::asio::redirect_error(boost::asio::use_awaitable, ec)));
            },
            options.timeout);
        if (connect_ec) {
            co_return std::unexpected(connect_ec);
        }

        if (options.tcp_no_delay) {
            boost::system::error_code opt_ec;
            socket.set_option(boost::asio::ip::tcp::no_delay(true), opt_ec);
        }
        if (options.keep_alive) {
            boost::system::error_code opt_ec;
            socket.set_option(boost::asio::socket_base::keep_alive(true), opt_ec);
        }

        boost::system::error_code remote_ec;
        auto remote = socket.remote_endpoint(remote_ec);
        if (remote_ec) {
            remote = endpoint;
        }
        co_return AnyStream(next_stream_id(), std::move(socket), remote, host_string);
    }

    boost::asio::awaitable<std::expected<AnyStream, boost::system::error_code>> connect_any(boost::asio::ip::address address, uint16_t port, ConnectOptions options) {
        boost::asio::ip::tcp::endpoint endpoint(address, port);
        auto hostname = address.to_string();

        if (options.transport == TransportMode::tls) {
            auto ctx = options.tls_context ? options.tls_context : default_client_tls_context(options.verify_peer);
            TlsStream tls_stream(boost::asio::make_strand(context()), *ctx);
            auto connect_ec = co_await run_with_timeout(
                [&](boost::asio::cancellation_slot slot, boost::system::error_code& ec) -> boost::asio::awaitable<void> {
                    co_await tls_stream.next_layer().async_connect(
                        endpoint,
                        boost::asio::bind_cancellation_slot(
                            slot,
                            boost::asio::redirect_error(boost::asio::use_awaitable, ec)));
                },
                options.timeout);
            if (connect_ec) {
                co_return std::unexpected(connect_ec);
            }

            auto hs_ec = co_await run_with_timeout(
                [&](boost::asio::cancellation_slot slot, boost::system::error_code& ec) -> boost::asio::awaitable<void> {
                    co_await tls_stream.async_handshake(
                        boost::asio::ssl::stream_base::client,
                        boost::asio::bind_cancellation_slot(
                            slot,
                            boost::asio::redirect_error(boost::asio::use_awaitable, ec)));
                },
                options.timeout);
            if (hs_ec) {
                co_return std::unexpected(hs_ec);
            }

            boost::system::error_code remote_ec;
            auto remote = tls_stream.next_layer().remote_endpoint(remote_ec);
            if (remote_ec) {
                remote = endpoint;
            }
            co_return AnyStream(next_stream_id(), std::move(tls_stream), remote, hostname);
        }

        TcpStream socket(boost::asio::make_strand(context()));
        auto connect_ec = co_await run_with_timeout(
            [&](boost::asio::cancellation_slot slot, boost::system::error_code& ec) -> boost::asio::awaitable<void> {
                co_await socket.async_connect(
                    endpoint,
                    boost::asio::bind_cancellation_slot(
                        slot,
                        boost::asio::redirect_error(boost::asio::use_awaitable, ec)));
            },
            options.timeout);
        if (connect_ec) {
            co_return std::unexpected(connect_ec);
        }

        if (options.tcp_no_delay) {
            boost::system::error_code opt_ec;
            socket.set_option(boost::asio::ip::tcp::no_delay(true), opt_ec);
        }
        if (options.keep_alive) {
            boost::system::error_code opt_ec;
            socket.set_option(boost::asio::socket_base::keep_alive(true), opt_ec);
        }

        boost::system::error_code remote_ec;
        auto remote = socket.remote_endpoint(remote_ec);
        if (remote_ec) {
            remote = endpoint;
        }
        co_return AnyStream(next_stream_id(), std::move(socket), remote, hostname);
    }

    void run(int numThreads) {
        // the default argument for numThreads is std::thread::hardware_concurrency()
        // the actual thread count will be at least 1, but that's the main thread which
        // will also run the io_context, so we need to create at most numThreads - 1 additional threads
        // all will run context().run()
        std::vector<std::thread> threads;
        for(int i = 1; i < numThreads; ++i) {
            threads.emplace_back([](){
                context().run();
            });
        }
        context().run();
        for(auto& thread : threads) {
            thread.join();
        }
    }

    boost::asio::awaitable<void> waitForever(boost::asio::cancellation_signal& signal) {
        auto exec = co_await boost::asio::this_coro::executor;
        boost::asio::steady_timer timer(exec);
        timer.expires_at(boost::asio::steady_timer::time_point::max());
        boost::system::error_code ec;
        co_await timer.async_wait(
            boost::asio::bind_cancellation_slot(
                signal.slot(),
                boost::asio::redirect_error(boost::asio::use_awaitable, ec)));
        co_return;
    }
}
