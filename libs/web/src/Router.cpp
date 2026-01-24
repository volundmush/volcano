#include <volcano/web/Router.hpp>

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

Router& Router::get_or_create(std::string_view path) {
    if (path.empty() || path == "/") {
        return *this;
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

    if(!current) {
        throw std::runtime_error("Failed to create or retrieve router for path.");
    }

    return *current;
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

std::optional<std::reference_wrapper<Router::RequestEndpoint>> Router::request_handler(http::verb verb) {
    auto it = request_handlers_.find(verb);
    if (it == request_handlers_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<std::reference_wrapper<Router::WebSocketEndpoint>> Router::websocket_handler() {
    if (!websocket_handler_) {
        return std::nullopt;
    }
    return *websocket_handler_;
}

Router& Router::add_router(std::string_view path) {
    return get_or_create(path);
}

void Router::add_request_handler(std::string_view path, http::verb verb, RequestHandler handler) {
    add_request_handler(path, verb, EndpointGuard{}, std::move(handler));
}

void Router::add_request_handler(std::string_view path, http::verb verb, EndpointGuard guard, RequestHandler handler) {
    auto& router = get_or_create(path);
    auto [it, inserted] = router.request_handlers_.emplace(verb, RequestEndpoint{std::move(guard), std::move(handler)});
    if (!inserted) {
        throw std::runtime_error("Request handler already registered for this verb.");
    }
}

void Router::add_websocket_handler(std::string_view path, WebSocketHandler handler) {
    add_websocket_handler(path, EndpointGuard{}, std::move(handler));
}

void Router::add_websocket_handler(std::string_view path, EndpointGuard guard, WebSocketHandler handler) {
    auto& router = get_or_create(path);
    if (router.websocket_handler_) {
        throw std::runtime_error("WebSocket handler already registered for this path.");
    }

    router.websocket_handler_ = WebSocketEndpoint{std::move(guard), std::move(handler)};
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

} // namespace vol::web
