#include "build_config.h"
#include "cli/cli.h"
#include "cli/color.h"
#include "common/charm.h"
#include "common/eval.h"
#include "common/gen/charm_data.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <iostream>
#include <numeric>
#include <print>
#include <ratio>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
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

    void print_charm_stats(const eval_result& result, const std::vector<charm>& charms, const config& cfg)
    {
        std::array<double, ABILITY_COUNT> stats{};

        for (const auto charm_index : result.charms)
        {
            const auto& charm = charms[charm_index];
            for (size_t ability = 0; ability < ABILITY_COUNT; ability++)
            {
                stats.at(ability) += charm.charm_data.at(ability);
            }
        }
        
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

        for (const auto charm_index : result.charms)
        {
            const auto& charm = charms[charm_index];
            std::println(std::cout, "  {} (" green("{}") ")", colorize(charm.color, charm.name), charm.charm_power);
            curr_cp += charm.charm_power;
        }

        std::println(std::cout, "Charm power used: " green("{}") "/" green("{}"), curr_cp, config.max_cp);
        std::println(std::cout, "Set stats: ");
        print_charm_stats(result, charms, config);
    }

    struct algo_info_printer
    {
        void operator()(naive_algo_flags flags)
        {
            std::println(std::cout, "MTCE algorithm: " yellow("naive") " with " green("{}") " worker(s)", flags.threads);
        }
    };

    struct algo_invoker
    {
        const std::vector<charm>& charms;
        const config& config;
        bool print_algo;

        // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
        static void naive_trace_prune(std::vector<std::string_view>& abilities, std::vector<std::string_view>& charms)
        {
            std::println(std::cout, gray("prune - only considering abilities:"));
            for (const auto ability : abilities)
            {
                std::println(std::cout, gray("  - {}"), ability);
            }

            std::println(std::cout, gray("prune - only considering charms:"));
            for (const auto charm : charms)
            {
                std::println(std::cout, gray("  - {}"), charm);
            }
        }

        auto operator()(naive_algo_flags flags) -> eval_result
        {
            naive_tracing_config trace;

            if (flags.enable_trace)
            {
                trace.trace_prune = algo_invoker::naive_trace_prune;
            }

            return evaluate_naive(
                {
                    .charms = charms,
                    .max_cp = config.max_cp,
                    .weights = config.to_weights(),
                    .threads = flags.threads,
                },
                trace
            );
        }
    };
} // namespace

auto main(int argc, const char* const* argv) -> int
{
    auto [config, in, benchmark, algo, bot_mode] = parse_args(argc, argv);
    auto enable_benchmark = benchmark != 0;
    auto charms = read_charms(std::string(in));

    auto run_profiled = [&]() {
        auto start = std::chrono::high_resolution_clock::now();

        // run the charm evaluation
        auto result = std::visit(
            algo_invoker{
                .charms = charms,
                .config = config,
            },
            algo
        );

        auto end = std::chrono::high_resolution_clock::now();
        return std::make_pair(end - start, result);
    };

    if (bot_mode)
    {
        auto [time, result] = run_profiled();
        std::println(std::cout, "{}", result.utility_value);

        for (auto charm_id : result.charms)
        {
            std::println(std::cout, "{}", charm_id);
        }

        std::println();

        // TODO: add an option to not print ansi escape - currently, we have to strip this in js 
        print_charm_stats(result, charms, config);
    }
    else if (!enable_benchmark)
    {
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

        std::visit(algo_info_printer{}, algo);

        auto [time, result] = run_profiled();

        std::println(std::cout, "Charm eval took " green("{:.4f}") " milliseconds", std::chrono::duration<double, std::milli>(time).count());
        print_results(result, charms, config);
    }
    else
    {
        // run the charm evaluation
        auto result = std::visit(
            algo_invoker{
                .charms = charms,
                .config = config,
            },
            algo
        );

        std::vector<double> times;

        times.reserve(benchmark);

        for (size_t i = 0; i < benchmark; i++)
        {
            auto [time, ignored] = run_profiled();
            auto ns_count = std::chrono::duration<double, std::nano>(time).count();
            times.push_back(ns_count);
            std::println(std::cout, "run " green("{}") " took " green("{:.4f}") " ns", i, ns_count);
        }

        double sum = std::accumulate(times.begin(), times.end(), 0.0);
        double mean = sum / times.size();

        std::vector<double> diff(times.size());
        std::ranges::transform(times, diff.begin(), [mean](double x) { return x - mean; });
        double sq_sum = std::inner_product(diff.begin(), diff.end(), diff.begin(), 0.0);
        double stddev = std::sqrt(sq_sum / times.size()) / times.size();

        std::println(std::cout, "mean = " green("{:.4f}") " stddev = " green("{:.4f}"), mean, stddev);
    }
}
