#include "volcano/web/HttpClient.hpp"

#include "volcano/net/Base.hpp"
#include "volcano/net/net.hpp"

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <openssl/ssl.h>

#include <stdexcept>

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

    boost::asio::awaitable<void> HttpSession::connect() {
        if (is_open()) {
            co_return;
        }

        boost::asio::ip::tcp::endpoint endpoint(target_.address, target_.port);
        auto host = target_.host();

        if (target_.scheme == HttpScheme::https) {
            auto ctx = tls_context_ ? tls_context_ : default_tls_context();
            boost::asio::ssl::stream<volcano::net::TcpStream> tls_stream(
                boost::asio::make_strand(volcano::net::context()), *ctx);

            boost::system::error_code ec;
            co_await tls_stream.next_layer().async_connect(
                endpoint, boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            if (ec) {
                throw std::runtime_error("TLS connect failed: " + ec.message());
            }

            if (!host.empty()) {
                SSL_set_tlsext_host_name(tls_stream.native_handle(), host.c_str());
            }

            co_await tls_stream.async_handshake(
                boost::asio::ssl::stream_base::client,
                boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            if (ec) {
                throw std::runtime_error("TLS handshake failed: " + ec.message());
            }

            auto remote = tls_stream.next_layer().remote_endpoint(ec);
            if (ec) {
                remote = endpoint;
            }

            stream_.emplace(next_session_id(), std::move(tls_stream), remote, host);
            co_return;
        }

        volcano::net::TcpStream socket(boost::asio::make_strand(volcano::net::context()));
        boost::system::error_code ec;
        co_await socket.async_connect(endpoint, boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec) {
            throw std::runtime_error("TCP connect failed: " + ec.message());
        }
        auto remote = socket.remote_endpoint(ec);
        if (ec) {
            remote = endpoint;
        }
        stream_.emplace(next_session_id(), std::move(socket), remote, host);
    }

    boost::asio::awaitable<HttpResponse> HttpSession::request(HttpRequest request) {
        co_await connect();

        request.version(11);
        request.keep_alive(true);
        if (request.find(http::field::host) == request.end()) {
            request.set(http::field::host, target_.host());
        }

        buffer_.consume(buffer_.size());
        boost::system::error_code ec;
        co_await http::async_write(*stream_, request, boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec) {
            close();
            throw std::runtime_error("HTTP write failed: " + ec.message());
        }

        HttpResponse response;
        co_await http::async_read(*stream_, buffer_, response, boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec) {
            close();
            throw std::runtime_error("HTTP read failed: " + ec.message());
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

    boost::asio::awaitable<HttpResponse> HttpClient::request(HttpRequest request) {
        auto session = co_await pool_->acquire();
        try {
            auto response = co_await session->request(std::move(request));
            pool_->release(session);
            co_return response;
        } catch (...) {
            session->close();
            pool_->release(session);
            throw;
        }
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
