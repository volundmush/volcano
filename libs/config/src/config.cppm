module;

#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <volcano/net/net.hpp>

export module volcano.config;

export import volcano.dotenv;
import volcano.log;

export namespace volcano::config {

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
    };

    Config init(std::string_view log_file);

} // namespace volcano::config

namespace volcano::config {
    Config init(std::string_view log_file) {
        auto log_options = volcano::log::Options();
        log_options.file_path = "logs/" + std::string(log_file) + ".log";
        volcano::log::init(log_options);
        volcano::dotenv::load_env_file(".env", false);
        volcano::dotenv::load_env_file(".env.local", true);

        auto get_env = [](const char* key) -> const char* {
            const char* value = std::getenv(key);
            return (value && *value) ? value : nullptr;
        };

        auto parse_port = [](const char* value, std::string_view label) -> std::optional<uint16_t> {
            if (!value) {
                return std::nullopt;
            }
            char* end = nullptr;
            const long parsed = std::strtol(value, &end, 10);
            if (end != value && parsed > 0 && parsed <= 65535) {
                return static_cast<uint16_t>(parsed);
            }
            LERROR("Invalid %s: %s", std::string(label).c_str(), value);
            return std::nullopt;
        };

        Config cfg{};

        if (const char* host = get_env("HTTP_HOST")) {
            cfg.http.address = boost::asio::ip::make_address(host);
        }
        if (auto port = parse_port(get_env("HTTP_PORT"), "HTTP_PORT")) {
            cfg.http.port = *port;
        }

        if (const char* host = get_env("HTTPS_HOST")) {
            cfg.https.address = boost::asio::ip::make_address(host);
        }
        if (auto port = parse_port(get_env("HTTPS_PORT"), "HTTPS_PORT")) {
            cfg.https.port = *port;
        }

        if (const char* host = get_env("TELNET_HOST")) {
            cfg.telnet.address = boost::asio::ip::make_address(host);
        }
        if (auto port = parse_port(get_env("TELNET_PORT"), "TELNET_PORT")) {
            cfg.telnet.port = *port;
        }

        if (const char* host = get_env("TELNETS_HOST")) {
            cfg.telnets.address = boost::asio::ip::make_address(host);
        }
        if (auto port = parse_port(get_env("TELNETS_PORT"), "TELNETS_PORT")) {
            cfg.telnets.port = *port;
        }

        if (const char* cert = get_env("TLS_CERT_FILE")) {
            cfg.tls.cert_path = cert;
        }
        if (const char* key = get_env("TLS_KEY_FILE")) {
            cfg.tls.key_path = key;
        }

        if (const char* secret = get_env("JWT_SECRET")) {
            cfg.jwt.secret = secret;
        }
        if (const char* exp = get_env("JWT_EXPIRY_MINUTES")) {
            char* end = nullptr;
            const long value = std::strtol(exp, &end, 10);
            if (end != exp && value > 0) {
                cfg.jwt.expiry_minutes = static_cast<int>(value);
            } else {
                LERROR("Invalid JWT_EXPIRY_MINUTES: %s", exp);
            }
        }
        if (const char* exp = get_env("JWT_REFRESH_EXPIRY_MINUTES")) {
            char* end = nullptr;
            const long value = std::strtol(exp, &end, 10);
            if (end != exp && value > 0) {
                cfg.jwt.refresh_expiry_minutes = static_cast<int>(value);
            } else {
                LERROR("Invalid JWT_REFRESH_EXPIRY_MINUTES: %s", exp);
            }
        }
        if (const char* iss = get_env("JWT_ISSUER")) {
            cfg.jwt.issuer = iss;
        }
        if (const char* aud = get_env("JWT_AUDIENCE")) {
            cfg.jwt.audience = aud;
        }

        return cfg;
    }
}
