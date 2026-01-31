#include "volcano/ansi/Text.hpp"

#include <algorithm>
#include <string_view>
#include <utility>

namespace volcano::ansi {
    Span::Span(std::size_t start, std::size_t end, Style style)
        : start_(start), end_(end), style_(std::move(style)) {}

    std::size_t Span::start() const { return start_; }

    std::size_t Span::end() const { return end_; }

    const Style& Span::style() const { return style_; }

    Segment::Segment(std::string text, std::optional<Style> style)
        : text_(std::move(text)), style_(std::move(style)) {}

    const std::string& Segment::text() const { return text_; }

    const std::optional<Style>& Segment::style() const { return style_; }

    Text::Text() = default;

    Text::Text(std::string text) : plain_text_(std::move(text)) {}

    const std::string& Text::plain() const { return plain_text_; }

    const std::vector<Span>& Text::spans() const { return spans_; }

    void Text::set_plain(std::string text)
    {
        plain_text_ = std::move(text);
    }

    void Text::append(std::string text, std::optional<Style> style)
    {
        const std::size_t start = plain_text_.size();
        plain_text_.append(text);
        if (style) {
            spans_.emplace_back(start, plain_text_.size(), std::move(*style));
        }
    }

    void Text::add_span(Span span)
    {
        spans_.emplace_back(std::move(span));
    }

    void Text::add_style(Style style, std::size_t start, std::size_t end)
    {
        if (start >= end) {
            return;
        }
        spans_.emplace_back(start, end, std::move(style));
    }

    std::vector<Segment> Text::render_segments(ColorMode mode) const
    {
        std::vector<Segment> segments;
        if (plain_text_.empty()) {
            return segments;
        }

        std::vector<std::optional<Style>> per_char(plain_text_.size());
        for (const auto& span : spans_) {
            const std::size_t start = std::min(span.start(), plain_text_.size());
            const std::size_t end = std::min(span.end(), plain_text_.size());
            for (std::size_t i = start; i < end; ++i) {
                if (per_char[i]) {
                    per_char[i] = *per_char[i] + span.style();
                } else {
                    per_char[i] = span.style();
                }
            }
        }

        std::string buffer;
        buffer.reserve(plain_text_.size());
        std::optional<Style> current_style = per_char[0];
        buffer.push_back(plain_text_[0]);

        for (std::size_t i = 1; i < plain_text_.size(); ++i) {
            if (per_char[i] != current_style) {
                segments.emplace_back(std::move(buffer), current_style);
                buffer.clear();
                current_style = per_char[i];
            }
            buffer.push_back(plain_text_[i]);
        }

        segments.emplace_back(std::move(buffer), current_style);
        return segments;
    }

    std::string render(const IRenderable& renderable, ColorMode mode)
    {
        const auto segments = renderable.render_segments(mode);
        if (segments.empty()) {
            return {};
        }

        constexpr std::string_view reset = "\x1b[0m";
        std::string out;

        for (const auto& segment : segments) {
            if (segment.style()) {
                const std::string prefix = to_ansi_escape(*segment.style(), mode);
                out.append(prefix);
                out.append(segment.text());
                if (mode != ColorMode::None) {
                    out.append(reset);
                }
            } else {
                out.append(segment.text());
            }
        }

        return out;
    }
}
