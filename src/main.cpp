#include "build.h"
#include "charm.h"
#include "cli.h"
#include "color.h"
#include "eval.h"
#include "src/charm_data.h"
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <iostream>
#include <ostream>
#include <ratio>
#include <string>
#include <vector>

using namespace mtce;

namespace
{
    inline constexpr size_t CAPPED_COLOR = 0xe49b20;
    inline constexpr size_t NORMAL_COLOR = 0x4ac2e5;
    inline constexpr size_t NEGATIVE_COLOR = 0xd02e28;

    // fucked up algo
    template <bool Plus = true>
    constexpr auto format(size_t ability, double value) -> std::string
    {
        if (EFFECT_ROUND_TO_INTEGER.at(ability))
        {
            return std::format(Plus ? "{:+}{}" : "{}{}", (int32_t)std::round(value), EFFECT_IS_PERCENT.at(ability) ? "%" : "");
        }

        return std::format(Plus ? "{:+.2f}{}" : "{:.2f}{}", std::round(value * 100) / 100.0, EFFECT_IS_PERCENT.at(ability) ? "%" : "");
    }

    void print_charm_stats(const std::array<double, ABILITY_COUNT>& stats, const config& cfg)
    {
        std::println(std::cout, "Set stats: ");
        for (size_t ability = 0; ability < ABILITY_COUNT; ability++)
        {
            auto display_name = EFFECT_DISPLAY_NAMES.at(ability);
            auto caps = EFFECT_CAPS.at(ability);
            auto value = stats.at(ability);
            bool is_important = cfg.ability_weights.contains(std::string(EFFECT_NAMES.at(ability)));
            bool is_negative = caps < 0;

            std::string line;

            if (value == 0)
            {
                if (!is_important)
                {
                    continue;
                }

                line = std::format(gray("{}"), display_name);
            }
            else if ((value < 0) == is_negative)
            {
                // "good" charm - check for caps
                if (std::abs(caps) < std::abs(value))
                {
                    // capped - display overflow
                    auto delta = std::abs(value) - std::abs(caps);
                    line = colorize(
                        CAPPED_COLOR, std::format("{} {} ({} overflow)", display_name, format(ability, caps), format<false>(ability, delta))
                    );
                }
                else if (std::abs(caps) == std::abs(value))
                {
                    // capped - don't display overflow since cap == value
                    line = colorize(CAPPED_COLOR, std::format("{} {}", display_name, format(ability, caps)));
                }
                else
                {
                    // uncapped
                    line = colorize(NORMAL_COLOR, std::format("{} {}", display_name, format(ability, value)));
                }
            }
            else
            {
                // bad stat wtf!!
                line = colorize(NEGATIVE_COLOR, std::format("{} {}", display_name, format(ability, value)));
            }

            // c++ is weird about this for some reason...
            if (is_important)
            {
                std::println(std::cout, important("{}"), line);
            }
            else
            {
                std::println(std::cout, "{}", line);
            }
        }
    }

    void print_results(const eval_result& result, const std::vector<charm>& charms, const config& config)
    {
        // dump data
        // warning - this algo is ugly
        std::println(std::cout, "Optimal charm set (" green("{}") "): ", result.utility_value);
        std::uint32_t curr_cp = 0;

        std::array<double, ABILITY_COUNT> charm_set_stats{};

        for (const auto charm_index : result.charms)
        {
            const auto& charm = charms[charm_index];
            std::println(std::cout, "  {} (" green("{}") ")", colorize(charm.color, charm.name), charm.charm_power);
            curr_cp += charm.charm_power;

            for (size_t ability = 0; ability < ABILITY_COUNT; ability++)
            {
                charm_set_stats.at(ability) += charm.charm_data.at(ability);
            }
        }

        std::println(std::cout, "Charm power used: " green("{}") "/" green("{}"), curr_cp, config.max_cp);

        print_charm_stats(charm_set_stats, config);
    }
} // namespace

auto main(int argc, const char* const* argv) -> int
{
    auto [config_file, in] = parse_args(argc, argv);
    auto config = read_config(std::string(config_file));
    auto charms = read_charms(std::string(in));

    std::println(std::cout, "Starting MTCE " yellow(VERSION));
    std::println(std::cout, "Config: ");
    std::println(std::cout, "  charm_power" gray(" = ") green("{}"), config.max_cp);
    std::println(std::cout, "Weights: ");

    for (const auto& [key, value] : config.ability_weights)
    {
        std::println(std::cout, "  {}" gray(" = ") green("{}"), key, value);
    }

    std::println(std::cout, "Charms: ");

    for (const auto& charm : charms)
    {
        std::println(std::cout, "  {} (" green("{}") ")", colorize(charm.color, charm.name), charm.charm_power);
    }

    std::println(std::cout, "MTCE algorithm: " yellow("naive") " with " green("{}") " workers", 1);
    auto start = std::chrono::high_resolution_clock::now();
    auto result = evaluate_naive(charms, config.max_cp, config.to_weights());
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> time = end - start;
    std::println(std::cout, "Charm eval took " green("{:.4f}") " milliseconds", time.count());

    print_results(result, charms, config);
}
