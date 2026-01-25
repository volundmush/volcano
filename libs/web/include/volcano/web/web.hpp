#pragma once

#include <memory>
#include <nlohmann/json.hpp>

#include "Router.hpp"
#include "HttpClient.hpp"


namespace volcano::web {

volcano::net::ClientHandler make_router_handler(std::shared_ptr<Router> router);

std::expected<nlohmann::json, std::string> parse_json_body(HttpRequest& req);

} // namespace vol::web
