#include "jwt/jwt.hpp"
#include <nlohmann/json.hpp>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <chrono>
#include <string_view>

namespace {
    std::string hmacSha256(std::string_view data, std::string_view secret) {
        unsigned int length = 0;
        unsigned char digest[EVP_MAX_MD_SIZE];
        HMAC(EVP_sha256(),
             reinterpret_cast<const unsigned char*>(secret.data()),
             static_cast<int>(secret.size()),
             reinterpret_cast<const unsigned char*>(data.data()),
             data.size(),
             digest,
             &length);
        return std::string(reinterpret_cast<const char*>(digest), length);
    }
}

namespace dbat::jwt {

    JwtConfig jwt_config;

    constexpr std::string_view jwt_header = R"({"alg":"HS256","typ":"JWT"})";

    std::string create(const nlohmann::json& payload, std::string_view secret, std::chrono::seconds expiration) {
        nlohmann::json payload_with_exp = payload;
        const auto now = std::chrono::system_clock::now();
        const auto exp_time = now + expiration;
        const auto exp_seconds = std::chrono::duration_cast<std::chrono::seconds>(exp_time.time_since_epoch()).count();
        payload_with_exp["exp"] = exp_seconds;

        const std::string header_part = base64UrlEncode(std::string(jwt_header));
        const std::string payload_part = base64UrlEncode(payload_with_exp.dump());
        const std::string signing_input = header_part + "." + payload_part;
        const std::string signature = hmacSha256(signing_input, secret);
        const std::string signature_part = base64UrlEncode(signature);

        return signing_input + "." + signature_part;
    }

    std::expected<nlohmann::json, std::string> verify(std::string_view token, std::string_view secret) {
        const auto first_dot = token.find('.');
        if (first_dot == std::string_view::npos) {
            return std::unexpected("invalid token format");
        }
        const auto second_dot = token.find('.', first_dot + 1);
        if (second_dot == std::string_view::npos || token.find('.', second_dot + 1) != std::string_view::npos) {
            return std::unexpected("invalid token format");
        }

        auto header_part = token.substr(0, first_dot);
        auto payload_part = token.substr(first_dot + 1, second_dot - first_dot - 1);
        auto signature_part = token.substr(second_dot + 1);

        const std::string signing_input = std::string(header_part) + "." + std::string(payload_part);
        auto expected_signature = base64UrlEncode(hmacSha256(signing_input, secret));

        if (expected_signature.size() != signature_part.size() ||
            CRYPTO_memcmp(expected_signature.data(), signature_part.data(), expected_signature.size()) != 0) {
            return std::unexpected("invalid token signature");
        }

        auto decoded_header = base64UrlDecode(header_part);
        if (decoded_header.empty() && !header_part.empty()) {
            return std::unexpected("invalid token header");
        }

        const std::string decoded_payload = base64UrlDecode(payload_part);
        if (decoded_payload.empty() && !payload_part.empty()) {
            return std::unexpected("invalid token payload");
        }

        nlohmann::json header_json;
        nlohmann::json payload_json;
        try {
            header_json = nlohmann::json::parse(decoded_header);
        } catch (const std::exception& ex) {
            return std::unexpected(std::string("invalid token header json: ") + ex.what());
        }

        if (header_json.value("alg", "") != "HS256" || header_json.value("typ", "") != "JWT") {
            return std::unexpected("unsupported token header");
        }

        try {
            payload_json = nlohmann::json::parse(decoded_payload);
        } catch (const std::exception& ex) {
            return std::unexpected(std::string("invalid token payload json: ") + ex.what());
        }

        if (payload_json.contains("exp") && payload_json["exp"].is_number_integer()) {
            const auto exp_seconds = payload_json["exp"].get<long long>();
            const auto now_seconds = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            if (now_seconds >= exp_seconds) {
                return std::unexpected("token expired");
            }
        }

        return payload_json;
    }

    std::string base64UrlEncode(std::string_view input) {
        if (input.empty()) {
            return {};
        }

        const auto encoded_length = static_cast<int>(4 * ((input.size() + 2) / 3));
        std::string encoded;
        encoded.resize(static_cast<size_t>(encoded_length));

        const int output_length = EVP_EncodeBlock(
            reinterpret_cast<unsigned char*>(encoded.data()),
            reinterpret_cast<const unsigned char*>(input.data()),
            static_cast<int>(input.size()));

        if (output_length <= 0) {
            return {};
        }

        encoded.resize(static_cast<size_t>(output_length));
        for (char& ch : encoded) {
            if (ch == '+') {
                ch = '-';
            } else if (ch == '/') {
                ch = '_';
            }
        }

        while (!encoded.empty() && encoded.back() == '=') {
            encoded.pop_back();
        }

        return encoded;
    }

    std::string base64UrlDecode(std::string_view input) {
        if (input.empty()) {
            return {};
        }

        std::string padded = std::string(input);
        for (char& ch : padded) {
            if (ch == '-') {
                ch = '+';
            } else if (ch == '_') {
                ch = '/';
            }
        }

        const auto mod = padded.size() % 4;
        if (mod == 2) {
            padded.append("==");
        } else if (mod == 3) {
            padded.append("=");
        } else if (mod == 1) {
            return {};
        }

        size_t padding = 0;
        if (!padded.empty()) {
            if (padded.back() == '=') {
                padding++;
                if (padded.size() >= 2 && padded[padded.size() - 2] == '=') {
                    padding++;
                }
            }
        }

        std::string decoded;
        decoded.resize((padded.size() / 4) * 3);
        const int output_length = EVP_DecodeBlock(
            reinterpret_cast<unsigned char*>(decoded.data()),
            reinterpret_cast<const unsigned char*>(padded.data()),
            static_cast<int>(padded.size()));

        if (output_length < 0) {
            return {};
        }

        int adjusted_length = output_length - static_cast<int>(padding);
        if (adjusted_length < 0) {
            adjusted_length = 0;
        }
        decoded.resize(static_cast<size_t>(adjusted_length));
        return decoded;
    }

        std::int64_t now_seconds() {
            return static_cast<std::int64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
        }

        nlohmann::json base_claims(nlohmann::json& claims) {
            claims["iat"] = now_seconds();
            claims["iss"] = jwt_config.issuer;
            claims["aud"] = jwt_config.audience;
            return claims;
        }

        std::string create_access_token(nlohmann::json&& claims) {
            base_claims(claims);
            claims["token_use"] = "access";
            return dbat::jwt::create(claims, jwt_config.secret, jwt_config.token_expiry);
        }

        std::string create_refresh_token(nlohmann::json&& claims) {
            base_claims(claims);
            claims["token_use"] = "refresh";
            return dbat::jwt::create(claims, jwt_config.secret, jwt_config.refresh_token_expiry);
        }

        nlohmann::json build_token_response(std::string_view access_token, std::string_view refresh_token) {
            nlohmann::json response_json;
            response_json["token_type"] = "Bearer";
            response_json["access_token"] = access_token;
            response_json["refresh_token"] = refresh_token;
            response_json["expires_in"] = static_cast<int>(jwt_config.token_expiry.count());
            return response_json;
        }
}