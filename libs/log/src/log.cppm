module;

#include <filesystem>
#include <memory>
#include <source_location>
#include <string>
#include <utility>
#include <vector>


#include <spdlog/spdlog.h>
#include <spdlog/common.h>
#include <spdlog/async.h>
#include <spdlog/async_logger.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/syslog_sink.h>

#if !defined(_WIN32)
#include <syslog.h>
#endif

export module volcano.log;

export namespace volcano::log {
    struct Options {
        // Where to write logs:
        bool to_console = true;
        bool to_file = true;
        std::string file_path = "logs/mud.log";
        std::size_t max_file_bytes = 5 * 1024 * 1024; // 5MB
        std::size_t max_files = 3;
        bool to_syslog = false; // *nix only

        // Behavior:
        bool async = true;
        int level = SPDLOG_LEVEL_INFO;
        int flush_on = SPDLOG_LEVEL_WARN;
        std::string pattern = "[%Y-%m-%d %H:%M:%S.%e] [%t] [%^%l%$] [%s:%#] %v";
        bool enable_backtrace = true;
        int backtrace_lines = 64;
    };

    // Must be called once at startup (safe to call again; it re-initializes).
    void init(const Options& opts = {});

    // Change level at runtime (e.g., from an admin command).
    void set_level(int lvl);

    // ---- Primary API (fmt-style, compile-time checked) ----
    template <typename... Args>
    inline void log(std::source_location loc, int lvl,
                    fmt::format_string<Args...> fmtstr,
                    Args&&... args)
    {
        auto* logger = spdlog::default_logger_raw();
        if (!logger) [[unlikely]]
            return;

        // Skip formatting if level is disabled:
        if (!logger->should_log(static_cast<spdlog::level::level_enum>(lvl)))
            return;

        logger->log(
            spdlog::source_loc{loc.file_name(), static_cast<int>(loc.line()), loc.function_name()},
            static_cast<spdlog::level::level_enum>(lvl),
            fmtstr, std::forward<Args>(args)...);
    }

    // Runtime format string (keeps your current call sites happy)
    template <typename... Args>
    inline void log_runtime(std::source_location loc, int lvl,
                            fmt::string_view fmt_str_or_msg,
                            Args&&... args)
    {
        auto* logger = spdlog::default_logger_raw();
        if (!logger) [[unlikely]]
            return;

        if (!logger->should_log(static_cast<spdlog::level::level_enum>(lvl)))
            return;

        logger->log(
            spdlog::source_loc{loc.file_name(), static_cast<int>(loc.line()), loc.function_name()},
            static_cast<spdlog::level::level_enum>(lvl),
            fmt::runtime(fmt_str_or_msg),
            std::forward<Args>(args)...);
    }
} // namespace volcano::log

// ---- Handy inline functions (capture source location automatically) ----
export template <typename... Args>
inline void LTRACE(fmt::format_string<Args...> fmtstr, Args&&... args)
{
    ::volcano::log::log(std::source_location::current(), SPDLOG_LEVEL_TRACE, fmtstr, std::forward<Args>(args)...);
}

export template <typename... Args>
inline void LDEBUG(fmt::format_string<Args...> fmtstr, Args&&... args)
{
    ::volcano::log::log(std::source_location::current(), SPDLOG_LEVEL_DEBUG, fmtstr, std::forward<Args>(args)...);
}

export template <typename... Args>
inline void LINFO(fmt::format_string<Args...> fmtstr, Args&&... args)
{
    ::volcano::log::log(std::source_location::current(), SPDLOG_LEVEL_INFO, fmtstr, std::forward<Args>(args)...);
}

export template <typename... Args>
inline void LWARN(fmt::format_string<Args...> fmtstr, Args&&... args)
{
    ::volcano::log::log(std::source_location::current(), SPDLOG_LEVEL_WARN, fmtstr, std::forward<Args>(args)...);
}

export template <typename... Args>
inline void LERROR(fmt::format_string<Args...> fmtstr, Args&&... args)
{
    ::volcano::log::log(std::source_location::current(), SPDLOG_LEVEL_ERROR, fmtstr, std::forward<Args>(args)...);
}

export template <typename... Args>
inline void LCRIT(fmt::format_string<Args...> fmtstr, Args&&... args)
{
    ::volcano::log::log(std::source_location::current(), SPDLOG_LEVEL_CRITICAL, fmtstr, std::forward<Args>(args)...);
}

namespace volcano::log {
    namespace detail {
        std::shared_ptr<spdlog::logger> make_logger(const Options& o) {
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
            }
            return std::make_shared<spdlog::logger>("mud", sinks.begin(), sinks.end());
        }
    } // namespace detail

    void init(const Options& opts) {
        auto logger = detail::make_logger(opts);

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
} // namespace volcano::log
