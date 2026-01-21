#pragma once

#include <memory>

#include "Router.hpp"

namespace volcano::web {

volcano::net::ClientHandler make_router_handler(std::shared_ptr<Router> router);
} // namespace vol::web
