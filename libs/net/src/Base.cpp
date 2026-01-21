#include "volcano/net/Base.hpp"

namespace volcano::net {
    boost::asio::io_context& context() {
        static boost::asio::io_context ioc;
        return ioc;
    }
}