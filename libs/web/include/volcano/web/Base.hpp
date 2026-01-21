#pragma once
#include "volcano/net/Server.hpp"

#include <expected>
#include <string>
#include <string_view>
#include <unordered_map>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

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
        http::field content_type_field = http::field::content_type;
        std::string content_type = "text/plain";
    };

    using Parameters = std::unordered_map<std::string, std::string>;

    using RequestHandler = std::function<boost::asio::awaitable<HttpAnswer>(volcano::net::AnyStream &, HttpRequest &, Parameters&)>;
    using WebSocketHandler = std::function<boost::asio::awaitable<void>(WebSocketStream&, HttpRequest&, Parameters&)>;

    
}