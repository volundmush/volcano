#pragma once

#include <string>
#include <chrono>
#include <expected>
#include <nlohmann/json_fwd.hpp>

namespace dbat::jwt {

      std::string base64UrlEncode(std::string_view input);
      std::string base64UrlDecode(std::string_view input);

      class JwtContext {
      public:
          std::string secret;
          std::chrono::seconds token_expiry{3600};
          std::chrono::seconds refresh_token_expiry{std::chrono::hours(24 * 7)};
          std::string issuer{"volcano"};
          std::string audience{"volcano-client"};

          std::string create(const nlohmann::json& payload, std::chrono::seconds expiration) const;
          std::expected<nlohmann::json, std::string> verify(std::string_view token) const;

          std::string create_access_token(nlohmann::json&& claims) const;
          std::string create_refresh_token(nlohmann::json&& claims) const;
          nlohmann::json build_token_response(std::string_view access_token, std::string_view refresh_token) const;

      private:
          static std::int64_t now_seconds();
          nlohmann::json base_claims(nlohmann::json& claims) const;
      };

} // namespace dbat::jwt