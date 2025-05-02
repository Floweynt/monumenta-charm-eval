#pragma once

#include "charm.h"
#include "gen/charm_data.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>
#include <vector>

namespace mtce
{
    using charm_weights = std::array<std::int32_t, ABILITY_COUNT>;

    struct eval_config
    {
        std::vector<charm> charms;
        uint32_t max_cp;
        charm_weights weights;
        std::size_t threads;
    };

    struct eval_result
    {
        int64_t utility_value;
        std::vector<charm_id> charms;
    };

    struct naive_tracing_config 
    {
        std::function<void(std::vector<std::string_view>& abilities, std::vector<std::string_view>& charms)> trace_prune;
    };

    auto evaluate_naive(const eval_config& config, const naive_tracing_config& trace = {}) -> eval_result;
} // namespace mtce
