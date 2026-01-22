module;

#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/url.hpp>

export module volcano.web;

import volcano.net;
import nlohmann.json;

export namespace volcano::web {
    namespace http = boost::beast::http;
    using WebSocketStream = boost::beast::websocket::stream<volcano::net::AnyStream>;
    using HttpRequest = http::request<http::string_body>;
    using HttpResponse = http::response<http::string_body>;

    struct HttpAnswer {
        http::status status;
        std::string body;
        std::string content_type = "text/plain";
    };

    using Parameters = std::unordered_map<std::string, std::string>;

    struct RequestContext {
        HttpRequest& request;
        Parameters& params;
        boost::urls::params_view query;
    };

    using RequestHandler = std::function<boost::asio::awaitable<HttpAnswer>(volcano::net::AnyStream&, RequestContext&)>;
    using WebSocketHandler = std::function<boost::asio::awaitable<void>(WebSocketStream&, RequestContext&)>;

    class Router {
    public:
        Router();
        using RouterRef = std::reference_wrapper<Router>;

        RouterRef add_router(std::string_view path);
        void add_request_handler(std::string_view path, http::verb verb, RequestHandler handler);
        void add_websocket_handler(std::string_view path, WebSocketHandler handler);
        void register_parameter(std::string_view type, std::string_view pattern);
        void register_parameter(std::string_view type, std::function<bool(std::string_view)> validator);

        struct MatchResult {
            Router* node;
            Parameters params;
        };

        std::optional<MatchResult> match(std::string_view path) const;
        bool has_request_handlers() const;
        std::optional<std::reference_wrapper<RequestHandler>> request_handler(http::verb verb);
        std::optional<std::reference_wrapper<WebSocketHandler>> websocket_handler();

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

        RouterRef get_or_create(std::string_view path);

        ChildMap static_children_;
        ChildMap param_children_;
        std::unordered_map<http::verb, RequestHandler> request_handlers_;
        std::optional<WebSocketHandler> websocket_handler_;
        std::shared_ptr<Registry> registry_;
    };

    volcano::net::ClientHandler make_router_handler(std::shared_ptr<Router> router);

    std::expected<nlohmann::json, std::string> parse_json_body(HttpRequest& req);
} // namespace volcano::web

namespace volcano::web {
    Router::Router() : registry_(std::make_shared<Registry>()) {
        registry_->params.emplace("string", ParamSpec{std::regex("[^/]+"), {}, "[^/]+"});
    }

    std::expected<Router::Segment, std::string> Router::parse_segment(std::string_view raw) {
        if (raw.empty()) {
            return std::unexpected("Empty path segment.");
        }

        if (raw.front() != ':') {
            return Segment{false, std::string(raw)};
        }

        if (raw.size() == 1) {
            return std::unexpected("Parameter segment requires a name.");
        }

        return Segment{true, std::string(raw)};
    }

    std::vector<std::string_view> Router::split_path(std::string_view path) {
        std::vector<std::string_view> parts;
        std::size_t i = 0;
        while (i < path.size()) {
            while (i < path.size() && path[i] == '/') {
                ++i;
            }
            if (i >= path.size()) {
                break;
            }
            std::size_t j = i;
            while (j < path.size() && path[j] != '/') {
                ++j;
            }
            parts.emplace_back(path.substr(i, j - i));
            i = j;
        }
        return parts;
    }

    std::pair<std::string, std::string> Router::parse_param_key(std::string_view key) {
        std::string_view trimmed = key;
        if (!trimmed.empty() && trimmed.front() == ':') {
            trimmed.remove_prefix(1);
        }

        auto separator = trimmed.find(':');
        if (separator == std::string_view::npos) {
            return {"string", std::string(trimmed)};
        }

        std::string type(trimmed.substr(0, separator));
        std::string name(trimmed.substr(separator + 1));
        if (type.empty()) {
            type = "string";
        }
        return {std::move(type), std::move(name)};
    }

    auto Router::get_or_create(std::string_view path) -> RouterRef {
        if (path.empty() || path == "/") {
            return RouterRef{*this};
        }

        Router* current = this;
        for (auto segment_view : split_path(path)) {
            auto parsed = parse_segment(segment_view);
            if (!parsed) {
                throw std::runtime_error(parsed.error());
            }

            const auto& segment = parsed.value();
            ChildMap& map = segment.is_param ? current->param_children_ : current->static_children_;
            auto it = map.find(segment.key);
            if (it == map.end()) {
                auto next = std::make_unique<Router>();
                next->registry_ = current->registry_;
                auto [inserted, _] = map.emplace(segment.key, std::move(next));
                it = inserted;
            }
            current = it->second.get();
        }

        return RouterRef{*current};
    }

    std::optional<Router::MatchResult> Router::match(std::string_view path) const {
        if (path.empty() || path == "/") {
            return MatchResult{const_cast<Router*>(this), Parameters{}};
        }

        const Router* current = this;
        Parameters params;

        for (auto segment_view : split_path(path)) {
            auto static_it = current->static_children_.find(std::string(segment_view));
            if (static_it != current->static_children_.end()) {
                current = static_it->second.get();
                continue;
            }

            bool matched_param = false;
            for (const auto& [key, child] : current->param_children_) {
                auto [type, name] = parse_param_key(key);
                auto spec_it = current->registry_->params.find(type);
                if (spec_it == current->registry_->params.end()) {
                    continue;
                }

                const auto& spec = spec_it->second;
                if (spec.validator) {
                    if (!spec.validator(segment_view)) {
                        continue;
                    }
                } else if (spec.regex) {
                    if (!std::regex_match(segment_view.begin(), segment_view.end(), *spec.regex)) {
                        continue;
                    }
                } else {
                    continue;
                }

                params[name] = std::string(segment_view);
                current = child.get();
                matched_param = true;
                break;
            }

            if (!matched_param) {
                return std::nullopt;
            }
        }

        return MatchResult{const_cast<Router*>(current), std::move(params)};
    }

    bool Router::has_request_handlers() const {
        return !request_handlers_.empty();
    }

    std::optional<std::reference_wrapper<RequestHandler>> Router::request_handler(http::verb verb) {
        auto it = request_handlers_.find(verb);
        if (it == request_handlers_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    std::optional<std::reference_wrapper<WebSocketHandler>> Router::websocket_handler() {
        if (!websocket_handler_) {
            return std::nullopt;
        }
        return *websocket_handler_;
    }

    auto Router::add_router(std::string_view path) -> RouterRef {
        return get_or_create(path);
    }

    void Router::add_request_handler(std::string_view path, http::verb verb, RequestHandler handler) {
        auto& router = get_or_create(path).get();
        auto [it, inserted] = router.request_handlers_.emplace(verb, std::move(handler));
        if (!inserted) {
            throw std::runtime_error("Request handler already registered for this verb.");
        }
    }

    void Router::add_websocket_handler(std::string_view path, WebSocketHandler handler) {
        auto& router = get_or_create(path).get();
        if (router.websocket_handler_) {
            throw std::runtime_error("WebSocket handler already registered for this path.");
        }

        router.websocket_handler_ = std::move(handler);
    }

    void Router::register_parameter(std::string_view type, std::string_view pattern) {
        if (type.empty()) {
            throw std::runtime_error("Parameter type is empty.");
        }
        if (pattern.empty()) {
            throw std::runtime_error("Parameter regex is empty.");
        }

        try {
            const std::string pattern_str(pattern);
            std::regex compiled;
            compiled = std::regex(pattern_str);
            registry_->params[std::string(type)] = ParamSpec{std::optional<std::regex>{std::move(compiled)}, {}, pattern_str};
        } catch (const std::regex_error&) {
            throw std::runtime_error("Invalid regex for parameter type.");
        }
    }

    void Router::register_parameter(std::string_view type, std::function<bool(std::string_view)> validator) {
        if (type.empty()) {
            throw std::runtime_error("Parameter type is empty.");
        }
        if (!validator) {
            throw std::runtime_error("Parameter validator is empty.");
        }

        registry_->params[std::string(type)] = ParamSpec{std::nullopt, std::move(validator), "<validator>"};
    }

    static HttpResponse make_response(const HttpRequest& req, HttpAnswer answer) {
        HttpResponse res{answer.status, req.version()};
        res.set(http::field::content_type, answer.content_type);
        res.keep_alive(req.keep_alive());
        res.body() = std::move(answer.body);
        res.prepare_payload();
        return res;
    }

    std::expected<nlohmann::json, std::string> parse_json_body(HttpRequest& req) {
        try {
            auto json = nlohmann::json::parse(req.body());
            return json;
        } catch (const nlohmann::json::parse_error& e) {
            return std::unexpected(std::string("Failed to parse JSON body: ") + e.what());
        }
    }

    volcano::net::ClientHandler make_router_handler(std::shared_ptr<Router> router) {
        return [router](volcano::net::AnyStream&& stream) -> boost::asio::awaitable<void> {
            boost::beast::flat_buffer buffer;

            for (;;) {
                HttpRequest req;
                boost::system::error_code ec;
                co_await http::async_read(stream, buffer, req, boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                if (ec == http::error::end_of_stream) {
                    break;
                }
                if (ec) {
                    co_return;
                }

                auto parsed = boost::urls::parse_origin_form(req.target());
                std::string path_storage;
                if (parsed) {
                    path_storage = std::string(parsed->path());
                } else {
                    auto target = req.target();
                    path_storage = std::string(target);
                }
                std::string_view path = path_storage;

                auto match = router->match(path);
                if (!match) {
                    HttpAnswer answer{http::status::not_found, "Not Found"};
                    auto res = make_response(req, std::move(answer));
                    co_await http::async_write(stream, res, boost::asio::use_awaitable);
                    continue;
                }

                auto& node = *match->node;
                auto params = std::move(match->params);

                auto ctx = RequestContext{req, params, {}};
                ctx.query = boost::urls::url_view(req.target()).params();

                if (boost::beast::websocket::is_upgrade(req)) {
                    auto ws_handler = node.websocket_handler();
                    if (!ws_handler) {
                        HttpAnswer answer{http::status::not_found, "Not Found"};
                        auto res = make_response(req, std::move(answer));
                        co_await http::async_write(stream, res, boost::asio::use_awaitable);
                        continue;
                    }

                    WebSocketStream ws(std::move(stream));
                    co_await ws.async_accept(req, boost::asio::use_awaitable);
                    co_await ws_handler->get()(ws, ctx);
                    co_return;
                }

                auto handler = node.request_handler(req.method());
                if (!handler) {
                    HttpAnswer answer{node.has_request_handlers() ? http::status::method_not_allowed : http::status::not_found,
                                      node.has_request_handlers() ? "Method Not Allowed" : "Not Found"};
                    auto res = make_response(req, std::move(answer));
                    co_await http::async_write(stream, res, boost::asio::use_awaitable);
                    continue;
                }

                auto answer = co_await handler->get()(stream, ctx);
                auto res = make_response(req, std::move(answer));
                co_await http::async_write(stream, res, boost::asio::use_awaitable);
            }

            boost::system::error_code ec;
            stream.lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
            co_return;
        };
    }
}
