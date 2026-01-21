#pragma once
#include <memory>
#include <vector>
#include <string_view>
#include <unordered_map>
#include <stdexcept>

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
    std::string_view rsargs;
    std::vector<std::string_view> args, lhslist, rhslist, lhscomm, rhscomm;
    bool equals_present;
    std::unordered_map<std::string, std::string> variables;
};

class CommandError : public std::runtime_error {
public:
    explicit CommandError(const std::string& message) : std::runtime_error(message) {}
};
}