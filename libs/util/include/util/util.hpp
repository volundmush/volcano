#pragma once

#include <string>
#include <string_view>

namespace vol::util {

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

} // namespace vol::util
