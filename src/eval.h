#pragma once

#include "charm.h"
#include "charm_data.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace mtce
{
    using charm_weights = std::array<std::int32_t, ABILITY_COUNT>;

    struct eval_result
    {
        int64_t utility_value;
        std::vector<charm_id> charms;
    };

    auto evaluate_naive(const std::vector<charm>& charms, uint32_t max_cp, const charm_weights& weights, std::size_t threads = 0) -> eval_result;
} // namespace mtce
