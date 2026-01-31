#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <enchantum/bitflags.hpp>
#include <enchantum/bitset.hpp>
#include <enchantum/bitwise_operators.hpp>

namespace volcano::ansi {
    
    struct AnsiColor {
        std::uint8_t color;

        friend bool operator==(const AnsiColor&, const AnsiColor&) = default;
    };

    struct XtermColor {
        std::uint8_t color;

        friend bool operator==(const XtermColor&, const XtermColor&) = default;
    };

    struct TrueColor {
        std::uint8_t r;
        std::uint8_t g;
        std::uint8_t b;

        friend bool operator==(const TrueColor&, const TrueColor&) = default;
    };

    using Color = std::variant<AnsiColor, XtermColor, TrueColor>;

    enum class Attribute : std::uint16_t {
        None = 0,
        Bold = 1,
        Dim = 2,
        Italic = 4,
        Underline = 8,
        Blink = 16,
        Blink2 = 32,
        Reverse = 64,
        Conceal = 128,
        Strike = 256,
        Underline2 = 512,
        Frame = 1024,
        Encircle = 2048,
        Overline = 4096
    };
    ENCHANTUM_DEFINE_BITWISE_FOR(Attribute)

    enum class ColorMode : std::uint8_t {
        None = 0,
        Ansi16 = 1,
        Xterm256 = 2,
        TrueColor = 3
    };

    class Style {
        public:
        Style();
        Style(std::optional<Color> fg, std::optional<Color> bg, Attribute attrs = Attribute::None);

        [[nodiscard]] const std::optional<Color>& foreground() const;
        [[nodiscard]] const std::optional<Color>& background() const;
        [[nodiscard]] Attribute attributes() const;

        [[nodiscard]] bool has_foreground() const;
        [[nodiscard]] bool has_background() const;
        [[nodiscard]] bool has_attribute(Attribute attr) const;

        Style& set_foreground(Color color);
        Style& set_background(Color color);
        Style& clear_foreground();
        Style& clear_background();
        Style& set_attributes(Attribute attrs);
        Style& add_attributes(Attribute attrs);
        Style& remove_attributes(Attribute attrs);

        friend Style operator+(const Style& lhs, const Style& rhs);

        Style& operator+=(const Style& other);

        friend bool operator==(const Style&, const Style&) = default;

        private:
        std::optional<Color> foreground_{};
        std::optional<Color> background_{};
        Attribute attributes_{Attribute::None};
    };

    struct Rgb {
        std::uint8_t r;
        std::uint8_t g;
        std::uint8_t b;
    };
    [[nodiscard]] TrueColor xterm_to_truecolor(std::uint8_t index);
    [[nodiscard]] TrueColor to_truecolor(const Color& color);
    [[nodiscard]] std::uint8_t nearest_ansi16_index(TrueColor color);
    [[nodiscard]] std::uint8_t truecolor_to_xterm(TrueColor color);
    [[nodiscard]] AnsiColor to_ansi16(const Color& color);
    [[nodiscard]] XtermColor to_xterm256(const Color& color);
    [[nodiscard]] std::string to_ansi_escape(const Style& style, ColorMode mode);

     



}