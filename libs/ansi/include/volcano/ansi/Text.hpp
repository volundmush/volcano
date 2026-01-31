#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "Color.hpp"

namespace volcano::ansi {
    class Span {
        public:
        Span(std::size_t start, std::size_t end, Style style);

        [[nodiscard]] std::size_t start() const;
        [[nodiscard]] std::size_t end() const;
        [[nodiscard]] const Style& style() const;

        private:
        std::size_t start_;
        std::size_t end_;
        Style style_;
    };

    class Segment {
        public:
        Segment(std::string text, std::optional<Style> style = std::nullopt);

        [[nodiscard]] const std::string& text() const;
        [[nodiscard]] const std::optional<Style>& style() const;

        private:
        std::string text_;
        std::optional<Style> style_;
    };

    class IRenderable {
        public:
        virtual ~IRenderable() = default;
        virtual std::vector<Segment> render_segments(ColorMode mode) const = 0;
    };

    class Text : public IRenderable {
        public:
        Text();
        explicit Text(std::string text);

        [[nodiscard]] const std::string& plain() const;
        [[nodiscard]] const std::vector<Span>& spans() const;

        void set_plain(std::string text);
        void append(std::string text, std::optional<Style> style = std::nullopt);
        void add_span(Span span);
        void add_style(Style style, std::size_t start, std::size_t end);

        std::vector<Segment> render_segments(ColorMode mode) const override;

        private:
        std::string plain_text_;
        std::vector<Span> spans_;
    };

    [[nodiscard]] std::string render(const IRenderable& renderable, ColorMode mode);
}