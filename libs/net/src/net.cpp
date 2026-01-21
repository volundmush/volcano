#include "net/net.hpp"
#include "net/Server.hpp"

#include <boost/algorithm/string.hpp>

namespace vol::net {

    TlsConfig tls_config;
    Config tcp_config;

    std::expected<boost::asio::ip::address, boost::system::error_code> parse_address(std::string_view addr_str) {
        if(boost::iequals(addr_str, "any") || boost::iequals(addr_str, "*")) {
            return boost::asio::ip::address_v6::any();
        }
        try {
            auto address = boost::asio::ip::make_address(std::string(addr_str));
            return address;
        } catch(const std::exception& e) {
            boost::system::error_code ec = boost::asio::error::invalid_argument;
            return std::unexpected(ec);
        }
    }

    std::expected<std::shared_ptr<boost::asio::ssl::context>, std::string> create_ssl_context(std::filesystem::path cert_path, std::filesystem::path key_path) {
        try {
            if(cert_path.empty() || key_path.empty()) {
                return std::unexpected("Certificate path or key path is empty.");
            }
            if(!std::filesystem::exists(cert_path)) {
                return std::unexpected("Certificate file does not exist: " + cert_path.string());
            }
            if(!std::filesystem::exists(key_path)) {
                return std::unexpected("Key file does not exist: " + key_path.string());
            }
            auto ssl_context = std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tls_server);
            ssl_context->set_options(
                boost::asio::ssl::context::default_workarounds |
                boost::asio::ssl::context::no_sslv2 |
                boost::asio::ssl::context::no_sslv3 |
                boost::asio::ssl::context::single_dh_use
            );
            ssl_context->use_certificate_chain_file(cert_path.string());
            ssl_context->use_private_key_file(key_path.string(), boost::asio::ssl::context::pem);
            return ssl_context;
        } catch (const std::exception& e) {
            return std::unexpected(std::string("Failed to initialize TLS context: ") + e.what());
        }
    }

    std::vector<std::shared_ptr<Server>> servers;

    void bind_server(boost::asio::ip::address address, uint16_t port, std::shared_ptr<boost::asio::ssl::context> tls_context, std::function<boost::asio::awaitable<void>(AnyStream&&)> handle_client) {
        auto &src = servers.emplace_back(std::make_shared<Server>(address, port, tls_context, std::move(handle_client)));
        src->run();
        return;
    }

}
