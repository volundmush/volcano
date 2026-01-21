#include <net/Connection.hpp>

namespace vol::net {

AnyStream::AnyStream(int64_t id, TcpStream stream) : stream_(std::move(stream)), id_(id) {}

AnyStream::AnyStream(int64_t id, TlsStream stream) : stream_(std::move(stream)), id_(id) {}

bool AnyStream::is_tls() const {
    return std::holds_alternative<TlsStream>(stream_);
}

int64_t AnyStream::id() const {
    return id_;
}

AnyStream::executor_type AnyStream::get_executor() {
    if (auto* tcp = std::get_if<TcpStream>(&stream_)) {
        return tcp->get_executor();
    }
    return std::get<TlsStream>(stream_).get_executor();
}

AnyStream::executor_type AnyStream::get_executor() const {
    return const_cast<AnyStream*>(this)->get_executor();
}

AnyStream::lowest_layer_type& AnyStream::lowest_layer() {
    if (auto* tcp = std::get_if<TcpStream>(&stream_)) {
        return tcp->lowest_layer();
    }
    return std::get<TlsStream>(stream_).lowest_layer();
}

AnyStream::lowest_layer_type& AnyStream::lowest_layer() const {
    return const_cast<AnyStream*>(this)->lowest_layer();
}

} // namespace vol::net
