#pragma once

#include "Base.hpp"

#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/url.hpp>

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <chrono>
#include <expected>
#include <system_error>

namespace volcano::web {

    enum class HttpScheme {
        http,
        https
    };

    struct HttpTarget {
        HttpScheme scheme{HttpScheme::http};
        boost::asio::ip::address address{boost::asio::ip::address_v6::any()};
        uint16_t port{80};
        std::string host_header;

        [[nodiscard]] std::string host() const {
            if (!host_header.empty()) {
                return host_header;
            }
            return address.to_string();
        }
    };

    inline bool operator==(const HttpTarget& lhs, const HttpTarget& rhs) {
        return lhs.scheme == rhs.scheme && lhs.address == rhs.address && lhs.port == rhs.port;
    }

    struct HttpTargetHash {
        std::size_t operator()(const HttpTarget& target) const noexcept {
            std::size_t seed = 0;
            auto hash_combine = [&seed](std::size_t value) {
                seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
            };

            hash_combine(static_cast<std::size_t>(target.scheme));
            hash_combine(std::hash<unsigned short>{}(target.port));
            if (target.address.is_v4()) {
                hash_combine(std::hash<uint32_t>{}(target.address.to_v4().to_uint()));
            } else {
                auto bytes = target.address.to_v6().to_bytes();
                const std::string_view view(reinterpret_cast<const char*>(bytes.data()), bytes.size());
                hash_combine(std::hash<std::string_view>{}(view));
            }
            return seed;
        }
    };

    struct HttpPoolOptions {
        std::size_t max_sessions{8};
        std::shared_ptr<boost::asio::ssl::context> tls_context{};
        std::chrono::milliseconds request_timeout{std::chrono::seconds(30)};
    };

    class HttpSession {
    public:
        explicit HttpSession(HttpTarget target, std::shared_ptr<boost::asio::ssl::context> tls_context = {});

        HttpSession(const HttpSession&) = delete;
        HttpSession& operator=(const HttpSession&) = delete;
        HttpSession(HttpSession&&) noexcept = default;
        HttpSession& operator=(HttpSession&&) noexcept = default;

        [[nodiscard]] const HttpTarget& target() const {
            return target_;
        }

        [[nodiscard]] bool is_open() const;
        void close();

        boost::asio::awaitable<std::expected<HttpResponse, std::string>> request(
            HttpRequest request,
            std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    private:
        boost::asio::awaitable<std::expected<void, std::string>> connect(
            std::optional<std::chrono::milliseconds> timeout);

        HttpTarget target_;
        std::shared_ptr<boost::asio::ssl::context> tls_context_;
        std::optional<volcano::net::AnyStream> stream_;
        boost::beast::flat_buffer buffer_;
    };

    class HttpSessionPool {
    public:
        HttpSessionPool(HttpTarget target, HttpPoolOptions options = {});

        [[nodiscard]] const HttpTarget& target() const {
            return target_;
        }

        [[nodiscard]] const HttpPoolOptions& options() const {
            return options_;
        }

        boost::asio::awaitable<std::shared_ptr<HttpSession>> acquire();
        void release(const std::shared_ptr<HttpSession>& session);

    private:
        HttpTarget target_;
        HttpPoolOptions options_;
        std::atomic<std::size_t> created_{0};
        std::mutex mutex_;
        boost::asio::experimental::concurrent_channel<void(boost::system::error_code, std::shared_ptr<HttpSession>)> channel_;
    };

    class HttpSessionPools {
    public:
        std::shared_ptr<HttpSessionPool> pool_for(const HttpTarget& target, HttpPoolOptions options = {});

    private:
        std::mutex mutex_;
        std::unordered_map<HttpTarget, std::weak_ptr<HttpSessionPool>, HttpTargetHash> pools_;
    };

    class HttpClient {
    public:
        explicit HttpClient(std::shared_ptr<HttpSessionPool> pool);
        explicit HttpClient(HttpTarget target, HttpPoolOptions options = {});

        [[nodiscard]] const HttpTarget& target() const {
            return pool_->target();
        }

        boost::asio::awaitable<std::expected<HttpResponse, std::string>> request(
            HttpRequest request,
            std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    private:
        std::shared_ptr<HttpSessionPool> pool_;
    };

    boost::asio::awaitable<std::expected<HttpTarget, std::string>> parse_http_target(std::string_view url);

} // namespace volcano::web
