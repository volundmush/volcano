#pragma once

#include "Base.hpp"

#include <stdexcept>
#include <functional>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace volcano::web {

    class Router {
    public:
        Router();

        Router& add_router(std::string_view path);
        void add_request_handler(std::string_view path, http::verb verb, RequestHandler handler);
        void add_request_handler(std::string_view path, http::verb verb, EndpointGuard guard, RequestHandler handler);
        void add_websocket_handler(std::string_view path, WebSocketHandler handler);
        void add_websocket_handler(std::string_view path, EndpointGuard guard, WebSocketHandler handler);
        void register_parameter(std::string_view type, std::string_view pattern);
        void register_parameter(std::string_view type, std::function<bool(std::string_view)> validator);

        struct MatchResult {
            Router* node;
            Parameters params;
        };

        std::optional<MatchResult> match(std::string_view path) const;
        bool has_request_handlers() const;
        struct RequestEndpoint {
            EndpointGuard guard;
            RequestHandler handler;
        };

        struct WebSocketEndpoint {
            EndpointGuard guard;
            WebSocketHandler handler;
        };

        std::optional<std::reference_wrapper<RequestEndpoint>> request_handler(http::verb verb);
        std::optional<std::reference_wrapper<WebSocketEndpoint>> websocket_handler();

    private:
        struct Segment {
            bool is_param{false};
            std::string key;
        };

        struct ParamSpec {
            std::optional<std::regex> regex;
            std::function<bool(std::string_view)> validator;
            std::string pattern;
        };

        struct Registry {
            std::unordered_map<std::string, ParamSpec> params;
        };

        using ChildMap = std::unordered_map<std::string, std::unique_ptr<Router>>;

        static std::expected<Segment, std::string> parse_segment(std::string_view raw);
        static std::vector<std::string_view> split_path(std::string_view path);
        static std::pair<std::string, std::string> parse_param_key(std::string_view key);

        Router& get_or_create(std::string_view path);

        ChildMap static_children_;
        ChildMap param_children_;
        std::unordered_map<http::verb, RequestEndpoint> request_handlers_;
        std::optional<WebSocketEndpoint> websocket_handler_;
        std::shared_ptr<Registry> registry_;
    };
}