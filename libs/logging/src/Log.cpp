#include "logging/Log.hpp"
#include <vector>
#include <filesystem>

namespace dbat::log {
static std::shared_ptr<spdlog::logger> make_logger(const Options& o) {
    std::vector<spdlog::sink_ptr> sinks;

    if (o.to_console) {
        sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    }
    if (o.to_file) {
        std::filesystem::create_directories(std::filesystem::path(o.file_path).parent_path());
        sinks.emplace_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            o.file_path, o.max_file_bytes, o.max_files));
    }
#if !defined(_WIN32)
    if (o.to_syslog) {
        sinks.emplace_back(std::make_shared<spdlog::sinks::syslog_sink_mt>("mud", 0, LOG_USER, true));
    }
#endif

    if (o.async) {
        static bool pool_inited = false;
        if (!pool_inited) {
            // Queue size, worker threads
            spdlog::init_thread_pool(1 << 16, 1);
            pool_inited = true;
        }
        auto logger = std::make_shared<spdlog::async_logger>(
            "mud", sinks.begin(), sinks.end(), spdlog::thread_pool(),
            spdlog::async_overflow_policy::overrun_oldest);
        return logger;
    } else {
        return std::make_shared<spdlog::logger>("mud", sinks.begin(), sinks.end());
    }
}

void init(const Options& opts) {
    auto logger = make_logger(opts);

    logger->set_level(static_cast<spdlog::level::level_enum>(opts.level));
    logger->set_pattern(opts.pattern);
    logger->flush_on(static_cast<spdlog::level::level_enum>(opts.flush_on));

    if (opts.enable_backtrace) {
        logger->enable_backtrace(opts.backtrace_lines);
    }
    
    spdlog::register_logger(logger);
    spdlog::set_default_logger(logger);
    
}

void set_level(int lvl) {
    if (auto* lg = spdlog::default_logger_raw()) {
        lg->set_level(static_cast<spdlog::level::level_enum>(lvl));
    }
}

} // namespace mud::log