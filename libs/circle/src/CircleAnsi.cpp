#include "volcano/circle/CircleAnsi.hpp"

#include <charconv>
#include <cctype>
#include <string>

#define ANSISTART "\x1B["
#define ANSISEP ';'
#define ANSISEPSTR ";"
#define ANSIEND 'm'
#define ANSIENDSTR "m"

/* Attributes */
#define AA_NORMAL "0"
#define AA_BOLD "1"
#define AA_UNDERLINE "4"
#define AA_BLINK "5"
#define AA_REVERSE "7"
#define AA_INVIS "8"
/* Foreground colors */
#define AF_BLACK "30"
#define AF_RED "31"
#define AF_GREEN "32"
#define AF_YELLOW "33"
#define AF_BLUE "34"
#define AF_MAGENTA "35"
#define AF_CYAN "36"
#define AF_WHITE "37"
/* Background colors */
#define AB_BLACK "40"
#define AB_RED "41"
#define AB_GREEN "42"
#define AB_YELLOW "43"
#define AB_BLUE "44"
#define AB_MAGENTA "45"
#define AB_CYAN "46"
#define AB_WHITE "47"

namespace volcano::circle
{

    constexpr std::string_view RANDOM_COLORS = "bgcrmywBGCRMWY";
    
    volcano::ansi::Text toText(std::string_view txt, const std::unordered_map<uint8_t, std::string>& custom_colors) {
        volcano::ansi::Text result;
        std::string buffer;
        std::optional<volcano::ansi::Style> current_style;

        auto handle_end = [&]() {
            if(!buffer.empty()) {
                result.append(buffer, current_style);
                buffer.clear();
            }
        };

        auto enable_attribute = [&](volcano::ansi::Attribute attribute) {
            handle_end();
            if(!current_style) {
                current_style = volcano::ansi::Style{};
            }
            current_style->add_attributes(attribute);
        };

        auto enable_ansi_color = [&](char code, bool bold, bool background) {
            handle_end();
            if(!current_style) {
                current_style = volcano::ansi::Style{};
            }
            volcano::ansi::Color color;
            switch(code) {
                case 'd': case '0': color = volcano::ansi::named_colors.at("black"); break;
                case 'b': case '1': color = volcano::ansi::named_colors.at("blue"); break;
                case 'g': case '2': color = volcano::ansi::named_colors.at("green"); break;
                case 'c': case '3': color = volcano::ansi::named_colors.at("cyan"); break;
                case 'r': case '4': color = volcano::ansi::named_colors.at("red"); break;
                case 'm': case '5': color = volcano::ansi::named_colors.at("magenta"); break;
                case 'y': case '6': color = volcano::ansi::named_colors.at("yellow"); break;
                case 'w': case '7': color = volcano::ansi::named_colors.at("white"); break;
                default: return;
            }
            if(background) {
                current_style->set_background(color);
            } else {
                current_style->set_foreground(color);
            }
            if(bold) {
                current_style->add_attributes(volcano::ansi::Attribute::Bold);
            }
        };

        auto handle_user_color = [&](int color_index) {
            handle_end();
            if(!current_style) {
                current_style = volcano::ansi::Style{};
            }
            auto it = custom_colors.find(static_cast<uint8_t>(color_index));
            volcano::ansi::Color color;
            if(it != custom_colors.end()) {
                auto named_it = volcano::ansi::named_colors.find(it->second);
                if(named_it != volcano::ansi::named_colors.end()) {
                    color = named_it->second;
                } else {
                    color = volcano::ansi::named_colors.at("black");
                }
            } else {
                color = volcano::ansi::named_colors.at("black");
            }
            current_style->set_foreground(color);
        };

        auto handle_expanded_color = [&](std::string_view sub) {
            // sub is either a name (no spaces), a number, or r,g,b
            // if we can't resolve it, we treat it as ansi black.
            handle_end();
            if(!current_style) {
                current_style = volcano::ansi::Style{};
            }

            auto set_black = [&]() {
                current_style->set_foreground(volcano::ansi::named_colors.at("black"));
            };

            auto trim = [](std::string_view view) -> std::string_view {
                std::size_t start = 0;
                std::size_t end = view.size();
                while(start < end && std::isspace(static_cast<unsigned char>(view[start]))) {
                    ++start;
                }
                while(end > start && std::isspace(static_cast<unsigned char>(view[end - 1]))) {
                    --end;
                }
                return view.substr(start, end - start);
            };

            sub = trim(sub);
            if(sub.empty()) {
                set_black();
                return;
            }

            auto parse_int = [&](std::string_view view, int& value) -> bool {
                view = trim(view);
                if(view.empty()) {
                    return false;
                }
                int tmp = 0;
                auto result = std::from_chars(view.data(), view.data() + view.size(), tmp);
                if(result.ec != std::errc{} || result.ptr != view.data() + view.size()) {
                    return false;
                }
                value = tmp;
                return true;
            };

            auto comma_pos = sub.find(',');
            if(comma_pos != std::string_view::npos) {
                int r = 0;
                int g = 0;
                int b = 0;
                auto second = sub.find(',', comma_pos + 1);
                if(second == std::string_view::npos) {
                    set_black();
                    return;
                }
                if(!parse_int(sub.substr(0, comma_pos), r) ||
                   !parse_int(sub.substr(comma_pos + 1, second - comma_pos - 1), g) ||
                   !parse_int(sub.substr(second + 1), b)) {
                    set_black();
                    return;
                }
                if(r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
                    set_black();
                    return;
                }
                current_style->set_foreground(volcano::ansi::TrueColor{
                    static_cast<std::uint8_t>(r),
                    static_cast<std::uint8_t>(g),
                    static_cast<std::uint8_t>(b)});
                return;
            }

            int index = 0;
            if(parse_int(sub, index)) {
                if(index < 0 || index > 255) {
                    set_black();
                    return;
                }
                if(index < 16) {
                    current_style->set_foreground(volcano::ansi::AnsiColor{static_cast<std::uint8_t>(index)});
                } else {
                    current_style->set_foreground(volcano::ansi::XtermColor{static_cast<std::uint8_t>(index)});
                }
                return;
            }

            std::string name(sub.begin(), sub.end());
            for(char& ch : name) {
                if(ch == ' ') {
                    ch = '_';
                } else {
                    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                }
            }

            auto it = volcano::ansi::named_colors.find(name);
            if(it == volcano::ansi::named_colors.end()) {
                set_black();
                return;
            }
            current_style->set_foreground(it->second);
        };

        std::size_t pos = 0;
        while(true) {
            if(pos >= txt.size()) { // end of string reached, so terminate.
                handle_end();
                break;
            }
            if(txt[pos] != '@') { // normal character, append to buffer.
                buffer.push_back(txt[pos++]);
                continue;
            }
            // we found a color code
            pos++;

            if(pos >= txt.size()) {
                // we reached the end of the string, so we treat it as a literal '@'
                buffer.push_back('@');
                handle_end();
                break;
            }

            char code = txt[pos];
            switch(code) {
                case '@': {
                    // escaped @
                    buffer.push_back('@');
                    pos++;
                    continue;
                }
                case 'n': { // ansi reset.
                    handle_end();
                    current_style = std::nullopt;
                    pos++;
                    continue;
                }
                case 'd':
                case 'b':
                case 'g':
                case 'c':
                case 'r':
                case 'm':
                case 'y':
                case 'w': {
                    // normal ansi codes
                    enable_ansi_color(code, false, false);
                    pos++;
                    continue;
                }
                case 'D':
                case 'B':
                case 'G':
                case 'C':
                case 'R':
                case 'M':
                case 'Y':
                case 'W': {
                    // normal ansi with bold enabled.
                    enable_ansi_color(tolower(code), true, false);
                    pos++;
                    continue;
                }
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7': {
                    // background colors
                    enable_ansi_color(code, false, true);
                    pos++;
                    continue;
                }
                case 'l': { // enable blink
                    enable_attribute(volcano::ansi::Attribute::Blink);
                    pos++;
                    continue;
                }
                case 'o': { // enable bold
                    enable_attribute(volcano::ansi::Attribute::Bold);
                    pos++;
                    continue;
                }
                case 'u': { // enable underline
                    enable_attribute(volcano::ansi::Attribute::Underline);
                    pos++;
                    continue;
                }
                case 'e': { // enable reverse
                    enable_attribute(volcano::ansi::Attribute::Reverse);
                    pos++;
                    continue;
                }
                case 'x': { // random color
                    char random_code = RANDOM_COLORS[rand() % (sizeof(RANDOM_COLORS) - 1)];
                    enable_ansi_color(tolower(random_code), isupper(random_code), false);
                    pos++;
                    continue;
                } 
                case '[': { // user-defined color
                    // a user-defined color is of the form @[<number>, so we must find the end of the number.
                    auto start = pos + 1;
                    auto end = start;
                    while(true) {
                        if(end >= txt.size() || !isdigit(static_cast<unsigned char>(txt[end]))) {
                            break;
                        }
                        end++;
                    }
                    if(start == end) {
                        // no digits found.
                        pos++;
                        continue;
                    }
                    // we have a number
                    int color_index = atoi(std::string(txt.substr(start, end - start)).c_str());
                    pos = end;
                    handle_user_color(color_index);
                    continue;
                }
                case '<': { // XTERM or TRUECOLOR, which is in the formats: @<name>, @<number>, or @<r,g,b>
                    auto start = pos + 1;
                    auto end = start;
                    // find terminating '>'. if none found, ignore.
                    while(true) {
                        if(end >= txt.size() || txt[end] == '>') {
                            break;
                        }
                        end++;
                    }
                    if(end >= txt.size() || txt[end] != '>') {
                        // no terminating '>' found, ignore.
                        pos++;
                        continue;
                    }
                    pos = end + 1;
                    auto sub = txt.substr(start, end - start);
                    handle_expanded_color(sub);
                    continue;
                }
            }
        }

        return result;
    }

    // A C++ version of proc_color from comm.c. it returns the colored string.
    std::string processColors(std::string_view txt, volcano::ansi::ColorMode mode, const std::unordered_map<uint8_t, std::string>& custom_colors)
    {
        auto text = toText(txt, custom_colors);
        return volcano::ansi::render(text, mode);
    }

    size_t countColors(std::string_view txt)
    {
        auto text = toText(txt);
        auto stripped = text.plain();
        return txt.size() - stripped.size();
    }

    bool isColorChar(char c)
    {
        switch (c)
        {
        case 'n':
        case 'b':
        case 'B':
        case 'g':
        case 'G':
        case 'm':
        case 'M':
        case 'r':
        case 'R':
        case 'y':
        case 'Y':
        case 'w':
        case 'W':
        case 'k':
        case 'K':
        case '0':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case 'l':
        case 'u':
        case 'o':
        case 'e':
            // case 'x':
            return true;
        default:
            return false;
        }
    }
}
