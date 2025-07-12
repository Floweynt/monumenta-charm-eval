#pragma once

#include <algorithm>
#include <cstddef>
#include <ranges>
#include <string_view>
#include <vector>

namespace mtce
{
    constexpr auto is_space(char ch) -> bool { return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r'; }

    inline auto trim(std::string_view str) -> std::string_view
    {
        // Find first non-space
        const auto* begin_it = std::ranges::find_if_not(str, is_space);
        // Find last non-space (reverse view)
        const auto* end_it = std::ranges::find_if_not(std::ranges::reverse_view(str), is_space).base();

        // Convert iterators to pointers for string_view ctor
        const char* begin = begin_it == str.end() ? str.data() + str.size() : &*begin_it;
        const char* end = end_it == str.end() ? str.data() + str.size() : &*end_it;

        if (begin > end)
        {
            return {};
        }

        return {begin, static_cast<size_t>(end - begin)};
    }

    inline auto split_string_view(std::string_view str, char delim) -> std::vector<std::string_view>
    {
        std::vector<std::string_view> result;
        size_t start = 0;
        while (start <= str.size())
        {
            size_t end = str.find(delim, start);
            if (end == std::string_view::npos)
            {
                end = str.size();
            }
            result.emplace_back(str.data() + start, end - start);
            start = end + 1;
        }
        return result;
    }
} // namespace mtce
