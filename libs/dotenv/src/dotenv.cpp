#include "dotenv/dotenv.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace vol::dotenv {

    static std::string trim(std::string_view input) {
        size_t start = 0;
        size_t end = input.size();

        while (start < end && std::isspace(static_cast<unsigned char>(input[start]))) {
            ++start;
        }
        while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1]))) {
            --end;
        }
        return std::string(input.substr(start, end - start));
    }

    static bool starts_with(std::string_view value, std::string_view prefix) {
        return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
    }

    static std::string strip_quotes(std::string_view value) {
        if (value.size() >= 2) {
            const char first = value.front();
            const char last = value.back();
            if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
                return std::string(value.substr(1, value.size() - 2));
            }
        }
        return std::string(value);
    }

    void LoadResult::merge(const LoadResult& other) {
        loaded += other.loaded;
        skipped += other.skipped;
        errors += other.errors;
        error_messages.insert(error_messages.end(), other.error_messages.begin(), other.error_messages.end());
    }

    static bool set_env_var(const std::string& key, const std::string& value, bool override_existing) {
        if (key.empty()) {
            return false;
        }
        const char* existing = std::getenv(key.c_str());
        if (existing && !override_existing) {
            return false;
        }

    #if defined(_WIN32)
        return _putenv_s(key.c_str(), value.c_str()) == 0;
    #else
        return ::setenv(key.c_str(), value.c_str(), override_existing ? 1 : 0) == 0;
    #endif
    }

    LoadResult load_env_file(const std::filesystem::path& path, bool override_existing) {
        LoadResult result;

        if (!std::filesystem::exists(path)) {
            result.skipped++;
            return result;
        }

        std::ifstream file(path);
        if (!file) {
            result.errors++;
            result.error_messages.push_back("Failed to open " + path.string());
            return result;
        }

        std::string line;
        std::size_t line_no = 0;
        while (std::getline(file, line)) {
            ++line_no;
            auto trimmed = trim(line);
            if (trimmed.empty() || trimmed[0] == '#') {
                continue;
            }

            if (starts_with(trimmed, "export ")) {
                trimmed = trim(trimmed.substr(7));
            }

            const auto eq_pos = trimmed.find('=');
            if (eq_pos == std::string::npos) {
                result.errors++;
                result.error_messages.push_back("Invalid line " + std::to_string(line_no) + " in " + path.string());
                continue;
            }

            auto key = trim(trimmed.substr(0, eq_pos));
            auto value = trim(trimmed.substr(eq_pos + 1));

            if (!key.empty() && key.front() == '"' && key.back() == '"') {
                result.errors++;
                result.error_messages.push_back("Invalid key on line " + std::to_string(line_no) + " in " + path.string());
                continue;
            }

            auto unquoted = strip_quotes(value);

            if (set_env_var(key, unquoted, override_existing)) {
                result.loaded++;
            } else {
                result.skipped++;
            }
        }

        return result;
    }

    LoadResult load_env_files(const std::vector<std::filesystem::path>& paths,
                              bool override_existing_for_all) {
        LoadResult combined;
        for (const auto& path : paths) {
            auto res = load_env_file(path, override_existing_for_all);
            combined.merge(res);
        }
        return combined;
    }

    std::string get_env(std::string_view key, std::string_view fallback) {
        std::string key_str(key);
        const char* value = std::getenv(key_str.c_str());
        if (value && *value) {
            return std::string(value);
        }
        return std::string(fallback);
    }

} // namespace vol::dotenv
