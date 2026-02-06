#pragma once
#include <memory>
#include <vector>
#include <string_view>
#include <unordered_map>
#include <stdexcept>
#include <fmt/format.h>
#include <fmt/ranges.h>

namespace volcano::mud {
    class CommandData {
    std::shared_ptr<const std::string> _original;

    public:
    CommandData() = default;
    CommandData(std::string_view command_line);

    std::string_view original;
    std::string_view cmd;
    std::vector<std::string_view> switches;
    std::string_view switch_mod;
    std::string_view argument;
    std::vector<std::string_view> arguments;
    std::string_view lsargs;
    std::string_view lstrim;
    std::string_view rsargs;
    std::string_view rstrim;
    std::vector<std::string_view> args, lhslist, rhslist, lhscomm, rhscomm;
    bool equals_present;
    std::unordered_map<std::string, std::string> variables;
};

auto inline format_as(const CommandData& cmd_data) {
    return fmt::format("CommandData(cmd='{}', switches=[{}], switch_mod='{}', argument='{}', lsargs='{}', rsargs='{}')",
        cmd_data.cmd,
        fmt::join(cmd_data.switches, ", "),
        cmd_data.switch_mod,
        cmd_data.argument,
        cmd_data.lsargs,
        cmd_data.rsargs);
}

class CommandError : public std::runtime_error {
public:
    explicit CommandError(const std::string& message) : std::runtime_error(message) {}
};
}