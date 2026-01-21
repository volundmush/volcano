#pragma once

#include <boost/asio.hpp>

namespace volcano::net {
    extern boost::asio::io_context& context();
}