#pragma once

#include <memory>

#include "web/Router.hpp"

namespace vol::web {

vol::net::ClientHandler make_router_handler(std::shared_ptr<Router> router);

} // namespace vol::web
