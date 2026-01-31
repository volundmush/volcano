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
    }

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
