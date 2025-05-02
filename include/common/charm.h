#pragma once

#include "gen/charm_data.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

namespace mtce
{
    inline static constexpr std::size_t CHARM_COUNT_MAX = 7;

    using charm_id = std::uint32_t;
    inline static constexpr charm_id MISSING_ID = -1U;

    struct charm
    {
        std::uint32_t charm_power;
        std::uint32_t color;
        std::string name;
        bool has_upgrade;
        std::array<double, ABILITY_COUNT> charm_data;
    };

    // no good alternative...
    // NOLINTNEXTLINE(cert-err58-cpp)
    inline static const std::unordered_map<std::string_view, std::uint16_t> NAME_TO_ID = []() {
        std::unordered_map<std::string_view, std::uint16_t> map;
        for (auto i = 0; i < ABILITY_COUNT; i++)
        {
            map[EFFECT_NAMES.at(i)] = i;
        }
        return map;
    }();
} // namespace mtce
