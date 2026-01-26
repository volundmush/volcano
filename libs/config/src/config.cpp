#include "volcano/config/config.hpp"
#include "volcano/dotenv/dotenv.hpp"
#include "volcano/log/Log.hpp"
#include "volcano/net/net.hpp"
#include <cstdlib>
#include <stdexcept>

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
            throw std::runtime_error(std::string("Invalid ") + std::string(label) + ": " + value);
        };

        auto parse_address_env = [&](const char* key, auto& out) {
            if (const char* host = get_env(key)) {
                auto parsed = volcano::net::parse_address(host);
                if (!parsed) {
                    throw std::runtime_error(std::string("Invalid ") + key + ": " + host);
                }
                out = *parsed;
            }
        };

        auto parse_address_list = [&](const char* key, std::vector<boost::asio::ip::address>& out) {
            if (const char* value = get_env(key)) {
                std::vector<boost::asio::ip::address> parsed_list;
                std::string token;
                auto push_token = [&]() {
                    if (token.empty()) {
                        return;
                    }
                    auto parsed = volcano::net::parse_address(token);
                    if (!parsed) {
                        throw std::runtime_error(std::string("Invalid ") + key + " entry: " + token);
                    }
                    parsed_list.push_back(*parsed);
                    token.clear();
                };

                for (const char ch : std::string(value)) {
                    if (ch == ',' || ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
                        push_token();
                    } else {
                        token.push_back(ch);
                    }
                }
                push_token();

                if (parsed_list.empty()) {
                    throw std::runtime_error(std::string("Invalid ") + key + ": empty list");
                }
                out = std::move(parsed_list);
            }
        };

        Config cfg{};

        parse_address_env("HTTP_HOST", cfg.http.address);
        if (auto port = parse_port(get_env("HTTP_PORT"), "HTTP_PORT")) {
            cfg.http.port = *port;
        }

        parse_address_env("HTTPS_HOST", cfg.https.address);
        if (auto port = parse_port(get_env("HTTPS_PORT"), "HTTPS_PORT")) {
            cfg.https.port = *port;
        }

        parse_address_env("TELNET_HOST", cfg.telnet.address);
        if (auto port = parse_port(get_env("TELNET_PORT"), "TELNET_PORT")) {
            cfg.telnet.port = *port;
        }

        parse_address_env("TELNETS_HOST", cfg.telnets.address);
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
                throw std::runtime_error(std::string("Invalid JWT_EXPIRY_MINUTES: ") + exp);
            }
        }
        if (const char* exp = get_env("JWT_REFRESH_EXPIRY_MINUTES")) {
            char* end = nullptr;
            const long value = std::strtol(exp, &end, 10);
            if (end != exp && value > 0) {
                cfg.jwt.refresh_expiry_minutes = static_cast<int>(value);
            } else {
                throw std::runtime_error(std::string("Invalid JWT_REFRESH_EXPIRY_MINUTES: ") + exp);
            }
        }
        if (const char* iss = get_env("JWT_ISSUER")) {
            cfg.jwt.issuer = iss;
        }
        if (const char* aud = get_env("JWT_AUDIENCE")) {
            cfg.jwt.audience = aud;
        }

        if (const char* server_address = get_env("SERVER_ADDRESS")) {
            cfg.server_address = server_address;
        }

        parse_address_list("TRUSTED_PROXIES", cfg.trusted_proxies);

        return cfg;
    }
}
