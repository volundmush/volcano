#pragma once

#include <string>
#include <chrono>
#include <expected>
#include <nlohmann/json_fwd.hpp>

namespace dbat::jwt {
     std::string create(const nlohmann::json& payload, std::string_view secret, std::chrono::seconds expiration = std::chrono::seconds(3600));

     std::expected<nlohmann::json, std::string> verify(std::string_view token, std::string_view secret);
     
     std::string base64UrlEncode(std::string_view input);
     std::string base64UrlDecode(std::string_view input);

     struct JwtConfig {
        std::string secret;
        std::chrono::seconds token_expiry{3600};
        std::chrono::seconds refresh_token_expiry{std::chrono::hours(24 * 7)};
        std::string issuer{"volcano"};
        std::string audience{"volcano-client"};
    };

    extern JwtConfig jwt_config;

    std::string create_access_token(nlohmann::json&& claims);
    std::string create_refresh_token(nlohmann::json&& claims);
    nlohmann::json build_token_response(std::string_view access_token, std::string_view refresh_token);


} // namespace dbat::jwt