#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <string_view>
#include <vector>

namespace mtce
{
    constexpr auto trim(std::string_view str) -> std::string_view
    {
        auto is_space = +[](char ch) { return std::isspace(static_cast<unsigned char>(ch)); };
        const auto* begin = std::ranges::find_if_not(str, is_space);
        const auto* end = std::ranges::find_if_not(std::ranges::reverse_view(str), is_space).base();

        if (begin >= end)
        {
            return {};
        }

        return {begin, static_cast<size_t>(std::distance(begin, end))};
    }

    constexpr auto split_string_view(std::string_view str, char delimiter) -> std::vector<std::string_view>
    {
        std::vector<std::string_view> result;
        size_t start = 0;

        while (start < str.size())
        {
            size_t end = str.find(delimiter, start);
            if (end == std::string_view::npos)
            {
                result.emplace_back(str.substr(start));
                break;
            }
            result.emplace_back(str.substr(start, end - start));
            start = end + 1;
        }

        return result;
    }
} // namespace mtce
