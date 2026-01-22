#include "volcano/mud/Command.hpp"
#include <regex>
#include <boost/algorithm/string.hpp>

namespace volcano::mud {
    static std::regex cmd_regex(R"(^([A-Za-z0-9-.]+)(?:\/(([A-Za-z0-9-.]+)(?:\/([A-Za-z0-9-.]+)){0,}))?(?:\:([A-Za-z0-9-.]+))?(?:\s+(.*)?)?)", std::regex::icase);

CommandData::CommandData(std::string_view txt_in) : _original(std::make_unique<std::string>(txt_in)) {
    // Take ownership of a copy of the input, then we'll make everything else
    // a view to it.
    original = std::string_view(_original->data(), _original->size());

    std::cmatch m; // match_results<const char*>
    if (std::regex_match(_original->data(), _original->data() + _original->size(), m, cmd_regex)) {
        auto to_sv = [](const std::cmatch& cm, size_t i) -> std::string_view {
            return cm[i].matched ? std::string_view(cm[i].first, cm[i].length()) : std::string_view{};
        };
        cmd = to_sv(m, 1);
        std::string_view switch_part = to_sv(m, 2);
        if(!switch_part.empty())
            boost::split(switches, switch_part, boost::is_any_of("/"), boost::token_compress_on);
        switch_mod  = to_sv(m, 5);
        argument    = to_sv(m, 6);
        argument = boost::trim_copy(argument);
        boost::split(arguments, argument, boost::is_space(), boost::token_compress_on);
        equals_present = boost::icontains(argument, "=");
        if(equals_present) {
            auto pos = argument.find('=');
            lsargs = argument.substr(0, pos);
            rsargs = argument.substr(pos + 1);
        }
        if(!lsargs.empty()) {
            boost::split(lhslist, lsargs, boost::is_space(), boost::token_compress_on);
            boost::split(lhscomm, lsargs, boost::is_any_of(","), boost::token_compress_on);
        }
        if(!rsargs.empty()) {
            boost::split(rhslist, rsargs, boost::is_space(), boost::token_compress_on);
            boost::split(rhscomm, rsargs, boost::is_any_of(","), boost::token_compress_on);
        }
    }

}
}