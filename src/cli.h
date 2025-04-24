#pragma once

#include "charm.h"
#include "eval.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace mtce
{
    struct naive_algo_flags
    {
        size_t threads;
    };

    using algo_info_t = std::variant<naive_algo_flags>;

    struct cli_options
    {
        std::string_view config;
        std::string_view charm_input_file;
        algo_info_t algo;
    };

    struct config
    {
        std::uint8_t max_cp;
        std::unordered_map<std::string, std::int32_t> ability_weights;

        [[nodiscard]] constexpr auto to_weights() const -> charm_weights
        {
            charm_weights weights{};

            for (const auto& [name, value] : ability_weights)
            {
                if (NAME_TO_ID.contains(name))
                {
                    weights.at(NAME_TO_ID.at(name)) = value;
                }
            }

            return weights;
        }
    };

    auto parse_args(int argc, const char* const* argv) -> cli_options;
    auto read_config(const std::string& path) -> config;
    auto read_charms(const std::string& path) -> std::vector<charm>;
} // namespace mtce
