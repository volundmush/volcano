#pragma once
#include <string>
#include <string_view>
#include <memory>
#include <source_location>
#include <fmt/format.h>
#include <fmt/printf.h>

// If you vendor fmt already, prefer using the external fmt in spdlog.
// In CMake: target_compile_definitions(your_target PRIVATE SPDLOG_FMT_EXTERNAL=1)
#include <spdlog/spdlog.h>
#include <spdlog/common.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/syslog_sink.h>
#include <spdlog/async.h>
#include <spdlog/async_logger.h>

namespace dbat::log
{
    struct Options
    {
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
    void init(const Options &opts = {});

    // Change level at runtime (e.g., from an admin command).
    void set_level(int lvl);

    // ---- Primary API (fmt-style, compile-time checked) ----
    template <typename... Args>
    inline void log(std::source_location loc, int lvl,
                    fmt::format_string<Args...> fmtstr,
                    Args &&...args)
    {
        auto *logger = spdlog::default_logger_raw();
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
                            Args &&...args)
    {
        auto *logger = spdlog::default_logger_raw();
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

    // ---- Compatibility shims ----
}

// ---- Handy macros (capture source location automatically) ----
// These mirror spdlog's active-level gating if you set SPDLOG_ACTIVE_LEVEL.
#define LTRACE(...) ::dbat::log::log(std::source_location::current(), SPDLOG_LEVEL_TRACE, __VA_ARGS__)
#define LDEBUG(...) ::dbat::log::log(std::source_location::current(), SPDLOG_LEVEL_DEBUG, __VA_ARGS__)
#define LINFO(...) ::dbat::log::log(std::source_location::current(), SPDLOG_LEVEL_INFO, __VA_ARGS__)
#define LWARN(...) ::dbat::log::log(std::source_location::current(), SPDLOG_LEVEL_WARN, __VA_ARGS__)
#define LERROR(...) ::dbat::log::log(std::source_location::current(), SPDLOG_LEVEL_ERROR, __VA_ARGS__)
#define LCRIT(...) ::dbat::log::log(std::source_location::current(), SPDLOG_LEVEL_CRITICAL, __VA_ARGS__)

// Legacy printf-style logger kept intact (formats with fmt::sprintf)
template <typename... Args>
inline void basic_mud_log_helper(std::source_location loc, fmt::string_view printf_style_fmt, Args &&...args)
{
    try
    {
        std::string line = fmt::sprintf(printf_style_fmt, std::forward<Args>(args)...);
        if (!line.empty())
            // Use compile-time-checked formatting to write the final string
            dbat::log::log(loc, SPDLOG_LEVEL_INFO, "{}", line);
    }
    catch (const std::exception &e)
    {
        dbat::log::log(loc, SPDLOG_LEVEL_ERROR, "SYSERR: Format error in basic_mud_log: {} (template: {})", e.what(), printf_style_fmt);
    }
}

#define basic_mud_log(...) ::basic_mud_log_helper(std::source_location::current(), __VA_ARGS__)
