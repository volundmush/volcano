#pragma once
#define BOOST_ASIO_HAS_IO_URING 1

#include <boost/asio.hpp>

namespace volcano::net {
    extern boost::asio::io_context& context();
}