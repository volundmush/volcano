module;

#include <algorithm>
#include <charconv>
#include <expected>
#include <functional>
#include <format>
#include <memory>
#include <ranges>
#include <regex>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <enchantum/enchantum.hpp>
#include <fmt/format.h>

export module volcano.util;

export namespace volcano::util {

    // ----- util.hpp -----
    inline std::string_view trim_left(std::string_view input) {
        while (!input.empty() && (input.front() == ' ' || input.front() == '\t' || input.front() == '\n' || input.front() == '\r')) {
            input.remove_prefix(1);
        }
        return input;
    }

    inline std::string_view trim_right(std::string_view input) {
        while (!input.empty() && (input.back() == ' ' || input.back() == '\t' || input.back() == '\n' || input.back() == '\r')) {
            input.remove_suffix(1);
        }
        return input;
    }

    inline std::string_view trim(std::string_view input) {
        return trim_right(trim_left(input));
    }

    // ----- FilterWeak.hpp -----
    template <class Range>
    auto filter_shared(Range&& container) {
        using std::views::filter;
        using std::views::transform;

        return std::forward<Range>(container) | filter([](auto& w) { return !w.expired(); }) |
               transform([](auto& w) { return w.lock(); });
    }

    template <class Range>
    auto filter_raw(Range&& container) {
        using std::views::filter;
        using std::views::transform;

        return std::forward<Range>(container) | filter([](auto& w) { return !w.expired(); }) |
               transform([](auto& w) { return w.lock().get(); });
    }

    // ----- PartialMatch.hpp -----
    struct default_key_t {
        template <class T>
        std::string operator()(const T& v) const {
            using U = std::decay_t<T>;
            if constexpr (std::is_same_v<U, std::string>) {
                return v;
            } else if constexpr (std::is_arithmetic_v<U>) {
                return std::to_string(v);
            } else if constexpr (requires { v.first; }) {
                return (*this)(v.first);
            } else if constexpr (requires { std::string{v}; }) {
                return std::string{v};
            } else {
                static_assert(sizeof(T) == 0,
                              "No default key for this type. Provide a key extractor returning std::string.");
            }
        }
    };

    template <class Range, class KeyFn = default_key_t>
    auto partialMatch(std::string_view match_text, Range&& range, bool exact = false, KeyFn key = {})
        -> std::expected<std::ranges::iterator_t<Range>, std::string>
    {
        using std::begin;
        using std::end;
        using It = std::ranges::iterator_t<Range>;

        std::vector<std::pair<std::string, It>> items;
        for (It it = begin(range); it != end(range); ++it) {
            items.emplace_back(key(*it), it);
        }

        std::sort(items.begin(), items.end(), [](auto& a, auto& b) { return a.first < b.first; });

        for (auto& [k, it] : items) {
            if (boost::iequals(k, match_text) || (!exact && boost::istarts_with(k, match_text))) {
                return it;
            }
        }

        std::string choices;
        for (size_t i = 0; i < items.size(); ++i) {
            if (i)
                choices += ", ";
            choices += items[i].first;
        }

        return std::unexpected("Choices are: " + choices);
    }

    // ----- Parse.hpp -----
    inline const std::regex parseRangeRegex(R"(^(\d+)(-(\d+))?$)", std::regex::icase);

    template <typename T, typename Container = std::vector<T>>
        requires std::is_arithmetic_v<T>
    std::expected<Container, std::string> parseRanges(std::string_view txt) {
        Container out;
        std::smatch match;

        std::vector<std::string> parts;
        boost::split(parts, txt, boost::is_any_of(" "), boost::token_compress_on);

        for (const auto& part : parts) {
            if (!std::regex_search(part, match, parseRangeRegex)) {
                return std::unexpected(std::format("Invalid range part: '{}'", part));
            }

            int first = std::stoi(match[1].str());
            if (match[3].matched) {
                int last = std::stoi(match[3].str());
                for (int i = first; i <= last; ++i) {
                    out.push_back(i);
                }
            } else {
                out.push_back(first);
            }
        }

        return out;
    }

    template <typename T = int>
        requires std::is_arithmetic_v<T>
    std::expected<T, std::string> parseNumber(std::string_view arg, std::string_view context, T min_value = T{}) {
        if (arg.empty()) {
            return std::unexpected(std::format("No {} provided.\r\n", context));
        }

        T value{};
        auto [ptr, ec] = std::from_chars(arg.data(), arg.data() + arg.size(), value);

        if (ec == std::errc::invalid_argument) {
            return std::unexpected(std::format("Invalid {}: {}\r\n", context, arg));
        } else if (ec == std::errc::result_out_of_range) {
            return std::unexpected(std::format("{} out of range: {}\r\n", context, arg));
        } else if (ptr != arg.data() + arg.size()) {
            return std::unexpected(std::format("Invalid trailing characters in {}: {}\r\n", context, arg));
        }

        if (value < min_value) {
            return std::unexpected(std::format("{} must be at least {}\r\n", context, min_value));
        }

        return value;
    }

    // ----- Enum.hpp -----
    template <typename FlagEnum, typename MapType = std::unordered_map<std::string, FlagEnum>>
        requires std::is_enum_v<FlagEnum>
    auto getEnumMap(const std::function<bool(FlagEnum v)>& filter = {}) {
        MapType flag_map;
        for (const auto& [val, name] : enchantum::entries<FlagEnum>) {
            if (filter && !filter(val))
                continue;
            flag_map[std::string(name)] = val;
        }
        return flag_map;
    }

    template <typename FlagEnum, typename ListType = std::vector<FlagEnum>>
        requires std::is_enum_v<FlagEnum>
    auto getEnumList(const std::function<bool(FlagEnum v)>& filter = {}) {
        ListType flag_list;
        for (const auto& [val, name] : enchantum::entries<FlagEnum>) {
            if (filter && !filter(val))
                continue;
            flag_list.emplace_back(val);
        }
        return flag_list;
    }

    template <typename FlagEnum, typename ListType = std::vector<std::string>>
        requires std::is_enum_v<FlagEnum>
    auto getEnumNameList(const std::function<bool(FlagEnum v)>& filter = {}) {
        ListType flag_list;
        for (const auto& [val, name] : enchantum::entries<FlagEnum>) {
            if (filter && !filter(val))
                continue;
            flag_list.emplace_back(name);
        }
        return flag_list;
    }

    template <typename T>
        requires std::is_enum_v<T>
    std::expected<T, std::string> chooseEnum(std::string_view arg, std::string_view context, const std::function<bool(T)> filter = {}) {
        auto emap = getEnumMap<T>(filter);

        auto res = partialMatch(arg, emap);
        if (!res) {
            return std::unexpected("No match found for " + std::string(context) + " '" + std::string(arg) + "'. " + res.error());
        }
        return res.value()->second;
    }

    template <typename EnumType>
        requires std::is_enum_v<EnumType>
    std::expected<std::string, std::string> handleSetEnum(EnumType& field, std::string_view arg, std::string_view fieldName, const std::function<bool(EnumType)>& filter = {}) {
        if (arg.empty()) {
            return fmt::format("You must provide a value for {}.", fieldName);
        }
        auto res = chooseEnum<EnumType>(arg, std::string(fieldName), filter);
        if (!res) {
            return std::unexpected(res.error());
        }
        field = res.value();
        return fmt::format("Set {} to {}.", fieldName, enchantum::to_string(field));
    }

} // namespace volcano::util
