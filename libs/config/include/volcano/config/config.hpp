#pragma once
#include <string>
#include <string_view>
#include <cstdint>
#include <filesystem>
#include <vector>
#include <volcano/net/net.hpp>

namespace volcano::config {

    struct EndPointConfig {
        boost::asio::ip::address address = boost::asio::ip::address_v6::any();
        uint16_t port{0};
    };

    struct TlsContext {
        std::filesystem::path cert_path;
        std::filesystem::path key_path;
    };

    struct JwtSecret {
        std::string secret;
        int expiry_minutes{60};
        int refresh_expiry_minutes{10080}; // 7 days
        std::string issuer{"volcano-server"};
        std::string audience{"volcano-client"};
    };

    struct Config {
        EndPointConfig http;
        EndPointConfig https;
        EndPointConfig telnet;
        EndPointConfig telnets;
        TlsContext tls;
        JwtSecret jwt;
        std::string server_address;
        std::vector<boost::asio::ip::address> trusted_proxies{
            boost::asio::ip::address_v4::loopback(),
            boost::asio::ip::address_v6::loopback()
        };
    };

    Config init(std::string_view log_file);
}
