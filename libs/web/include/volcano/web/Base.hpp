#pragma once
#include "volcano/net/Server.hpp"
#include "volcano/jwt/jwt.hpp"

#include <expected>
#include <string>
#include <string_view>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/url.hpp>

namespace volcano::web
{
    namespace http = boost::beast::http;
    using WebSocketStream = boost::beast::websocket::stream<volcano::net::AnyStream>;
    using HttpRequest = http::request<http::string_body>;
    using HttpResponse = http::response<http::string_body>;

    struct HttpAnswer
    {
        http::status status;
        std::string body;
        std::string content_type = "text/plain";
    };

    using Parameters = std::unordered_map<std::string, std::string>;

    struct ClientInfo {
        std::string hostname;
        boost::asio::ip::address address;
    };

    struct RequestContext
    {
        ClientInfo client;
        ClientInfo connection;
        HttpRequest& request;
        Parameters& params;
        boost::urls::params_view query;
        nlohmann::json user_data;
    };

    using EndpointGuard = std::function<boost::asio::awaitable<std::optional<HttpAnswer>>(volcano::net::AnyStream &, RequestContext&)>;
    using RequestHandler = std::function<boost::asio::awaitable<HttpAnswer>(volcano::net::AnyStream &, RequestContext&)>;
    using WebSocketHandler = std::function<boost::asio::awaitable<void>(WebSocketStream&, RequestContext&)>;

    std::expected<nlohmann::json, HttpAnswer> authorize_bearer(HttpRequest& req, const volcano::jwt::JwtContext& jwt_ctx);
}