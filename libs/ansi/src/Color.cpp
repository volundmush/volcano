#include "volcano/ansi/Color.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

namespace volcano::ansi {
    namespace {
        constexpr std::array<Rgb, 16> kAnsi16Palette = {
            Rgb{0, 0, 0},       // black
            Rgb{205, 0, 0},     // red
            Rgb{0, 205, 0},     // green
            Rgb{205, 205, 0},   // yellow
            Rgb{0, 0, 238},     // blue
            Rgb{205, 0, 205},   // magenta
            Rgb{0, 205, 205},   // cyan
            Rgb{229, 229, 229}, // white (light gray)
            Rgb{127, 127, 127}, // bright black (dark gray)
            Rgb{255, 0, 0},     // bright red
            Rgb{0, 255, 0},     // bright green
            Rgb{255, 255, 0},   // bright yellow
            Rgb{92, 92, 255},   // bright blue
            Rgb{255, 0, 255},   // bright magenta
            Rgb{0, 255, 255},   // bright cyan
            Rgb{255, 255, 255}  // bright white
        };

        inline Color color_from_index(std::uint8_t index)
        {
            if (index < 16) {
                return AnsiColor{index};
            }
            return XtermColor{index};
        }
    }

    const std::unordered_map<std::string, Color> named_colors = [] {
        std::unordered_map<std::string, Color> map;

        auto add_name = [&](std::string name, std::uint8_t index) {
            map.emplace(name, color_from_index(index));

            std::string hyphen = name;
            std::string compact = name;

            for (char& ch : hyphen) {
                if (ch == '_') {
                    ch = '-';
                }
            }

            compact.erase(
                std::remove_if(compact.begin(), compact.end(), [](char ch) {
                    return ch == '_' || ch == '-';
                }),
                compact.end());

            if (hyphen != name) {
                map.emplace(hyphen, color_from_index(index));
            }
            if (compact != name) {
                map.emplace(compact, color_from_index(index));
            }
        };

        add_name("black", 0);
        add_name("red", 1);
        add_name("green", 2);
        add_name("yellow", 3);
        add_name("blue", 4);
        add_name("magenta", 5);
        add_name("cyan", 6);
        add_name("white", 7);
        add_name("bright_black", 8);
        add_name("bright_red", 9);
        add_name("bright_green", 10);
        add_name("bright_yellow", 11);
        add_name("bright_blue", 12);
        add_name("bright_magenta", 13);
        add_name("bright_cyan", 14);
        add_name("bright_white", 15);
        add_name("grey0", 16);
        add_name("gray0", 16);
        add_name("navy_blue", 17);
        add_name("dark_blue", 18);
        add_name("blue3", 20);
        add_name("blue1", 21);
        add_name("dark_green", 22);
        add_name("deep_sky_blue4", 25);
        add_name("dodger_blue3", 26);
        add_name("dodger_blue2", 27);
        add_name("green4", 28);
        add_name("spring_green4", 29);
        add_name("turquoise4", 30);
        add_name("deep_sky_blue3", 32);
        add_name("dodger_blue1", 33);
        add_name("green3", 40);
        add_name("spring_green3", 41);
        add_name("dark_cyan", 36);
        add_name("light_sea_green", 37);
        add_name("deep_sky_blue2", 38);
        add_name("deep_sky_blue1", 39);
        add_name("spring_green2", 47);
        add_name("cyan3", 43);
        add_name("dark_turquoise", 44);
        add_name("turquoise2", 45);
        add_name("green1", 46);
        add_name("spring_green1", 48);
        add_name("medium_spring_green", 49);
        add_name("cyan2", 50);
        add_name("cyan1", 51);
        add_name("dark_red", 88);
        add_name("deep_pink4", 125);
        add_name("purple4", 55);
        add_name("purple3", 56);
        add_name("blue_violet", 57);
        add_name("orange4", 94);
        add_name("grey37", 59);
        add_name("gray37", 59);
        add_name("medium_purple4", 60);
        add_name("slate_blue3", 62);
        add_name("royal_blue1", 63);
        add_name("chartreuse4", 64);
        add_name("dark_sea_green4", 71);
        add_name("pale_turquoise4", 66);
        add_name("steel_blue", 67);
        add_name("steel_blue3", 68);
        add_name("cornflower_blue", 69);
        add_name("chartreuse3", 76);
        add_name("cadet_blue", 73);
        add_name("sky_blue3", 74);
        add_name("steel_blue1", 81);
        add_name("pale_green3", 114);
        add_name("sea_green3", 78);
        add_name("aquamarine3", 79);
        add_name("medium_turquoise", 80);
        add_name("chartreuse2", 112);
        add_name("sea_green2", 83);
        add_name("sea_green1", 85);
        add_name("aquamarine1", 122);
        add_name("dark_slate_gray2", 87);
        add_name("dark_magenta", 91);
        add_name("dark_violet", 128);
        add_name("purple", 129);
        add_name("light_pink4", 95);
        add_name("plum4", 96);
        add_name("medium_purple3", 98);
        add_name("slate_blue1", 99);
        add_name("yellow4", 106);
        add_name("wheat4", 101);
        add_name("grey53", 102);
        add_name("gray53", 102);
        add_name("light_slate_grey", 103);
        add_name("light_slate_gray", 103);
        add_name("medium_purple", 104);
        add_name("light_slate_blue", 105);
        add_name("dark_olive_green3", 149);
        add_name("dark_sea_green", 108);
        add_name("light_sky_blue3", 110);
        add_name("sky_blue2", 111);
        add_name("dark_sea_green3", 150);
        add_name("dark_slate_gray3", 116);
        add_name("sky_blue1", 117);
        add_name("chartreuse1", 118);
        add_name("light_green", 120);
        add_name("pale_green1", 156);
        add_name("dark_slate_gray1", 123);
        add_name("red3", 160);
        add_name("medium_violet_red", 126);
        add_name("magenta3", 164);
        add_name("dark_orange3", 166);
        add_name("indian_red", 167);
        add_name("hot_pink3", 168);
        add_name("medium_orchid3", 133);
        add_name("medium_orchid", 134);
        add_name("medium_purple2", 140);
        add_name("dark_goldenrod", 136);
        add_name("light_salmon3", 173);
        add_name("rosy_brown", 138);
        add_name("grey63", 139);
        add_name("gray63", 139);
        add_name("medium_purple1", 141);
        add_name("gold3", 178);
        add_name("dark_khaki", 143);
        add_name("navajo_white3", 144);
        add_name("grey69", 145);
        add_name("gray69", 145);
        add_name("light_steel_blue3", 146);
        add_name("light_steel_blue", 147);
        add_name("yellow3", 184);
        add_name("dark_sea_green2", 157);
        add_name("light_cyan3", 152);
        add_name("light_sky_blue1", 153);
        add_name("green_yellow", 154);
        add_name("dark_olive_green2", 155);
        add_name("dark_sea_green1", 193);
        add_name("pale_turquoise1", 159);
        add_name("deep_pink3", 162);
        add_name("magenta2", 200);
        add_name("hot_pink2", 169);
        add_name("orchid", 170);
        add_name("medium_orchid1", 207);
        add_name("orange3", 172);
        add_name("light_pink3", 174);
        add_name("pink3", 175);
        add_name("plum3", 176);
        add_name("violet", 177);
        add_name("light_goldenrod3", 179);
        add_name("tan", 180);
        add_name("misty_rose3", 181);
        add_name("thistle3", 182);
        add_name("plum2", 183);
        add_name("khaki3", 185);
        add_name("light_goldenrod2", 222);
        add_name("light_yellow3", 187);
        add_name("grey84", 188);
        add_name("gray84", 188);
        add_name("light_steel_blue1", 189);
        add_name("yellow2", 190);
        add_name("dark_olive_green1", 192);
        add_name("honeydew2", 194);
        add_name("light_cyan1", 195);
        add_name("red1", 196);
        add_name("deep_pink2", 197);
        add_name("deep_pink1", 199);
        add_name("magenta1", 201);
        add_name("orange_red1", 202);
        add_name("indian_red1", 204);
        add_name("hot_pink", 206);
        add_name("dark_orange", 208);
        add_name("salmon1", 209);
        add_name("light_coral", 210);
        add_name("pale_violet_red1", 211);
        add_name("orchid2", 212);
        add_name("orchid1", 213);
        add_name("orange1", 214);
        add_name("sandy_brown", 215);
        add_name("light_salmon1", 216);
        add_name("light_pink1", 217);
        add_name("pink1", 218);
        add_name("plum1", 219);
        add_name("gold1", 220);
        add_name("navajo_white1", 223);
        add_name("misty_rose1", 224);
        add_name("thistle1", 225);
        add_name("yellow1", 226);
        add_name("light_goldenrod1", 227);
        add_name("khaki1", 228);
        add_name("wheat1", 229);
        add_name("cornsilk1", 230);
        add_name("grey100", 231);
        add_name("gray100", 231);
        add_name("grey3", 232);
        add_name("gray3", 232);
        add_name("grey7", 233);
        add_name("gray7", 233);
        add_name("grey11", 234);
        add_name("gray11", 234);
        add_name("grey15", 235);
        add_name("gray15", 235);
        add_name("grey19", 236);
        add_name("gray19", 236);
        add_name("grey23", 237);
        add_name("gray23", 237);
        add_name("grey27", 238);
        add_name("gray27", 238);
        add_name("grey30", 239);
        add_name("gray30", 239);
        add_name("grey35", 240);
        add_name("gray35", 240);
        add_name("grey39", 241);
        add_name("gray39", 241);
        add_name("grey42", 242);
        add_name("gray42", 242);
        add_name("grey46", 243);
        add_name("gray46", 243);
        add_name("grey50", 244);
        add_name("gray50", 244);
        add_name("grey54", 245);
        add_name("gray54", 245);
        add_name("grey58", 246);
        add_name("gray58", 246);
        add_name("grey62", 247);
        add_name("gray62", 247);
        add_name("grey66", 248);
        add_name("gray66", 248);
        add_name("grey70", 249);
        add_name("gray70", 249);
        add_name("grey74", 250);
        add_name("gray74", 250);
        add_name("grey78", 251);
        add_name("gray78", 251);
        add_name("grey82", 252);
        add_name("gray82", 252);
        add_name("grey85", 253);
        add_name("gray85", 253);
        add_name("grey89", 254);
        add_name("gray89", 254);
        add_name("grey93", 255);
        add_name("gray93", 255);

        return map;
    }();

    Style::Style() = default;

    Style::Style(std::optional<Color> fg, std::optional<Color> bg, Attribute attrs)
        : foreground_(std::move(fg)), background_(std::move(bg)), attributes_(attrs) {}

    const std::optional<Color>& Style::foreground() const { return foreground_; }

    const std::optional<Color>& Style::background() const { return background_; }

    Attribute Style::attributes() const { return attributes_; }

    bool Style::has_foreground() const { return foreground_.has_value(); }

    bool Style::has_background() const { return background_.has_value(); }

    bool Style::has_attribute(Attribute attr) const { return (attributes_ & attr) != Attribute::None; }

    Style& Style::set_foreground(Color color)
    {
        foreground_ = std::move(color);
        return *this;
    }

    Style& Style::set_background(Color color)
    {
        background_ = std::move(color);
        return *this;
    }

    Style& Style::clear_foreground()
    {
        foreground_.reset();
        return *this;
    }

    Style& Style::clear_background()
    {
        background_.reset();
        return *this;
    }

    Style& Style::set_attributes(Attribute attrs)
    {
        attributes_ = attrs;
        return *this;
    }

    Style& Style::add_attributes(Attribute attrs)
    {
        attributes_ = attributes_ | attrs;
        return *this;
    }

    Style& Style::remove_attributes(Attribute attrs)
    {
        attributes_ = attributes_ & static_cast<Attribute>(~static_cast<std::uint16_t>(attrs));
        return *this;
    }

    Style operator+(const Style& lhs, const Style& rhs)
    {
        Style out = lhs;
        if (rhs.foreground_) {
            out.foreground_ = rhs.foreground_;
        }
        if (rhs.background_) {
            out.background_ = rhs.background_;
        }
        out.attributes_ = lhs.attributes_ | rhs.attributes_;
        return out;
    }

    Style& Style::operator+=(const Style& other)
    {
        *this = *this + other;
        return *this;
    }

    TrueColor xterm_to_truecolor(std::uint8_t index)
    {
        if (index < 16) {
            const auto& c = kAnsi16Palette[index];
            return TrueColor{c.r, c.g, c.b};
        }
        if (index >= 232) {
            std::uint8_t gray = static_cast<std::uint8_t>(8 + (index - 232) * 10);
            return TrueColor{gray, gray, gray};
        }
        std::uint8_t idx = static_cast<std::uint8_t>(index - 16);
        std::uint8_t r = static_cast<std::uint8_t>(idx / 36);
        std::uint8_t g = static_cast<std::uint8_t>((idx / 6) % 6);
        std::uint8_t b = static_cast<std::uint8_t>(idx % 6);
        auto to_level = [](std::uint8_t v) -> std::uint8_t {
            return v == 0 ? 0 : static_cast<std::uint8_t>(55 + 40 * v);
        };
        return TrueColor{to_level(r), to_level(g), to_level(b)};
    }

    TrueColor to_truecolor(const Color& color)
    {
        if (auto p = std::get_if<TrueColor>(&color)) {
            return *p;
        }
        if (auto p = std::get_if<AnsiColor>(&color)) {
            const auto& c = kAnsi16Palette[p->color % 16];
            return TrueColor{c.r, c.g, c.b};
        }
        const auto* p = std::get_if<XtermColor>(&color);
        return xterm_to_truecolor(p->color);
    }

    std::uint8_t nearest_ansi16_index(TrueColor color)
    {
        std::uint32_t best = 0xFFFFFFFFu;
        std::uint8_t best_idx = 0;
        for (std::uint8_t i = 0; i < kAnsi16Palette.size(); ++i) {
            const auto& p = kAnsi16Palette[i];
            const int dr = static_cast<int>(color.r) - static_cast<int>(p.r);
            const int dg = static_cast<int>(color.g) - static_cast<int>(p.g);
            const int db = static_cast<int>(color.b) - static_cast<int>(p.b);
            const std::uint32_t dist = static_cast<std::uint32_t>(dr * dr + dg * dg + db * db);
            if (dist < best) {
                best = dist;
                best_idx = i;
            }
        }
        return best_idx;
    }

    std::uint8_t truecolor_to_xterm(TrueColor color)
    {
        auto to_cube = [](std::uint8_t v) -> std::uint8_t {
            if (v < 48) {
                return 0;
            }
            if (v < 114) {
                return 1;
            }
            return static_cast<std::uint8_t>((v - 35) / 40);
        };
        static constexpr std::array<std::uint8_t, 6> levels = {0, 95, 135, 175, 215, 255};

        const std::uint8_t r = to_cube(color.r);
        const std::uint8_t g = to_cube(color.g);
        const std::uint8_t b = to_cube(color.b);
        const std::uint8_t cube_index = static_cast<std::uint8_t>(16 + (36 * r) + (6 * g) + b);
        const TrueColor cube_color{levels[r], levels[g], levels[b]};

        const std::uint8_t gray_avg = static_cast<std::uint8_t>((static_cast<int>(color.r) + color.g + color.b) / 3);
        const std::uint8_t gray_index = static_cast<std::uint8_t>(std::clamp((gray_avg - 8) / 10, 0, 23));
        const std::uint8_t gray_level = static_cast<std::uint8_t>(8 + gray_index * 10);
        const TrueColor gray_color{gray_level, gray_level, gray_level};
        const std::uint8_t gray_xterm = static_cast<std::uint8_t>(232 + gray_index);

        auto dist2 = [](TrueColor a, TrueColor b) -> std::uint32_t {
            const int dr = static_cast<int>(a.r) - static_cast<int>(b.r);
            const int dg = static_cast<int>(a.g) - static_cast<int>(b.g);
            const int db = static_cast<int>(a.b) - static_cast<int>(b.b);
            return static_cast<std::uint32_t>(dr * dr + dg * dg + db * db);
        };

        return dist2(color, cube_color) <= dist2(color, gray_color) ? cube_index : gray_xterm;
    }

    AnsiColor to_ansi16(const Color& color)
    {
        if (auto p = std::get_if<AnsiColor>(&color)) {
            return *p;
        }
        if (auto p = std::get_if<XtermColor>(&color)) {
            if (p->color < 16) {
                return AnsiColor{p->color};
            }
            return AnsiColor{nearest_ansi16_index(xterm_to_truecolor(p->color))};
        }
        return AnsiColor{nearest_ansi16_index(to_truecolor(color))};
    }

    XtermColor to_xterm256(const Color& color)
    {
        if (auto p = std::get_if<XtermColor>(&color)) {
            return *p;
        }
        if (auto p = std::get_if<AnsiColor>(&color)) {
            return XtermColor{static_cast<std::uint8_t>(p->color % 16)};
        }
        return XtermColor{truecolor_to_xterm(to_truecolor(color))};
    }

    std::string to_ansi_escape(const Style& style, ColorMode mode)
    {
        if (mode == ColorMode::None) {
            return {};
        }

        std::array<int, 32> codes{};
        std::size_t count = 0;

        auto push = [&](int code) {
            if (count < codes.size()) {
                codes[count++] = code;
            }
        };

        if (style.has_attribute(Attribute::Bold)) push(1);
        if (style.has_attribute(Attribute::Dim)) push(2);
        if (style.has_attribute(Attribute::Italic)) push(3);
        if (style.has_attribute(Attribute::Underline)) push(4);
        if (style.has_attribute(Attribute::Blink)) push(5);
        if (style.has_attribute(Attribute::Blink2)) push(6);
        if (style.has_attribute(Attribute::Reverse)) push(7);
        if (style.has_attribute(Attribute::Conceal)) push(8);
        if (style.has_attribute(Attribute::Strike)) push(9);
        if (style.has_attribute(Attribute::Underline2)) push(21);
        if (style.has_attribute(Attribute::Frame)) push(51);
        if (style.has_attribute(Attribute::Encircle)) push(52);
        if (style.has_attribute(Attribute::Overline)) push(53);

        auto add_color = [&](const Color& color, bool background) {
            if (mode == ColorMode::Ansi16) {
                const auto ansi = to_ansi16(color);
                const bool bright = ansi.color >= 8;
                const int base = background ? (bright ? 100 : 40) : (bright ? 90 : 30);
                push(base + (ansi.color % 8));
            } else if (mode == ColorMode::Xterm256) {
                const auto xterm = to_xterm256(color);
                push(background ? 48 : 38);
                push(5);
                push(xterm.color);
            } else if (mode == ColorMode::TrueColor) {
                const auto rgb = to_truecolor(color);
                push(background ? 48 : 38);
                push(2);
                push(rgb.r);
                push(rgb.g);
                push(rgb.b);
            }
        };

        if (style.foreground()) {
            add_color(*style.foreground(), false);
        }
        if (style.background()) {
            add_color(*style.background(), true);
        }

        if (count == 0) {
            return {};
        }

        std::string out;
        out.reserve(4 + count * 4);
        out.append("\x1b[");
        for (std::size_t i = 0; i < count; ++i) {
            if (i > 0) {
                out.push_back(';');
            }
            out.append(std::to_string(codes[i]));
        }
        out.push_back('m');
        return out;
    }
}
