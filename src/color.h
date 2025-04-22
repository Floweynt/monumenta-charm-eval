#pragma once

#include <cstdint>
#include <format>
#include <iterator>

#define gray(x) "\x1b[38;2;128;128;128m" x "\x1b[0m"
#define yellow(x) "\x1b[38;2;229;158;103m" x "\x1b[0m"
#define green(x) "\x1b[38;2;107;176;93m" x "\x1b[0m"

namespace mtce
{
    template <typename T>
    struct colorize
    {
        uint32_t color;
        T value;

        constexpr colorize(uint32_t color, T value) : color(color), value(value) {}
    };

    template <typename T>
    colorize(uint32_t color, T value) -> colorize<T>;
} // namespace mtce

template <typename T>
struct std::formatter<mtce::colorize<T>>
{
    std::formatter<T> delegate{};

    constexpr auto parse(std::format_parse_context& ctx) { return delegate.parse(ctx); }

    auto format(const mtce::colorize<T>& obj, std::format_context& ctx) const
    {
        auto color_r = (obj.color & 0xff0000) >> 16;
        auto color_g = (obj.color & 0xff00) >> 8;
        auto color_b = (obj.color & 0xff);
        ctx.advance_to(std::format_to(ctx.out(), "\x1b[38;2;{};{};{}m", color_r, color_g, color_b));
        return std::format_to(delegate.format(obj.value, ctx), "\x1b[0m");
    }
};
