#include "net/Base.hpp"

namespace vol::net {
    boost::asio::io_context& context() {
        static boost::asio::io_context ioc;
        return ioc;
    }
}