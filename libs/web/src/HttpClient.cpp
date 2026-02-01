#include "volcano/web/HttpClient.hpp"

#include "volcano/net/Base.hpp"
#include "volcano/net/net.hpp"

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <system_error>

namespace volcano::web {

    namespace {
        std::atomic<int64_t> session_id_seed{1};

        int64_t next_session_id() {
            return session_id_seed.fetch_add(1, std::memory_order_relaxed);
        }

        std::shared_ptr<boost::asio::ssl::context> default_tls_context() {
            auto ctx = std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tls_client);
            ctx->set_default_verify_paths();
            ctx->set_verify_mode(boost::asio::ssl::verify_peer);
            return ctx;
        }

        std::string normalize_host_for_connect(std::string host) {
            if (host.empty()) {
                return host;
            }

            if (host.front() == '[') {
                auto end = host.find(']');
                if (end != std::string::npos) {
                    return host.substr(1, end - 1);
                }
            }

            auto colon = host.rfind(':');
            if (colon != std::string::npos && host.find(':') == colon) {
                auto port_part = host.substr(colon + 1);
                bool all_digits = !port_part.empty() && std::all_of(port_part.begin(), port_part.end(),
                    [](unsigned char c) { return std::isdigit(c) != 0; });
                if (all_digits) {
                    return host.substr(0, colon);
                }
            }

            return host;
        }

        std::error_code to_std_error(const boost::system::error_code& ec) {
            return std::error_code(ec.value(), std::system_category());
        }
    }

    HttpSession::HttpSession(HttpTarget target, std::shared_ptr<boost::asio::ssl::context> tls_context)
        : target_(std::move(target)), tls_context_(std::move(tls_context)) {}

    bool HttpSession::is_open() const {
        return stream_ && stream_->lowest_layer().is_open();
    }

    void HttpSession::close() {
        if (!stream_) {
            return;
        }
        boost::system::error_code ec;
        stream_->lowest_layer().close(ec);
        stream_.reset();
    }

    boost::asio::awaitable<std::expected<void, std::string>> HttpSession::connect(
        std::optional<std::chrono::milliseconds> timeout) {
        using namespace boost::asio::experimental::awaitable_operators;

        if (is_open()) {
            co_return std::expected<void, std::string>{};
        }

        auto host_header = target_.host_header;
        if (host_header.empty()) {
            host_header = target_.address.to_string();
        }
        auto connect_host = normalize_host_for_connect(host_header);

        volcano::net::ConnectOptions options{};
        if (target_.scheme == HttpScheme::https) {
            options.transport = volcano::net::TransportMode::tls;
            options.tls_context = tls_context_ ? tls_context_ : default_tls_context();
        }
        if (timeout) {
            options.timeout = *timeout;
        }

        auto connect_task = volcano::net::connect_any(connect_host, target_.port, options);
        std::expected<volcano::net::AnyStream, boost::system::error_code> connected =
            std::unexpected(boost::system::error_code{});

        if (timeout) {
            auto exec = co_await boost::asio::this_coro::executor;
            boost::asio::steady_timer timer(exec);
            timer.expires_after(*timeout);

            boost::system::error_code timer_ec;
            auto result = co_await (
                std::move(connect_task) ||
                timer.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, timer_ec))
            );

            if (result.index() == 1) {
                co_return std::unexpected("timed out");
            }

            connected = std::move(std::get<0>(result));
        } else {
            connected = co_await std::move(connect_task);
        }

        if (!connected) {
            co_return std::unexpected(connected.error().what());
        }

        stream_.emplace(std::move(*connected));
        co_return std::expected<void, std::string>{};
    }

    boost::asio::awaitable<std::expected<HttpResponse, std::string>> HttpSession::request(
        HttpRequest request,
        std::optional<std::chrono::milliseconds> timeout) {
        using namespace boost::asio::experimental::awaitable_operators;

        auto connected = co_await connect(timeout);
        if (!connected) {
            co_return std::unexpected(connected.error());
        }

        request.version(11);
        request.keep_alive(true);
        if (request.find(http::field::host) == request.end()) {
            request.set(http::field::host, target_.host());
        }

        buffer_.consume(buffer_.size());
        boost::system::error_code write_ec;
        auto write_task = http::async_write(*stream_, request, boost::asio::redirect_error(boost::asio::use_awaitable, write_ec));

        if (timeout) {
            auto exec = co_await boost::asio::this_coro::executor;
            boost::asio::steady_timer timer(exec);
            timer.expires_after(*timeout);

            boost::system::error_code timer_ec;
            auto result = co_await (
                std::move(write_task) ||
                timer.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, timer_ec))
            );

            if (result.index() == 1) {
                if (stream_) {
                    boost::system::error_code cancel_ec;
                    stream_->lowest_layer().cancel(cancel_ec);
                }
                close();
                co_return std::unexpected("timed out");
            }
        } else {
            co_await std::move(write_task);
        }

        if (write_ec) {
            close();
            co_return std::unexpected(write_ec.what());
        }

        HttpResponse response;
        boost::system::error_code read_ec;
        auto read_task = http::async_read(*stream_, buffer_, response, boost::asio::redirect_error(boost::asio::use_awaitable, read_ec));

        if (timeout) {
            auto exec = co_await boost::asio::this_coro::executor;
            boost::asio::steady_timer timer(exec);
            timer.expires_after(*timeout);

            boost::system::error_code timer_ec;
            auto result = co_await (
                std::move(read_task) ||
                timer.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, timer_ec))
            );

            if (result.index() == 1) {
                if (stream_) {
                    boost::system::error_code cancel_ec;
                    stream_->lowest_layer().cancel(cancel_ec);
                }
                close();
                co_return std::unexpected("timed out");
            }
        } else {
            co_await std::move(read_task);
        }

        if (read_ec) {
            close();
            co_return std::unexpected(read_ec.what());
        }

        if (!response.keep_alive()) {
            close();
        }

        co_return response;
    }

    HttpSessionPool::HttpSessionPool(HttpTarget target, HttpPoolOptions options)
        : target_(std::move(target)),
          options_(std::move(options)),
          channel_(volcano::net::context(), static_cast<int>(options_.max_sessions)) {}

    boost::asio::awaitable<std::shared_ptr<HttpSession>> HttpSessionPool::acquire() {
        {
            std::lock_guard lock(mutex_);
            if (created_ < options_.max_sessions) {
                ++created_;
                co_return std::make_shared<HttpSession>(target_, options_.tls_context);
            }
        }

        auto [ec, session] = co_await channel_.async_receive(boost::asio::as_tuple(boost::asio::use_awaitable));
        if (ec) {
            throw std::runtime_error("HTTP session pool closed: " + ec.message());
        }
        co_return session;
    }

    void HttpSessionPool::release(const std::shared_ptr<HttpSession>& session) {
        if (!session || !session->is_open()) {
            std::lock_guard lock(mutex_);
            if (created_ > 0) {
                --created_;
            }
            return;
        }
        if (!channel_.try_send(boost::system::error_code{}, session)) {
            session->close();
            std::lock_guard lock(mutex_);
            if (created_ > 0) {
                --created_;
            }
        }
    }

    std::shared_ptr<HttpSessionPool> HttpSessionPools::pool_for(const HttpTarget& target, HttpPoolOptions options) {
        std::lock_guard lock(mutex_);
        auto it = pools_.find(target);
        if (it != pools_.end()) {
            if (auto existing = it->second.lock()) {
                return existing;
            }
        }
        auto created = std::make_shared<HttpSessionPool>(target, std::move(options));
        pools_[target] = created;
        return created;
    }

    HttpClient::HttpClient(std::shared_ptr<HttpSessionPool> pool) : pool_(std::move(pool)) {
        if (!pool_) {
            throw std::runtime_error("HttpClient requires a valid HttpSessionPool");
        }
    }

    HttpClient::HttpClient(HttpTarget target, HttpPoolOptions options)
        : pool_(std::make_shared<HttpSessionPool>(std::move(target), std::move(options))) {}

    boost::asio::awaitable<std::expected<HttpResponse, std::string>> HttpClient::request(
        HttpRequest request,
        std::optional<std::chrono::milliseconds> timeout) {
        auto effective_timeout = timeout.value_or(pool_->options().request_timeout);

        std::shared_ptr<HttpSession> session;
        try {
            session = co_await pool_->acquire();
        } catch (...) {
            co_return std::unexpected("resource unavailable, try again later");
        }

        auto response = co_await session->request(std::move(request), effective_timeout);
        if (!response) {
            session->close();
        }
        pool_->release(session);
        co_return response;
    }

    boost::asio::awaitable<std::expected<HttpTarget, std::string>> parse_http_target(std::string_view url) {
        auto parsed = boost::urls::parse_uri(url);
        if (!parsed) {
            co_return std::unexpected(std::string("Invalid URL: ") + parsed.error().message());
        }

        auto view = *parsed;
        if (view.scheme().empty() || view.host().empty()) {
            co_return std::unexpected("URL must include scheme and host");
        }

        std::string scheme_str(view.scheme());
        HttpScheme scheme;
        if (scheme_str == "http") {
            scheme = HttpScheme::http;
        } else if (scheme_str == "https") {
            scheme = HttpScheme::https;
        } else {
            co_return std::unexpected("Unsupported URL scheme: " + scheme_str);
        }

        std::string host(view.host());
        if (host.empty()) {
            co_return std::unexpected("URL host is empty");
        }

        uint16_t port = scheme == HttpScheme::https ? 443 : 80;
        if (view.has_port()) {
            auto port_number = view.port_number();
            if (port_number == 0 || port_number > 65535) {
                co_return std::unexpected("Invalid URL port");
            }
            port = static_cast<uint16_t>(port_number);
        }

        std::string host_header;
        if (view.host_type() == boost::urls::host_type::ipv6) {
            host_header = "[" + host + "]";
        } else {
            host_header = host;
        }
        if (view.has_port() && ((scheme == HttpScheme::http && port != 80) || (scheme == HttpScheme::https && port != 443))) {
            host_header += ":" + std::to_string(port);
        }

        if (auto parsed_address = volcano::net::parse_address(host); parsed_address) {
            co_return HttpTarget{scheme, *parsed_address, port, host_header};
        }

        boost::asio::ip::tcp::resolver resolver(volcano::net::context());
        auto [ec, results] = co_await resolver.async_resolve(
            host, std::to_string(port), boost::asio::as_tuple(boost::asio::use_awaitable));
        if (ec) {
            co_return std::unexpected(std::string("Host resolution failed: ") + ec.message());
        }

        auto endpoint = results.begin()->endpoint();
        co_return HttpTarget{scheme, endpoint.address(), port, host_header};
    }

} // namespace volcano::web
