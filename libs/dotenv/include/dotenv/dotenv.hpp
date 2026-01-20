#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace vol::dotenv {

    struct LoadResult {
        std::size_t loaded{0};
        std::size_t skipped{0};
        std::size_t errors{0};
        std::vector<std::string> error_messages;

        void merge(const LoadResult& other);
    };

    // Load a single .env file if it exists.
    // If override_existing is true, values overwrite existing environment variables.
    LoadResult load_env_file(const std::filesystem::path& path, bool override_existing = false);

    // Load multiple .env files in order; later files can override earlier ones.
    LoadResult load_env_files(const std::vector<std::filesystem::path>& paths,
                              bool override_existing_for_all = false);

    // Utility for reading an env var with optional fallback.
    std::string get_env(std::string_view key, std::string_view fallback = {});

} // namespace vol::dotenv
