#pragma once

#include <boost/asio.hpp>

namespace vol::net {
    extern boost::asio::io_context& context();
}