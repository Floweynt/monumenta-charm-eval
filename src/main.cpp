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

using namespace mtce;

namespace
{
    inline constexpr size_t CAPPED_COLOR = 0xe49b20;
    inline constexpr size_t NORMAL_COLOR = 0x4ac2e5;
    inline constexpr size_t NEGATIVE_COLOR = 0xd02e28;

    constexpr auto format(size_t ability, double value) -> std::string
    {
        if (EFFECT_ROUND_TO_INTEGER.at(ability))
        {
            value = std::round(value);
        }
        else
        {
            value = std::round(value * 100) / 100.0;
        }

        return std::format("{:+.2f}{}", value, EFFECT_IS_PERCENT.at(ability) ? "%" : "");
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
    auto res = evaluate_naive(charms, config.max_cp, config.to_weights());
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> time = end - start;
    std::println(std::cout, "Charm eval took " green("{:.4f}") " milliseconds", time.count());

    // dump data
    // warning - this algo is ugly
    std::println(std::cout, "Optimal charm set: ");
    std::uint32_t curr_cp = 0;

    std::array<double, ABILITY_COUNT> charm_set_data{};

    for (const auto charm_index : res)
    {
        const auto& charm = charms[charm_index];
        std::println(std::cout, "  {} (" green("{}") ")", colorize(charm.color, charm.name), charm.charm_power);
        curr_cp += charm.charm_power;

        for (size_t ability = 0; ability < ABILITY_COUNT; ability++)
        {
            charm_set_data.at(ability) += charm.charm_data.at(ability);
        }
    }

    std::println(std::cout, "Charm power used: " green("{}") "/" green("{}"), curr_cp, config.max_cp);

    std::println(std::cout, "Set stats: ");

    for (const auto& [ability, _discard] : config.ability_weights)
    {
        auto index = NAME_TO_ID.at(ability);
        auto display_name = EFFECT_DISPLAY_NAMES.at(index);
        auto caps = EFFECT_CAPS.at(index);
        auto value = charm_set_data.at(index);

        bool is_negative = caps < 0;

        if (value == 0)
        {
            std::println(std::cout, "  " gray("{}"), display_name);
        }
        else if ((value < 0) == is_negative)
        {
            // "good" charm - check for caps
            if (std::abs(caps) < std::abs(value))
            {
                // capped - display overflow
                std::println(std::cout, "  {}", colorize(CAPPED_COLOR, std::format("{} {}", display_name, format(index, caps))));
            }
            else
            {
                // uncapped
                std::println(std::cout, "  {}", colorize(NORMAL_COLOR, std::format("{} {}", display_name, format(index, value))));
            }
        }
        else
        {
            // bad stat wtf!!
            std::println(std::cout, "  {}", colorize(NEGATIVE_COLOR, std::format("{} {}", display_name, format(index, value))));
        }
    }
}
