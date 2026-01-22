module;

#include <memory>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <boost/algorithm/string.hpp>

export module volcano.mud;

export namespace volcano::mud {

    class CommandData {
        std::unique_ptr<const std::string> _original;

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
        bool equals_present{false};
        std::unordered_map<std::string, std::string> variables;
    };

    class CommandError : public std::runtime_error {
    public:
        explicit CommandError(const std::string& message) : std::runtime_error(message) {}
    };

} // namespace volcano::mud

namespace volcano::mud {
    static std::regex cmd_regex(R"(^([A-Za-z0-9-.]+)(?:\/(([A-Za-z0-9-.]+)(?:\/([A-Za-z0-9-.]+)){0,}))?(?:\:([A-Za-z0-9-.]+))?(?:\s+(.*)?)?)", std::regex::icase);

    CommandData::CommandData(std::string_view txt_in) : _original(std::make_unique<std::string>(txt_in)) {
        original = std::string_view(_original->data(), _original->size());

        std::cmatch m;
        if (std::regex_match(_original->data(), _original->data() + _original->size(), m, cmd_regex)) {
            auto to_sv = [](const std::cmatch& cm, size_t i) -> std::string_view {
                return cm[i].matched ? std::string_view(cm[i].first, cm[i].length()) : std::string_view{};
            };
            cmd = to_sv(m, 1);
            std::string_view switch_part = to_sv(m, 2);
            if (!switch_part.empty())
                boost::split(switches, switch_part, boost::is_any_of("/"), boost::token_compress_on);
            switch_mod = to_sv(m, 5);
            argument = to_sv(m, 6);
            argument = boost::trim_copy(argument);
            boost::split(arguments, argument, boost::is_space(), boost::token_compress_on);
            equals_present = boost::icontains(argument, "=");
            if (equals_present) {
                auto pos = argument.find('=');
                lsargs = argument.substr(0, pos);
                rsargs = argument.substr(pos + 1);
            }
            if (!lsargs.empty()) {
                boost::split(lhslist, lsargs, boost::is_space(), boost::token_compress_on);
                boost::split(lhscomm, lsargs, boost::is_any_of(","), boost::token_compress_on);
            }
            if (!rsargs.empty()) {
                boost::split(rhslist, rsargs, boost::is_space(), boost::token_compress_on);
                boost::split(rhscomm, rsargs, boost::is_any_of(","), boost::token_compress_on);
            }
        }
    }
}
