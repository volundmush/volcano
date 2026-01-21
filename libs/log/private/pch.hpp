#pragma once
#include <string>
#include <string_view>
#include <memory>
#include <source_location>
#include <fmt/format.h>
#include <fmt/printf.h>
#include <vector>
#include <filesystem>

#include <spdlog/spdlog.h>
#include <spdlog/common.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/syslog_sink.h>
#include <spdlog/async.h>
#include <spdlog/async_logger.h>