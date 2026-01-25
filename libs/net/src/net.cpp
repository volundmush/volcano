#include "volcano/net/net.hpp"
#include "volcano/net/Server.hpp"

#include <boost/algorithm/string.hpp>

namespace volcano::net {

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

    void run(int numThreads) {
        // the default argument for numThreads is std::thread::hardware_concurrency()
        // the actual thread count will be at least 1, but that's the main thread which
        // will also run the io_context, so we need to create at most numThreads - 1 additional threads
        // all will run context().run()
        std::vector<std::thread> threads;
        for(int i = 1; i < numThreads; ++i) {
            threads.emplace_back([](){
                context().run();
            });
        }
        context().run();
        for(auto& thread : threads) {
            thread.join();
        }
    }
}
